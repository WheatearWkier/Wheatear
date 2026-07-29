#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/Layer.h"
#include "Wheatear/Core/Timestep.h"
#include "Wheatear/Renderer/EditorCamera.h"
#include "Wheatear/Scene/Components.h"
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

    enum UIEditHandle : int
    {
        UIEdit_None = 0,
        UIEdit_Move,
        UIEdit_Left,
        UIEdit_Right,
        UIEdit_Top,
        UIEdit_Bottom,
        UIEdit_TopLeft,
        UIEdit_TopRight,
        UIEdit_BottomLeft,
        UIEdit_BottomRight
    };

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
        bool OnMouseScrolled(MouseScrolledEvent& e);

        void UI_MenuBar();
        void UI_Toolbar();
        void UI_Viewport();
        void UI_CanvasEditor();
        void UI_DrawViewportUIOverlay();
        void UI_DrawCanvasOverlay(const glm::vec2& regionMin,
            const glm::vec2& regionSize,
            bool surfaceHovered,
            bool drawBackdrop,
            Entity canvasEntity);
        void UI_Stats();
        void UI_PlayerBuildStatus();
        void BuildDefaultDockspaceLayout(uint32_t dockspaceID);
        void FocusEditorCameraOnPrimarySceneCamera();

        void SyncPanels();
        void ClearEntitySelection();
        void CommitPendingGizmoEdit();
        void CommitPendingUIEdit();
        void UpdateUITextFontDuringUIResize(Entity entity);
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
        bool m_GizmoWasUsing = false;
        Entity m_GizmoEditEntity;
        TransformComponent m_GizmoStartTransform;

        bool m_ShowPhysicsColliders = false;
        bool m_ShowUIOutlines = true;
        bool m_HideUIInSceneViewport = false;
        bool m_UIEditorOpen = false;
        bool m_RequestDefaultDockspaceLayout = false;
        bool m_DefaultDockspaceLayoutBuilt = false;
        Entity m_UIEditingCanvas;
        int m_UIEditHandle = 0;
        int m_UIEditSurface = 0;
        Entity m_UIEditEntity;
        glm::vec2 m_UIEditStartMouse = { 0.0f, 0.0f };
        glm::vec4 m_UIEditStartRect = { 0.0f, 0.0f, 0.0f, 0.0f };
        UIWidgetComponent m_UIEditStartWidget;
        bool m_UIEditStartHadText = false;
        UITextComponent m_UIEditStartText;

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
