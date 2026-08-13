#include "wtpch.h"
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
#include "Panels/EditorCommands.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SpriteSheetPickerPanel.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/KeyCodes.h"
#include "Wheatear/Core/MouseButtonCodes.h"
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
#include <vector>

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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport");

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

        const auto w = static_cast<uint32_t>(std::max(m_ViewportSize.x, 0.0f));
        const auto h = static_cast<uint32_t>(std::max(m_ViewportSize.y, 0.0f));
        if (m_Framebuffer && m_ActiveScene && w > 0 && h > 0)
        {
            const auto& spec = m_Framebuffer->GetSpecification();
            if (spec.Width != w || spec.Height != h)
            {
                m_Framebuffer->Resize(w, h);
                m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
                m_ActiveScene->OnViewportResize(w, h);
                m_ActiveScene->SetViewportOffset(m_ViewportBounds[0].x, m_ViewportBounds[0].y);

                m_Framebuffer->Bind();
                RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
                RenderCommand::Clear();
                m_Framebuffer->ClearAttachment(2, -1);

                if (m_SceneState == SceneState::Play)
                {
                    Entity cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
                    if (cameraEntity
                        && cameraEntity.HasComponent<CameraComponent>()
                        && cameraEntity.HasComponent<TransformComponent>())
                    {
                        m_ActiveScene->RenderWithSceneCamera(
                            cameraEntity.GetComponent<CameraComponent>().Camera,
                            cameraEntity.GetComponent<TransformComponent>().GetTransform(),
                            true);
                    }
                }
                else
                {
                    m_ActiveScene->OnUpdateEditor(Timestep(0.0f), m_EditorCamera);
                }

                m_Framebuffer->Unbind();
            }
        }

        const uint64_t texID = m_Framebuffer->GetColorAttachmentRendererID();
        // OpenGL and ImGui use opposite vertical UV directions.
        ImGui::Image(
            reinterpret_cast<void*>(texID),
            ImVec2{ m_ViewportSize.x, m_ViewportSize.y },
            ImVec2{ 0, 1 },
            ImVec2{ 1, 0 }
        );
        m_ViewportImageHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);

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
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 32.0f),
            ImGuiCond_Always,
            ImVec2(0.5f, 0.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.78f, 0.82f, 0.81f, 0.94f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.54f, 0.66f, 0.65f, 0.62f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.88f, 0.91f, 0.90f, 0.78f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.74f, 0.84f, 0.83f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.64f, 0.77f, 0.76f, 1.0f));

        ImGui::Begin("##toolbar", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav);

        const ImVec2 iconSize(28.0f, 28.0f);
        auto sameLine = []() { ImGui::SameLine(0.0f, 6.0f); };
        auto groupGap = []()
        {
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0.0f, 10.0f);
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
        if (EditorWidgets::IconButton("##ToolbarNewScene", m_IconNewScene, "New Scene (Ctrl+N)", iconSize))
            NewScene();
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarOpenScene", m_IconOpenScene, "Open Scene (Ctrl+O)", iconSize))
            OpenScene();
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarSaveScene", m_IconSaveScene, "Save Scene (Ctrl+S)", iconSize))
            SaveScene();

        groupGap();

        const Ref<Texture2D>& playStopIcon = (m_SceneState == SceneState::Edit) ? m_IconPlay : m_IconStop;
        const char* playStopTooltip = (m_SceneState == SceneState::Edit) ? "Play (Ctrl+Enter)" : "Stop (Ctrl+Enter)";
        if (EditorWidgets::IconButton("##ToolbarPlayStop", playStopIcon, playStopTooltip, ImVec2(34.0f, 34.0f)))
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
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarSpriteSheet", m_IconSpriteSheet, "Sprite Sheet Picker", iconSize))
            m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarEventGraph", m_IconEventGraph, "Event Script Editor", iconSize))
            openToolByLabel("Event Script Editor");

        groupGap();

        if (EditorWidgets::IconButton("##ToolbarHealth", m_IconHealth, "Project Health", iconSize))
            openToolByLabel("Project Health");
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarPackage", m_IconPackage, "Package Player + Editor", iconSize, !m_PlayerBuildRunning))
            StartPlayerPackageBuild(false);
        sameLine();
        if (EditorWidgets::IconButton("##ToolbarResetLayout", m_IconResetLayout, "Reset Window Layout", iconSize))
        {
            m_RequestDefaultDockspaceLayout = true;
            m_DefaultDockspaceLayoutBuilt = false;
        }

        ImGui::End();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(4);
    }
} // namespace Wheatear
