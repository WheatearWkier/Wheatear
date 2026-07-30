#include "wtpch.h"
#include "EditorLayerBase.h"

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
#include "Wheatear/Renderer/Framebuffer.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneSerializer.h"
#include "Wheatear/Utils/PlatformUtils.h"
#include "Wheatear/UI/UIInputSystem.h"
#include "Wheatear/UI/UIWidgetLayout.h"
#include "Wheatear/Math/Math.h"
#include "Editor/EditorCanvasTools.h"
#include "Editor/EventScriptGraphPanel.h"
#include "Editor/EditorToolRegistry.h"
#include "Panels/AnimationEditorPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/EditorCommands.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SpriteSheetPickerPanel.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <system_error>
#include <unordered_set>
#include <vector>


namespace Wheatear {

    namespace {

        struct ViewportUIEntry
        {
            Entity EntityRef;
            UIWidgetLayout::Rect Rect;
            int SortOrder = 0;
            std::string Name;
        };

        static EventScriptGraphPanel& GetEventScriptGraphPanel()
        {
            static EventScriptGraphPanel panel;
            return panel;
        }

        static ImVec2 ToScreenPoint(const glm::vec2& viewportMin,
            const glm::vec2& viewportSize,
            float normalizedX,
            float normalizedY)
        {
            return {
                viewportMin.x + normalizedX * viewportSize.x,
                viewportMin.y + normalizedY * viewportSize.y
            };
        }

        static bool PointInScreenRect(const ImVec2& point, const ImVec2& min, const ImVec2& max)
        {
            return point.x >= min.x && point.x <= max.x
                && point.y >= min.y && point.y <= max.y;
        }

        static bool PointInNormalizedRect(const UIWidgetLayout::Rect& rect, const glm::vec2& point)
        {
            return point.x >= rect.Left && point.x <= rect.Right
                && point.y >= rect.Top && point.y <= rect.Bottom;
        }

        static UIWidgetLayout::Rect Vec4ToRect(const glm::vec4& value)
        {
            return { value.x, value.y, value.z, value.w };
        }

        static glm::vec4 RectToVec4(const UIWidgetLayout::Rect& rect)
        {
            return { rect.Left, rect.Right, rect.Top, rect.Bottom };
        }

        static void ApplyLocalRectToWidget(UIWidgetComponent& widget, UIWidgetLayout::Rect rect)
        {
            constexpr float minSize = 0.001f;
            if (rect.Right < rect.Left + minSize)
                rect.Right = rect.Left + minSize;
            if (rect.Bottom < rect.Top + minSize)
                rect.Bottom = rect.Top + minSize;

            const float width = rect.Right - rect.Left;
            const float height = rect.Bottom - rect.Top;
            const float centerX = (rect.Left + rect.Right) * 0.5f;
            const float centerY = (rect.Top + rect.Bottom) * 0.5f;

            widget.Size = { width, height };
            switch (widget.Anchor)
            {
            case UIAnchor::TopLeft:      widget.Position = { rect.Left, rect.Top }; break;
            case UIAnchor::TopCenter:    widget.Position = { centerX, rect.Top }; break;
            case UIAnchor::TopRight:     widget.Position = { rect.Right, rect.Top }; break;
            case UIAnchor::MiddleLeft:   widget.Position = { rect.Left, centerY }; break;
            case UIAnchor::MiddleCenter: widget.Position = { centerX, centerY }; break;
            case UIAnchor::MiddleRight:  widget.Position = { rect.Right, centerY }; break;
            case UIAnchor::BottomLeft:   widget.Position = { rect.Left, rect.Bottom }; break;
            case UIAnchor::BottomCenter: widget.Position = { centerX, rect.Bottom }; break;
            case UIAnchor::BottomRight:  widget.Position = { rect.Right, rect.Bottom }; break;
            }
        }

        static int HitResizeHandle(const ImVec2& mouse, const ImVec2& min, const ImVec2& max, float size)
        {
            const float centerX = (min.x + max.x) * 0.5f;
            const float centerY = (min.y + max.y) * 0.5f;
            const float half = size * 0.5f;

            auto hit = [&](float x, float y)
            {
                return mouse.x >= x - half && mouse.x <= x + half
                    && mouse.y >= y - half && mouse.y <= y + half;
            };

            if (hit(min.x, min.y)) return UIEdit_TopLeft;
            if (hit(max.x, min.y)) return UIEdit_TopRight;
            if (hit(min.x, max.y)) return UIEdit_BottomLeft;
            if (hit(max.x, max.y)) return UIEdit_BottomRight;
            if (hit(centerX, min.y)) return UIEdit_Top;
            if (hit(centerX, max.y)) return UIEdit_Bottom;
            if (hit(min.x, centerY)) return UIEdit_Left;
            if (hit(max.x, centerY)) return UIEdit_Right;
            return UIEdit_None;
        }

        static ImGuiMouseCursor CursorForUIHandle(int handle)
        {
            switch (handle)
            {
            case UIEdit_Left:
            case UIEdit_Right:
                return ImGuiMouseCursor_ResizeEW;
            case UIEdit_Top:
            case UIEdit_Bottom:
                return ImGuiMouseCursor_ResizeNS;
            case UIEdit_TopLeft:
            case UIEdit_BottomRight:
                return ImGuiMouseCursor_ResizeNWSE;
            case UIEdit_TopRight:
            case UIEdit_BottomLeft:
                return ImGuiMouseCursor_ResizeNESW;
            case UIEdit_Move:
                return ImGuiMouseCursor_Hand;
            default:
                return ImGuiMouseCursor_Arrow;
            }
        }

        static void DrawUIHandle(ImDrawList* drawList, const ImVec2& center, float size, ImU32 color)
        {
            const float half = size * 0.5f;
            drawList->AddRectFilled(
                { center.x - half, center.y - half },
                { center.x + half, center.y + half },
                color,
                1.5f);
            drawList->AddRect(
                { center.x - half, center.y - half },
                { center.x + half, center.y + half },
                IM_COL32(8, 20, 24, 230),
                1.5f,
                0,
                1.0f);
        }

        static void DrawDashedLine(ImDrawList* drawList,
            ImVec2 start,
            ImVec2 end,
            ImU32 color,
            float thickness,
            float dashLength = 7.0f,
            float gapLength = 5.0f)
        {
            const ImVec2 delta = { end.x - start.x, end.y - start.y };
            const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (length <= 0.001f)
                return;

            const ImVec2 direction = { delta.x / length, delta.y / length };
            float cursor = 0.0f;
            while (cursor < length)
            {
                const float segmentEnd = std::min(cursor + dashLength, length);
                drawList->AddLine(
                    { start.x + direction.x * cursor, start.y + direction.y * cursor },
                    { start.x + direction.x * segmentEnd, start.y + direction.y * segmentEnd },
                    color,
                    thickness);
                cursor += dashLength + gapLength;
            }
        }

        static void DrawDashedRect(ImDrawList* drawList,
            const ImVec2& min,
            const ImVec2& max,
            ImU32 color,
            float thickness)
        {
            DrawDashedLine(drawList, min, { max.x, min.y }, color, thickness);
            DrawDashedLine(drawList, { max.x, min.y }, max, color, thickness);
            DrawDashedLine(drawList, max, { min.x, max.y }, color, thickness);
            DrawDashedLine(drawList, { min.x, max.y }, min, color, thickness);
        }

        static bool UsesDashedUIOutline(Entity entity)
        {
            return entity
                && entity.HasComponent<UITextComponent>()
                && !entity.HasComponent<UIPanelComponent>()
                && !entity.HasComponent<UIImageComponent>()
                && !entity.HasComponent<UIButtonComponent>()
                && !entity.HasComponent<UIProgressBarComponent>()
                && !entity.HasComponent<UISliderComponent>()
                && !entity.HasComponent<UICheckboxComponent>();
        }

        static void DrawUIOutline(ImDrawList* drawList,
            const ImVec2& rectMin,
            const ImVec2& rectMax,
            Entity entity,
            ImU32 color,
            float thickness)
        {
            if (UsesDashedUIOutline(entity))
                DrawDashedRect(drawList, rectMin, rectMax, color, thickness);
            else
                drawList->AddRect(rectMin, rectMax, color, 0.0f, 0, thickness);
        }

        static ImU32 ColorToImU32(const glm::vec4& color)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
        }

        static Entity FindOwningCanvas(Entity entity, UIWidgetLayout::Context& layout)
        {
            if (!entity || !layout.ScenePtr)
                return {};
            if (entity.HasComponent<UICanvasComponent>())
                return entity;
            if (!entity.HasComponent<UIWidgetComponent>())
                return {};

            auto& registry = layout.ScenePtr->GetRegistry();
            std::unordered_set<uint32_t> visited;
            Entity current = entity;
            while (current && current.HasComponent<UIWidgetComponent>())
            {
                const uint32_t currentKey = static_cast<uint32_t>(static_cast<entt::entity>(current));
                if (!visited.insert(currentKey).second)
                    return {};

                const auto& widget = current.GetComponent<UIWidgetComponent>();
                const entt::entity parentID = layout.ResolveReference(widget.ParentEntity, widget.ParentTag);
                if (parentID == entt::null || !registry.valid(parentID))
                    return {};

                Entity parent{ parentID, layout.ScenePtr };
                if (parent.HasComponent<UICanvasComponent>())
                    return parent;

                current = parent;
            }

            return {};
        }

        static bool BelongsToCanvas(Entity entity, Entity canvas, UIWidgetLayout::Context& layout)
        {
            if (!entity || !canvas)
                return false;
            if (entity == canvas)
                return true;
            return FindOwningCanvas(entity, layout) == canvas;
        }

    } // namespace

    void EditorLayerBase::OnImGuiRender()
    {
        WT_PROFILE_FUNCTION();

        static bool dockspaceOpen   = true;
        static bool opt_fullscreen  = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                          | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", &dockspaceOpen, window_flags);
        ImGui::PopStyleVar();
        if (opt_fullscreen) ImGui::PopStyleVar(2);

        ImGuiIO&    io    = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        const float prevMinX = style.WindowMinSize.x;
        style.WindowMinSize.x = 400.0f;
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID id = ImGui::GetID("MyDockSpace");
            BuildDefaultDockspaceLayout(id);
            ImGui::DockSpace(id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        style.WindowMinSize.x = prevMinX;

        UI_MenuBar();

        m_SceneHierarchyPanel->OnImGuiRender();
        m_ContentBrowserPanel->OnImGuiRender();
        m_AnimationEditorPanel->SetEntity(m_SceneHierarchyPanel->GetSelectedEntity());
        m_AnimationEditorPanel->OnImGuiRender(m_LastTimestep);
        m_SpriteSheetPickerPanel->SetEntity(m_SceneHierarchyPanel->GetSelectedEntity());
        m_SpriteSheetPickerPanel->OnImGuiRender();
        EditorToolRegistry::ForEach([](const EditorToolDescriptor& tool)
        {
            if (tool.Draw)
                tool.Draw();
        });
        GetEventScriptGraphPanel().OnImGuiRender();

        UI_Stats();
        UI_PlayerBuildStatus();

        OnImGuiExtra();

        UI_CanvasEditor();
        UI_Viewport();
        UI_Toolbar();

        ImGui::End();
    }

    void EditorLayerBase::BuildDefaultDockspaceLayout(uint32_t dockspaceID)
    {
        if (!m_RequestDefaultDockspaceLayout || m_DefaultDockspaceLayoutBuilt)
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace = static_cast<ImGuiID>(dockspaceID);

        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodePos(dockspace, viewport->WorkPos);
        ImGui::DockBuilderSetNodeSize(dockspace, viewport->WorkSize);

        ImGuiID main = dockspace;
        ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.22f, nullptr, &main);
        ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.27f, nullptr, &main);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.27f, nullptr, &main);
        ImGuiID bottomRight = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.45f, nullptr, &bottom);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.30f, nullptr, &right);

        ImGui::DockBuilderDockWindow("Scene Hierarchy", left);
        ImGui::DockBuilderDockWindow("Properties", right);
        ImGui::DockBuilderDockWindow("Stats", rightBottom);
        ImGui::DockBuilderDockWindow("Player Build", rightBottom);
        ImGui::DockBuilderDockWindow("Content Browser", bottom);
        ImGui::DockBuilderDockWindow("Animation Editor", bottomRight);
        ImGui::DockBuilderDockWindow("Sprite Sheet Picker", bottomRight);
        ImGui::DockBuilderDockWindow("UI Canvas Editor", bottomRight);
        ImGui::DockBuilderDockWindow("Viewport", main);

        ImGui::DockBuilderFinish(dockspace);

        if (const char* iniPath = ImGui::GetIO().IniFilename)
            ImGui::SaveIniSettingsToDisk(iniPath);

        m_DefaultDockspaceLayoutBuilt = true;
        m_RequestDefaultDockspaceLayout = false;
    }

    void EditorLayerBase::FocusEditorCameraOnPrimarySceneCamera()
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene)
            return;

        Entity cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
        if (!cameraEntity || !cameraEntity.HasComponent<CameraComponent>() || !cameraEntity.HasComponent<TransformComponent>())
            return;

        const auto& transform = cameraEntity.GetComponent<TransformComponent>();
        m_EditorCamera.SetViewTransform(
            transform.Translation,
            transform.Rotation,
            10.0f);

        m_SceneHierarchyPanel->SetSelectedEntity(cameraEntity);
        m_AnimationEditorPanel->SetEntity(cameraEntity);
    }

    void EditorLayerBase::UI_MenuBar()
    {
        PollPlayerPackageBuild();

        if (!ImGui::BeginMenuBar()) return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New",          "Ctrl+N"))        NewScene();
            if (ImGui::MenuItem("Open...",      "Ctrl+O"))        OpenScene();
            if (ImGui::MenuItem("Save",         "Ctrl+S"))        SaveScene();
            if (ImGui::MenuItem("Save As...",   "Ctrl+Shift+S"))  SaveSceneAs();
            ImGui::Separator();
            if (ImGui::MenuItem("Package Player", nullptr, false, !m_PlayerBuildRunning))
                StartPlayerPackageBuild(false);
            if (ImGui::MenuItem("Package Player With C# Scripts", nullptr, false, !m_PlayerBuildRunning))
                StartPlayerPackageBuild(true);
            if (ImGui::MenuItem("Open Build Folder", nullptr, false, !m_LastPlayerBuildDirectory.empty()))
                PlayerPackager::OpenDirectory(m_LastPlayerBuildDirectory);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) Application::Get().Close();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CommandHistory::Get().CanUndo()))
                CommandHistory::Get().Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z", false, CommandHistory::Get().CanRedo()))
                CommandHistory::Get().Redo();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Focus Primary Camera", "Double Click LMB"))
                FocusEditorCameraOnPrimarySceneCamera();
            ImGui::MenuItem("UI Edit Outlines", nullptr, &m_ShowUIOutlines);
            ImGui::MenuItem("Hide UI In Scene Viewport", nullptr, &m_HideUIInSceneViewport);
            if (ImGui::MenuItem("UI Canvas Editor", nullptr, &m_UIEditorOpen))
            {
                if (!m_UIEditingCanvas)
                    m_UIEditingCanvas = m_SceneHierarchyPanel->GetSelectedEntity();
            }
            const EditorToolContext toolContext{ m_SceneHierarchyPanel->GetSelectedEntity() };
            EditorToolRegistry::ForEach([&](const EditorToolDescriptor& tool)
            {
                if (ImGui::MenuItem(tool.MenuLabel.c_str()) && tool.Open)
                    tool.Open(toolContext);
            });
            if (ImGui::MenuItem("Sprite Sheet Picker"))
                m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
            if (ImGui::MenuItem("Event Script Graph"))
            {
                std::string scriptPath = "assets/events/vertical_slice_flow.wts";
                std::string eventName;
                if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
                {
                    if (selected.HasComponent<EventScriptComponent>())
                    {
                        const auto& script = selected.GetComponent<EventScriptComponent>();
                        scriptPath = script.ScriptPath;
                        eventName = script.StartEvent;
                    }
                }
                GetEventScriptGraphPanel().Open(scriptPath, eventName);
            }
            ImGui::MenuItem("Physics Colliders", nullptr, &m_ShowPhysicsColliders);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Window Layout"))
            {
                m_RequestDefaultDockspaceLayout = true;
                m_DefaultDockspaceLayoutBuilt = false;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
    void EditorLayerBase::UI_Stats()
    {
        ImGui::Begin("Stats");

        const std::string hoveredName = m_HoveredEntity
            ? m_HoveredEntity.GetComponent<TagComponent>().Tag
            : "None";
        ImGui::Text("Hovered Entity: %s", hoveredName.c_str());

        const auto stats = Renderer2D::GetStats();
        ImGui::Text("Renderer2D Stats:");
        ImGui::Text("  Draw Calls: %d", stats.DrawCalls);
        ImGui::Text("  Quads:      %d", stats.QuadCount);
        ImGui::Text("  Vertices:   %d", stats.GetTotalVertexCount());
        ImGui::Text("  Indices:    %d", stats.GetTotalIndexCount());

        ImGui::End();
    }

    void EditorLayerBase::UI_PlayerBuildStatus()
    {
        PollPlayerPackageBuild();

        if (m_PlayerBuildStatus.empty() && !m_PlayerBuildRunning)
            return;

        ImGui::Begin("Player Build");
        ImGui::TextWrapped("%s", m_PlayerBuildStatus.c_str());
        if (m_PlayerBuildRunning)
            ImGui::TextDisabled("MSBuild and asset copy are running in the background...");
        else if (!m_LastPlayerBuildDirectory.empty())
        {
            if (ImGui::Button("Open Build Folder"))
                PlayerPackager::OpenDirectory(m_LastPlayerBuildDirectory);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_LastPlayerBuildDirectory.string().c_str());
        }
        ImGui::End();
    }

    void EditorLayerBase::UI_CanvasEditor()
    {
        if (!m_UIEditorOpen)
            return;

        ImGui::Begin("UI Canvas Editor", &m_UIEditorOpen);

        auto isValidEntity = [this](Entity entity)
        {
            return entity
                && m_ActiveScene
                && m_ActiveScene->GetRegistry().valid(
                    static_cast<entt::entity>(static_cast<uint32_t>(entity)));
        };

        if (!isValidEntity(m_UIEditingCanvas) || !m_UIEditingCanvas.HasComponent<UICanvasComponent>())
        {
            m_UIEditingCanvas = {};
            UIWidgetLayout::Context layout(m_ActiveScene.get());
            Entity selectedCanvas = FindOwningCanvas(m_SceneHierarchyPanel->GetSelectedEntity(), layout);
            if (selectedCanvas)
                m_UIEditingCanvas = selectedCanvas;

            if (m_ActiveScene)
            {
                auto& registry = m_ActiveScene->GetRegistry();
                for (auto entityID : registry.view<UICanvasComponent>())
                {
                    if (m_UIEditingCanvas)
                        break;
                    m_UIEditingCanvas = Entity{ entityID, m_ActiveScene.get() };
                }
            }
        }

        if (ImGui::Button("Use Selected Canvas"))
        {
            Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
            UIWidgetLayout::Context layout(m_ActiveScene.get());
            if (Entity owningCanvas = FindOwningCanvas(selected, layout))
                m_UIEditingCanvas = owningCanvas;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Hide UI In Scene Viewport", &m_HideUIInSceneViewport);

        if (!m_UIEditingCanvas || !m_UIEditingCanvas.HasComponent<UICanvasComponent>())
        {
            ImGui::TextWrapped("Select or create a UI Canvas to edit UI in this dedicated view.");
            ImGui::End();
            return;
        }

        const auto& canvas = m_UIEditingCanvas.GetComponent<UICanvasComponent>();
        const float referenceWidth = std::max(canvas.ReferenceWidth, 1.0f);
        const float referenceHeight = std::max(canvas.ReferenceHeight, 1.0f);
        const float aspect = referenceWidth / referenceHeight;

        ImGui::TextDisabled("Canvas: %s  %.0fx%.0f",
            m_UIEditingCanvas.GetName().c_str(),
            referenceWidth,
            referenceHeight);

        ImVec2 available = ImGui::GetContentRegionAvail();
        available.x = std::max(available.x, 64.0f);
        available.y = std::max(available.y, 64.0f);

        ImVec2 canvasSize = available;
        if (canvasSize.x / canvasSize.y > aspect)
            canvasSize.x = canvasSize.y * aspect;
        else
            canvasSize.y = canvasSize.x / aspect;

        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMin = {
            cursor.x + (available.x - canvasSize.x) * 0.5f,
            cursor.y
        };

        ImGui::SetCursorScreenPos(canvasMin);
        ImGui::InvisibleButton("##UICanvasSurface", canvasSize);
        const bool surfaceHovered = ImGui::IsItemHovered();

        UI_DrawCanvasOverlay(
            { canvasMin.x, canvasMin.y },
            { canvasSize.x, canvasSize.y },
            surfaceHovered,
            true,
            m_UIEditingCanvas);

        ImGui::End();
    }

    void EditorLayerBase::UI_DrawCanvasOverlay(const glm::vec2& regionMin,
        const glm::vec2& regionSize,
        bool surfaceHovered,
        bool drawBackdrop,
        Entity canvasEntity)
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene)
            return;
        if (regionSize.x <= 1.0f || regionSize.y <= 1.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasMin = { regionMin.x, regionMin.y };
        const ImVec2 canvasMax = { regionMin.x + regionSize.x, regionMin.y + regionSize.y };
        UIWidgetLayout::Context layout(m_ActiveScene.get());
        Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
        const bool selectedInsideThisCanvas = selected && selected != canvasEntity
            && BelongsToCanvas(selected, canvasEntity, layout);
        const bool canvasIsSelectionContext = selected == canvasEntity || selectedInsideThisCanvas;

        if (drawBackdrop)
        {
            const ImU32 background = canvasIsSelectionContext
                ? IM_COL32(20, 38, 35, 255)
                : IM_COL32(18, 23, 29, 255);
            const ImU32 border = canvasIsSelectionContext
                ? IM_COL32(87, 226, 188, 255)
                : IM_COL32(78, 115, 127, 255);
            drawList->AddRectFilled(canvasMin, canvasMax, background);
            drawList->AddRect(canvasMin, canvasMax, border, 0.0f, 0, canvasIsSelectionContext ? 2.5f : 1.5f);
        }

        std::vector<ViewportUIEntry> entries;

        auto& registry = m_ActiveScene->GetRegistry();
        auto view = registry.view<TagComponent, UIWidgetComponent>();
        for (auto entityID : view)
        {
            Entity entity{ entityID, m_ActiveScene.get() };
            if (!BelongsToCanvas(entity, canvasEntity, layout))
                continue;
            if (!UIWidgetLayout::ResolveVisible(layout, entityID))
                continue;

            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, entityID);
            if (rect.Right <= rect.Left || rect.Bottom <= rect.Top)
                continue;

            const auto& tag = view.get<TagComponent>(entityID).Tag;
            const auto& widget = view.get<UIWidgetComponent>(entityID);
            entries.push_back({ entity, rect, widget.SortOrder, tag });
        }

        std::sort(entries.begin(), entries.end(), [](const ViewportUIEntry& a, const ViewportUIEntry& b)
        {
            if (a.SortOrder != b.SortOrder)
                return a.SortOrder < b.SortOrder;
            return static_cast<uint32_t>(static_cast<entt::entity>(a.EntityRef))
                < static_cast<uint32_t>(static_cast<entt::entity>(b.EntityRef));
        });

        drawList->PushClipRect(canvasMin, canvasMax, true);

        const ImVec2 mouse = ImGui::GetMousePos();
        const bool mouseInCanvas = surfaceHovered
            && mouse.x >= canvasMin.x && mouse.x <= canvasMax.x
            && mouse.y >= canvasMin.y && mouse.y <= canvasMax.y;
        const glm::vec2 mouseNorm = {
            (mouse.x - canvasMin.x) / std::max(1.0f, regionSize.x),
            (mouse.y - canvasMin.y) / std::max(1.0f, regionSize.y)
        };

        for (const ViewportUIEntry& entry : entries)
        {
            const ImVec2 rectMin = ToScreenPoint(regionMin, regionSize, entry.Rect.Left, entry.Rect.Top);
            const ImVec2 rectMax = ToScreenPoint(regionMin, regionSize, entry.Rect.Right, entry.Rect.Bottom);

            if (drawBackdrop)
            {
                Entity entity = entry.EntityRef;
                if (entity.HasComponent<UIPanelComponent>())
                {
                    const auto& panel = entity.GetComponent<UIPanelComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(panel.BackgroundColor));
                    if (panel.BorderThickness > 0.0f)
                        drawList->AddRect(rectMin, rectMax, ColorToImU32(panel.BorderColor), 0.0f, 0, panel.BorderThickness);
                }
                if (entity.HasComponent<UIButtonComponent>())
                {
                    const auto& button = entity.GetComponent<UIButtonComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(button.NormalColor), 4.0f);
                }
                if (entity.HasComponent<UIProgressBarComponent>())
                {
                    const auto& bar = entity.GetComponent<UIProgressBarComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(bar.BackgroundColor), 2.0f);
                    const float fillRight = rectMin.x + (rectMax.x - rectMin.x) * bar.GetNormalized();
                    drawList->AddRectFilled(rectMin, { fillRight, rectMax.y }, ColorToImU32(bar.ForegroundColor), 2.0f);
                }
                if (entity.HasComponent<UIImageComponent>())
                {
                    const auto& image = entity.GetComponent<UIImageComponent>();
                    drawList->AddRectFilled(rectMin, rectMax, ColorToImU32(image.Color), 2.0f);
                }
                if (entity.HasComponent<UITextComponent>())
                {
                    const auto& text = entity.GetComponent<UITextComponent>();
                    drawList->AddText({ rectMin.x + 4.0f, rectMin.y + 4.0f },
                        ColorToImU32(text.Color),
                        text.Text.c_str());
                }
            }

            const bool isSelected = selected == entry.EntityRef;
            const ImU32 color = isSelected
                ? IM_COL32(82, 230, 244, 245)
                : IM_COL32(112, 185, 196, drawBackdrop ? 145 : 105);
            if (m_ShowUIOutlines || drawBackdrop)
                DrawUIOutline(drawList, rectMin, rectMax, entry.EntityRef, color, isSelected ? 2.0f : 1.0f);

            if (isSelected)
            {
                const std::string label = entry.Name.empty() ? "UI Widget" : entry.Name;
                drawList->AddText({ rectMin.x + 4.0f, rectMin.y - 18.0f },
                    IM_COL32(160, 244, 250, 235),
                    label.c_str());
            }
        }

        Entity clickedUI;
        if (mouseInCanvas
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            int bestSort = std::numeric_limits<int>::min();
            for (const ViewportUIEntry& entry : entries)
            {
                if (entry.SortOrder >= bestSort && PointInNormalizedRect(entry.Rect, mouseNorm))
                {
                    clickedUI = entry.EntityRef;
                    bestSort = entry.SortOrder;
                }
            }

            if (clickedUI)
            {
                m_SceneHierarchyPanel->SetSelectedEntity(clickedUI);
                m_AnimationEditorPanel->SetEntity(clickedUI);
                selected = clickedUI;
            }
        }

        if (m_UIEditHandle != UIEdit_None && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            CommitPendingUIEdit();
            drawList->PopClipRect();
            return;
        }

        if (!selected || !selected.HasComponent<UIWidgetComponent>())
        {
            drawList->PopClipRect();
            return;
        }

        const entt::entity selectedID = static_cast<entt::entity>(selected);
        if (!registry.valid(selectedID) || !registry.all_of<UIWidgetComponent>(selectedID))
        {
            drawList->PopClipRect();
            return;
        }

        UIWidgetLayout::Rect selectedRect = UIWidgetLayout::ResolveRect(layout, selectedID);
        ImVec2 rectMin = ToScreenPoint(regionMin, regionSize, selectedRect.Left, selectedRect.Top);
        ImVec2 rectMax = ToScreenPoint(regionMin, regionSize, selectedRect.Right, selectedRect.Bottom);

        auto& widget = selected.GetComponent<UIWidgetComponent>();
        constexpr float handleSize = 9.0f;
        const ImU32 handleColor = IM_COL32(86, 230, 244, 245);
        const float centerX = (rectMin.x + rectMax.x) * 0.5f;
        const float centerY = (rectMin.y + rectMax.y) * 0.5f;
        DrawUIHandle(drawList, rectMin, handleSize, handleColor);
        DrawUIHandle(drawList, { centerX, rectMin.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMax.x, rectMin.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMin.x, centerY }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMax.x, centerY }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMin.x, rectMax.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { centerX, rectMax.y }, handleSize, handleColor);
        DrawUIHandle(drawList, rectMax, handleSize, handleColor);

        const int hoveredHandle = HitResizeHandle(mouse, rectMin, rectMax, handleSize + 6.0f);
        const int hoveredOperation = hoveredHandle != UIEdit_None
            ? hoveredHandle
            : (PointInScreenRect(mouse, rectMin, rectMax) ? UIEdit_Move : UIEdit_None);
        if (hoveredOperation != UIEdit_None)
            ImGui::SetMouseCursor(CursorForUIHandle(hoveredOperation));

        if (mouseInCanvas
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && (selected == clickedUI || (!clickedUI && hoveredHandle != UIEdit_None))
            && hoveredOperation != UIEdit_None
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            m_UIEditHandle = hoveredOperation;
            m_UIEditSurface = 2;
            m_UIEditEntity = selected;
            m_UIEditStartMouse = mouseNorm;
            m_UIEditStartRect = RectToVec4(UIWidgetLayout::WidgetToLocalRect(widget));
            m_UIEditStartWidget = std::make_unique<UIWidgetComponent>(widget);
            m_UIEditStartHadText = selected.HasComponent<UITextComponent>();
            m_UIEditStartText.reset();
            if (m_UIEditStartHadText)
                m_UIEditStartText = std::make_unique<UITextComponent>(selected.GetComponent<UITextComponent>());
        }

        if (m_UIEditHandle != UIEdit_None && m_UIEditSurface == 2 && m_UIEditEntity == selected)
        {
            float parentWidth = 1.0f;
            float parentHeight = 1.0f;
            const entt::entity parentID = layout.ResolveReference(widget.ParentEntity, widget.ParentTag);
            if (parentID != entt::null)
            {
                if (parentID != entt::null && registry.valid(parentID) && registry.all_of<UIWidgetComponent>(parentID))
                {
                    const UIWidgetLayout::Rect parentRect = UIWidgetLayout::ResolveRect(layout, parentID);
                    parentWidth = std::max(parentRect.Right - parentRect.Left, 0.0001f);
                    parentHeight = std::max(parentRect.Bottom - parentRect.Top, 0.0001f);
                }
            }

            const glm::vec2 delta = {
                (mouseNorm.x - m_UIEditStartMouse.x) / parentWidth,
                (mouseNorm.y - m_UIEditStartMouse.y) / parentHeight
            };

            UIWidgetLayout::Rect localRect = Vec4ToRect(m_UIEditStartRect);
            switch (m_UIEditHandle)
            {
            case UIEdit_Move: localRect.Left += delta.x; localRect.Right += delta.x; localRect.Top += delta.y; localRect.Bottom += delta.y; break;
            case UIEdit_Left: localRect.Left += delta.x; break;
            case UIEdit_Right: localRect.Right += delta.x; break;
            case UIEdit_Top: localRect.Top += delta.y; break;
            case UIEdit_Bottom: localRect.Bottom += delta.y; break;
            case UIEdit_TopLeft: localRect.Left += delta.x; localRect.Top += delta.y; break;
            case UIEdit_TopRight: localRect.Right += delta.x; localRect.Top += delta.y; break;
            case UIEdit_BottomLeft: localRect.Left += delta.x; localRect.Bottom += delta.y; break;
            case UIEdit_BottomRight: localRect.Right += delta.x; localRect.Bottom += delta.y; break;
            default: break;
            }

            constexpr float minSize = 0.0025f;
            if (localRect.Right < localRect.Left + minSize)
            {
                if (m_UIEditHandle == UIEdit_Left || m_UIEditHandle == UIEdit_TopLeft || m_UIEditHandle == UIEdit_BottomLeft)
                    localRect.Left = localRect.Right - minSize;
                else
                    localRect.Right = localRect.Left + minSize;
            }
            if (localRect.Bottom < localRect.Top + minSize)
            {
                if (m_UIEditHandle == UIEdit_Top || m_UIEditHandle == UIEdit_TopLeft || m_UIEditHandle == UIEdit_TopRight)
                    localRect.Top = localRect.Bottom - minSize;
                else
                    localRect.Bottom = localRect.Top + minSize;
            }

            ApplyLocalRectToWidget(widget, localRect);
            UpdateUITextFontDuringUIResize(selected);
        }

        drawList->PopClipRect();
    }

    void EditorLayerBase::UI_DrawViewportUIOverlay()
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene)
            return;
        if (m_HideUIInSceneViewport)
            return;
        if (m_ViewportSize.x <= 1.0f || m_ViewportSize.y <= 1.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        UIWidgetLayout::Context layout(m_ActiveScene.get());
        std::vector<ViewportUIEntry> entries;

        auto& registry = m_ActiveScene->GetRegistry();
        auto view = registry.view<TagComponent, UIWidgetComponent>();
        for (auto entityID : view)
        {
            if (!UIWidgetLayout::ResolveVisible(layout, entityID))
                continue;

            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, entityID);
            if (rect.Right <= rect.Left || rect.Bottom <= rect.Top)
                continue;

            const auto& tag = view.get<TagComponent>(entityID).Tag;
            const auto& widget = view.get<UIWidgetComponent>(entityID);
            entries.push_back({ Entity{ entityID, m_ActiveScene.get() }, rect, widget.SortOrder, tag });
        }

        std::sort(entries.begin(), entries.end(), [](const ViewportUIEntry& a, const ViewportUIEntry& b)
        {
            if (a.SortOrder != b.SortOrder)
                return a.SortOrder < b.SortOrder;
            return static_cast<uint32_t>(static_cast<entt::entity>(a.EntityRef))
                < static_cast<uint32_t>(static_cast<entt::entity>(b.EntityRef));
        });

        const glm::vec2 viewportMin = m_ViewportBounds[0];
        const glm::vec2 viewportSize = m_ViewportSize;
        const ImVec2 mouse = ImGui::GetMousePos();
        const bool mouseInViewport = mouse.x >= m_ViewportBounds[0].x && mouse.x <= m_ViewportBounds[1].x
            && mouse.y >= m_ViewportBounds[0].y && mouse.y <= m_ViewportBounds[1].y;
        const glm::vec2 mouseNorm = {
            (mouse.x - m_ViewportBounds[0].x) / std::max(1.0f, m_ViewportSize.x),
            (mouse.y - m_ViewportBounds[0].y) / std::max(1.0f, m_ViewportSize.y)
        };

        Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();

        if (m_ShowUIOutlines)
        {
            for (const ViewportUIEntry& entry : entries)
            {
                const bool isSelected = selected == entry.EntityRef;
                const ImVec2 rectMin = ToScreenPoint(viewportMin, viewportSize, entry.Rect.Left, entry.Rect.Top);
                const ImVec2 rectMax = ToScreenPoint(viewportMin, viewportSize, entry.Rect.Right, entry.Rect.Bottom);
                const ImU32 color = isSelected
                    ? IM_COL32(82, 230, 244, 245)
                    : IM_COL32(112, 185, 196, 105);
                DrawUIOutline(drawList, rectMin, rectMax, entry.EntityRef, color, isSelected ? 2.0f : 1.0f);

                if (isSelected)
                {
                    const std::string label = entry.Name.empty() ? "UI Widget" : entry.Name;
                    drawList->AddText({ rectMin.x + 4.0f, rectMin.y - 18.0f },
                        IM_COL32(160, 244, 250, 235),
                        label.c_str());
                }
            }
        }

        Entity clickedUI;
        if (mouseInViewport && ImGui::IsWindowHovered()
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT)
            && !ImGuizmo::IsOver())
        {
            int bestSort = std::numeric_limits<int>::min();
            for (const ViewportUIEntry& entry : entries)
            {
                if (entry.SortOrder >= bestSort && PointInNormalizedRect(entry.Rect, mouseNorm))
                {
                    clickedUI = entry.EntityRef;
                    bestSort = entry.SortOrder;
                }
            }

            if (clickedUI)
            {
                m_SceneHierarchyPanel->SetSelectedEntity(clickedUI);
                m_AnimationEditorPanel->SetEntity(clickedUI);
                selected = clickedUI;
            }
        }

        if (!selected || !selected.HasComponent<UIWidgetComponent>())
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                m_UIEditHandle = UIEdit_None;
                m_UIEditSurface = 0;
                m_UIEditEntity = {};
                m_UIEditStartHadText = false;
            }
            return;
        }

        const entt::entity selectedID = static_cast<entt::entity>(selected);
        if (!registry.valid(selectedID) || !registry.all_of<UIWidgetComponent>(selectedID))
            return;

        UIWidgetLayout::Rect selectedRect = UIWidgetLayout::ResolveRect(layout, selectedID);
        ImVec2 rectMin = ToScreenPoint(viewportMin, viewportSize, selectedRect.Left, selectedRect.Top);
        ImVec2 rectMax = ToScreenPoint(viewportMin, viewportSize, selectedRect.Right, selectedRect.Bottom);

        auto& widget = selected.GetComponent<UIWidgetComponent>();
        const entt::entity selectedParentID = layout.ResolveReference(widget.ParentEntity, widget.ParentTag);
        if (selectedParentID != entt::null)
        {
            if (registry.valid(selectedParentID) && registry.all_of<UIWidgetComponent>(selectedParentID))
            {
                const UIWidgetLayout::Rect parentRect = UIWidgetLayout::ResolveRect(layout, selectedParentID);
                const ImVec2 parentMin = ToScreenPoint(viewportMin, viewportSize, parentRect.Left, parentRect.Top);
                const ImVec2 parentMax = ToScreenPoint(viewportMin, viewportSize, parentRect.Right, parentRect.Bottom);
                drawList->AddRect(parentMin, parentMax, IM_COL32(246, 183, 76, 155), 0.0f, 0, 1.5f);
            }
        }

        constexpr float handleSize = 9.0f;
        const ImU32 handleColor = IM_COL32(86, 230, 244, 245);
        const float centerX = (rectMin.x + rectMax.x) * 0.5f;
        const float centerY = (rectMin.y + rectMax.y) * 0.5f;
        DrawUIHandle(drawList, rectMin, handleSize, handleColor);
        DrawUIHandle(drawList, { centerX, rectMin.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMax.x, rectMin.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMin.x, centerY }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMax.x, centerY }, handleSize, handleColor);
        DrawUIHandle(drawList, { rectMin.x, rectMax.y }, handleSize, handleColor);
        DrawUIHandle(drawList, { centerX, rectMax.y }, handleSize, handleColor);
        DrawUIHandle(drawList, rectMax, handleSize, handleColor);

        const int hoveredHandle = HitResizeHandle(mouse, rectMin, rectMax, handleSize + 6.0f);
        const int hoveredOperation = hoveredHandle != UIEdit_None
            ? hoveredHandle
            : (PointInScreenRect(mouse, rectMin, rectMax) ? UIEdit_Move : UIEdit_None);
        if (hoveredOperation != UIEdit_None)
            ImGui::SetMouseCursor(CursorForUIHandle(hoveredOperation));

        if (mouseInViewport && ImGui::IsWindowHovered()
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && (selected == clickedUI || (!clickedUI && hoveredHandle != UIEdit_None))
            && hoveredOperation != UIEdit_None
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            m_UIEditHandle = hoveredOperation;
            m_UIEditSurface = 1;
            m_UIEditEntity = selected;
            m_UIEditStartMouse = mouseNorm;
            m_UIEditStartRect = RectToVec4(UIWidgetLayout::WidgetToLocalRect(widget));
            m_UIEditStartWidget = std::make_unique<UIWidgetComponent>(widget);
            m_UIEditStartHadText = selected.HasComponent<UITextComponent>();
            m_UIEditStartText.reset();
            if (m_UIEditStartHadText)
                m_UIEditStartText = std::make_unique<UITextComponent>(selected.GetComponent<UITextComponent>());
        }

        if (m_UIEditHandle != UIEdit_None && m_UIEditSurface == 1 && m_UIEditEntity == selected)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                CommitPendingUIEdit();
                return;
            }

            float parentWidth = 1.0f;
            float parentHeight = 1.0f;
            const entt::entity parentID = layout.ResolveReference(widget.ParentEntity, widget.ParentTag);
            if (parentID != entt::null)
            {
                if (parentID != entt::null && registry.valid(parentID) && registry.all_of<UIWidgetComponent>(parentID))
                {
                    const UIWidgetLayout::Rect parentRect = UIWidgetLayout::ResolveRect(layout, parentID);
                    parentWidth = std::max(parentRect.Right - parentRect.Left, 0.0001f);
                    parentHeight = std::max(parentRect.Bottom - parentRect.Top, 0.0001f);
                }
            }

            const glm::vec2 delta = {
                (mouseNorm.x - m_UIEditStartMouse.x) / parentWidth,
                (mouseNorm.y - m_UIEditStartMouse.y) / parentHeight
            };

            UIWidgetLayout::Rect localRect = Vec4ToRect(m_UIEditStartRect);
            switch (m_UIEditHandle)
            {
            case UIEdit_Move:
                localRect.Left += delta.x;
                localRect.Right += delta.x;
                localRect.Top += delta.y;
                localRect.Bottom += delta.y;
                break;
            case UIEdit_Left:
                localRect.Left += delta.x;
                break;
            case UIEdit_Right:
                localRect.Right += delta.x;
                break;
            case UIEdit_Top:
                localRect.Top += delta.y;
                break;
            case UIEdit_Bottom:
                localRect.Bottom += delta.y;
                break;
            case UIEdit_TopLeft:
                localRect.Left += delta.x;
                localRect.Top += delta.y;
                break;
            case UIEdit_TopRight:
                localRect.Right += delta.x;
                localRect.Top += delta.y;
                break;
            case UIEdit_BottomLeft:
                localRect.Left += delta.x;
                localRect.Bottom += delta.y;
                break;
            case UIEdit_BottomRight:
                localRect.Right += delta.x;
                localRect.Bottom += delta.y;
                break;
            default:
                break;
            }

            constexpr float minSize = 0.0025f;
            if (localRect.Right < localRect.Left + minSize)
            {
                if (m_UIEditHandle == UIEdit_Left || m_UIEditHandle == UIEdit_TopLeft || m_UIEditHandle == UIEdit_BottomLeft)
                    localRect.Left = localRect.Right - minSize;
                else
                    localRect.Right = localRect.Left + minSize;
            }
            if (localRect.Bottom < localRect.Top + minSize)
            {
                if (m_UIEditHandle == UIEdit_Top || m_UIEditHandle == UIEdit_TopLeft || m_UIEditHandle == UIEdit_TopRight)
                    localRect.Top = localRect.Bottom - minSize;
                else
                    localRect.Bottom = localRect.Top + minSize;
            }

            if (Input::IsKeyPressed(WT_KEY_LEFT_CONTROL) || Input::IsKeyPressed(WT_KEY_RIGHT_CONTROL))
            {
                constexpr float grid = 0.005f;
                auto snap = [](float value)
                {
                    constexpr float localGrid = 0.005f;
                    return std::round(value / localGrid) * localGrid;
                };
                localRect.Left = snap(localRect.Left);
                localRect.Right = snap(localRect.Right);
                localRect.Top = snap(localRect.Top);
                localRect.Bottom = snap(localRect.Bottom);
                (void)grid;
            }

            ApplyLocalRectToWidget(widget, localRect);
            UpdateUITextFontDuringUIResize(selected);
        }
    }

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

        const uint64_t texID = m_Framebuffer->GetColorAttachmentRendererID();
        // OpenGL and ImGui use opposite vertical UV directions.
        ImGui::Image(
            reinterpret_cast<void*>(texID),
            ImVec2{ m_ViewportSize.x, m_ViewportSize.y },
            ImVec2{ 0, 1 },
            ImVec2{ 1, 0 }
        );

        const ImVec2 windowPos  = ImGui::GetWindowPos();
        const ImVec2 minBound   = { windowPos.x + viewportOffset.x, windowPos.y + viewportOffset.y };
        m_ViewportBounds[0] = { minBound.x, minBound.y };
        m_ViewportBounds[1] = { minBound.x + m_ViewportSize.x, minBound.y + m_ViewportSize.y };

        if (m_SceneState == SceneState::Edit
            && m_ViewportHovered
            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            FocusEditorCameraOnPrimarySceneCamera();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                const wchar_t*        wpath    = static_cast<const wchar_t*>(payload->Data);
                std::filesystem::path fullPath = GetEditorAssetPath() / wpath;

                if (fullPath.extension() == AssetFileType::SceneExtension)
                    OpenScene(fullPath);
                else if (fullPath.extension() == AssetFileType::PrefabExtension)
                    InstantiatePrefab(fullPath);
            }
            ImGui::EndDragDropTarget();
        }

        UI_DrawViewportUIOverlay();

        Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
        const bool selectedIsUI = selected && selected.HasComponent<UIWidgetComponent>();

        // Gizmo
        if (selected && !selectedIsUI && m_GizmoType != -1 && m_SceneState == SceneState::Edit)
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

        const auto& colors = ImGui::GetStyle().Colors;
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(colors[ImGuiCol_ButtonHovered].x, colors[ImGuiCol_ButtonHovered].y,
                   colors[ImGuiCol_ButtonHovered].z, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(colors[ImGuiCol_ButtonActive].x,  colors[ImGuiCol_ButtonActive].y,
                   colors[ImGuiCol_ButtonActive].z,  0.5f));

        ImGui::Begin("##toolbar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const float size = ImGui::GetWindowHeight();
        const Ref<Texture2D>& icon = (m_SceneState == SceneState::Edit) ? m_IconPlay : m_IconStop;

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x * 0.5f - size * 0.5f);
        if (ImGui::ImageButton(
                "##PlayStop",
                static_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1)))
        {
            if (m_SceneState == SceneState::Edit) TransitionToPlay();
            else                                  TransitionToStop();
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

} // namespace Wheatear
