#include "wepch.h"
#include "EditorLayerBase.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Events/Event.h"
#include "Wheatear/Events/KeyEvent.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/ImGui/ImGuiLayer.h"
#include "Wheatear/Renderer/Framebuffer.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/Renderer3D.h"
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
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorPlatform.h"
#include "Editor/EditorWidgets.h"
#include "Editor/EditorToolRegistry.h"
#include "Panels/AnimationEditorPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/DataFileEditorPanel.h"
#include "Panels/EditorHelpPanel.h"
#include "Panels/InputBindingsPanel.h"
#include "Panels/ConsolePanel.h"
#include "Editor/EditorCommands.h"
#include "Panels/SceneHierarchy/SceneHierarchyPanel.h"
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
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <system_error>
#include <unordered_set>


namespace Wheatear {

    namespace {

        struct ViewportUIEntry
        {
            Entity EntityRef;
            UIWidgetLayout::Rect Rect;
            int SortOrder = 0;
            std::string Name;
        };




        static bool PointInNormalizedRect(const UIWidgetLayout::Rect& rect, const glm::vec2& point)
        {
            return point.x >= rect.Left && point.x <= rect.Right
                && point.y >= rect.Top && point.y <= rect.Bottom;
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

        // Handle Ctrl+wheel before panels consume the wheel for scrolling or
        // their own zoom, so changing UI scale doesn't also scroll the tree.
        ImGuiIO& scaleIo = ImGui::GetIO();
        if (scaleIo.KeyCtrl && scaleIo.MouseWheel != 0.0f && !scaleIo.WantTextInput
            && !ImGui::IsAnyItemActive())
        {
            ImGuiWindow* hovered = GImGui ? GImGui->HoveredWindow : nullptr;
            while (hovered && hovered->ParentWindow)
                hovered = hovered->ParentWindow;
            const std::string hoveredName = hovered ? hovered->Name : "";
            const bool panelOwnsCtrlWheel =
                hoveredName == "Viewport"
                || hoveredName == "UI Canvas Editor"
                || hoveredName == "Content Browser"
                || hoveredName == "Animation Editor"
                || hoveredName == "Sprite Sheet Picker"
                || hoveredName == "WAO Behavior Graph"
                || hoveredName == "Event Script Editor";
            if (!panelOwnsCtrlWheel && !hoveredName.empty()
                && !m_ViewportImageHovered && !m_ViewportHovered)
            {
                if (ImGuiLayer* gui = Application::Get().GetImGuiLayer())
                {
                    gui->AdjustUiScale(scaleIo.MouseWheel > 0.0f ? 0.05f : -0.05f);
                    scaleIo.MouseWheel = 0.0f;
                }
            }
        }

        static bool dockspaceOpen   = true;
        static bool opt_fullscreen  = true;
        // Central node is pass-through: dragging a panel into the viewport
        // centre docks it as a floating tab instead of squeezing the view.
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        UI_TitleBar();
        const float chromeScale = Application::Get().GetImGuiLayer()
            ? Application::Get().GetImGuiLayer()->GetUiScale()
            : 1.0f;
        const float titleBarHeight = kTitleBarHeight * chromeScale;
        const float statusBarHeight = kStatusBarHeight * chromeScale;

        if (opt_fullscreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x,
                viewport->WorkPos.y + titleBarHeight));
            ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x,
                viewport->WorkSize.y - titleBarHeight - statusBarHeight));
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
        const ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");

        // Menu bar and the fixed action bar must be submitted before the
        // DockSpace so the dockspace content region starts below them. This
        // is what keeps scene/editor tabs free of overlapping toolbar buttons.
        UI_MenuBar();
        if (m_ToolbarVisible)
            UI_Toolbar();

        const float prevMinX = style.WindowMinSize.x;
        style.WindowMinSize.x = 320.0f;
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            BuildDefaultDockspaceLayout(dockspaceId);
            ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspace_flags);
            if (ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport"))
            {
                if (viewportWindow->DockId != 0)
                    EditorFloatingWindow::SetDefaultDockNode(viewportWindow->DockId);
            }
        }
        style.WindowMinSize.x = prevMinX;

        m_SceneHierarchyPanel->OnImGuiRender();
        m_ContentBrowserPanel->OnImGuiRender();
        if (!Is3DWorkspace())
        {
            m_AnimationEditorPanel->SetEntity(m_SceneHierarchyPanel->GetSelectedEntity());
            if (m_AnimationEditorOpen)
                m_AnimationEditorPanel->OnImGuiRender(m_LastTimestep);
            m_SpriteSheetPickerPanel->SetEntity(m_SceneHierarchyPanel->GetSelectedEntity());
            m_SpriteSheetPickerPanel->OnImGuiRender();
        }
        m_DataFileEditorPanel->OnImGuiRender();
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
        ProcessDeferredViewportAssetDrop();

        DrawUnsavedChangesModal();

        m_HelpPanel->OnImGuiRender();
        m_InputBindingsPanel->OnImGuiRender();
        m_ConsolePanel->OnImGuiRender();

        ImGui::End();

        UI_StatusBar();
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

    void EditorLayerBase::ActivateHierarchyEntity(Entity entity, bool additive)
    {
        if (m_SceneState != SceneState::Edit || !entity)
            return;

        if (additive)
        {
            // Ctrl+click: toggle membership in the scene multi-select while
            // keeping the clicked entity as the primary (anchor) selection.
            const UUID uuid = entity.GetUUID();
            auto it = std::find(m_SceneMultiSelect.begin(), m_SceneMultiSelect.end(), uuid);
            if (it != m_SceneMultiSelect.end())
                m_SceneMultiSelect.erase(it);
            else
                m_SceneMultiSelect.push_back(uuid);
            SelectEditorEntity(entity, !entity.HasComponent<UIWidgetComponent>());
            return;
        }

        m_SceneMultiSelect.clear();
        SelectEditorEntity(entity, !entity.HasComponent<UIWidgetComponent>());

        if (entity.HasComponent<UIWidgetComponent>())
        {
            OpenCanvasEditorForEntity(entity);
            return;
        }

        FrameEditorCameraOnEntity(entity);
    }

    void EditorLayerBase::ToggleSceneMultiSelect(Entity entity)
    {
        if (!entity)
            return;
        const UUID uuid = entity.GetUUID();
        auto it = std::find(m_SceneMultiSelect.begin(), m_SceneMultiSelect.end(), uuid);
        if (it != m_SceneMultiSelect.end())
            m_SceneMultiSelect.erase(it);
        else
            m_SceneMultiSelect.push_back(uuid);
    }

    void EditorLayerBase::ClearSceneMultiSelect()
    {
        m_SceneMultiSelect.clear();
    }

    void EditorLayerBase::SyncBuiltinContentToProject()
    {
        const std::filesystem::path engineRoot = AssetPath::GetEngineRoot();
        const std::filesystem::path projectRoot = AssetPath::GetProjectRoot();
        const std::filesystem::path templateRoot = engineRoot / "ContentTemplates";

        // The engine root no longer carries a live copy of game content;
        // ContentTemplates is the creation/sync seed for projects.
        const std::vector<std::pair<std::string, std::string>> entries = {
            { "gameplay/actions",               "assets/gameplay/actions" },
            { "gameplay/content_manifest.yaml", "assets/gameplay/content_manifest.yaml" },
            { "gameplay/progression",           "assets/gameplay/progression" },
            { "fonts",                          "assets/fonts" },
            { "input/action_bindings.yaml",     "assets/input/action_bindings.yaml" },
        };

        size_t copied = 0;
        size_t skipped = 0;
        std::error_code ec;

        for (const auto& [sourceRel, targetRel] : entries)
        {
            const std::filesystem::path source = templateRoot / sourceRel;
            const std::filesystem::path target = projectRoot / targetRel;
            if (!std::filesystem::exists(source, ec))
                continue;

            if (std::filesystem::is_directory(source, ec))
            {
                for (const auto& entry :
                    std::filesystem::recursive_directory_iterator(source, ec))
                {
                    if (ec || !entry.is_regular_file(ec))
                        continue;
                    const std::filesystem::path relative =
                        std::filesystem::relative(entry.path(), source, ec);
                    const std::filesystem::path destination = target / relative;
                    if (std::filesystem::exists(destination, ec))
                    {
                        ++skipped;
                        continue;
                    }
                    std::filesystem::create_directories(
                        destination.parent_path(), ec);
                    std::filesystem::copy_file(entry.path(), destination,
                        std::filesystem::copy_options::none, ec);
                    if (!ec)
                        ++copied;
                }
            }
            else if (std::filesystem::exists(source, ec))
            {
                if (std::filesystem::exists(target, ec))
                {
                    ++skipped;
                    continue;
                }
                std::filesystem::create_directories(target.parent_path(), ec);
                std::filesystem::copy_file(source, target,
                    std::filesystem::copy_options::none, ec);
                if (!ec)
                    ++copied;
            }
        }

        // Refresh the registry so the copied content shows up immediately.
        AssetRegistry::Get().Scan(projectRoot);
        AssetRegistry::Get().WriteRegistry();

        m_BuiltinSyncStatus = "Copied " + std::to_string(copied)
            + " file(s) to project (" + std::to_string(skipped)
            + " already present, kept).";
    }

    // Scene-entity counterpart of UI_AlignSelection: operates on world-space
    // TransformComponent positions instead of normalized widget rects. The
    // hierarchy selection acts as the anchor; each affected entity commits one
    // undo record.
    void EditorLayerBase::AlignSceneSelection(UISelectionAction action)
    {
        if (m_SceneState != SceneState::Edit || !m_ActiveScene)
            return;

        Entity anchor = m_SceneHierarchyPanel->GetSelectedEntity();
        if (!anchor || !anchor.HasComponent<TransformComponent>())
            return;

        const glm::vec3 anchorPos = anchor.GetComponent<TransformComponent>().Translation;

        std::vector<Entity> targets;
        for (const UUID& uuid : m_SceneMultiSelect)
        {
            Entity entity = m_ActiveScene->GetEntityByUUID(uuid);
            if (!entity || !entity.HasComponent<TransformComponent>())
                continue;
            if (entity == anchor)
                continue;
            targets.push_back(entity);
        }
        if (targets.empty())
            return;

        // Distribute: sort by center axis, keep first/last, spread the rest.
        if (action == UISelectionAction::DistributeHorizontal
            || action == UISelectionAction::DistributeVertical)
        {
            const bool horizontal = action == UISelectionAction::DistributeHorizontal;
            std::sort(targets.begin(), targets.end(),
                [horizontal](Entity a, Entity b)
                {
                    const glm::vec3& pa = a.GetComponent<TransformComponent>().Translation;
                    const glm::vec3& pb = b.GetComponent<TransformComponent>().Translation;
                    return horizontal ? pa.x < pb.x : pa.y < pb.y;
                });
            if (targets.size() >= 3)
            {
                const glm::vec3& firstPos =
                    targets.front().GetComponent<TransformComponent>().Translation;
                const glm::vec3& lastPos =
                    targets.back().GetComponent<TransformComponent>().Translation;
                const float first = horizontal ? firstPos.x : firstPos.y;
                const float last = horizontal ? lastPos.x : lastPos.y;
                for (size_t i = 1; i + 1 < targets.size(); ++i)
                {
                    const float t = static_cast<float>(i)
                        / static_cast<float>(targets.size() - 1);
                    Entity target = targets[i];
                    auto& transform = target.GetComponent<TransformComponent>();
                    TransformComponent before = transform;
                    if (horizontal)
                        transform.Translation.x = first + (last - first) * t;
                    else
                        transform.Translation.y = first + (last - first) * t;
                    if (transform.Translation != before.Translation)
                    {
                        CommandHistory::Get().Push(
                            MakeComponentValueCommand(target, before, transform));
                    }
                }
            }
            return;
        }

        for (Entity target : targets)
        {
            auto& transform = target.GetComponent<TransformComponent>();
            TransformComponent before = transform;
            switch (action)
            {
            case UISelectionAction::AlignLeft:
            case UISelectionAction::AlignRight:
            case UISelectionAction::AlignHCenter:
                transform.Translation.x = anchorPos.x;
                break;
            case UISelectionAction::AlignTop:
            case UISelectionAction::AlignBottom:
            case UISelectionAction::AlignVCenter:
                transform.Translation.y = anchorPos.y;
                break;
            default:
                break;
            }
            if (transform.Translation != before.Translation)
            {
                CommandHistory::Get().Push(
                    MakeComponentValueCommand(target, before, transform));
            }
        }
    }

    void EditorLayerBase::CopySelectedEntity()
    {
        if (m_SceneState != SceneState::Edit || !m_EditorScene)
            return;

        Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
        if (!selected || !selected.HasComponent<IDComponent>())
            return;

        const std::filesystem::path clipboardPath =
            AssetPath::GetProjectRoot() / "Intermediate" / "Clipboard.wtprefab";
        std::error_code error;
        std::filesystem::create_directories(clipboardPath.parent_path(), error);
        if (SceneSerializer::SerializePrefab(selected, clipboardPath))
            m_HasClipboardEntity = true;
    }

    void EditorLayerBase::PasteClipboardEntity()
    {
        if (m_SceneState != SceneState::Edit || !m_EditorScene || !m_HasClipboardEntity)
            return;

        const std::filesystem::path clipboardPath =
            AssetPath::GetProjectRoot() / "Intermediate" / "Clipboard.wtprefab";
        if (!std::filesystem::exists(clipboardPath))
        {
            m_HasClipboardEntity = false;
            return;
        }

        Entity root = SceneSerializer::DeserializePrefab(clipboardPath, m_EditorScene.get());
        if (!root)
            return;

        // UI roots whose original parent was outside the copied subtree end up
        // with a dangling ParentEntity after remap; re-parent under the current
        // selection (or detach to the canvas root).
        if (root.HasComponent<UIWidgetComponent>())
        {
            auto& widget = root.GetComponent<UIWidgetComponent>();
            if (widget.ParentEntity != 0)
            {
                Entity parent = m_EditorScene->GetEntityByUUID(widget.ParentEntity);
                if (!parent || parent.GetUUID() == root.GetUUID())
                {
                    Entity current = m_SceneHierarchyPanel->GetSelectedEntity();
                    if (current && current != root
                        && current.HasComponent<UIWidgetComponent>())
                        widget.ParentEntity = current.GetUUID();
                    else
                        widget.ParentEntity = 0;
                }
            }
        }

        // Unique name (mirrors InstantiatePrefab).
        const std::string baseName = root.GetName() + "Copy";
        std::string finalName = baseName;
        int index = 1;
        while (true)
        {
            bool exists = false;
            for (auto id : m_EditorScene->GetRegistry().view<IDComponent>())
            {
                Entity other{ id, m_EditorScene.get() };
                if (other && other != root && other.GetName() == finalName)
                    exists = true;
            }
            if (!exists)
                break;
            finalName = baseName + std::to_string(index++);
        }
        if (root.HasComponent<TagComponent>())
            root.GetComponent<TagComponent>().Tag = finalName;

        m_SceneMultiSelect.clear();
        SelectEditorEntity(root, true);
        FrameEditorCameraOnEntity(root);
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
        const bool twoD = !Is3DWorkspace();

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
            if (twoD)
            {
                glm::vec2 halfExtent(0.5f);
                if (entity.HasComponent<SpriteRendererComponent>())
                {
                    const auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                    halfExtent = glm::abs(glm::vec2(transform.Scale)
                        * sprite.DrawScale) * 0.5f;
                }
                else if (entity.HasComponent<CircleRendererComponent>())
                {
                    halfExtent = glm::abs(glm::vec2(transform.Scale)) * 0.5f;
                }
                halfExtent = glm::max(halfExtent, glm::vec2(0.25f));
                minBounds = glm::min(minBounds,
                    transform.Translation - glm::vec3(halfExtent, 0.0f));
                maxBounds = glm::max(maxBounds,
                    transform.Translation + glm::vec3(halfExtent, 0.0f));
            }
            else
            {
                const float radius = EntityFrameRadius(entity);
                minBounds = glm::min(minBounds,
                    transform.Translation - glm::vec3(radius));
                maxBounds = glm::max(maxBounds,
                    transform.Translation + glm::vec3(radius));
            }
            foundRenderable = true;
        }

        if (!foundRenderable)
        {
            if (Entity camera = m_ActiveScene->GetPrimaryCameraEntity())
                FocusEditorCameraOnPrimarySceneCamera();
            return;
        }

        const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
        if (twoD)
        {
            // Fit the 2D bounds to the viewport instead of using the 3D
            // spherical radius (which made wide sprites appear tiny).
            const float halfWidth = std::max((maxBounds.x - minBounds.x) * 0.5f, 0.5f);
            const float halfHeight = std::max((maxBounds.y - minBounds.y) * 0.5f, 0.5f);
            const float aspect = (m_ViewportSize.x > 1.0f && m_ViewportSize.y > 1.0f)
                ? m_ViewportSize.x / m_ViewportSize.y
                : 16.0f / 9.0f;
            const float tanHalfFov = std::tan(glm::radians(15.0f));
            const float distance = std::max(
                halfHeight / tanHalfFov,
                halfWidth / (tanHalfFov * std::max(aspect, 0.25f))) * 1.12f;
            m_EditorCamera.Frame(center, std::max(distance, 2.0f));
        }
        else
        {
            const glm::vec3 extent = maxBounds - minBounds;
            const float radius = std::max(glm::length(extent) * 0.5f, 1.0f);
            m_EditorCamera.Frame(center, std::max(radius * 3.0f, 10.0f));
        }
        m_SceneHierarchyPanel->SetSelectedEntity({});
        m_AnimationEditorPanel->SetEntity({});
    }

    void EditorLayerBase::UI_TitleBar()
    {
        const float s = Application::Get().GetImGuiLayer()
            ? Application::Get().GetImGuiLayer()->GetUiScale()
            : 1.0f;
        const float textScale = ImGui::GetIO().FontGlobalScale;
        const float titleHeight = kTitleBarHeight * s;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titleHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.040f, 0.047f, 0.060f, 1.0f));

        ImGui::Begin("##EditorTitleBar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + ImGui::GetWindowWidth(),
            min.y + ImGui::GetWindowHeight());
        const float centerY = min.y + titleHeight * 0.5f;
        Window& nativeWindow = Application::Get().GetWindow();
        ImGuiLayer* gui = Application::Get().GetImGuiLayer();
        const bool darkTheme = !gui || gui->IsDarkTheme();
        const ImU32 titleBg = darkTheme ? IM_COL32(10, 12, 17, 255)
                                        : IM_COL32(238, 234, 226, 255);
        const ImU32 titleBorder = darkTheme ? IM_COL32(43, 48, 60, 255)
                                            : IM_COL32(202, 196, 184, 255);
        const ImU32 titleInk = darkTheme ? IM_COL32(205, 210, 220, 255)
                                         : IM_COL32(45, 48, 56, 255);
        const ImU32 titleMuted = darkTheme ? IM_COL32(175, 181, 194, 255)
                                           : IM_COL32(92, 96, 106, 255);
        const ImU32 controlHover = darkTheme ? IM_COL32(40, 44, 56, 255)
                                             : IM_COL32(218, 212, 200, 255);

        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton("##TitleDrag",
            ImVec2(ImGui::GetWindowWidth() - 144.0f * s, titleHeight));
        const bool titleHovered = ImGui::IsItemHovered();
        if (titleHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            nativeWindow.ToggleMaximizeWindow();
        else if (ImGui::IsItemActivated())
            nativeWindow.BeginWindowDrag();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, titleBg);
        drawList->AddLine(ImVec2(min.x, max.y - 1.0f),
            ImVec2(max.x, max.y - 1.0f), titleBorder, 1.0f);

        EditorWidgets::BrandMark(
            ImVec2(min.x + 22.0f * s, centerY), 23.0f * s, false);
        drawList->AddText(ImGui::GetFont(), 17.0f * textScale,
            ImVec2(min.x + 40.0f * s, centerY - 10.0f * s), titleInk, "Wheatear");
        const float brandWidth = ImGui::CalcTextSize("Wheatear").x
            * (17.0f / 16.0f);
        const float studioWidth = ImGui::CalcTextSize("STUDIO").x
            * (13.0f / 16.0f);
        drawList->AddText(ImGui::GetFont(), 13.0f * textScale,
            ImVec2(min.x + 40.0f * s + brandWidth + 7.0f * s,
                centerY - 8.0f * s),
            IM_COL32(217, 170, 95, 220), "STUDIO");

        const float badgeX = std::max(min.x + 158.0f * s,
            min.x + 40.0f * s + brandWidth + studioWidth + 20.0f * s);
        ImGui::SetCursorScreenPos(ImVec2(badgeX, min.y + 8.0f * s));
        EditorWidgets::WorkspaceBadge(GetWorkspaceLabel(), Is3DWorkspace());

        std::string sceneTitle = m_EditorScenePath.empty()
            ? EditorLocale::Text("Untitled Scene", "未命名场景")
            : m_EditorScenePath.filename().string();
        if (m_SceneDirty)
            sceneTitle += " *";
        const ImVec2 titleSize = ImGui::CalcTextSize(sceneTitle.c_str());
        drawList->AddText(
            ImVec2((min.x + max.x - titleSize.x) * 0.5f, centerY - titleSize.y * 0.5f),
            titleInk, sceneTitle.c_str());

        auto windowButton = [&](const char* id, float x, bool closeButton,
            bool maximizeButton)
        {
            ImGui::SetCursorScreenPos(ImVec2(x, min.y));
            const bool clicked = ImGui::InvisibleButton(id,
                ImVec2(46.0f * s, titleHeight));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 bmin(x, min.y);
            const ImVec2 bmax(x + 46.0f * s, max.y - 1.0f);
            if (hovered)
            {
                drawList->AddRectFilled(bmin, bmax,
                    closeButton ? IM_COL32(165, 45, 45, 255) : controlHover);
            }

            const ImVec2 c(x + 23.0f * s, centerY);
            const ImU32 ink = hovered ? IM_COL32(245, 245, 248, 255)
                                      : titleMuted;
            const float glyph = 5.0f * s;
            if (closeButton)
            {
                drawList->AddLine(ImVec2(c.x - glyph, c.y - glyph),
                    ImVec2(c.x + glyph, c.y + glyph), ink, 1.4f);
                drawList->AddLine(ImVec2(c.x + glyph, c.y - glyph),
                    ImVec2(c.x - glyph, c.y + glyph), ink, 1.4f);
            }
            else if (maximizeButton)
            {
                drawList->AddRect(ImVec2(c.x - glyph, c.y - glyph),
                    ImVec2(c.x + glyph, c.y + glyph), ink, 1.0f, 0, 1.2f);
            }
            else
            {
                drawList->AddLine(ImVec2(c.x - 6.0f * s, c.y),
                    ImVec2(c.x + 6.0f * s, c.y), ink, 1.4f);
            }

            if (clicked)
            {
                if (closeButton)
                    nativeWindow.CloseWindow();
                else if (maximizeButton)
                    nativeWindow.ToggleMaximizeWindow();
                else
                    nativeWindow.MinimizeWindow();
            }
        };

        windowButton("##WindowMinimize", max.x - 138.0f * s, false, false);
        windowButton("##WindowMaximize", max.x - 92.0f * s, false, true);
        windowButton("##WindowClose", max.x - 46.0f * s, true, false);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void EditorLayerBase::UI_MenuBar()
    {
        PollPlayerPackageBuild();

        if (!ImGui::BeginMenuBar()) return;

        const EditorToolContext toolContext{ m_SceneHierarchyPanel->GetSelectedEntity(), m_ActiveScene.get() };
        auto drawMenuIcon = [&](const Ref<Texture2D>& icon)
        {
            const float iconSize = 16.0f;
            if (icon)
                ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                    ImVec2(iconSize, iconSize), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            else
                ImGui::Dummy(ImVec2(iconSize, iconSize));
            ImGui::SameLine();
        };
        auto localizedToolLabel = [](const char* label) -> const char*
        {
            if (!label) return "";
            if (std::strcmp(label, "UI Canvas Editor") == 0) return EditorLocale::Text("UI Canvas Editor", "UI 画布编辑器");
            if (std::strcmp(label, "Animation Editor") == 0) return EditorLocale::Text("Animation Editor", "动画编辑器");
            if (std::strcmp(label, "Sprite Sheet Picker") == 0) return EditorLocale::Text("Sprite Sheet Picker", "序列帧选择器");
            if (std::strcmp(label, "Event Script Editor") == 0) return EditorLocale::Text("Event Script Editor", "事件脚本编辑器");
            if (std::strcmp(label, "VN Script Editor") == 0) return EditorLocale::Text("VN Script Editor", "视觉小说脚本编辑器");
            if (std::strcmp(label, "Side Combat Tuning Editor") == 0) return EditorLocale::Text("Side Combat Tuning Editor", "横版战斗调参编辑器");
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

        if (ImGui::BeginMenu(EditorLocale::Text("File", "文件")))
        {
            drawMenuIcon(m_IconNewScene);
            if (ImGui::MenuItem(EditorLocale::Text("New", "新建"), "Ctrl+N"))        NewScene();
            drawMenuIcon(m_IconOpenScene);
            if (ImGui::MenuItem(EditorLocale::Text("Open...", "打开..."), "Ctrl+O"))  OpenScene();
            drawMenuIcon(m_IconSaveScene);
            if (ImGui::MenuItem(EditorLocale::Text("Save", "保存"), "Ctrl+S"))        SaveScene();
            drawMenuIcon(m_IconSaveScene);
            if (ImGui::MenuItem(EditorLocale::Text("Save As...", "另存为..."), "Ctrl+Shift+S"))  SaveSceneAs();
            ImGui::Separator();
            drawMenuIcon(m_IconLogout);
            if (ImGui::MenuItem(EditorLocale::Text("Exit", "退出")))
                RequestSceneChange(PendingSceneAction::Exit);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Edit", "编辑")))
        {
            drawMenuIcon(m_IconUndo);
            if (ImGui::MenuItem(EditorLocale::Text("Undo", "撤销"), "Ctrl+Z", false, CommandHistory::Get().CanUndo()))
                CommandHistory::Get().Undo();
            drawMenuIcon(m_IconRedo);
            if (ImGui::MenuItem(EditorLocale::Text("Redo", "重做"), "Ctrl+Y / Ctrl+Shift+Z", false, CommandHistory::Get().CanRedo()))
                CommandHistory::Get().Redo();
            ImGui::Separator();
            if (ImGui::MenuItem(EditorLocale::Text("Copy", "复制"), "Ctrl+C",
                    false, m_SceneHierarchyPanel->GetSelectedEntity()))
                CopySelectedEntity();
            if (ImGui::MenuItem(EditorLocale::Text("Paste", "粘贴"), "Ctrl+V",
                    false, HasClipboardEntity()))
                PasteClipboardEntity();
            ImGui::Separator();
            if (ImGui::BeginMenu(EditorLocale::Text("Align", "对齐（多选）")))
            {
                const bool hasMulti = !m_SceneMultiSelect.empty();
                if (ImGui::MenuItem(EditorLocale::Text("Align Left (X)", "对齐 X 轴"), nullptr, false, hasMulti))
                    AlignSceneSelection(UISelectionAction::AlignLeft);
                if (ImGui::MenuItem(EditorLocale::Text("Align Right (X)", "对齐 X 轴"), nullptr, false, hasMulti))
                    AlignSceneSelection(UISelectionAction::AlignRight);
                if (ImGui::MenuItem(EditorLocale::Text("Align Top (Y)", "对齐 Y 轴"), nullptr, false, hasMulti))
                    AlignSceneSelection(UISelectionAction::AlignTop);
                if (ImGui::MenuItem(EditorLocale::Text("Align Bottom (Y)", "对齐 Y 轴"), nullptr, false, hasMulti))
                    AlignSceneSelection(UISelectionAction::AlignBottom);
                ImGui::Separator();
                if (ImGui::MenuItem(EditorLocale::Text("Distribute Horizontally", "水平等距分布"), nullptr, false, hasMulti))
                    AlignSceneSelection(UISelectionAction::DistributeHorizontal);
                if (ImGui::MenuItem(EditorLocale::Text("Distribute Vertically", "垂直等距分布"), nullptr, false, hasMulti))
                    AlignSceneSelection(UISelectionAction::DistributeVertical);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Run", "运行")))
        {
            if (m_SceneState == SceneState::Edit)
            {
                drawMenuIcon(m_IconPlay);
                if (ImGui::MenuItem(EditorLocale::Text("Play", "播放"), "Ctrl+Enter"))
                    TransitionToPlay();
            }
            else
            {
                drawMenuIcon(m_IconStop);
                if (ImGui::MenuItem(EditorLocale::Text("Stop", "停止"), "Ctrl+Enter"))
                    TransitionToStop();
            }

            drawMenuIcon(m_IconFocus);
            if (ImGui::MenuItem(EditorLocale::Text("Frame Selection", "聚焦选中"), "F"))
            {
                if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
                    ActivateHierarchyEntity(selected);
                else
                    FrameEditorCameraOnScene();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("UI", "界面")))
        {
            drawMenuIcon(nullptr);
            ImGui::MenuItem(EditorLocale::Text("UI Edit Outlines", "UI 编辑轮廓线"), nullptr, &m_ShowUIOutlines);
            drawMenuIcon(m_IconUICanvas);
            if (ImGui::MenuItem(EditorLocale::Text("UI Canvas Editor", "UI 画布编辑器"), nullptr, &m_UIEditorOpen))
            {
                EditorFloatingWindow::Dock("UI Canvas Editor");
                m_FocusCanvasEditor = true;
                if (!m_UIEditingCanvas)
                    m_UIEditingCanvas = m_SceneHierarchyPanel->GetSelectedEntity();
            }
            if (!Is3DWorkspace())
            {
                drawMenuIcon(m_IconSpriteSheet);
                if (ImGui::MenuItem(EditorLocale::Text("Sprite Sheet Picker", "序列帧选择器")))
                    m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
            }
            drawMenuIcon(nullptr);
            if (ImGui::MenuItem(EditorLocale::Text("Generate UI Templates", "生成内置 UI 模板")))
            {
                UITemplateFactory::WriteBuiltinTemplateAssets(AssetPath::GetProjectRoot());
                AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
                AssetRegistry::Get().WriteRegistry();
            }
            drawToolsByCategory(EditorToolCategory::UI);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Window", "窗口")))
        {
            drawLocalizedFloatingWindowItem("UI Canvas Editor", [&]()
            {
                m_UIEditorOpen = true;
                m_FocusCanvasEditor = true;
            });
            if (!Is3DWorkspace())
            {
                drawMenuIcon(m_IconPanel);
                if (ImGui::MenuItem(EditorLocale::Text("Animation Editor", "动画编辑器"),
                        nullptr, &m_AnimationEditorOpen))
                {
                    if (m_AnimationEditorOpen)
                        EditorFloatingWindow::Dock("Animation Editor");
                }
                drawLocalizedFloatingWindowItem("Sprite Sheet Picker", [&]()
                {
                    m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
                });
            }
            drawLocalizedFloatingWindowItem("Event Script Editor", [&]() { openToolByLabel("Event Script Editor"); });
            drawLocalizedFloatingWindowItem("VN Script Editor", [&]() { openToolByLabel("VN Script Editor"); });
            drawLocalizedFloatingWindowItem("Side Combat Tuning Editor", [&]() { openToolByLabel("Side Combat Tuning Editor"); });
            drawLocalizedFloatingWindowItem("WAO Action Editor", [&]() { openToolByLabel("WAO Action Editor"); });
            drawLocalizedFloatingWindowItem("Asset Alias / Manifest Editor", [&]() { openToolByLabel("Asset Alias / Manifest Editor"); });
            drawLocalizedFloatingWindowItem("Project Health", [&]() { openToolByLabel("Project Health"); });
            ImGui::Separator();
            drawMenuIcon(m_IconResetLayout);
            if (ImGui::MenuItem(EditorLocale::Text("Reset Window Layout", "重置窗口布局")))
            {
                m_RequestDefaultDockspaceLayout = true;
                m_DefaultDockspaceLayoutBuilt = false;
            }
            drawMenuIcon(nullptr);
            ImGui::MenuItem(EditorLocale::Text("Action Toolbar", "操作工具栏"), nullptr, &m_ToolbarVisible);
            drawToolsByCategory(EditorToolCategory::Window);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Gameplay", "玩法")))
        {
            drawToolsByCategory(EditorToolCategory::Gameplay);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Assets", "资源")))
        {
            drawMenuIcon(m_IconRefresh);
            if (ImGui::MenuItem(EditorLocale::Text("Rescan Asset Registry", "重扫资源注册表")))
            {
                AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
                AssetRegistry::Get().WriteRegistry();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(EditorLocale::Text(
                "Sync Content Templates To Project",
                "同步内容模板到项目")))
            {
                SyncBuiltinContentToProject();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", EditorLocale::Text(
                    "Copies the content templates (gameplay data / fonts / "
                    "input bindings) into this project so the project is "
                    "self-contained. Existing project files are kept.",
                    "把内容模板（玩法数据 / 字体 / 输入绑定）复制到当前项目，"
                    "使项目自包含。已存在的项目文件不会被覆盖。"));
            if (!m_BuiltinSyncStatus.empty())
                ImGui::TextDisabled("%s", m_BuiltinSyncStatus.c_str());
            drawToolsByCategory(EditorToolCategory::Assets);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Build", "构建")))
        {
            drawMenuIcon(m_IconPackage);
            if (ImGui::MenuItem(EditorLocale::Text("Package Game + Editor", "打包游戏 + 编辑器"), nullptr, false, !m_PlayerBuildRunning))
                StartPlayerPackageBuild();
            drawMenuIcon(nullptr);
            if (ImGui::MenuItem(EditorLocale::Text("Open Player Folder", "打开玩家目录"), nullptr, false, !m_LastPlayerBuildDirectory.empty()))
                EditorPlatform::OpenDirectory(m_LastPlayerBuildDirectory);
            drawMenuIcon(nullptr);
            if (ImGui::MenuItem(EditorLocale::Text("Open Editor Folder", "打开编辑器目录"), nullptr, false, !m_LastEditorBuildDirectory.empty()))
                EditorPlatform::OpenDirectory(m_LastEditorBuildDirectory);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Diagnostics", "诊断")))
        {
            drawToolsByCategory(EditorToolCategory::Diagnostics);
            ImGui::MenuItem(EditorLocale::Text("Physics Colliders", "物理碰撞体"), nullptr, &m_ShowPhysicsColliders);
            ImGui::MenuItem(EditorLocale::Text("Stats Window", "统计窗口"), nullptr, &m_ShowStats);
            if (ImGui::MenuItem(EditorLocale::Text("Console", "日志")))
                m_ConsolePanel->SetOpen(true);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Settings", "设置")))
        {
            drawMenuIcon(m_IconLanguage);
            if (ImGui::BeginMenu(EditorLocale::Text("Language", "语言")))
            {
                EditorLocale::DrawLanguageMenu();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(EditorLocale::Text("Theme", "主题")))
            {
                ImGuiLayer* gui = Application::Get().GetImGuiLayer();
                if (gui)
                {
                    if (ImGui::MenuItem(EditorLocale::Text("Midnight", "午夜"), nullptr,
                            gui->IsDarkTheme()))
                    {
                        gui->SetDarkThemeColors();
                    }
                    if (ImGui::MenuItem(EditorLocale::Text("Paper", "纸张"), nullptr,
                            !gui->IsDarkTheme()))
                    {
                        gui->SetLightThemeColors();
                    }
                }
                ImGui::EndMenu();
            }
            drawMenuIcon(m_IconSettings);
            if (ImGui::BeginMenu(EditorLocale::Text("UI Scale", "界面缩放")))
            {
                ImGuiLayer* gui = Application::Get().GetImGuiLayer();
                if (gui)
                {
                    const float scales[] = {
                        0.85f, 0.90f, 1.00f, 1.10f, 1.25f, 1.30f,
                        1.50f, 1.75f, 2.00f
                    };
                    for (float scale : scales)
                    {
                        char label[32];
                        std::snprintf(label, sizeof(label), "%d%%",
                            static_cast<int>(scale * 100.0f + 0.5f));
                        if (ImGui::MenuItem(label, nullptr,
                                std::abs(gui->GetUiScale() - scale) < 0.01f))
                        {
                            gui->SetUiScale(scale);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem(EditorLocale::Text(
                            "Reset to 100%", "恢复 100%")))
                    {
                        gui->SetUiScale(1.0f);
                    }
                    ImGui::TextDisabled("%s", EditorLocale::Text(
                        "Ctrl + mouse wheel over a panel",
                        "在面板上按住 Ctrl 滚轮缩放"));
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Help", "帮助")))
        {
            drawMenuIcon(m_IconInfo);
            if (ImGui::MenuItem(EditorLocale::Text("Editor Help", "编辑器帮助"), "F1"))
                m_HelpPanel->SetOpen(true);
            ImGui::Separator();
            drawMenuIcon(m_IconSettings);
            if (ImGui::MenuItem(EditorLocale::Text("Input Bindings", "输入绑定")))
                m_InputBindingsPanel->SetOpen(true);
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
        ImGui::Text(EditorLocale::Text("Hovered Entity: %s", "悬停实体: %s"), hoveredName.c_str());

        const auto stats = Renderer2D::GetStats();
        ImGui::Text(EditorLocale::Text("Renderer2D Stats:", "Renderer2D 统计:"));
        ImGui::Text(EditorLocale::Text("  Draw Calls: %d", "  绘制调用: %d"), stats.DrawCalls);
        ImGui::Text(EditorLocale::Text("  Quads:      %d", "  四边形:   %d"), stats.QuadCount);
        ImGui::Text(EditorLocale::Text("  Vertices:   %d", "  顶点:     %d"), stats.GetTotalVertexCount());
        ImGui::Text(EditorLocale::Text("  Indices:    %d", "  索引:     %d"), stats.GetTotalIndexCount());

        ImGui::End();
    }

    void EditorLayerBase::UI_StatusBar()
    {
        const float s = Application::Get().GetImGuiLayer()
            ? Application::Get().GetImGuiLayer()->GetUiScale()
            : 1.0f;
        const float statusHeight = kStatusBarHeight * s;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x,
            viewport->WorkPos.y + viewport->WorkSize.y - statusHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
            ImVec2(12.0f * s, 3.0f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
            ImVec2(8.0f * s, 2.0f * s));

        const bool darkTheme = !Application::Get().GetImGuiLayer()
            || Application::Get().GetImGuiLayer()->IsDarkTheme();
        const ImVec4 background = darkTheme
            ? ImVec4(0.045f, 0.050f, 0.064f, 1.0f)
            : ImVec4(0.925f, 0.914f, 0.890f, 1.0f);
        const ImVec4 border = darkTheme
            ? ImVec4(0.16f, 0.18f, 0.22f, 1.0f)
            : ImVec4(0.78f, 0.76f, 0.72f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, background);

        ImGui::Begin("##EditorStatusBar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 windowPos = ImGui::GetWindowPos();
        drawList->AddLine(
            ImVec2(windowPos.x, windowPos.y),
            ImVec2(windowPos.x + ImGui::GetWindowWidth(), windowPos.y),
            ImGui::ColorConvertFloat4ToU32(border), 1.0f);

        ImFont* smallFont = Application::Get().GetImGuiLayer()
            ? Application::Get().GetImGuiLayer()->GetSmallFont()
            : nullptr;
        if (smallFont)
            ImGui::PushFont(smallFont);

        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        const ImVec4 muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

        const auto& specification = Application::Get().GetSpecification();
        const std::string projectName = specification.ProjectRoot.empty()
            ? std::string("No Project")
            : specification.ProjectRoot.filename().string();
        ImGui::TextColored(accent, "%s", projectName.c_str());
        ImGui::SameLine();
        ImGui::TextColored(muted, "|");
        ImGui::SameLine();
        ImGui::TextColored(accent, "%s", GetWorkspaceLabel());

        ImGui::SameLine();
        ImGui::TextColored(muted, "|");
        ImGui::SameLine();

        const bool playing = m_SceneState == SceneState::Play;
        ImGui::TextColored(playing
                ? ImVec4(0.95f, 0.52f, 0.32f, 1.0f)
                : ImVec4(0.42f, 0.78f, 0.58f, 1.0f),
            "%s", playing ? "PLAY" : "EDIT");

        ImGui::SameLine();
        const std::string sceneName = m_EditorScenePath.empty()
            ? EditorLocale::Text("Untitled Scene", "未命名场景")
            : m_EditorScenePath.filename().string();
        ImGui::Text("%s%s", sceneName.c_str(), m_SceneDirty ? " *" : "");

        if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
        {
            if (selected.HasComponent<TagComponent>())
            {
                ImGui::SameLine();
                ImGui::TextColored(muted, "|");
                ImGui::SameLine();
                ImGui::TextDisabled("%s", selected.GetComponent<TagComponent>().Tag.c_str());
            }
        }

        if (m_PlayerBuildRunning)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.40f, 1.0f), "%s",
                EditorLocale::Text("| Packaging...", "| 打包中..."));
        }

        char performance[192];
        char performanceCompact[128];
        const auto stats2D = Renderer2D::GetStats();
        const auto stats3D = Renderer3D::GetStats();
        const float uiScale = Application::Get().GetImGuiLayer()
            ? Application::Get().GetImGuiLayer()->GetUiScale()
            : 1.0f;
        const int uiPercent = static_cast<int>(uiScale * 100.0f + 0.5f);
        std::snprintf(performance, sizeof(performance),
            "%.0f FPS   %.2f ms   2D %u draws / %u quads   3D %u draws / %u meshes   UI %d%%",
            ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate > 0.0f ? 1000.0f / ImGui::GetIO().Framerate : 0.0f,
            stats2D.DrawCalls, stats2D.QuadCount,
            stats3D.DrawCalls, stats3D.MeshCount, uiPercent);
        std::snprintf(performanceCompact, sizeof(performanceCompact),
            "%.0f FPS   2D %u   3D %u   UI %d%%",
            ImGui::GetIO().Framerate,
            stats2D.DrawCalls, stats3D.DrawCalls, uiPercent);

        const char* performanceText =
            ImGui::CalcTextSize(performance).x > ImGui::GetWindowWidth() * 0.52f
                ? performanceCompact
                : performance;
        const float rightWidth = ImGui::CalcTextSize(performanceText).x;
        const float rightX = ImGui::GetWindowWidth() - rightWidth - 12.0f * s;
        if (ImGui::GetCursorPosX() < rightX)
            ImGui::SameLine(rightX);
        const ImVec4 fpsColor = ImGui::GetIO().Framerate >= 55.0f
            ? muted
            : (ImGui::GetIO().Framerate >= 25.0f
                ? ImVec4(0.92f, 0.76f, 0.38f, 1.0f)
                : ImVec4(0.94f, 0.44f, 0.40f, 1.0f));
        ImGui::TextColored(fpsColor, "%s", performanceText);

        if (smallFont)
            ImGui::PopFont();
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(4);
    }

    void EditorLayerBase::UI_PlayerBuildScenePicker()
    {
        if (!m_PackageScenePickerOpen)
            return;

        ImGui::OpenPopup("Choose Startup Scene");
        ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Choose Startup Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("The packaged player boots into this scene. "
                "Defaults to the currently open scene.");
            ImGui::Separator();

            if (EditorContentPickers::DrawAssetField("Startup Scene",
                    m_PackageSceneInput,
                    EditorWidgets::AssetReferenceKind::Scene, 512))
            {
            }

            ImGui::Separator();
            const bool scenePathValid = (m_PackageSceneInput.rfind("assets/scenes/", 0) == 0);
            if (!scenePathValid)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
                    "Scene path must start with assets/scenes/");

            ImGui::Separator();
            ImGui::TextDisabled("Release build (packaging is the final product)");
            ImGui::Separator();
            ImGui::BeginDisabled(!scenePathValid);
            if (ImGui::Button("Package"))
            {
                m_PackageScenePath = m_PackageSceneInput;
                m_PackageScenePickerOpen = false;
                ExecutePlayerPackageBuild();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                m_PackageScenePickerOpen = false;

            ImGui::EndPopup();
        }
    }

    void EditorLayerBase::UI_PlayerBuildStatus()
    {
        PollPlayerPackageBuild();

        if (m_PackageScenePickerOpen)
            UI_PlayerBuildScenePicker();

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


} // namespace Wheatear
