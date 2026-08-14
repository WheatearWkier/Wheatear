#include "wepch.h"
#include "EditorLayerBase.h"

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
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorPlatform.h"
#include "Editor/EditorWidgets.h"
#include "Editor/EditorToolRegistry.h"
#include "Panels/AnimationEditorPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/EditorHelpPanel.h"
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
        style.WindowMinSize.x = 480.0f;
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

        DrawUnsavedChangesModal();

        m_HelpPanel->OnImGuiRender();

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
        auto drawMenuIcon = [&](const Ref<Texture2D>& icon)
        {
            const float iconSize = 16.0f;
            if (icon)
                ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                    ImVec2(iconSize, iconSize));
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

        if (ImGui::BeginMenu(EditorLocale::Text("Scene", "场景")))
        {
            drawMenuIcon(m_IconNewScene);
            if (ImGui::MenuItem(EditorLocale::Text("New", "新建"), "Ctrl+N"))        NewScene();
            drawMenuIcon(m_IconOpenScene);
            if (ImGui::MenuItem(EditorLocale::Text("Open...", "打开..."), "Ctrl+O"))  OpenScene();
            drawMenuIcon(m_IconSaveScene);
            if (ImGui::MenuItem(EditorLocale::Text("Save", "保存"), "Ctrl+S"))        SaveScene();
            drawMenuIcon(m_IconSaveScene);
            if (ImGui::MenuItem(EditorLocale::Text("Save As...", "另存为..."), "Ctrl+Shift+S"))  SaveSceneAs();
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
            drawMenuIcon(m_IconSpriteSheet);
            if (ImGui::MenuItem(EditorLocale::Text("Sprite Sheet Picker", "序列帧选择器")))
                m_SpriteSheetPickerPanel->OpenForEntity(m_SceneHierarchyPanel->GetSelectedEntity());
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
            drawLocalizedFloatingWindowItem("Animation Editor", [&]()
            {
                EditorFloatingWindow::OpenFloating("Animation Editor");
            });
            drawLocalizedFloatingWindowItem("Sprite Sheet Picker", [&]()
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
            drawMenuIcon(m_IconResetLayout);
            if (ImGui::MenuItem(EditorLocale::Text("Reset Window Layout", "重置窗口布局")))
            {
                m_RequestDefaultDockspaceLayout = true;
                m_DefaultDockspaceLayoutBuilt = false;
            }
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
            drawToolsByCategory(EditorToolCategory::Assets);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Build", "构建")))
        {
            drawMenuIcon(m_IconPackage);
            if (ImGui::MenuItem(EditorLocale::Text("Package Player + Editor", "打包玩家 + 编辑器"), nullptr, false, !m_PlayerBuildRunning))
                StartPlayerPackageBuild(false);
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
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("App", "应用")))
        {
            drawMenuIcon(m_IconLanguage);
            if (ImGui::BeginMenu(EditorLocale::Text("Language", "语言")))
            {
                EditorLocale::DrawLanguageMenu();
                ImGui::EndMenu();
            }
            ImGui::Separator();
            drawMenuIcon(m_IconLogout);
            if (ImGui::MenuItem(EditorLocale::Text("Exit", "退出")))
                RequestSceneChange(PendingSceneAction::Exit);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Help", "帮助")))
        {
            drawMenuIcon(m_IconInfo);
            if (ImGui::MenuItem(EditorLocale::Text("Editor Help", "编辑器帮助"), "F1"))
                m_HelpPanel->SetOpen(true);
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
            if (!(m_PackageSceneInput.rfind("assets/scenes/", 0) == 0))
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
                    "Scene path must start with assets/scenes/");

            ImGui::Separator();
            static const char* kConfigurations[] = { "Debug", "Release", "Dist" };
            int configIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(kConfigurations); ++i)
                if (m_PackageConfiguration == kConfigurations[i]) { configIndex = i; break; }
            if (ImGui::Combo("Build Configuration", &configIndex, kConfigurations, IM_ARRAYSIZE(kConfigurations)))
                m_PackageConfiguration = kConfigurations[configIndex];
            ImGui::SameLine();
            EditorWidgets::HelpTooltip("MSBuild configuration used to build the player and editor.");

            ImGui::Separator();
            if (ImGui::Button("Package"))
            {
                m_PackageScenePath = m_PackageSceneInput;
                m_PackageScenePickerOpen = false;
                ExecutePlayerPackageBuild(m_PackageEnableScripts);
            }
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
