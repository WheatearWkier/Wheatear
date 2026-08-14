#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/Layer.h"
#include "Wheatear/Core/Timestep.h"
#include "Wheatear/Renderer/EditorCamera.h"
#include "Wheatear/Scene/Entity.h"
#include "Build/PlayerPackager.h"

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace Wheatear {

    class AnimationEditorPanel;
    class ContentBrowserPanel;
    class EditorHelpPanel;
    class Framebuffer;
    class InputBindingsPanel;
    class KeyPressedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;
    class MouseScrolledEvent;
    class Scene;
    struct SceneTransitionRequest;
    class SceneHierarchyPanel;
    class SpriteSheetPickerPanel;
    class Texture2D;
    struct TransformComponent;
    struct UITextComponent;
    struct UIWidgetComponent;

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
        ~EditorLayerBase() override;

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
        bool ConsumePlayModeRuntimeCommands();

        // Play-mode pause / single-frame stepping (designer iteration aid).
        void TogglePlayPause();
        void StepPlayFrame();

        void NewScene();
        void OpenScene();
        void OpenScene(const std::filesystem::path& path);
        void SaveScene();
        void SaveSceneAs();
        void SerializeScene(Ref<Scene> scene, const std::filesystem::path& path);
        void InstantiatePrefab(const std::filesystem::path& path);
        void InstantiateUITemplate(const std::filesystem::path& path);
        void OnDuplicateEntity();

        // Unsaved-changes protection: any command executed through the editor
        // command history marks the scene dirty; New/Open/Exit ask first.
        void MarkSceneDirty() { m_SceneDirty = true; }

        SceneState GetSceneState() const { return m_SceneState; }
        Ref<Scene> GetActiveScene() const { return m_ActiveScene; }
        const EditorCamera& GetEditorCamera() const { return m_EditorCamera; }
        EditorCamera& GetEditorCamera() { return m_EditorCamera; }

        SceneHierarchyPanel& GetHierarchyPanel();
        ContentBrowserPanel& GetContentBrowserPanel();

    private:
        bool OnKeyPressed(KeyPressedEvent& e);
        bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
        bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);
        bool OnMouseScrolled(MouseScrolledEvent& e);

        void UI_MenuBar();
        void UI_Toolbar();
        void UI_Viewport();
        void UI_CanvasEditor();
        void UI_DrawCanvasSceneReference(const glm::vec2& regionMin,
            const glm::vec2& regionSize);
        void UI_DrawCanvasOverlay(const glm::vec2& regionMin,
            const glm::vec2& regionSize,
            bool surfaceHovered,
            bool drawBackdrop,
            Entity canvasEntity);
        void UI_Stats();
        void UI_PlayerBuildStatus();
        void UI_PlayerBuildScenePicker();
        void BuildDefaultDockspaceLayout(uint32_t dockspaceID);
        void FocusEditorCameraOnPrimarySceneCamera();

        enum class PendingSceneAction { None, New, Open, Exit };
        void RequestSceneChange(PendingSceneAction action);
        void ExecutePendingSceneAction();
        void DrawUnsavedChangesModal();

        void SyncPanels();
        void ClearEntitySelection();
        void CommitPendingGizmoEdit();
        void CommitPendingUIEdit();
        void UpdateUITextFontDuringUIResize(Entity entity);
        void StartPlayerPackageBuild();
        void ExecutePlayerPackageBuild();
        void PollPlayerPackageBuild();
        void ProcessDeferredViewportAssetDrop();
        void SelectEditorEntity(Entity entity, bool preferMoveGizmo);
        Entity PickViewportEditorEntity(const glm::vec2& screenMouse);
        Entity PickSceneSpriteEntityAtViewportPoint(const glm::vec2& screenMouse);
        Entity PickUIEntityAtCanvasPoint(const glm::vec2& regionMin,
            const glm::vec2& regionSize,
            Entity canvasEntity,
            const glm::vec2& screenMouse);
        void ActivateHierarchyEntity(Entity entity);
        void OpenCanvasEditorForEntity(Entity entity);
        void FrameEditorCameraOnEntity(Entity entity);
        void FrameEditorCameraOnScene();
        void LoadPlayScene(const std::filesystem::path& scenePath);
        void ApplyPendingVisualNovelLoad();
        bool ConsumePlayModeSceneTransitionRequests();
        void ExecutePlayModeSceneTransitionRequest(const SceneTransitionRequest& request);

    protected:
        Ref<Framebuffer> m_Framebuffer;
        Ref<Framebuffer> m_UIReferenceFramebuffer;
        EditorCamera m_EditorCamera;

        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_EditorScene;
        std::filesystem::path m_EditorScenePath;
        std::filesystem::path m_PlayScenePath;
        SceneState m_SceneState = SceneState::Edit;
        bool m_PlayPaused = false;
        bool m_StepOnce = false;

        bool m_SceneDirty = false;
        bool m_ShowUnsavedModal = false;
        PendingSceneAction m_PendingSceneAction = PendingSceneAction::None;
        std::filesystem::path m_PendingScenePath;

        Entity m_HoveredEntity;

        glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
        glm::vec2 m_ViewportBounds[2] = {};
        bool m_ViewportFocused = false;
        bool m_ViewportHovered = false;
        bool m_ViewportImageHovered = false;
        bool m_EditorCameraViewportMouseDown = false;
        bool m_PlayModeViewportMouseDown = false;

        int m_GizmoType = -1;
        bool m_GizmoWasUsing = false;
        Entity m_GizmoEditEntity;
        std::unique_ptr<TransformComponent> m_GizmoStartTransform;

        bool m_ShowPhysicsColliders = false;
        bool m_ShowUIOutlines = true;
        bool m_UIEditorOpen = true;
        bool m_FocusCanvasEditor = false;
        bool m_UIEditorMouseOverCanvas = false;
        glm::vec2 m_UIEditorCanvasBounds[2] = {};
        bool m_ShowStats = true;
        bool m_ContentDrawerOpen = false;
        bool m_RequestDefaultDockspaceLayout = false;
        bool m_DefaultDockspaceLayoutBuilt = false;
        Entity m_UIEditingCanvas;
        int m_UIEditHandle = 0;
        Entity m_UIEditEntity;
        glm::vec2 m_UIEditStartMouse = { 0.0f, 0.0f };
        glm::vec4 m_UIEditStartRect = { 0.0f, 0.0f, 0.0f, 0.0f };
        std::unique_ptr<UIWidgetComponent> m_UIEditStartWidget;
        bool m_UIEditStartHadText = false;
        std::unique_ptr<UITextComponent> m_UIEditStartText;

        Ref<Texture2D> m_IconPlay;
        Ref<Texture2D> m_IconStop;
        Ref<Texture2D> m_IconPause;
        Ref<Texture2D> m_IconStep;
        Ref<Texture2D> m_IconNewScene;
        Ref<Texture2D> m_IconOpenScene;
        Ref<Texture2D> m_IconSaveScene;
        Ref<Texture2D> m_IconPackage;
        Ref<Texture2D> m_IconHealth;
        Ref<Texture2D> m_IconUICanvas;
        Ref<Texture2D> m_IconSpriteSheet;
        Ref<Texture2D> m_IconEventGraph;
        Ref<Texture2D> m_IconFocus;
        Ref<Texture2D> m_IconResetLayout;

        // Menu-bar icons (Lucide, see scripts/Generate-EditorIcons.ps1).
        Ref<Texture2D> m_IconUndo;
        Ref<Texture2D> m_IconRedo;
        Ref<Texture2D> m_IconRefresh;
        Ref<Texture2D> m_IconLanguage;
        Ref<Texture2D> m_IconLogout;
        Ref<Texture2D> m_IconFolder;
        Ref<Texture2D> m_IconPencil;
        Ref<Texture2D> m_IconPanel;
        Ref<Texture2D> m_IconGameplay;
        Ref<Texture2D> m_IconBarChart;
        Ref<Texture2D> m_IconSettings;
        Ref<Texture2D> m_IconSearch;
        Ref<Texture2D> m_IconClose;
        Ref<Texture2D> m_IconPlus;
        Ref<Texture2D> m_IconInfo;

        std::unique_ptr<SceneHierarchyPanel> m_SceneHierarchyPanel;
        std::unique_ptr<ContentBrowserPanel> m_ContentBrowserPanel;
        std::unique_ptr<AnimationEditorPanel> m_AnimationEditorPanel;
        std::unique_ptr<SpriteSheetPickerPanel> m_SpriteSheetPickerPanel;
        std::unique_ptr<EditorHelpPanel> m_HelpPanel;
        std::unique_ptr<InputBindingsPanel> m_InputBindingsPanel;
        Timestep m_LastTimestep;

        std::future<PlayerPackageResult> m_PlayerBuildFuture;
        bool m_PlayerBuildRunning = false;
        std::string m_PlayerBuildStatus;
        bool m_PackageScenePickerOpen = false;
        std::string m_PackageScenePath;
        std::string m_PackageSceneInput;
        std::string m_PackageConfiguration = "Debug";
        std::filesystem::path m_LastPlayerBuildDirectory;
        std::filesystem::path m_LastEditorBuildDirectory;
        std::filesystem::path m_DeferredSceneOpenPath;
        std::filesystem::path m_DeferredPrefabInstantiatePath;
        std::filesystem::path m_DeferredUITemplateInstantiatePath;
        std::string m_DeferredSheetCellPath;
        int m_DeferredSheetCellIndex = -1;
        std::string m_DeferredSheetCellSubRect;
        int m_PendingVisualNovelLoadSlot = 0;
    };

} // namespace Wheatear
