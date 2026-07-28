#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/Layer.h"
#include "Wheatear/Core/Timestep.h"
#include "Wheatear/Renderer/EditorCamera.h"
#include "Wheatear/Scene/Entity.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/AnimationEditorPanel.h"
#include "Build/PlayerPackager.h"

#include <cstdint>
#include <filesystem>
#include <future>
#include <string>

#include <glm/glm.hpp>

namespace Wheatear {

    class Framebuffer;
    class KeyPressedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;
    class Scene;
    class Texture2D;

    class EditorLayerBase : public Layer
    {
    public:
        enum class SceneState : uint8_t
        {
            Edit = 0,
            Play = 1
        };

    public:
        explicit EditorLayerBase(const std::string& debugName);
        virtual ~EditorLayerBase() = default;

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(Timestep ts) override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

    protected:
        virtual void OnBeginRender() {}
        virtual void OnOverlayRender() {}
        virtual void OnPostSceneUpdate() {}
        virtual void OnImGuiExtra() {}

        void TransitionToEditScene(Ref<Scene> newScene,
            const std::filesystem::path& scenePath = {});
        void TransitionToPlay();
        void TransitionToStop();

        void NewScene();
        void OpenScene();
        void OpenScene(const std::filesystem::path& path);
        void SaveScene();
        void SaveSceneAs();
        void SerializeScene(Ref<Scene> scene, const std::filesystem::path& path);
        void InstantiatePrefab(const std::filesystem::path& path);
        void OnDuplicateEntity();

        SceneState GetSceneState() const { return m_SceneState; }
        Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
        const EditorCamera& GetEditorCamera() const { return m_EditorCamera; }
        EditorCamera& GetEditorCamera() { return m_EditorCamera; }
        const glm::vec2& GetViewportSize() const { return m_ViewportSize; }
        bool IsViewportHovered() const { return m_ViewportHovered; }

        SceneHierarchyPanel& GetHierarchyPanel() { return m_SceneHierarchyPanel; }
        ContentBrowserPanel& GetContentBrowserPanel() { return m_ContentBrowserPanel; }

    private:
        bool OnKeyPressed(KeyPressedEvent& e);
        bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
        bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);

        void UI_MenuBar();
        void UI_Toolbar();
        void UI_Viewport();
        void UI_Stats();
        void UI_PlayerBuildStatus();

        void SyncPanels();
        void ClearEntitySelection();
        void StartPlayerPackageBuild(bool enableScripts = false);
        void PollPlayerPackageBuild();

    protected:
        Ref<Framebuffer> m_Framebuffer;
        EditorCamera m_EditorCamera;

        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_EditorScene;
        std::filesystem::path m_EditorScenePath;
        SceneState m_SceneState = SceneState::Edit;

        Entity m_HoveredEntity;

        glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
        glm::vec2 m_ViewportBounds[2] = {};
        bool m_ViewportFocused = false;
        bool m_ViewportHovered = false;

        int m_GizmoType = -1;
        bool m_ShowPhysicsColliders = false;

        Ref<Texture2D> m_IconPlay;
        Ref<Texture2D> m_IconStop;

        SceneHierarchyPanel m_SceneHierarchyPanel;
        ContentBrowserPanel m_ContentBrowserPanel;
        AnimationEditorPanel m_AnimationEditorPanel;
        Timestep m_LastTimestep;

        std::future<PlayerPackageResult> m_PlayerBuildFuture;
        bool m_PlayerBuildRunning = false;
        std::string m_PlayerBuildStatus;
        std::filesystem::path m_LastPlayerBuildDirectory;
    };

} // namespace Wheatear
