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
#include "Modules/SideCombat/SideCombatTuningEditorPanel.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Panels/EditorCommands.h"

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

    void EditorLayerBase::OnAttach()
    {
        WT_PROFILE_FUNCTION();

        m_IconPlay = Texture2D::Create("Resources/Icons/play.png");
        m_IconStop = Texture2D::Create("Resources/Icons/stop.png");

        EditorCanvasTools::Configure({
            [this](Entity canvasEntity)
            {
                m_UIEditingCanvas = canvasEntity;
                m_UIEditorOpen = true;
            },
            [this]() { return m_HideUIInSceneViewport; },
            [this](bool hidden) { m_HideUIInSceneViewport = hidden; }
        });

        FramebufferSpecification fbSpec;
        fbSpec.Attachments = {
            FramebufferTextureFormat::RGBA8,        // attachment 0: 棰滆壊锛堜笉鍙橈級
            FramebufferTextureFormat::RGBA16F,      // attachment 1: 瑙嗗浘绌洪棿娉曠嚎锛堟柊澧烇級
            FramebufferTextureFormat::RED_INTEGER,  // attachment 2: entity ID锛堝師 1锛岀幇鍦?2锛?            FramebufferTextureFormat::Depth
        };
        fbSpec.Width  = 1920;
        fbSpec.Height = 1080;
        m_Framebuffer = Framebuffer::Create(fbSpec);

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
        if (m_SceneState == SceneState::Play)
            m_ActiveScene->OnRuntimeStop();
        else
            m_ActiveScene->OnEditorStop();
        EditorCanvasTools::Reset();
    }

    void EditorLayerBase::OnUpdate(Timestep ts)
    {
        WT_PROFILE_FUNCTION();

        // 鈹€鈹€ Framebuffer resize 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
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

        // 鈹€鈹€ 瀛愮被娓叉煋鍓嶇疆锛圔eginScene/Skybox绛夛級鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        OnBeginRender();

        // 鈹€鈹€ 鍦烘櫙鏇存柊 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        switch (m_SceneState)
        {
        case SceneState::Edit:
        {
            m_EditorCamera.OnUpdate(ts);

            std::vector<std::pair<entt::entity, bool>> hiddenUIWidgets;
            if (m_HideUIInSceneViewport)
            {
                auto& registry = m_ActiveScene->GetRegistry();
                for (auto entity : registry.view<UIWidgetComponent>())
                {
                    auto& widget = registry.get<UIWidgetComponent>(entity);
                    hiddenUIWidgets.emplace_back(entity, widget.Visible);
                    widget.Visible = false;
                }
            }

            m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);

            if (!hiddenUIWidgets.empty())
            {
                auto& registry = m_ActiveScene->GetRegistry();
                for (const auto& [entity, wasVisible] : hiddenUIWidgets)
                {
                    if (registry.valid(entity) && registry.all_of<UIWidgetComponent>(entity))
                        registry.get<UIWidgetComponent>(entity).Visible = wasVisible;
                }
            }
            break;
        }
        case SceneState::Play:
            m_ActiveScene->OnUpdateRuntime(ts);
            break;
        }

        // 鈹€鈹€ Framebuffer rebind锛圫cene Update鍙兘瑙ｇ粦浜咶B锛夆攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        m_Framebuffer->Bind();
        RenderCommand::SetViewport(0, 0,
            static_cast<uint32_t>(m_ViewportSize.x),
            static_cast<uint32_t>(m_ViewportSize.y));

        // 鈹€鈹€ Entity picking 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
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

        // 鍙兘鍦ㄨ繖閲屾斁鍦ㄩ『搴忎笂浼氭湁鐐归棶棰橈紵
        OnPostSceneUpdate();

        // 鈹€鈹€ 瀛愮被鍙犲姞娓叉煋锛圕ollider鍙鍖?鍏夋簮Gizmo/Grid绛夛級鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        OnOverlayRender();

        m_Framebuffer->Unbind();
        m_LastTimestep = ts;
    }

    void EditorLayerBase::OnEvent(Event& event)
    {
        if (m_ViewportHovered)
            m_EditorCamera.OnEvent(event);

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnKeyPressed));
        dispatcher.Dispatch<MouseButtonPressedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseButtonPressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseButtonReleased));
        dispatcher.Dispatch<MouseScrolledEvent>(WT_BIND_EVENT_FN(EditorLayerBase::OnMouseScrolled));
    }

    // =========================================================================
    // 浜嬩欢澶勭悊
    // =========================================================================

    bool EditorLayerBase::OnKeyPressed(KeyPressedEvent& e)
    {
        if (e.GetRepeatCount() > 0) return false;

        const bool ctrl  = Input::IsKeyPressed(WT_KEY_LEFT_CONTROL)  || Input::IsKeyPressed(WT_KEY_RIGHT_CONTROL);
        const bool shift = Input::IsKeyPressed(WT_KEY_LEFT_SHIFT)    || Input::IsKeyPressed(WT_KEY_RIGHT_SHIFT);

        switch (e.GetKeyCode())
        {
        // 鏂囦欢鎿嶄綔
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

        // 瀹炰綋鎿嶄綔
        case WT_KEY_D: if (ctrl) OnDuplicateEntity(); break;
        case WT_KEY_DELETE:
            if (m_SceneState == SceneState::Edit)
            {
                if (Entity sel = m_SceneHierarchyPanel.GetSelectedEntity())
                {
                    m_SceneHierarchyPanel.SetSelectedEntity({});
                    auto command = std::make_unique<EntityCreateCommand>(m_ActiveScene.get(), sel, false);
                    command->Execute();
                    CommandHistory::Get().Push(std::move(command));
                }
            }
            break;

        // Gizmo锛堥琛屾ā寮忎笅涓嶅搷搴旓級
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

    bool EditorLayerBase::OnMouseScrolled(MouseScrolledEvent& e)
    {
        if (m_SceneState != SceneState::Play || !m_ActiveScene || !m_ViewportHovered)
            return false;

        return UIInputSystem::OnMouseScrolled(
            m_ActiveScene.get(),
            e.GetYOffset(),
            Input::GetMouseX() - m_ViewportBounds[0].x,
            Input::GetMouseY() - m_ViewportBounds[0].y,
            static_cast<uint32_t>(m_ViewportSize.x),
            static_cast<uint32_t>(m_ViewportSize.y));
    }

    // =========================================================================
    // 鍦烘櫙鐘舵€佹満
    // =========================================================================

} // namespace Wheatear
