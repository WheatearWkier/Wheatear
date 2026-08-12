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
#include "Assets/AssetRegistry.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/EditorCanvasTools.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorPlatform.h"
#include "Editor/EditorWidgets.h"
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
#include <cstring>
#include <functional>
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

        static void ProcessCanvasEditorHistoryShortcuts()
        {
            ImGuiIO& io = ImGui::GetIO();
            if (!io.KeyCtrl || io.WantTextInput || ImGui::IsAnyItemActive())
                return;

            if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                if (CommandHistory::Get().CanRedo())
                    CommandHistory::Get().Redo();
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                if (CommandHistory::Get().CanUndo())
                    CommandHistory::Get().Undo();
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                if (CommandHistory::Get().CanRedo())
                    CommandHistory::Get().Redo();
            }
        }

        static ImU32 ColorToImU32(const glm::vec4& color)
        {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
        }

        static Entity FindSingleCanvas(UIWidgetLayout::Context& layout)
        {
            if (!layout.ScenePtr)
                return {};

            auto& registry = layout.ScenePtr->GetRegistry();
            Entity found;
            for (auto entityID : registry.view<UICanvasComponent>())
            {
                if (found)
                    return {};
                found = Entity{ entityID, layout.ScenePtr };
            }

            return found;
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
                const entt::entity parentID = layout.ResolveReference(widget.ParentEntity);
                if (parentID == entt::null || !registry.valid(parentID))
                    return FindSingleCanvas(layout);

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

        static float EntityFrameRadius(Entity entity)
        {
            if (!entity)
                return 1.0f;

            const auto& transform = entity.GetComponent<TransformComponent>();
            float radius = 0.75f * std::max({
                std::abs(transform.Scale.x),
                std::abs(transform.Scale.y),
                std::abs(transform.Scale.z),
                1.0f
            });

            if (entity.HasComponent<SpriteRendererComponent>())
            {
                const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                const glm::vec2 uvSize = glm::abs(sprite.UVMax - sprite.UVMin);
                const float drawScale = std::max(std::abs(sprite.DrawScale.x), std::abs(sprite.DrawScale.y));
                radius = std::max(radius, 0.5f * drawScale * std::max(uvSize.x, uvSize.y));
            }
            else if (entity.HasComponent<CircleRendererComponent>())
            {
                radius = std::max(radius, 0.5f);
            }
            else if (entity.HasComponent<MeshRendererComponent>())
            {
                radius = std::max(radius, 1.0f);
            }

            return radius;
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

        if (m_ShowStats)
            UI_Stats();
        UI_PlayerBuildStatus();

        OnImGuiExtra();

        UI_Viewport();
        UI_CanvasEditor();
        UI_Toolbar();
        ProcessDeferredViewportAssetDrop();

        ImGui::End();
    }

    void EditorLayerBase::FocusEditorCameraOnPrimarySceneCamera()
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene)
            return;

        Entity cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
        if (!cameraEntity || !cameraEntity.HasComponent<CameraComponent>() || !cameraEntity.HasComponent<TransformComponent>())
            return;

        const auto& transform = cameraEntity.GetComponent<TransformComponent>();
        const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        const float distance = camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic
            ? std::max(camera.GetOrthographicSize() * 0.75f, 6.0f)
            : 12.0f;
        m_EditorCamera.SetViewTransform(transform.Translation, transform.Rotation, distance);

        m_SceneHierarchyPanel->SetSelectedEntity(cameraEntity);
        m_AnimationEditorPanel->SetEntity(cameraEntity);
    }

    void EditorLayerBase::ActivateHierarchyEntity(Entity entity)
    {
        if (m_SceneState != SceneState::Edit || !entity)
            return;

        SelectEditorEntity(entity, !entity.HasComponent<UIWidgetComponent>());

        if (entity.HasComponent<UIWidgetComponent>())
        {
            OpenCanvasEditorForEntity(entity);
            return;
        }

        FrameEditorCameraOnEntity(entity);
    }

    void EditorLayerBase::SelectEditorEntity(Entity entity, bool preferMoveGizmo)
    {
        m_SceneHierarchyPanel->SetSelectedEntity(entity);
        m_AnimationEditorPanel->SetEntity(entity);

        if (!entity)
            return;

        if (entity.HasComponent<UIWidgetComponent>())
        {
            UIWidgetLayout::Context layout(m_ActiveScene.get());
            Entity canvas = FindOwningCanvas(entity, layout);
            if (!canvas && entity.HasComponent<UICanvasComponent>())
                canvas = entity;
            if (canvas)
            {
                m_UIEditingCanvas = canvas;
                m_UIEditorOpen = true;
            }
            return;
        }

        if (preferMoveGizmo
            && entity.HasComponent<TransformComponent>()
            && !entity.HasComponent<UIWidgetComponent>())
        {
            m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
        }
    }

    void EditorLayerBase::OpenCanvasEditorForEntity(Entity entity)
    {
        if (!entity || !m_ActiveScene || !entity.HasComponent<UIWidgetComponent>())
            return;

        UIWidgetLayout::Context layout(m_ActiveScene.get());
        Entity canvas = FindOwningCanvas(entity, layout);
        if (!canvas && entity.HasComponent<UICanvasComponent>())
            canvas = entity;
        if (!canvas)
            return;

        m_UIEditingCanvas = canvas;
        m_UIEditorOpen = true;
        m_FocusCanvasEditor = true;
    }

    void EditorLayerBase::FrameEditorCameraOnEntity(Entity entity)
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene
            || !entity || !entity.HasComponent<TransformComponent>())
            return;

        const auto& transform = entity.GetComponent<TransformComponent>();
            m_EditorCamera.Frame(transform.Translation, std::max(EntityFrameRadius(entity) * 4.0f, 10.0f));
    }

    void EditorLayerBase::FrameEditorCameraOnScene()
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene)
            return;

        auto& registry = m_ActiveScene->GetRegistry();
        glm::vec3 minBounds( std::numeric_limits<float>::max());
        glm::vec3 maxBounds(-std::numeric_limits<float>::max());
        bool foundRenderable = false;

        for (auto entityID : registry.view<TransformComponent>())
        {
            Entity entity{ entityID, m_ActiveScene.get() };
            if (IsEditorHidden(entity))
                continue;
            if (entity.HasComponent<UIWidgetComponent>() || entity.HasComponent<CameraComponent>())
                continue;
            if (!entity.HasComponent<SpriteRendererComponent>()
                && !entity.HasComponent<CircleRendererComponent>()
                && !entity.HasComponent<MeshRendererComponent>())
                continue;

            const auto& transform = entity.GetComponent<TransformComponent>();
            const float radius = EntityFrameRadius(entity);
            minBounds = glm::min(minBounds, transform.Translation - glm::vec3(radius));
            maxBounds = glm::max(maxBounds, transform.Translation + glm::vec3(radius));
            foundRenderable = true;
        }

        if (!foundRenderable)
        {
            if (Entity camera = m_ActiveScene->GetPrimaryCameraEntity())
                FocusEditorCameraOnPrimarySceneCamera();
            return;
        }

        const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
        const glm::vec3 extent = maxBounds - minBounds;
        const float radius = std::max(glm::length(extent) * 0.5f, 1.0f);
            m_EditorCamera.Frame(center, std::max(radius * 3.0f, 10.0f));
        m_SceneHierarchyPanel->SetSelectedEntity({});
        m_AnimationEditorPanel->SetEntity({});
    }

    void EditorLayerBase::UI_MenuBar()
    {
        PollPlayerPackageBuild();

        if (!ImGui::BeginMenuBar()) return;

        const EditorToolContext toolContext{ m_SceneHierarchyPanel->GetSelectedEntity() };
        auto localizedToolLabel = [](const char* label) -> const char*
        {
            if (!label) return "";
            if (std::strcmp(label, "Event Script Editor") == 0) return EditorLocale::Text("Event Script Editor", "事件脚本编辑器");
            if (std::strcmp(label, "VN Script Editor") == 0) return EditorLocale::Text("VN Script Editor", "视觉小说脚本编辑器");
            if (std::strcmp(label, "Side Combat Tuning Editor") == 0) return EditorLocale::Text("Side Combat Tuning Editor", "横版战斗调参编辑器");
            if (std::strcmp(label, "Side Combat HUD Preset Editor") == 0) return EditorLocale::Text("Side Combat HUD Preset Editor", "横版战斗 HUD 预设编辑器");
            if (std::strcmp(label, "WAO Action Editor") == 0) return EditorLocale::Text("WAO Action Editor", "WAO 动作编辑器");
            if (std::strcmp(label, "Asset Alias / Manifest Editor") == 0) return EditorLocale::Text("Asset Alias / Manifest Editor", "资产别名 / 清单编辑器");
            if (std::strcmp(label, "Progression Content Editor") == 0) return EditorLocale::Text("Progression Content Editor", "成长内容编辑器");
            if (std::strcmp(label, "Project Health") == 0) return EditorLocale::Text("Project Health", "项目健康检查");
            return label;
        };
        auto drawToolsByCategory = [&](EditorToolCategory category)
        {
            EditorToolRegistry::ForEach([&](const EditorToolDescriptor& tool)
            {
                if (tool.Category == category && ImGui::MenuItem(localizedToolLabel(tool.MenuLabel.c_str())) && tool.Open)
                    tool.Open(toolContext);
            });
        };
        auto openToolByLabel = [&](const char* label)
        {
            EditorToolRegistry::ForEach([&](const EditorToolDescriptor& tool)
            {
                if (tool.MenuLabel == label && tool.Open)
                    tool.Open(toolContext);
            });
        };
        auto drawFloatingWindowItem = [&](const char* title, const std::function<void()>& open)
        {
            if (EditorFloatingWindow::DrawFloatingMenuItem(title)
                && EditorFloatingWindow::IsFloating(title)
                && open)
            {
                open();
            }
        };
        auto drawLocalizedFloatingWindowItem = [&](const char* title, const std::function<void()>& open)
        {
            const bool floating = EditorFloatingWindow::IsFloating(title);
            const std::string label = std::string(floating
                ? EditorLocale::Text("Dock ", "停靠 ")
                : EditorLocale::Text("Pop Out ", "弹出 "))
                + localizedToolLabel(title);
            if (ImGui::MenuItem(label.c_str()))
            {
                if (floating)
                    EditorFloatingWindow::Dock(title);
                else
                    EditorFloatingWindow::OpenFloating(title);
                if (!floating && open)
                    open();
            }
        };

        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::MenuItem("New",          "Ctrl+N"))        NewScene();
            if (ImGui::MenuItem("Open...",      "Ctrl+O"))        OpenScene();
            if (ImGui::MenuItem("Save",         "Ctrl+S"))        SaveScene();
            if (ImGui::MenuItem("Save As...",   "Ctrl+Shift+S"))  SaveSceneAs();
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

        if (ImGui::BeginMenu("Run"))
        {
            if (m_SceneState == SceneState::Edit)
            {
                if (ImGui::MenuItem("Play", "Ctrl+Enter"))
                    TransitionToPlay();
            }
            else
            {
                if (ImGui::MenuItem("Stop", "Ctrl+Enter"))
                    TransitionToStop();
            }

            if (ImGui::MenuItem("Frame Selection", "F"))
            {
                if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
                    ActivateHierarchyEntity(selected);
                else
                    FrameEditorCameraOnScene();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("UI"))
        {
            ImGui::MenuItem("UI Edit Outlines", nullptr, &m_ShowUIOutlines);
            if (ImGui::MenuItem("UI Canvas Editor", nullptr, &m_UIEditorOpen))
            {
                EditorFloatingWindow::Dock("UI Canvas Editor");
                m_FocusCanvasEditor = true;
                if (!m_UIEditingCanvas)
                    m_UIEditingCanvas = m_SceneHierarchyPanel->GetSelectedEntity();
            }
            if (ImGui::MenuItem("UI Canvas Editor Floating"))
            {
                m_UIEditorOpen = true;
                EditorFloatingWindow::OpenFloating("UI Canvas Editor");
                m_FocusCanvasEditor = true;
                if (!m_UIEditingCanvas)
                    m_UIEditingCanvas = m_SceneHierarchyPanel->GetSelectedEntity();
            }
            if (ImGui::MenuItem("Sprite Sheet Picker"))
                m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
            if (ImGui::MenuItem("Generate UI Templates"))
            {
                UITemplateFactory::WriteBuiltinTemplateAssets(AssetPath::GetProjectRoot());
                AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
                AssetRegistry::Get().WriteRegistry();
            }
            drawToolsByCategory(EditorToolCategory::UI);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            drawFloatingWindowItem("UI Canvas Editor", [&]()
            {
                m_UIEditorOpen = true;
                m_FocusCanvasEditor = true;
            });
            EditorFloatingWindow::DrawFloatingMenuItem("Animation Editor");
            drawFloatingWindowItem("Sprite Sheet Picker", [&]()
            {
                m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
            });
            drawLocalizedFloatingWindowItem("Event Script Editor", [&]() { openToolByLabel("Event Script Editor"); });
            drawLocalizedFloatingWindowItem("VN Script Editor", [&]() { openToolByLabel("VN Script Editor"); });
            drawLocalizedFloatingWindowItem("Side Combat Tuning Editor", [&]() { openToolByLabel("Side Combat Tuning Editor"); });
            drawLocalizedFloatingWindowItem("Side Combat HUD Preset Editor", [&]() { openToolByLabel("Side Combat HUD Preset Editor"); });
            drawLocalizedFloatingWindowItem("WAO Action Editor", [&]() { openToolByLabel("WAO Action Editor"); });
            drawLocalizedFloatingWindowItem("Asset Alias / Manifest Editor", [&]() { openToolByLabel("Asset Alias / Manifest Editor"); });
            drawLocalizedFloatingWindowItem("Project Health", [&]() { openToolByLabel("Project Health"); });
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Window Layout"))
            {
                m_RequestDefaultDockspaceLayout = true;
                m_DefaultDockspaceLayoutBuilt = false;
            }
            drawToolsByCategory(EditorToolCategory::Window);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Gameplay"))
        {
            drawToolsByCategory(EditorToolCategory::Gameplay);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem("Rescan Asset Registry"))
            {
                AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
                AssetRegistry::Get().WriteRegistry();
            }
            drawToolsByCategory(EditorToolCategory::Assets);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build"))
        {
            if (ImGui::MenuItem("Package Player + Editor", nullptr, false, !m_PlayerBuildRunning))
                StartPlayerPackageBuild(false);
            if (ImGui::MenuItem("Open Player Folder", nullptr, false, !m_LastPlayerBuildDirectory.empty()))
                EditorPlatform::OpenDirectory(m_LastPlayerBuildDirectory);
            if (ImGui::MenuItem("Open Editor Folder", nullptr, false, !m_LastEditorBuildDirectory.empty()))
                EditorPlatform::OpenDirectory(m_LastEditorBuildDirectory);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Diagnostics"))
        {
            drawToolsByCategory(EditorToolCategory::Diagnostics);
            ImGui::MenuItem("Physics Colliders", nullptr, &m_ShowPhysicsColliders);
            ImGui::MenuItem("Stats Window", nullptr, &m_ShowStats);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("App"))
        {
            if (ImGui::BeginMenu(EditorLocale::Text("Language", "语言")))
            {
                EditorLocale::DrawLanguageMenu();
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Exit")) Application::Get().Close();
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
    void EditorLayerBase::UI_Stats()
    {
        ImGui::Begin("Stats");

        const std::string hoveredName = m_HoveredEntity && !IsEditorHidden(m_HoveredEntity)
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
            if (ImGui::Button("Open Player Folder"))
                EditorPlatform::OpenDirectory(m_LastPlayerBuildDirectory);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_LastPlayerBuildDirectory.string().c_str());

            if (!m_LastEditorBuildDirectory.empty())
            {
                if (ImGui::Button("Open Editor Folder"))
                    EditorPlatform::OpenDirectory(m_LastEditorBuildDirectory);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", m_LastEditorBuildDirectory.string().c_str());
            }
        }
        ImGui::End();
    }

    void EditorLayerBase::UI_CanvasEditor()
    {
        m_UIEditorMouseOverCanvas = false;

        if (!m_UIEditorOpen)
            return;

        if (m_FocusCanvasEditor)
        {
            ImGui::SetNextWindowFocus();
            m_FocusCanvasEditor = false;
        }

        EditorFloatingWindow::Begin("UI Canvas Editor", &m_UIEditorOpen, 0, { 1280.0f, 760.0f });

        auto isValidEntity = [this](Entity entity)
        {
            return entity
                && m_ActiveScene
                && entity.GetScene() == m_ActiveScene.get()
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
        ImGui::BeginDisabled(m_SceneState != SceneState::Edit || !CommandHistory::Get().CanUndo());
        if (ImGui::Button("Undo"))
            CommandHistory::Get().Undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SceneState != SceneState::Edit || !CommandHistory::Get().CanRedo());
        if (ImGui::Button("Redo"))
            CommandHistory::Get().Redo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SceneState != SceneState::Edit);
        if (ImGui::Button("Save"))
            SaveScene();
        ImGui::EndDisabled();
        ImGui::SameLine();
        EditorFloatingWindow::DrawToggleButton("UI Canvas Editor");

        if (!m_UIEditingCanvas || !m_UIEditingCanvas.HasComponent<UICanvasComponent>())
        {
            ImGui::TextWrapped("Select or create a UI Canvas to edit UI in this dedicated view.");
            EditorFloatingWindow::End();
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
        const ImVec2 canvasMax = { canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y };
        const bool canvasRectHovered = ImGui::IsMouseHoveringRect(canvasMin, canvasMax, true);
        const bool surfaceHovered = canvasRectHovered
            && (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)
                || ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem));
        m_UIEditorMouseOverCanvas = surfaceHovered;
        m_UIEditorCanvasBounds[0] = { canvasMin.x, canvasMin.y };
        m_UIEditorCanvasBounds[1] = { canvasMax.x, canvasMax.y };

        if (m_SceneState == SceneState::Edit
            && (surfaceHovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
        {
            ProcessCanvasEditorHistoryShortcuts();
        }

        UI_DrawCanvasSceneReference(
            { canvasMin.x, canvasMin.y },
            { canvasSize.x, canvasSize.y });

        UI_DrawCanvasOverlay(
            { canvasMin.x, canvasMin.y },
            { canvasSize.x, canvasSize.y },
            surfaceHovered,
            false,
            m_UIEditingCanvas);

        EditorFloatingWindow::End();
    }

    void EditorLayerBase::UI_DrawCanvasSceneReference(const glm::vec2& regionMin,
        const glm::vec2& regionSize)
    {
        if (!m_Framebuffer || regionSize.x <= 1.0f || regionSize.y <= 1.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = { regionMin.x, regionMin.y };
        const ImVec2 max = { regionMin.x + regionSize.x, regionMin.y + regionSize.y };
        drawList->AddRectFilled(min, max, IM_COL32(28, 32, 38, 255));
        drawList->AddImage(
            reinterpret_cast<void*>(static_cast<uintptr_t>(m_UIReferenceFramebuffer
                ? m_UIReferenceFramebuffer->GetColorAttachmentRendererID()
                : m_Framebuffer->GetColorAttachmentRendererID())),
            min,
            max,
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f),
            IM_COL32(255, 255, 255, 255));
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
        std::unordered_set<uint32_t> selectedHiddenVisiting;
        const bool selectedHiddenInEditor = IsEditorUIHidden(selected, layout, selectedHiddenVisiting);
        const bool selectedInsideThisCanvas = selected && selected != canvasEntity
            && !selectedHiddenInEditor
            && BelongsToCanvas(selected, canvasEntity, layout);
        const bool canvasIsSelectionContext = selected == canvasEntity || selectedInsideThisCanvas;

        if (drawBackdrop)
        {
            const ImU32 background = canvasIsSelectionContext
                ? IM_COL32(20, 38, 35, 48)
                : IM_COL32(18, 23, 29, 42);
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
            std::unordered_set<uint32_t> hiddenVisiting;
            if (IsEditorUIHidden(entity, layout, hiddenVisiting))
                continue;
            if (!UIWidgetLayout::ResolveEditorVisible(layout, entityID))
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
            || ImGui::IsMouseHoveringRect(canvasMin, canvasMax, true);
        const glm::vec2 mouseNorm = {
            (mouse.x - canvasMin.x) / std::max(1.0f, regionSize.x),
            (mouse.y - canvasMin.y) / std::max(1.0f, regionSize.y)
        };
        constexpr float handleSize = 9.0f;
        int selectedHandleUnderMouse = UIEdit_None;
        if (selectedInsideThisCanvas)
        {
            const entt::entity selectedHandleID = static_cast<entt::entity>(selected);
            if (registry.valid(selectedHandleID) && registry.all_of<UIWidgetComponent>(selectedHandleID))
            {
                const UIWidgetLayout::Rect selectedHitRect = UIWidgetLayout::ResolveRect(layout, selectedHandleID);
                const ImVec2 selectedHitMin = ToScreenPoint(regionMin, regionSize, selectedHitRect.Left, selectedHitRect.Top);
                const ImVec2 selectedHitMax = ToScreenPoint(regionMin, regionSize, selectedHitRect.Right, selectedHitRect.Bottom);
                selectedHandleUnderMouse = HitResizeHandle(mouse, selectedHitMin, selectedHitMax, handleSize + 6.0f);
            }
        }
        Entity clickedUI;
        if (mouseInCanvas
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            if (selectedHandleUnderMouse != UIEdit_None)
            {
                clickedUI = selected;
            }
            else
            {
                int bestSort = std::numeric_limits<int>::min();
                float bestArea = std::numeric_limits<float>::max();
                for (const ViewportUIEntry& entry : entries)
                {
                    if (entry.EntityRef == canvasEntity || !PointInNormalizedRect(entry.Rect, mouseNorm))
                        continue;

                    const float area = std::max(entry.Rect.Right - entry.Rect.Left, 0.0f)
                        * std::max(entry.Rect.Bottom - entry.Rect.Top, 0.0f);
                    if (entry.SortOrder > bestSort
                        || (entry.SortOrder == bestSort && area <= bestArea))
                    {
                        clickedUI = entry.EntityRef;
                        bestSort = entry.SortOrder;
                        bestArea = area;
                    }
                }
            }

            if (clickedUI)
            {
                SelectEditorEntity(clickedUI, false);
                selected = clickedUI;
            }
        }

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
            if (isSelected || m_ShowUIOutlines || drawBackdrop)
                DrawUIOutline(drawList, rectMin, rectMax, entry.EntityRef, color, isSelected ? 2.0f : 1.0f);

            if (isSelected)
            {
                const std::string label = entry.Name.empty() ? "UI Widget" : entry.Name;
                const ImVec2 labelPos = {
                    rectMin.x + 4.0f,
                    rectMin.y - 18.0f >= canvasMin.y ? rectMin.y - 18.0f : rectMin.y + 4.0f
                };
                drawList->AddText(labelPos, IM_COL32(160, 244, 250, 235), label.c_str());
            }
            else if (m_ShowUIOutlines)
            {
                const std::string label = entry.Name.empty() ? "UI Widget" : entry.Name;
                const ImVec2 labelPos = {
                    rectMin.x + 4.0f,
                    rectMin.y - 18.0f >= canvasMin.y ? rectMin.y - 18.0f : rectMin.y + 4.0f
                };
                drawList->AddText(labelPos, IM_COL32(150, 192, 198, 170), label.c_str());
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

        std::unordered_set<uint32_t> handleHiddenVisiting;
        if (IsEditorUIHidden(selected, layout, handleHiddenVisiting))
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

        if (selected == canvasEntity)
        {
            drawList->PopClipRect();
            return;
        }

        UIWidgetLayout::Rect selectedRect = UIWidgetLayout::ResolveRect(layout, selectedID);
        ImVec2 rectMin = ToScreenPoint(regionMin, regionSize, selectedRect.Left, selectedRect.Top);
        ImVec2 rectMax = ToScreenPoint(regionMin, regionSize, selectedRect.Right, selectedRect.Bottom);

        auto& widget = selected.GetComponent<UIWidgetComponent>();
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
            && (selected == clickedUI || hoveredOperation == UIEdit_Move || hoveredHandle != UIEdit_None)
            && hoveredOperation != UIEdit_None
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            m_UIEditHandle = hoveredOperation;
            m_UIEditEntity = selected;
            m_UIEditStartMouse = mouseNorm;
            m_UIEditStartRect = RectToVec4(UIWidgetLayout::WidgetToLocalRect(widget));
            m_UIEditStartWidget = std::make_unique<UIWidgetComponent>(widget);
            m_UIEditStartHadText = selected.HasComponent<UITextComponent>();
            m_UIEditStartText.reset();
            if (m_UIEditStartHadText)
                m_UIEditStartText = std::make_unique<UITextComponent>(selected.GetComponent<UITextComponent>());
        }

        if (m_UIEditHandle != UIEdit_None && m_UIEditEntity == selected)
        {
            float parentWidth = 1.0f;
            float parentHeight = 1.0f;
            const entt::entity parentID = layout.ResolveReference(widget.ParentEntity);
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

    Entity EditorLayerBase::PickViewportEditorEntity(const glm::vec2& screenMouse)
    {
        auto isValidEntity = [this](Entity entity)
        {
            return entity
                && m_ActiveScene
                && entity.GetScene() == m_ActiveScene.get()
                && m_ActiveScene->GetRegistry().valid(
                    static_cast<entt::entity>(static_cast<uint32_t>(entity)));
        };

        if (isValidEntity(m_HoveredEntity) && !IsEditorHidden(m_HoveredEntity))
            return m_HoveredEntity;

        return PickSceneSpriteEntityAtViewportPoint(screenMouse);
    }

    Entity EditorLayerBase::PickUIEntityAtCanvasPoint(const glm::vec2& regionMin,
        const glm::vec2& regionSize,
        Entity canvasEntity,
        const glm::vec2& screenMouse)
    {
        if (!m_ActiveScene || !canvasEntity || !canvasEntity.HasComponent<UICanvasComponent>())
            return {};
        if (regionSize.x <= 1.0f || regionSize.y <= 1.0f)
            return {};
        if (screenMouse.x < regionMin.x || screenMouse.x > regionMin.x + regionSize.x
            || screenMouse.y < regionMin.y || screenMouse.y > regionMin.y + regionSize.y)
            return {};

        const glm::vec2 mouseNorm = {
            (screenMouse.x - regionMin.x) / std::max(1.0f, regionSize.x),
            (screenMouse.y - regionMin.y) / std::max(1.0f, regionSize.y)
        };

        UIWidgetLayout::Context layout(m_ActiveScene.get());
        auto& registry = m_ActiveScene->GetRegistry();
        auto view = registry.view<TagComponent, UIWidgetComponent>();

        Entity picked;
        int bestSort = std::numeric_limits<int>::min();
        float bestArea = std::numeric_limits<float>::max();

        for (auto entityID : view)
        {
            Entity entity{ entityID, m_ActiveScene.get() };
            if (entity == canvasEntity || !BelongsToCanvas(entity, canvasEntity, layout))
                continue;
            std::unordered_set<uint32_t> hiddenVisiting;
            if (IsEditorUIHidden(entity, layout, hiddenVisiting))
                continue;
            if (!UIWidgetLayout::ResolveEditorVisible(layout, entityID))
                continue;

            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, entityID);
            if (rect.Right <= rect.Left || rect.Bottom <= rect.Top)
                continue;
            if (!UIWidgetLayout::RectVisibleInClipAndViewport(layout, entityID, rect))
                continue;
            if (!PointInNormalizedRect(rect, mouseNorm))
                continue;

            const auto& widget = view.get<UIWidgetComponent>(entityID);
            const float area = std::max(rect.Right - rect.Left, 0.0f)
                * std::max(rect.Bottom - rect.Top, 0.0f);
            if (widget.SortOrder > bestSort || (widget.SortOrder == bestSort && area <= bestArea))
            {
                picked = entity;
                bestSort = widget.SortOrder;
                bestArea = area;
            }
        }

        return picked;
    }

    Entity EditorLayerBase::PickSceneSpriteEntityAtViewportPoint(const glm::vec2& screenMouse)
    {
        if (!m_ActiveScene || m_ViewportSize.x <= 1.0f || m_ViewportSize.y <= 1.0f)
            return {};
        if (screenMouse.x < m_ViewportBounds[0].x || screenMouse.x > m_ViewportBounds[1].x
            || screenMouse.y < m_ViewportBounds[0].y || screenMouse.y > m_ViewportBounds[1].y)
            return {};

        auto& registry = m_ActiveScene->GetRegistry();
        auto view = registry.view<TransformComponent, SpriteRendererComponent>();
        const glm::mat4 viewProjection = m_EditorCamera.GetViewProjection();
        const glm::vec4 corners[4] = {
            { -0.5f, -0.5f, 0.0f, 1.0f },
            {  0.5f, -0.5f, 0.0f, 1.0f },
            {  0.5f,  0.5f, 0.0f, 1.0f },
            { -0.5f,  0.5f, 0.0f, 1.0f }
        };

        Entity picked;
        float bestZ = -std::numeric_limits<float>::max();
        float bestArea = std::numeric_limits<float>::max();

        for (auto entityID : view)
        {
            Entity entity{ entityID, m_ActiveScene.get() };
            if (IsEditorHidden(entity))
                continue;

            auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entityID);
            if (sprite.Color.a <= 0.01f)
                continue;

            glm::mat4 drawTransform = transform.GetTransform();
            if (sprite.DrawOffset.x != 0.0f || sprite.DrawOffset.y != 0.0f)
                drawTransform *= glm::translate(glm::mat4(1.0f), { sprite.DrawOffset.x, sprite.DrawOffset.y, 0.0f });
            if (sprite.DrawScale.x != 1.0f || sprite.DrawScale.y != 1.0f)
                drawTransform *= glm::scale(glm::mat4(1.0f), { sprite.DrawScale.x, sprite.DrawScale.y, 1.0f });

            glm::vec2 minPoint = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
            glm::vec2 maxPoint = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
            bool projectedAnyCorner = false;

            for (const glm::vec4& corner : corners)
            {
                const glm::vec4 clip = viewProjection * drawTransform * corner;
                if (clip.w <= 0.0001f)
                    continue;

                const glm::vec3 ndc = glm::vec3(clip) / clip.w;
                const glm::vec2 projected = {
                    m_ViewportBounds[0].x + (ndc.x * 0.5f + 0.5f) * m_ViewportSize.x,
                    m_ViewportBounds[0].y + (1.0f - (ndc.y * 0.5f + 0.5f)) * m_ViewportSize.y
                };
                minPoint = glm::min(minPoint, projected);
                maxPoint = glm::max(maxPoint, projected);
                projectedAnyCorner = true;
            }

            if (!projectedAnyCorner)
                continue;
            if (screenMouse.x < minPoint.x || screenMouse.x > maxPoint.x
                || screenMouse.y < minPoint.y || screenMouse.y > maxPoint.y)
                continue;

            const float z = transform.Translation.z;
            const float area = std::max(maxPoint.x - minPoint.x, 0.0f)
                * std::max(maxPoint.y - minPoint.y, 0.0f);
            if (!picked || z > bestZ || (z == bestZ && area < bestArea))
            {
                picked = entity;
                bestZ = z;
                bestArea = area;
            }
        }

        return picked;
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
