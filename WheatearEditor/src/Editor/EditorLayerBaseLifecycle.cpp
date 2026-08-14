#include "wepch.h"
#include "EditorLayerBase.h"
#include "Editor/EditorLocale.h"

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
#include "Wheatear/Modules/GameplayModuleRuntime.h"
#include "Wheatear/Utils/PlatformUtils.h"
#include "Wheatear/UI/UIInputSystem.h"
#include "Wheatear/UI/UIWidgetLayout.h"
#include "Wheatear/Math/Math.h"
#include "Editor/EditorCanvasTools.h"
#include "Panels/AnimationEditorPanel.h"
#include "Editor/EditorCommands.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/SceneHierarchy/SceneHierarchyPanel.h"

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


namespace Wheatear {

    namespace {

        static bool BoundsHaveArea(const glm::vec2 bounds[2])
        {
            return bounds[1].x > bounds[0].x && bounds[1].y > bounds[0].y;
        }

        static bool PointInBounds(const glm::vec2& point, const glm::vec2 bounds[2])
        {
            return point.x >= bounds[0].x && point.x <= bounds[1].x
                && point.y >= bounds[0].y && point.y <= bounds[1].y;
        }

        static bool PointInBoundsInset(const glm::vec2& point, const glm::vec2 bounds[2], float inset)
        {
            return point.x > bounds[0].x + inset && point.x < bounds[1].x - inset
                && point.y > bounds[0].y + inset && point.y < bounds[1].y - inset;
        }

        static glm::uvec2 ResolveCanvasReferenceSize(Entity canvasEntity)
        {
            constexpr uint32_t fallbackWidth = 1920;
            constexpr uint32_t fallbackHeight = 1080;
            constexpr uint32_t maxReferenceSize = 8192;

            if (!canvasEntity || !canvasEntity.HasComponent<UICanvasComponent>())
                return { fallbackWidth, fallbackHeight };

            const auto& canvas = canvasEntity.GetComponent<UICanvasComponent>();
            const uint32_t width = std::clamp(
                static_cast<uint32_t>(std::round(std::max(canvas.ReferenceWidth, 1.0f))),
                1u,
                maxReferenceSize);
            const uint32_t height = std::clamp(
                static_cast<uint32_t>(std::round(std::max(canvas.ReferenceHeight, 1.0f))),
                1u,
                maxReferenceSize);

            return { width, height };
        }

    } // namespace

    // The command history marks the scene dirty through this layer instance.
    static EditorLayerBase* s_ActiveEditorLayer = nullptr;

    void EditorLayerBase::OnAttach()
    {
        WT_PROFILE_FUNCTION();

        // Restore the persisted editor language (falls back to the OS language
        // when the user has never pinned a choice).
        EditorLocale::ApplyFromSettings();

        s_ActiveEditorLayer = this;
        CommandHistory::SetDirtyCallback([]()
        {
            if (s_ActiveEditorLayer)
                s_ActiveEditorLayer->MarkSceneDirty();
        });

        m_IconPlay = Texture2D::Create("Resources/Icons/Editor/play.png");
        m_IconStop = Texture2D::Create("Resources/Icons/Editor/stop.png");
        m_IconPause = Texture2D::Create("Resources/Icons/Editor/pause.png");
        m_IconStep = Texture2D::Create("Resources/Icons/Editor/step.png");
        m_IconNewScene = Texture2D::Create("Resources/Icons/Editor/new_scene.png");
        m_IconOpenScene = Texture2D::Create("Resources/Icons/Editor/open_scene.png");
        m_IconSaveScene = Texture2D::Create("Resources/Icons/Editor/save_scene.png");
        m_IconPackage = Texture2D::Create("Resources/Icons/Editor/package.png");
        m_IconHealth = Texture2D::Create("Resources/Icons/Editor/health.png");
        m_IconUICanvas = Texture2D::Create("Resources/Icons/Editor/ui_canvas.png");
        m_IconSpriteSheet = Texture2D::Create("Resources/Icons/Editor/sprite_sheet.png");
        m_IconEventGraph = Texture2D::Create("Resources/Icons/Editor/event_graph.png");
        m_IconFocus = Texture2D::Create("Resources/Icons/Editor/focus.png");
        m_IconResetLayout = Texture2D::Create("Resources/Icons/Editor/reset_layout.png");

        m_IconUndo = Texture2D::Create("Resources/Icons/Editor/undo.png");
        m_IconRedo = Texture2D::Create("Resources/Icons/Editor/redo.png");
        m_IconRefresh = Texture2D::Create("Resources/Icons/Editor/refresh.png");
        m_IconLanguage = Texture2D::Create("Resources/Icons/Editor/language.png");
        m_IconLogout = Texture2D::Create("Resources/Icons/Editor/logout.png");
        m_IconFolder = Texture2D::Create("Resources/Icons/Editor/folder.png");
        m_IconPencil = Texture2D::Create("Resources/Icons/Editor/pencil.png");
        m_IconPanel = Texture2D::Create("Resources/Icons/Editor/panel.png");
        m_IconGameplay = Texture2D::Create("Resources/Icons/Editor/gameplay.png");
        m_IconBarChart = Texture2D::Create("Resources/Icons/Editor/bar_chart.png");
        m_IconSettings = Texture2D::Create("Resources/Icons/Editor/settings.png");
        m_IconSearch = Texture2D::Create("Resources/Icons/Editor/search.png");
        m_IconClose = Texture2D::Create("Resources/Icons/Editor/close.png");
        m_IconPlus = Texture2D::Create("Resources/Icons/Editor/plus.png");
        m_IconInfo = Texture2D::Create("Resources/Icons/Editor/info.png");

        EditorCanvasTools::Configure({
            [this](Entity canvasEntity)
            {
                m_UIEditingCanvas = canvasEntity;
                m_UIEditorOpen = true;
                m_FocusCanvasEditor = true;
            }
        });

        FramebufferSpecification fbSpec;
        fbSpec.Attachments = {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::RGBA16F,
            FramebufferTextureFormat::RED_INTEGER,
            FramebufferTextureFormat::Depth,
        };
        fbSpec.Width  = 1920;
        fbSpec.Height = 1080;
        m_Framebuffer = Framebuffer::Create(fbSpec);

        FramebufferSpecification uiReferenceSpec;
        uiReferenceSpec.Attachments = {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::RGBA16F,
            FramebufferTextureFormat::RED_INTEGER,
            FramebufferTextureFormat::Depth,
        };
        uiReferenceSpec.Width = 1920;
        uiReferenceSpec.Height = 1080;
        m_UIReferenceFramebuffer = Framebuffer::Create(uiReferenceSpec);

        m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
        Input::SetCursorMode(CursorMode::Normal);

        if (const char* iniPath = ImGui::GetIO().IniFilename)
        {
            std::error_code error;
            const std::filesystem::path path = iniPath;
            m_RequestDefaultDockspaceLayout =
                !std::filesystem::exists(path, error)
                || std::filesystem::file_size(path, error) == 0;
        }
        else
        {
            m_RequestDefaultDockspaceLayout = true;
        }

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
        s_ActiveEditorLayer = nullptr;
        Input::ClearMouseInputBounds();
        if (m_SceneState == SceneState::Play)
            m_ActiveScene->OnRuntimeStop();
        else
            m_ActiveScene->OnEditorStop();
        EditorCanvasTools::Reset();
    }

    void EditorLayerBase::OnUpdate(Timestep ts)
    {
        WT_PROFILE_FUNCTION();

        // Render one queued asset thumbnail per frame (scenes/prefabs/UI
        // templates/meshes) so browsing the content browser never stalls.
        m_ContentBrowserPanel->OnUpdate();

        // Hot-reload WAO action recipes while playing (editor tuning applies
        // without restarting play).
        if (m_SceneState == SceneState::Play)
            UpdateActionHotReload();

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
        RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.10f, 1.0f });
        RenderCommand::Clear();
        m_Framebuffer->ClearAttachment(2, -1);

        m_ActiveScene->SetViewportOffset(m_ViewportBounds[0].x, m_ViewportBounds[0].y);

        OnBeginRender();

        switch (m_SceneState)
        {
        case SceneState::Edit:
        {
            const glm::vec2 mouse = { Input::GetMouseX(), Input::GetMouseY() };
            const bool mouseInViewport = BoundsHaveArea(m_ViewportBounds)
                && PointInBounds(mouse, m_ViewportBounds);
            const bool anyEditorCameraButtonDown =
                Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT)
                || Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_MIDDLE)
                || Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_RIGHT);
            if (!anyEditorCameraButtonDown)
                m_EditorCameraViewportMouseDown = false;

            if ((mouseInViewport && !anyEditorCameraButtonDown)
                || m_EditorCameraViewportMouseDown
                || m_EditorCamera.GetMode() == EditorCamera::Mode::Fly)
            {
                m_EditorCamera.OnUpdate(ts);
            }
            m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);

            if (m_UIEditorOpen && m_UIReferenceFramebuffer)
            {
                Entity cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
                if (cameraEntity
                    && cameraEntity.HasComponent<CameraComponent>()
                    && cameraEntity.HasComponent<TransformComponent>())
                {
                    const glm::uvec2 referenceSize = ResolveCanvasReferenceSize(m_UIEditingCanvas);
                    const auto& uiReferenceSpec = m_UIReferenceFramebuffer->GetSpecification();
                    if (uiReferenceSpec.Width != referenceSize.x || uiReferenceSpec.Height != referenceSize.y)
                        m_UIReferenceFramebuffer->Resize(referenceSize.x, referenceSize.y);

                    const uint32_t previousViewportWidth = m_ActiveScene->GetViewportWidth();
                    const uint32_t previousViewportHeight = m_ActiveScene->GetViewportHeight();
                    if (previousViewportWidth != referenceSize.x || previousViewportHeight != referenceSize.y)
                        m_ActiveScene->OnViewportResize(referenceSize.x, referenceSize.y);

                    m_UIReferenceFramebuffer->Bind();
                    RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.10f, 1.0f });
                    RenderCommand::Clear();
                    m_UIReferenceFramebuffer->ClearAttachment(2, -1);
                    m_ActiveScene->RenderWithSceneCamera(
                        cameraEntity.GetComponent<CameraComponent>().Camera,
                        cameraEntity.GetComponent<TransformComponent>().GetTransform(),
                        true);
                    m_UIReferenceFramebuffer->Unbind();

                    uint32_t restoreWidth = previousViewportWidth;
                    uint32_t restoreHeight = previousViewportHeight;
                    if (restoreWidth == 0 || restoreHeight == 0)
                    {
                        restoreWidth = static_cast<uint32_t>(std::max(m_ViewportSize.x, 0.0f));
                        restoreHeight = static_cast<uint32_t>(std::max(m_ViewportSize.y, 0.0f));
                    }
                    if (restoreWidth > 0 && restoreHeight > 0
                        && (restoreWidth != referenceSize.x || restoreHeight != referenceSize.y))
                    {
                        m_ActiveScene->OnViewportResize(restoreWidth, restoreHeight);
                    }

                    m_Framebuffer->Bind();
                    RenderCommand::SetViewport(0, 0,
                        static_cast<uint32_t>(m_ViewportSize.x),
                        static_cast<uint32_t>(m_ViewportSize.y));
                }
            }
            break;
        }
        case SceneState::Play:
        {
            const bool leftMouseHeld = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
            if (!leftMouseHeld)
                m_PlayModeViewportMouseDown = false;

            const bool editorMouseDragActive = ImGui::IsAnyItemActive() && !m_ViewportImageHovered;
            if (BoundsHaveArea(m_ViewportBounds))
            {
                if ((leftMouseHeld && !m_PlayModeViewportMouseDown) || editorMouseDragActive)
                {
                    Input::SetMouseInputBounds(1.0f, 1.0f, 0.0f, 0.0f);
                }
                else
                {
                    Input::SetMouseInputBounds(
                        m_ViewportBounds[0].x,
                        m_ViewportBounds[0].y,
                        m_ViewportBounds[1].x,
                        m_ViewportBounds[1].y);
                }
            }
            else
            {
                Input::SetMouseInputBounds(1.0f, 1.0f, 0.0f, 0.0f);
            }

            // Pause / single-frame stepping: when paused the runtime is not
            // advanced (scene still renders); StepPlayFrame runs exactly one
            // update then re-pauses.
            if (!m_PlayPaused || m_StepOnce)
            {
                m_ActiveScene->OnUpdateRuntime(ts);
                m_StepOnce = false;
            }
            Input::ClearMouseInputBounds();
            ConsumePlayModeRuntimeCommands();
            break;
        }
        }

        m_Framebuffer->Bind();
        RenderCommand::SetViewport(0, 0,
            static_cast<uint32_t>(m_ViewportSize.x),
            static_cast<uint32_t>(m_ViewportSize.y));

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

        OnPostSceneUpdate();

        OnOverlayRender();

        m_Framebuffer->Unbind();
        m_LastTimestep = ts;
    }

    void EditorLayerBase::OnEvent(Event& event)
    {
        const glm::vec2 mouse = { Input::GetMouseX(), Input::GetMouseY() };
        if (m_SceneState == SceneState::Edit
            && BoundsHaveArea(m_ViewportBounds)
            && PointInBounds(mouse, m_ViewportBounds))
        {
            m_EditorCamera.OnEvent(event);
        }

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnKeyPressed));
        dispatcher.Dispatch<MouseButtonPressedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseButtonPressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseButtonReleased));
        dispatcher.Dispatch<MouseScrolledEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseScrolled));
    }

    // =========================================================================
    // =========================================================================

    bool EditorLayerBase::OnKeyPressed(KeyPressedEvent& e)
    {
        if (e.GetRepeatCount() > 0) return false;

        const bool ctrl  = Input::IsKeyPressed(WT_KEY_LEFT_CONTROL)  || Input::IsKeyPressed(WT_KEY_RIGHT_CONTROL);
        const bool shift = Input::IsKeyPressed(WT_KEY_LEFT_SHIFT)    || Input::IsKeyPressed(WT_KEY_RIGHT_SHIFT);

        // Focus guard: while a text field or any ImGui widget is active the
        // editor hotkeys must not fire (typing "Delete" into a search box must
        // not delete the selected entity). Esc always passes so it can cancel.
        ImGuiIO& io = ImGui::GetIO();
        const bool editingText = io.WantTextInput || ImGui::IsAnyItemActive();
        if (editingText && e.GetKeyCode() != WT_KEY_ESCAPE)
            return false;

        switch (e.GetKeyCode())
        {
        case WT_KEY_ESCAPE:
            // Esc: close the content drawer, then stop play mode, then clear
            // the selection.
            if (m_ContentDrawerOpen)
            {
                m_ContentDrawerOpen = false;
                m_ContentBrowserPanel->SetDrawerMode(false);
            }
            else if (m_SceneState == SceneState::Play)
                TransitionToStop();
            else if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
                m_SceneHierarchyPanel->SetSelectedEntity({});
            break;
        case WT_KEY_N: if (ctrl) NewScene();  break;
        case WT_KEY_O: if (ctrl) OpenScene(); break;
        case WT_KEY_Z:
            if (ctrl && shift) CommandHistory::Get().Redo();
            else if (ctrl) CommandHistory::Get().Undo();
            break;
        case WT_KEY_Y:
            if (ctrl) CommandHistory::Get().Redo();
            break;
        case WT_KEY_S:
            if (ctrl && shift) SaveSceneAs();
            else if (ctrl)     SaveScene();
            break;
        case WT_KEY_ENTER:
        case WT_KEY_KP_ENTER:
            if (ctrl && shift)
            {
                // Restart play: stop and immediately re-enter play with the
                // latest scene state (scene edits apply without an extra click).
                if (m_SceneState == SceneState::Play)
                {
                    TransitionToStop();
                    TransitionToPlay();
                }
            }
            else if (ctrl)
            {
                if (m_SceneState == SceneState::Edit)
                    TransitionToPlay();
                else
                    TransitionToStop();
            }

            break;
        case WT_KEY_F6:
            if (m_SceneState == SceneState::Play)
                TogglePlayPause();
            break;
        case WT_KEY_F7:
            if (m_SceneState == SceneState::Play)
                StepPlayFrame();
            break;

        case WT_KEY_D: if (ctrl) OnDuplicateEntity(); break;
        case WT_KEY_F:
            if (m_SceneState == SceneState::Edit)
            {
                if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
                    ActivateHierarchyEntity(selected);
                else
                    FrameEditorCameraOnScene();
            }
            break;
        case WT_KEY_DELETE:
            if (m_SceneState == SceneState::Edit)
            {
                if (Entity sel = m_SceneHierarchyPanel->GetSelectedEntity())
                {
                    m_SceneHierarchyPanel->SetSelectedEntity({});
                    auto command = std::make_unique<EntityCreateCommand>(m_ActiveScene.get(), sel, false);
                    command->Execute();
                    CommandHistory::Get().Push(std::move(command));
                }
            }
            break;

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
        const glm::vec2 mouse = { Input::GetMouseX(), Input::GetMouseY() };
        const bool mouseInViewport = BoundsHaveArea(m_ViewportBounds)
            && PointInBoundsInset(mouse, m_ViewportBounds, 1.0f);
        const bool mouseInViewportImage = mouseInViewport && m_ViewportImageHovered;
        if (m_SceneState == SceneState::Edit)
            m_EditorCameraViewportMouseDown = mouseInViewport;

        if (e.GetMouseButton() != WT_MOUSE_BUTTON_LEFT) return false;

        if (m_SceneState == SceneState::Play)
        {
            m_PlayModeViewportMouseDown = mouseInViewportImage;
            if (m_PlayModeViewportMouseDown)
                UIInputSystem::OnMousePressed(m_ActiveScene.get());
            else
                UIInputSystem::Reset();
            return false;
        }

        if (m_SceneState == SceneState::Edit
            && mouseInViewport
            && !ImGuizmo::IsOver()
            && !Input::IsKeyPressed(WT_KEY_LEFT_ALT))
        {
            SelectEditorEntity(PickViewportEditorEntity(mouse), true);
        }
        return false;
    }

    bool EditorLayerBase::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
    {
        if (m_SceneState == SceneState::Edit)
            m_EditorCameraViewportMouseDown = false;

        if (e.GetMouseButton() == WT_MOUSE_BUTTON_LEFT && m_SceneState == SceneState::Play)
        {
            const glm::vec2 mouse = { Input::GetMouseX(), Input::GetMouseY() };
            const bool mouseInViewport = BoundsHaveArea(m_ViewportBounds)
                && PointInBoundsInset(mouse, m_ViewportBounds, 1.0f);

            if (m_PlayModeViewportMouseDown && mouseInViewport)
                UIInputSystem::OnMouseReleased(m_ActiveScene.get());
            else
                UIInputSystem::Reset();

            m_PlayModeViewportMouseDown = false;
        }
        return false;
    }

    bool EditorLayerBase::OnMouseScrolled(MouseScrolledEvent& e)
    {
        if (m_SceneState != SceneState::Play || !m_ActiveScene)
            return false;

        const glm::vec2 mouse = { Input::GetMouseX(), Input::GetMouseY() };
        if (!BoundsHaveArea(m_ViewportBounds)
            || !PointInBoundsInset(mouse, m_ViewportBounds, 1.0f)
            || !m_ViewportImageHovered)
            return false;

        return UIInputSystem::OnMouseScrolled(
            m_ActiveScene.get(),
            e.GetYOffset(),
            mouse.x - m_ViewportBounds[0].x,
            mouse.y - m_ViewportBounds[0].y,
            static_cast<uint32_t>(m_ViewportSize.x),
            static_cast<uint32_t>(m_ViewportSize.y));
    }

    // =========================================================================
    // =========================================================================

} // namespace Wheatear
