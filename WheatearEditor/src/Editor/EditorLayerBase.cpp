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
#include "Wheatear/Math/Math.h"

#include <imgui/imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <chrono>


namespace Wheatear {

    // =========================================================================
    // 构�?    // =========================================================================

    EditorLayerBase::EditorLayerBase(const std::string& debugName)
        : Layer(debugName)
    {
    }

    // =========================================================================
    // Layer 生命周期
    // =========================================================================

    void EditorLayerBase::OnAttach()
    {
        WT_PROFILE_FUNCTION();

        m_IconPlay = Texture2D::Create("Resources/Icons/play.png");
        m_IconStop = Texture2D::Create("Resources/Icons/stop.png");

        FramebufferSpecification fbSpec;
        fbSpec.Attachments = {
            FramebufferTextureFormat::RGBA8,        // attachment 0: 颜色（不变）
            FramebufferTextureFormat::RGBA16F,      // attachment 1: 视图空间法线（新增）
            FramebufferTextureFormat::RED_INTEGER,  // attachment 2: entity ID（原 1，现�?2�?            FramebufferTextureFormat::Depth
        };
        fbSpec.Width  = 1920;
        fbSpec.Height = 1080;
        m_Framebuffer = Framebuffer::Create(fbSpec);

        m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
        Input::SetCursorMode(CursorMode::Normal);
        // Load the initial scene from command line, or create an empty scene.
        Ref<Scene> startScene = CreateRef<Scene>();
        std::filesystem::path startPath;

        auto args = Application::Get().GetCommandLineArgs();
        if (args.Count > 1)
        {
            startPath = AssetPath::Resolve(args[1]);
            SceneSerializer serializer(startScene);
            if (!serializer.DeserializeYaml(startPath))
            {
                WT_CORE_WARN("Failed to load command-line scene: {}", startPath.string());
                startScene = CreateRef<Scene>();
                startPath.clear();
            }
        }

        TransitionToEditScene(startScene, startPath);
    }

    void EditorLayerBase::OnDetach()
    {
        WT_PROFILE_FUNCTION();
        if (m_SceneState == SceneState::Play)
            m_ActiveScene->OnRuntimeStop();
        else
            m_ActiveScene->OnEditorStop();
    }

    void EditorLayerBase::OnUpdate(Timestep ts)
    {
        WT_PROFILE_FUNCTION();

        // ── Framebuffer resize ───────────────────────────────────────────────
        {
            const auto& spec = m_Framebuffer->GetSpecification();
            const auto  w = static_cast<uint32_t>(m_ViewportSize.x);
            const auto  h = static_cast<uint32_t>(m_ViewportSize.y);
            if (w > 0 && h > 0 && (spec.Width != w || spec.Height != h))
            {
                m_Framebuffer->Resize(w, h);
                m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
                m_ActiveScene->OnViewportResize(w, h);
            }
        }

        Renderer2D::ResetStats();

        m_Framebuffer->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
        RenderCommand::Clear();
        m_Framebuffer->ClearAttachment(2, -1);

        m_ActiveScene->SetViewportOffset(m_ViewportBounds[0].x, m_ViewportBounds[0].y);

        // ── 子类渲染前置（BeginScene/Skybox等）──────────────────────────────
        OnBeginRender();

        // ── 场景更新 ─────────────────────────────────────────────────────────
        switch (m_SceneState)
        {
        case SceneState::Edit:
            m_EditorCamera.OnUpdate(ts);
            m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);
            break;
        case SceneState::Play:
            m_ActiveScene->OnUpdateRuntime(ts);
            break;
        }

        // ── Framebuffer rebind（Scene Update可能解绑了FB）────────────────────
        m_Framebuffer->Bind();
        RenderCommand::SetViewport(0, 0,
            static_cast<uint32_t>(m_ViewportSize.x),
            static_cast<uint32_t>(m_ViewportSize.y));

        // ── Entity picking ────────────────────────────────────────────────────
        {
            auto [mx, my] = ImGui::GetMousePos();
            mx -= m_ViewportBounds[0].x;
            my -= m_ViewportBounds[0].y;
            const glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
            my = viewportSize.y - my;
            const int mouseX = static_cast<int>(mx);
            const int mouseY = static_cast<int>(my);
            const auto& fbSpec = m_Framebuffer->GetSpecification();
            if (mouseX >= 0 && mouseX < static_cast<int>(fbSpec.Width) &&
                mouseY >= 0 && mouseY < static_cast<int>(fbSpec.Height))
            {
                int pixelData = m_Framebuffer->ReadPixel(2, mouseX, mouseY);
                m_HoveredEntity = (pixelData == -1)
                    ? Entity()
                    : Entity(static_cast<entt::entity>(pixelData), m_ActiveScene.get());
            }
        }

        // 可能在这里放在顺序上会有点问题？
        OnPostSceneUpdate();

        // ── 子类叠加渲染（Collider可视�?光源Gizmo/Grid等）─────────────────
        OnOverlayRender();

        m_Framebuffer->Unbind();
        m_LastTimestep = ts;
    }

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
            ImGui::SetNextWindowPos(viewport->GetWorkPos());
            ImGui::SetNextWindowSize(viewport->GetWorkSize());
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
            ImGui::DockSpace(id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        style.WindowMinSize.x = prevMinX;

        UI_MenuBar();

        m_SceneHierarchyPanel.OnImGuiRender();
        m_ContentBrowserPanel.OnImGuiRender();
        m_AnimationEditorPanel.SetEntity(m_SceneHierarchyPanel.GetSelectedEntity());
        m_AnimationEditorPanel.OnImGuiRender(m_LastTimestep);

        UI_Stats();
        UI_PlayerBuildStatus();

        // ── 子类专属面板（Settings/IBL等）──────────────────────────────────
        OnImGuiExtra();

        UI_Viewport();
        UI_Toolbar();

        ImGui::End();
    }

    void EditorLayerBase::OnEvent(Event& event)
    {
        if (m_ViewportHovered)
            m_EditorCamera.OnEvent(event);

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnKeyPressed));
        dispatcher.Dispatch<MouseButtonPressedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseButtonPressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseButtonReleased));
    }

    // =========================================================================
    // 事件处理
    // =========================================================================

    bool EditorLayerBase::OnKeyPressed(KeyPressedEvent& e)
    {
        if (e.GetRepeatCount() > 0) return false;

        const bool ctrl  = Input::IsKeyPressed(WT_KEY_LEFT_CONTROL)  || Input::IsKeyPressed(WT_KEY_RIGHT_CONTROL);
        const bool shift = Input::IsKeyPressed(WT_KEY_LEFT_SHIFT)    || Input::IsKeyPressed(WT_KEY_RIGHT_SHIFT);

        switch (e.GetKeyCode())
        {
        // 文件操作
        case WT_KEY_N: if (ctrl) NewScene();  break;
        case WT_KEY_O: if (ctrl) OpenScene(); break;
        case WT_KEY_S:
            if (ctrl && shift) SaveSceneAs();
            else if (ctrl)     SaveScene();
            break;

        // 实体操作
        case WT_KEY_D: if (ctrl) OnDuplicateEntity(); break;
        case WT_KEY_DELETE:
            if (m_SceneState == SceneState::Edit)
            {
                if (Entity sel = m_SceneHierarchyPanel.GetSelectedEntity())
                {
                    m_SceneHierarchyPanel.SetSelectedEntity({});
                    m_ActiveScene->DestroyEntity(sel);
                }
            }
            break;

        // Gizmo（飞行模式下不响应）
        case WT_KEY_Q: if (m_EditorCamera.GetMode() != EditorCamera::Mode::Fly) m_GizmoType = -1;                          break;
        case WT_KEY_W: if (m_EditorCamera.GetMode() != EditorCamera::Mode::Fly) m_GizmoType = ImGuizmo::OPERATION::TRANSLATE; break;
        case WT_KEY_E: if (m_EditorCamera.GetMode() != EditorCamera::Mode::Fly) m_GizmoType = ImGuizmo::OPERATION::ROTATE;    break;
        case WT_KEY_R: if (m_EditorCamera.GetMode() != EditorCamera::Mode::Fly) m_GizmoType = ImGuizmo::OPERATION::SCALE;     break;

        default: break;
        }
        return false;
    }

    bool EditorLayerBase::OnMouseButtonPressed(MouseButtonPressedEvent& e)
    {
        if (e.GetMouseButton() != WT_MOUSE_BUTTON_LEFT) return false;

        if (m_SceneState == SceneState::Play)
            UIInputSystem::OnMousePressed(m_ActiveScene.get());

        if (m_ViewportHovered && !ImGuizmo::IsOver() && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
            m_AnimationEditorPanel.SetEntity(m_HoveredEntity);
        }
        return false;
    }

    bool EditorLayerBase::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
    {
        if (e.GetMouseButton() == WT_MOUSE_BUTTON_LEFT && m_SceneState == SceneState::Play)
            UIInputSystem::OnMouseReleased(m_ActiveScene.get());
        return false;
    }

    // =========================================================================
    // 场景状态机
    // =========================================================================

    void EditorLayerBase::TransitionToEditScene(Ref<Scene> newScene,
                                                const std::filesystem::path& scenePath)
    {
        if (m_SceneState == SceneState::Play)
        {
            m_ActiveScene->OnRuntimeStop();
            Renderer2D::EndScene();
        }

        ClearEntitySelection();

        m_SceneState      = SceneState::Edit;
        m_EditorScene     = newScene;
        m_ActiveScene     = newScene;
        m_EditorScenePath = scenePath;

        if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
        {
            m_ActiveScene->OnViewportResize(
                static_cast<uint32_t>(m_ViewportSize.x),
                static_cast<uint32_t>(m_ViewportSize.y));
        }

        m_ActiveScene->OnEditorStart();
        SyncPanels();
    }

    void EditorLayerBase::TransitionToPlay()
    {
        WT_CORE_ASSERT(m_SceneState == SceneState::Edit, "TransitionToPlay called from non-Edit state");

        ClearEntitySelection();
        m_SceneState  = SceneState::Play;
        m_ActiveScene = Scene::Copy(m_EditorScene);
        m_EditorScene->OnEditorStop();
        m_ActiveScene->OnRuntimeStart();

        SyncPanels();
    }

    void EditorLayerBase::TransitionToStop()
    {
        WT_CORE_ASSERT(m_SceneState == SceneState::Play, "TransitionToStop called from non-Play state");

        m_ActiveScene->OnRuntimeStop();
        Renderer2D::EndScene();
        ClearEntitySelection();

        m_SceneState  = SceneState::Edit;
        m_ActiveScene = m_EditorScene;
        m_ActiveScene->OnEditorStart();

        SyncPanels();
    }

    void EditorLayerBase::SyncPanels()
    {
        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_AnimationEditorPanel.SetScene(m_ActiveScene);
    }

    void EditorLayerBase::ClearEntitySelection()
    {
        m_HoveredEntity = {};
        m_SceneHierarchyPanel.SetSelectedEntity({});
    }

    // =========================================================================
    // 场景文件操作
    // =========================================================================

    void EditorLayerBase::NewScene()
    {
        TransitionToEditScene(CreateRef<Scene>());
    }

    void EditorLayerBase::OpenScene()
    {
        const std::string filepath = FileDialogs::OpenFile(AssetFileType::SceneDialogFilter);
        if (!filepath.empty())
            OpenScene(filepath);
    }

    void EditorLayerBase::OpenScene(const std::filesystem::path& path)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(path);
        if (resolvedPath.extension() != AssetFileType::SceneExtension)
        {
            WT_CORE_WARN("Not a scene file: {}", resolvedPath.filename().string());
            return;
        }

        Ref<Scene> newScene = CreateRef<Scene>();
        SceneSerializer serializer(newScene);
        if (serializer.DeserializeYaml(resolvedPath))
            TransitionToEditScene(newScene, resolvedPath);
        else
            WT_CORE_ERROR("Failed to load scene: {}", resolvedPath.string());
    }

    void EditorLayerBase::SaveScene()
    {
        if (!m_EditorScenePath.empty())
            SerializeScene(m_ActiveScene, m_EditorScenePath);
        else
            SaveSceneAs();
    }

    void EditorLayerBase::SaveSceneAs()
    {
        const std::string filepath = FileDialogs::SaveFile(AssetFileType::SceneDialogFilter);
        if (!filepath.empty())
        {
            SerializeScene(m_ActiveScene, filepath);
            m_EditorScenePath = filepath;
        }
    }

    void EditorLayerBase::SerializeScene(Ref<Scene> scene, const std::filesystem::path& path)
    {
        SceneSerializer serializer(scene);
        serializer.SerializeYaml(path);
    }

    void EditorLayerBase::InstantiatePrefab(const std::filesystem::path& path)
    {
        if (m_SceneState != SceneState::Edit) return;

        Entity e = SceneSerializer::DeserializePrefab(path, m_EditorScene.get());
        if (!e) return;

        const std::string baseName  = e.GetName() + "Prefab";
        std::string       finalName = baseName;
        int               index     = 1;

        while (true)
        {
            bool exists = false;
            m_EditorScene->GetRegistry().each([&](auto id)
            {
                Entity other = { id, m_EditorScene.get() };
                if (other && other != e && other.GetName() == finalName)
                    exists = true;
            });
            if (!exists) break;
            finalName = baseName + std::to_string(index++);
        }

        if (Entity found = m_EditorScene->GetEntityByName(finalName))
            found.GetComponent<TagComponent>().Tag = finalName;

        m_SceneHierarchyPanel.SetSelectedEntity(e);
    }

    void EditorLayerBase::OnDuplicateEntity()
    {
        if (m_SceneState != SceneState::Edit) return;
        if (Entity selected = m_SceneHierarchyPanel.GetSelectedEntity())
            m_EditorScene->DuplicateEntity(selected);
    }

    // =========================================================================
    // 公共 UI
    // =========================================================================

    void EditorLayerBase::StartPlayerPackageBuild(bool enableScripts)
    {
        if (m_PlayerBuildRunning)
            return;

        if (m_SceneState == SceneState::Play)
            TransitionToStop();

        if (m_EditorScenePath.empty())
        {
            SaveSceneAs();
            if (m_EditorScenePath.empty())
            {
                m_PlayerBuildStatus = "Package canceled: save the scene first.";
                return;
            }
        }
        else
        {
            SaveScene();
        }

        PlayerPackageOptions options;
        options.StartupScene = m_EditorScenePath;
        options.Configuration = "Debug";
        options.EnableScripts = enableScripts;
        options.IncludeDebugSymbols = false;

        m_PlayerBuildStatus = enableScripts
            ? "Packaging player with C# scripts..."
            : "Packaging player without C# scripts...";
        m_PlayerBuildRunning = true;
        m_PlayerBuildFuture = std::async(std::launch::async, [options]() mutable
        {
            try
            {
                return PlayerPackager::PackagePlayer(options);
            }
            catch (const std::exception& e)
            {
                PlayerPackageResult result;
                result.Success = false;
                result.Message = std::string("Package failed: ") + e.what();
                return result;
            }
            catch (...)
            {
                PlayerPackageResult result;
                result.Success = false;
                result.Message = "Package failed with an unknown error.";
                return result;
            }
        });
    }

    void EditorLayerBase::PollPlayerPackageBuild()
    {
        if (!m_PlayerBuildRunning || !m_PlayerBuildFuture.valid())
            return;

        if (m_PlayerBuildFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
            return;

        PlayerPackageResult result = m_PlayerBuildFuture.get();
        m_PlayerBuildRunning = false;
        m_PlayerBuildStatus = result.Message;
        if (result.Success)
            m_LastPlayerBuildDirectory = result.PackageDirectory;
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
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const ImVec2 minBound   = { windowPos.x + viewportOffset.x, windowPos.y + viewportOffset.y };
        m_ViewportBounds[0] = { minBound.x, minBound.y };
        m_ViewportBounds[1] = { minBound.x + windowSize.x, minBound.y + windowSize.y };

        // 拖放处理
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

        // Screen-space UI edit handle. UI entities keep TransformComponent for ECS consistency,
        // but layout is driven by UIWidgetComponent instead of the world gizmo.
        Entity selected = m_SceneHierarchyPanel.GetSelectedEntity();
        const bool selectedIsUI = selected && selected.HasComponent<UIWidgetComponent>();
        if (selectedIsUI && m_SceneState == SceneState::Edit)
        {
            auto& widget = selected.GetComponent<UIWidgetComponent>();
            const glm::vec2 viewportPixelSize = m_ViewportSize;
            if (viewportPixelSize.x > 0.0f && viewportPixelSize.y > 0.0f)
            {
                float halfW = widget.Size.x * viewportPixelSize.x * 0.5f;
                float halfH = widget.Size.y * viewportPixelSize.y * 0.5f;
                float centerX = widget.Position.x * viewportPixelSize.x;
                float centerY = widget.Position.y * viewportPixelSize.y;

                switch (widget.Anchor)
                {
                case UIAnchor::TopLeft:
                    centerX += halfW;
                    centerY += halfH;
                    break;
                case UIAnchor::TopCenter:
                    centerY += halfH;
                    break;
                case UIAnchor::TopRight:
                    centerX -= halfW;
                    centerY += halfH;
                    break;
                case UIAnchor::MiddleLeft:
                    centerX += halfW;
                    break;
                case UIAnchor::MiddleCenter:
                    break;
                case UIAnchor::MiddleRight:
                    centerX -= halfW;
                    break;
                case UIAnchor::BottomLeft:
                    centerX += halfW;
                    centerY -= halfH;
                    break;
                case UIAnchor::BottomCenter:
                    centerY -= halfH;
                    break;
                case UIAnchor::BottomRight:
                    centerX -= halfW;
                    centerY -= halfH;
                    break;
                }

                const ImVec2 rectMin = { m_ViewportBounds[0].x + centerX - halfW, m_ViewportBounds[0].y + centerY - halfH };
                const ImVec2 rectMax = { m_ViewportBounds[0].x + centerX + halfW, m_ViewportBounds[0].y + centerY + halfH };
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRect(rectMin, rectMax, IM_COL32(82, 210, 224, 230), 0.0f, 0, 2.0f);

                const bool hoverUIRect = ImGui::IsMouseHoveringRect(rectMin, rectMax, false);
                if (hoverUIRect && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
                {
                    const ImVec2 delta = ImGui::GetIO().MouseDelta;
                    widget.Position.x = glm::clamp(widget.Position.x + delta.x / viewportPixelSize.x, 0.0f, 1.0f);
                    widget.Position.y = glm::clamp(widget.Position.y + delta.y / viewportPixelSize.y, 0.0f, 1.0f);
                }
            }
        }

        // Gizmo
        if (selected && !selectedIsUI && m_GizmoType != -1 && m_SceneState == SceneState::Edit)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(windowPos.x, windowPos.y,
                static_cast<float>(ImGui::GetWindowWidth()),
                static_cast<float>(ImGui::GetWindowHeight()));

            const glm::mat4 cameraProj = m_EditorCamera.GetProjection();
            const glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

            auto& tc        = selected.GetComponent<TransformComponent>();
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

            if (ImGuizmo::IsUsing())
            {
                glm::vec3 t, r, s;
                Math::DecomposeTransform(transform, t, r, s);
                tc.Translation  = t;
                tc.Rotation    += r - tc.Rotation;
                tc.Scale        = s;
            }
        }

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
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), 0))
        {
            if (m_SceneState == SceneState::Edit) TransitionToPlay();
            else                                  TransitionToStop();
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

} // namespace Wheatear
