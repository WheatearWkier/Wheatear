#include "wepch.h"
#include "EditorLayerBase.h"
#include "Assets/AssetRegistry.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/EditorCanvasTools.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorPlatform.h"
#include "Editor/EditorToolRegistry.h"
#include "Editor/EditorWidgets.h"
#include "Panels/AnimationEditorPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Editor/EditorCommands.h"
#include "Panels/SceneHierarchy/SceneHierarchyPanel.h"
#include "Panels/SpriteSheetPickerPanel.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Events/Event.h"
#include "Wheatear/Events/KeyEvent.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/ImGui/ImGuiLayer.h"
#include "Wheatear/Math/Math.h"
#include "Wheatear/Renderer/Framebuffer.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneSerializer.h"
#include "Wheatear/UI/UIInputSystem.h"
#include "Wheatear/UI/UIWidgetLayout.h"
#include "Wheatear/Utils/PlatformUtils.h"
#include <ImGuizmo.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <limits>
#include <memory>
#include <system_error>
#include <unordered_set>

namespace Wheatear {

    namespace {

        static std::filesystem::path ResolveContentBrowserPayloadPath(const ImGuiPayload* payload)
        {
            if (!payload || !payload->Data || payload->DataSize <= 0)
                return {};

            const size_t charCount = static_cast<size_t>(payload->DataSize) / sizeof(wchar_t);
            if (charCount == 0)
                return {};

            const wchar_t* rawPath = static_cast<const wchar_t*>(payload->Data);
            std::wstring relativeText(rawPath, rawPath + charCount);
            if (!relativeText.empty() && relativeText.back() == L'\0')
                relativeText.pop_back();
            if (relativeText.empty())
                return {};

            std::filesystem::path relativePath(relativeText);
            if (relativePath.is_absolute())
                return {};

            for (const auto& part : relativePath)
            {
                if (part.generic_string() == "..")
                    return {};
            }

            return (GetEditorAssetPath() / relativePath).lexically_normal();
        }
        static bool IsEditorHidden(Entity entity)
        {
            return entity && entity.HasComponent<EditorHiddenComponent>();
        }

        static bool IsEditorUIHidden(Entity entity,
            UIWidgetLayout::Context& layout,
            std::unordered_set<uint32_t>& visiting)
        {
            if (!entity || !layout.ScenePtr)
                return false;

            if (entity.HasComponent<EditorHiddenComponent>())
                return true;

            if (!entity.HasComponent<UIWidgetComponent>())
                return false;

            const uint32_t key = static_cast<uint32_t>(static_cast<entt::entity>(entity));
            if (!visiting.insert(key).second)
                return false;

            auto& registry = layout.ScenePtr->GetRegistry();
            const auto& widget = entity.GetComponent<UIWidgetComponent>();
            const entt::entity parentID = layout.ResolveReference(widget.ParentEntity);
            const bool hidden = parentID != entt::null && registry.valid(parentID)
                ? IsEditorUIHidden(Entity{ parentID, layout.ScenePtr }, layout, visiting)
                : false;
            visiting.erase(key);
            return hidden;
        }


    } // namespace

    void EditorLayerBase::UI_Viewport()
    {
        if (m_RequestViewportFocus)
        {
            // Select the Viewport tab inside its dock node. FocusWindow() does
            // not change the selected tab, so click the tab through the
            // internal tab bar state once the window exists.
            if (ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport"))
            {
                if (viewportWindow->DockNode && viewportWindow->DockNode->TabBar)
                {
                    viewportWindow->DockNode->TabBar->NextSelectedTabId =
                        viewportWindow->TabId;
                    m_RequestViewportFocus = false;
                }
            }
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport");

        // Compact in-viewport tool strip. It is part of the panel, not a
        // floating overlay, so it never covers scene tabs or the status bar.
        const float uiScale = Application::Get().GetImGuiLayer()
            ? Application::Get().GetImGuiLayer()->GetUiScale()
            : 1.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
            ImVec2(8.0f * uiScale, 4.0f * uiScale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
            ImVec2(4.0f * uiScale, 4.0f * uiScale));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
        ImGui::BeginChild("##ViewportToolStrip",
            ImVec2(0.0f, 40.0f * uiScale),
            false, ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
        auto gizmoTool = [&](const char* label, int operation)
        {
            const bool active = m_GizmoType == operation;
            if (active)
            {
                ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
                accent.w = 0.22f;
                ImGui::PushStyleColor(ImGuiCol_Button, accent);
            }
            if (ImGui::SmallButton(label))
                m_GizmoType = operation;
            if (active)
                ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        gizmoTool(EditorLocale::Text("Select (Q)", "选择 (Q)"), -1);
        gizmoTool(EditorLocale::Text("Move (W)", "移动 (W)"), ImGuizmo::OPERATION::TRANSLATE);
        gizmoTool(EditorLocale::Text("Rotate (E)", "旋转 (E)"), ImGuizmo::OPERATION::ROTATE);
        gizmoTool(EditorLocale::Text("Scale (R)", "缩放 (R)"), ImGuizmo::OPERATION::SCALE);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical,
            18.0f * uiScale);
        ImGui::SameLine();
        if (ImGui::SmallButton(EditorLocale::Text("Frame (F)", "聚焦 (F)")))
        {
            if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
                ActivateHierarchyEntity(selected);
            else
                FrameEditorCameraOnScene();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s",
            m_SceneState == SceneState::Play
                ? EditorLocale::Text("Play mode", "播放模式")
                : EditorLocale::Text("Edit mode", "编辑模式"));
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        const ImVec2 viewportOffset = ImGui::GetCursorPos();
        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        Application::Get().GetImGuiLayer()->BlockEvents(
            m_SceneState != SceneState::Play && !m_ViewportFocused && !m_ViewportHovered);

        const ImVec2 panelSize = ImGui::GetContentRegionAvail();
        m_ViewportSize = { panelSize.x, panelSize.y };
        m_ViewportImageHovered = false;

        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 minBound = { windowPos.x + viewportOffset.x, windowPos.y + viewportOffset.y };
        m_ViewportBounds[0] = { minBound.x, minBound.y };
        m_ViewportBounds[1] = { minBound.x + m_ViewportSize.x, minBound.y + m_ViewportSize.y };

        // Framebuffer resizing is deferred to OnUpdate, before the scene is
        // rendered. Resizing and rendering here produced a clear frame with
        // no 3D scene pass (OnBeginRender/skybox were skipped), which showed
        // up as a black flash between two sky frames while dragging a splitter.
        // The old texture is stretched for this one UI frame instead.

        const uint64_t texID = m_Framebuffer->GetColorAttachmentRendererID();
        // OpenGL and ImGui use opposite vertical UV directions.
        ImGui::Image(
            reinterpret_cast<void*>(texID),
            ImVec2{ m_ViewportSize.x, m_ViewportSize.y },
            ImVec2{ 0, 1 },
            ImVec2{ 1, 0 }
        );
        m_ViewportImageHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);

        // Right-click is reserved for camera navigation (hold + drag). The old
        // hierarchy create-entity popup was opened on right-click here, which
        // interrupted orbiting and showed a menu that belongs to the hierarchy.

        if (m_SceneState == SceneState::Edit
            && m_ViewportHovered
            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            if (m_HoveredEntity
                && !IsEditorHidden(m_HoveredEntity)
                && m_HoveredEntity.HasComponent<TransformComponent>())
                FrameEditorCameraOnEntity(m_HoveredEntity);
            else
                FrameEditorCameraOnScene();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                const std::filesystem::path fullPath = ResolveContentBrowserPayloadPath(payload);
                if (!fullPath.empty())
                {
                    if (fullPath.extension() == AssetFileType::SceneExtension)
                        m_DeferredSceneOpenPath = fullPath;
                    else if (fullPath.extension() == AssetFileType::PrefabExtension)
                        m_DeferredPrefabInstantiatePath = fullPath;
                    else if (fullPath.extension() == AssetFileType::UITemplateExtension)
                        m_DeferredUITemplateInstantiatePath = fullPath;
                }
            }

            // A sprite-sheet cell/rect dragged from the content browser: spawn
            // a sprite entity bound to (sheet, cell-or-rect) once the viewport
            // is idle. Payload: <path>\ncell:<index> | <path>\nrect:<name>.
            if (const ImGuiPayload* cellPayload = ImGui::AcceptDragDropPayload("SPRITE_SHEET_CELL"))
            {
                std::wstring text(static_cast<const wchar_t*>(cellPayload->Data));
                const size_t sep = text.find(L'\n');
                if (sep != std::wstring::npos)
                {
                    m_DeferredSheetCellPath = std::filesystem::path(text.substr(0, sep)).string();
                    const std::wstring ref = text.substr(sep + 1);
                    if (ref.rfind(L"rect:", 0) == 0)
                    {
                        m_DeferredSheetCellSubRect = std::filesystem::path(ref.substr(5)).string();
                        m_DeferredSheetCellIndex = -1;
                    }
                    else
                    {
                        const std::wstring indexText = ref.rfind(L"cell:", 0) == 0 ? ref.substr(5) : ref;
                        m_DeferredSheetCellIndex = std::stoi(indexText);
                        m_DeferredSheetCellSubRect.clear();
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
        const bool selectedIsUI = selected && selected.HasComponent<UIWidgetComponent>();
        const bool selectedHiddenInEditor = selected && IsEditorHidden(selected);

        // Gizmo
        if (selected && !selectedIsUI && !selectedHiddenInEditor && m_GizmoType != -1 && m_SceneState == SceneState::Edit)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
                m_ViewportSize.x,
                m_ViewportSize.y);

            const glm::mat4 cameraProj = m_EditorCamera.GetProjection();
            const glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

            auto& tc        = selected.GetComponent<TransformComponent>();
            const TransformComponent beforeGizmo = tc;
            glm::mat4 transform = tc.GetTransform();

            const bool  snap      = Input::IsKeyPressed(WT_KEY_LEFT_CONTROL);
            const float snapValue = (m_GizmoType == ImGuizmo::OPERATION::ROTATE) ? 45.0f : 0.5f;
            float snapArr[3] = { snapValue, snapValue, snapValue };
            float* snapPtr   = snap ? snapArr : nullptr;

            ImGuizmo::Manipulate(
                glm::value_ptr(cameraView), glm::value_ptr(cameraProj),
                static_cast<ImGuizmo::OPERATION>(m_GizmoType),
                ImGuizmo::LOCAL, glm::value_ptr(transform),
                nullptr, snapPtr);

            const bool gizmoUsing = ImGuizmo::IsUsing();
            if (gizmoUsing && !m_GizmoWasUsing)
            {
                m_GizmoWasUsing = true;
                m_GizmoEditEntity = selected;
                m_GizmoStartTransform = std::make_unique<TransformComponent>(beforeGizmo);
            }

            if (gizmoUsing)
            {
                glm::vec3 t, r, s;
                Math::DecomposeTransform(transform, t, r, s);
                tc.Translation  = t;
                tc.Rotation    += r - tc.Rotation;
                tc.Scale        = s;
            }
        }

        if (m_GizmoWasUsing && !ImGuizmo::IsUsing())
            CommitPendingGizmoEdit();

        ImGui::End();
        ImGui::PopStyleVar();
    }
    void EditorLayerBase::UI_Toolbar()
    {
        // Fixed chrome: the action bar lives between the menu bar and the
        // dockspace, so it can never cover scene tabs or the viewport. It can
        // be collapsed from the chevron on the right (or Window > Action
        // Toolbar); the dockspace then reclaims the full height.
        const ImGuiLayer* gui = Application::Get().GetImGuiLayer();
        const float s = gui ? gui->GetUiScale() : 1.0f;
        const float toolbarHeight = kToolbarHeight * s;
        const bool darkTheme = !gui || gui->IsDarkTheme();
        const ImVec4 barBg = darkTheme
            ? ImVec4(0.062f, 0.070f, 0.088f, 1.0f)
            : ImVec4(0.925f, 0.914f, 0.890f, 1.0f);
        const ImVec4 barBorder = darkTheme
            ? ImVec4(0.16f, 0.18f, 0.22f, 1.0f)
            : ImVec4(0.78f, 0.76f, 0.72f, 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
            ImVec2(12.0f * s, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
            ImVec2(4.0f * s, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f * s);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, barBg);
        ImGui::BeginChild("##EditorActionBar",
            ImVec2(0.0f, toolbarHeight),
            false, ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoNav);

        const ImVec2 iconSize(22.0f * s, 22.0f * s);
        const float buttonY = 5.0f * s;
        auto sameLine = [s]() { ImGui::SameLine(0.0f, 4.0f * s); };
        auto groupGap = [&barBorder, s]()
        {
            ImGui::SameLine(0.0f, 9.0f * s);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(p.x, p.y + 7.0f * s),
                ImVec2(p.x, p.y + 25.0f * s),
                ImGui::ColorConvertFloat4ToU32(barBorder), 1.0f);
            ImGui::Dummy(ImVec2(1.0f, 24.0f * s));
            ImGui::SameLine(0.0f, 9.0f * s);
        };
        auto openToolByLabel = [&](const char* label)
        {
            const EditorToolContext toolContext{ m_SceneHierarchyPanel->GetSelectedEntity() };
            EditorToolRegistry::ForEach([&](const EditorToolDescriptor& tool)
            {
                if (tool.MenuLabel == label && tool.Open)
                    tool.Open(toolContext);
            });
        };

        ImGui::SetCursorPosY(buttonY);
        if (EditorWidgets::IconButton("##ToolbarNewScene", m_IconNewScene, "New Scene (Ctrl+N)", iconSize))
            NewScene();
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarOpenScene", m_IconOpenScene, "Open Scene (Ctrl+O)", iconSize))
            OpenScene();
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarSaveScene", m_IconSaveScene, "Save Scene (Ctrl+S)", iconSize))
            SaveScene();

        groupGap();

        if (EditorWidgets::IconButton("##ToolbarUndo", m_IconUndo,
                EditorLocale::Text("Undo (Ctrl+Z)", "撤销 (Ctrl+Z)"), iconSize,
                CommandHistory::Get().CanUndo()))
            CommandHistory::Get().Undo();
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarRedo", m_IconRedo,
                EditorLocale::Text("Redo (Ctrl+Y)", "重做 (Ctrl+Y)"), iconSize,
                CommandHistory::Get().CanRedo()))
            CommandHistory::Get().Redo();

        groupGap();

        const Ref<Texture2D>& playStopIcon = (m_SceneState == SceneState::Edit) ? m_IconPlay : m_IconStop;
        const char* playStopTooltip = (m_SceneState == SceneState::Edit) ? "Play (Ctrl+Enter)" : "Stop (Ctrl+Enter)";
        if (EditorWidgets::IconButton("##ToolbarPlayStop", playStopIcon, playStopTooltip, ImVec2(28.0f * s, 28.0f * s)))
        {
            if (m_SceneState == SceneState::Edit)
                TransitionToPlay();
            else
                TransitionToStop();
        }

        if (m_SceneState == SceneState::Play)
        {
            sameLine();
            const char* pauseTooltip = m_PlayPaused
                ? "Resume (F6)" : "Pause (F6)";
            if (EditorWidgets::IconButton("##ToolbarPause",
                    m_PlayPaused ? m_IconPlay : m_IconPause,
                    pauseTooltip, iconSize))
            {
                TogglePlayPause();
            }
            sameLine();
            if (EditorWidgets::IconButton("##ToolbarStep", m_IconStep,
                    "Step One Frame (F7)", iconSize))
            {
                StepPlayFrame();
            }
        }
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarFocus", m_IconFocus, "Frame Selection (F)", iconSize))
        {
            if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
                ActivateHierarchyEntity(selected);
            else
                FrameEditorCameraOnScene();
        }

        groupGap();

        if (EditorWidgets::IconButton("##ToolbarUICanvas", m_IconUICanvas, "UI Canvas Editor", iconSize))
        {
            m_UIEditorOpen = true;
            EditorFloatingWindow::Dock("UI Canvas Editor");
            m_FocusCanvasEditor = true;
            if (!m_UIEditingCanvas)
                m_UIEditingCanvas = m_SceneHierarchyPanel->GetSelectedEntity();
        }
        if (!Is3DWorkspace())
        {
            sameLine();
            if (EditorWidgets::IconButton("##ToolbarSpriteSheet", m_IconSpriteSheet, "Sprite Sheet Picker", iconSize))
                m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
            sameLine();
            if (EditorWidgets::IconButton("##ToolbarAnimation", m_IconFilm,
                    EditorLocale::Text("Animation Editor", "动画编辑器"), iconSize))
            {
                m_AnimationEditorOpen = true;
                EditorFloatingWindow::Dock("Animation Editor");
            }
        }
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarEventGraph", m_IconEventGraph, "Event Script Editor", iconSize))
            openToolByLabel("Event Script Editor");

        groupGap();

        if (EditorWidgets::IconButton("##ToolbarHealth", m_IconHealth, "Project Health", iconSize))
            openToolByLabel("Project Health");
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarPackage", m_IconPackage, "Package Game + Editor", iconSize, !m_PlayerBuildRunning))
            StartPlayerPackageBuild();
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarResetLayout", m_IconResetLayout, "Reset Window Layout", iconSize))
        {
            m_RequestDefaultDockspaceLayout = true;
            m_DefaultDockspaceLayoutBuilt = false;
        }

        sameLine();
        if (EditorWidgets::IconButton("##ToolbarContentDrawer", m_IconFolder,
                "Content Drawer (Content Browser)", iconSize))
        {
            m_ContentDrawerOpen = !m_ContentDrawerOpen;
            m_ContentBrowserPanel->SetDrawerMode(m_ContentDrawerOpen);
        }

        // Right-aligned collapse chevron.
        const ImVec2 barOrigin = ImGui::GetWindowPos();
        const float barRight = barOrigin.x + ImGui::GetWindowWidth() - 14.0f * s;
        ImGui::SetCursorScreenPos(
            ImVec2(barRight - 24.0f * s, barOrigin.y + 8.0f * s));
        if (ImGui::ArrowButton("##CollapseToolbar", ImGuiDir_Up))
            m_ToolbarVisible = false;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", EditorLocale::Text(
                "Hide toolbar (Window > Action Toolbar)",
                "收起工具栏（窗口 > 操作工具栏）"));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(
            ImVec2(barOrigin.x, barOrigin.y + toolbarHeight - 1.0f),
            ImVec2(barOrigin.x + ImGui::GetWindowWidth(),
                barOrigin.y + toolbarHeight - 1.0f),
            ImGui::ColorConvertFloat4ToU32(barBorder), 1.0f);

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
} // namespace Wheatear
