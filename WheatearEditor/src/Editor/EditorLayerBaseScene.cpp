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
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/ImGui/ImGuiLayer.h"
#include "Wheatear/Modules/GameplayModuleRuntime.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Renderer/Framebuffer.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Runtime/SceneTransitionService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneSerializer.h"
#include "Wheatear/Utils/PlatformUtils.h"
#include "Wheatear/UI/UIInputSystem.h"
#include "Wheatear/UI/UIWidgetLayout.h"
#include "Wheatear/Math/Math.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/EditorCanvasTools.h"
#include "Panels/AnimationEditorPanel.h"
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

        static bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

    } // namespace

    void EditorLayerBase::TransitionToEditScene(Ref<Scene> newScene,
                                                const std::filesystem::path& scenePath)
    {
        if (m_SceneState == SceneState::Play)
        {
            m_ActiveScene->OnRuntimeStop();
            Renderer2D::EndScene();
        }

        ClearEntitySelection();
        Input::ClearMouseInputBounds();
        m_PlayModeViewportMouseDown = false;

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
        CommandHistory::Get().Clear();

        FrameEditorCameraOnScene();
    }

    void EditorLayerBase::TransitionToPlay()
    {
        WT_CORE_ASSERT(m_SceneState == SceneState::Edit, "TransitionToPlay called from non-Edit state");

        ClearEntitySelection();
        m_PendingVisualNovelLoadSlot = 0;
        WAO::ActionDebugHistory::Clear();
        UIInputSystem::Reset();
        Input::ClearMouseInputBounds();
        m_PlayModeViewportMouseDown = false;
        CommandBus::ClearQueuedCommands();
        SceneTransitionService::DrainRequests();
        m_SceneState = SceneState::Play;
        const std::filesystem::path previousPlayScenePath = m_PlayScenePath;
        m_ActiveScene = Scene::Copy(m_EditorScene);
        m_PlayScenePath = m_EditorScenePath;
        GameProgress::SetSceneTransitionContext(previousPlayScenePath, m_PlayScenePath);
        m_EditorScene->OnEditorStop();
        m_ActiveScene->OnRuntimeStart();

        SyncPanels();
    }

    void EditorLayerBase::TransitionToStop()
    {
        WT_CORE_ASSERT(m_SceneState == SceneState::Play, "TransitionToStop called from non-Play state");

        m_PendingVisualNovelLoadSlot = 0;
        UIInputSystem::Reset();
        Input::ClearMouseInputBounds();
        m_PlayModeViewportMouseDown = false;
        CommandBus::ClearQueuedCommands();
        SceneTransitionService::DrainRequests();
        m_ActiveScene->OnRuntimeStop();
        Renderer2D::EndScene();
        CommandBus::ClearQueuedCommands();
        ClearEntitySelection();

        m_SceneState = SceneState::Edit;
        m_ActiveScene = m_EditorScene;
        m_ActiveScene->OnEditorStart();
        m_PlayPaused = false;
        m_StepOnce = false;

        SyncPanels();
    }

    void EditorLayerBase::TogglePlayPause()
    {
        if (m_SceneState != SceneState::Play)
            return;
        m_PlayPaused = !m_PlayPaused;
        m_StepOnce = false;
    }

    void EditorLayerBase::StepPlayFrame()
    {
        if (m_SceneState != SceneState::Play)
            return;
        m_PlayPaused = true;
        m_StepOnce = true;
    }

    void EditorLayerBase::LoadPlayScene(const std::filesystem::path& scenePath)
    {
        if (m_SceneState != SceneState::Play)
            return;

        const std::filesystem::path resolvedPath = AssetPath::Resolve(scenePath);
        Ref<Scene> newScene = CreateRef<Scene>();
        SceneSerializer serializer(newScene);
        if (!serializer.DeserializeYaml(resolvedPath))
        {
            WT_CORE_ERROR("Play Mode failed to load scene: {}", resolvedPath.string());
            return;
        }

        const std::filesystem::path previousPlayScenePath = m_PlayScenePath;
        ClearEntitySelection();
        UIInputSystem::Reset();
        Input::ClearMouseInputBounds();
        m_PlayModeViewportMouseDown = false;
        CommandBus::ClearQueuedCommands();

        if (m_ActiveScene)
        {
            m_ActiveScene->OnRuntimeStop();
            Renderer2D::EndScene();
            CommandBus::ClearQueuedCommands();
        }

        m_ActiveScene = newScene;
        m_PlayScenePath = scenePath;
        GameProgress::SetSceneTransitionContext(previousPlayScenePath, m_PlayScenePath);
        ApplyPendingVisualNovelLoad();

        if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
        {
            m_ActiveScene->OnViewportResize(
                static_cast<uint32_t>(m_ViewportSize.x),
                static_cast<uint32_t>(m_ViewportSize.y));
        }

        m_ActiveScene->OnRuntimeStart();
        SyncPanels();
    }

    void EditorLayerBase::ApplyPendingVisualNovelLoad()
    {
        if (!m_ActiveScene || m_PendingVisualNovelLoadSlot <= 0)
            return;

        ApplyVisualNovelAutoLoadSlot(m_ActiveScene.get(), m_PendingVisualNovelLoadSlot);
        m_PendingVisualNovelLoadSlot = 0;
    }

    bool EditorLayerBase::ConsumePlayModeRuntimeCommands()
    {
        if (m_SceneState != SceneState::Play || !m_ActiveScene)
            return false;

        std::vector<std::string> commands;
        DrainGameplayRuntimeCommands(m_ActiveScene.get(), commands);
        for (const std::string& command : CommandBus::DrainRuntimeCommands())
            commands.push_back(command);

        bool consumed = false;
        for (const std::string& command : commands)
        {
            if (command.empty() || StartsWith(command, "script:"))
                continue;

            if (command == "quit")
            {
                TransitionToStop();
                return true;
            }

            consumed |= CommandBus::Execute(m_ActiveScene.get(), command).Handled;
            if (m_SceneState != SceneState::Play || !m_ActiveScene)
                break;
        }

        consumed |= ConsumePlayModeSceneTransitionRequests();
        return consumed;
    }

    bool EditorLayerBase::ConsumePlayModeSceneTransitionRequests()
    {
        std::vector<SceneTransitionRequest> requests = SceneTransitionService::DrainRequests();
        if (requests.empty())
            return false;

        ExecutePlayModeSceneTransitionRequest(requests.back());
        return true;
    }

    void EditorLayerBase::ExecutePlayModeSceneTransitionRequest(const SceneTransitionRequest& request)
    {
        if (m_SceneState != SceneState::Play)
            return;

        switch (request.Mode)
        {
        case SceneTransitionMode::LoadScene:
            LoadPlayScene(request.ScenePath);
            break;
        case SceneTransitionMode::NewGame:
            GameProgress::ResetForNewGame();
            m_PendingVisualNovelLoadSlot = 0;
            LoadPlayScene(request.ScenePath);
            break;
        case SceneTransitionMode::LoadGame:
            m_PendingVisualNovelLoadSlot = request.Slot;
            GameProgress::LoadSlot(request.Slot);
            LoadPlayScene(request.ScenePath);
            break;
        }
    }

    void EditorLayerBase::SyncPanels()
    {
        m_SceneHierarchyPanel->SetContext(m_ActiveScene);
        m_AnimationEditorPanel->SetScene(m_ActiveScene);
    }

    void EditorLayerBase::ClearEntitySelection()
    {
        m_HoveredEntity = {};
        m_EditorCameraViewportMouseDown = false;
        m_GizmoWasUsing = false;
        m_GizmoEditEntity = {};
        m_GizmoStartTransform.reset();
        m_UIEditingCanvas = {};
        m_UIEditHandle = UIEdit_None;
        m_UIEditEntity = {};
        m_UIEditStartHadText = false;
        m_UIEditStartWidget.reset();
        m_UIEditStartText.reset();
        m_SceneHierarchyPanel->SetSelectedEntity({});
        m_AnimationEditorPanel->SetEntity({});
        m_SpriteSheetPickerPanel->SetEntity({});
    }

    void EditorLayerBase::ProcessDeferredViewportAssetDrop()
    {
        if (m_DeferredSceneOpenPath.empty() &&
            m_DeferredPrefabInstantiatePath.empty() &&
            m_DeferredUITemplateInstantiatePath.empty())
        {
            return;
        }

        const std::filesystem::path scenePath = m_DeferredSceneOpenPath;
        const std::filesystem::path prefabPath = m_DeferredPrefabInstantiatePath;
        const std::filesystem::path uiTemplatePath = m_DeferredUITemplateInstantiatePath;
        m_DeferredSceneOpenPath.clear();
        m_DeferredPrefabInstantiatePath.clear();
        m_DeferredUITemplateInstantiatePath.clear();

        if (m_SceneState != SceneState::Edit)
            return;

        if (!scenePath.empty())
        {
            WT_CORE_INFO("Editor viewport opening dropped scene '{}'", scenePath.string());
            OpenScene(scenePath);
            return;
        }

        if (!prefabPath.empty())
        {
            InstantiatePrefab(prefabPath);
            return;
        }

        if (!uiTemplatePath.empty())
            InstantiateUITemplate(uiTemplatePath);
    }

    void EditorLayerBase::CommitPendingGizmoEdit()
    {
        if (m_GizmoEditEntity && m_GizmoStartTransform && m_GizmoEditEntity.HasComponent<TransformComponent>())
        {
            const TransformComponent after = m_GizmoEditEntity.GetComponent<TransformComponent>();
            CommandHistory::Get().Push(
                MakeComponentValueCommand(m_GizmoEditEntity, *m_GizmoStartTransform, after));
        }

        m_GizmoWasUsing = false;
        m_GizmoEditEntity = {};
        m_GizmoStartTransform.reset();
    }

    void EditorLayerBase::CommitPendingUIEdit()
    {
        if (m_UIEditEntity && m_UIEditStartWidget && m_UIEditEntity.HasComponent<UIWidgetComponent>())
        {
            const UIWidgetComponent after = m_UIEditEntity.GetComponent<UIWidgetComponent>();
            auto command = std::make_unique<CompositeCommand>();
            command->Add(MakeComponentValueCommand(m_UIEditEntity, *m_UIEditStartWidget, after));
            if (m_UIEditStartHadText && m_UIEditStartText && m_UIEditEntity.HasComponent<UITextComponent>())
            {
                const UITextComponent afterText = m_UIEditEntity.GetComponent<UITextComponent>();
                command->Add(MakeComponentValueCommand(m_UIEditEntity, *m_UIEditStartText, afterText));
            }
            CommandHistory::Get().Push(std::move(command));
        }

        m_UIEditHandle = UIEdit_None;
        m_UIEditEntity = {};
        m_UIEditStartHadText = false;
        m_UIEditStartWidget.reset();
        m_UIEditStartText.reset();
    }

    void EditorLayerBase::UpdateUITextFontDuringUIResize(Entity entity)
    {
        if (m_UIEditHandle == UIEdit_Move || !m_UIEditStartHadText || !m_UIEditStartWidget || !m_UIEditStartText)
            return;
        if (!entity || !entity.HasComponent<UIWidgetComponent>() || !entity.HasComponent<UITextComponent>())
            return;

        const auto& widget = entity.GetComponent<UIWidgetComponent>();
        auto& text = entity.GetComponent<UITextComponent>();

        const float startArea = std::max(0.000001f, m_UIEditStartWidget->Size.x * m_UIEditStartWidget->Size.y);
        const float currentArea = std::max(0.000001f, widget.Size.x * widget.Size.y);
        const float scale = std::clamp(std::sqrt(currentArea / startArea), 0.2f, 5.0f);
        text.FontSize = std::clamp(m_UIEditStartText->FontSize * scale, 1.0f, 256.0f);
    }

    // =========================================================================
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
            for (auto id : m_EditorScene->GetRegistry().view<IDComponent>())
            {
                Entity other = { id, m_EditorScene.get() };
                if (other && other != e && other.GetName() == finalName)
                    exists = true;
            }
            if (!exists) break;
            finalName = baseName + std::to_string(index++);
        }

        if (Entity found = m_EditorScene->GetEntityByName(finalName))
        {
            found.GetComponent<TagComponent>().Tag = finalName;
            m_EditorScene->InvalidateEntityLookupCache();
        }

        m_SceneHierarchyPanel->SetSelectedEntity(e);
    }

    void EditorLayerBase::InstantiateUITemplate(const std::filesystem::path& path)
    {
        if (m_SceneState != SceneState::Edit || !m_EditorScene)
            return;

        UUID parentID = 0;
        Entity selected = m_SceneHierarchyPanel->GetSelectedEntity();
        if (selected && selected.HasComponent<UIWidgetComponent>())
            parentID = selected.GetUUID();

        if (static_cast<uint64_t>(parentID) == 0)
        {
            for (auto id : m_EditorScene->GetRegistry().view<UICanvasComponent>())
            {
                Entity canvas{ id, m_EditorScene.get() };
                parentID = canvas.GetUUID();
                break;
            }
        }

        if (static_cast<uint64_t>(parentID) == 0)
        {
            Entity canvas = m_EditorScene->CreateEntity("UI Canvas");
            canvas.AddComponent<UICanvasComponent>();
            auto& widget = canvas.AddComponent<UIWidgetComponent>();
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = { 0.0f, 0.0f };
            widget.Size = { 1.0f, 1.0f };
            widget.SortOrder = 0;
            parentID = canvas.GetUUID();
        }

        std::vector<Entity> entities = UITemplateFactory::CreateFromAsset(m_EditorScene.get(), path, parentID);
        if (!entities.empty())
            m_SceneHierarchyPanel->SetSelectedEntity(entities.front());
    }

    void EditorLayerBase::OnDuplicateEntity()
    {
        if (m_SceneState != SceneState::Edit) return;
        if (Entity selected = m_SceneHierarchyPanel->GetSelectedEntity())
        {
            auto command = std::make_unique<EntityDuplicateCommand>(m_EditorScene.get(), selected);
            command->Execute();
            m_SceneHierarchyPanel->SetSelectedEntity(command->GetEntity());
            CommandHistory::Get().Push(std::move(command));
        }
    }

    // =========================================================================
    // =========================================================================

    void EditorLayerBase::StartPlayerPackageBuild(bool enableScripts)
    {
        if (m_PlayerBuildRunning)
            return;

        if (m_SceneState == SceneState::Play)
            TransitionToStop();

        // Save the current scene first so packaging uses the latest state, then
        // show a scene picker so the designer can choose the startup scene
        // (defaults to the currently open one).
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

        m_PackageScenePath = m_EditorScenePath.generic_string();
        m_PackageSceneInput = m_PackageScenePath;
        m_PackageEnableScripts = enableScripts;
        m_PackageScenePickerOpen = true;
    }

    void EditorLayerBase::ExecutePlayerPackageBuild(bool enableScripts)
    {
        PlayerPackageOptions options;
        options.StartupScene = m_PackageScenePath.empty() ? m_EditorScenePath : std::filesystem::path(m_PackageScenePath);
        options.Configuration = "Debug";
        options.EnableScripts = enableScripts;
        options.IncludeDebugSymbols = false;

        m_PlayerBuildStatus = enableScripts
            ? "Packaging player and editor with C# scripts..."
            : "Packaging player and editor without C# scripts...";
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
        {
            m_LastPlayerBuildDirectory = result.PackageDirectory;
            m_LastEditorBuildDirectory = result.EditorPackageDirectory;
            if (!result.ReportPath.empty())
                m_PlayerBuildStatus += "\nReport: " + result.ReportPath.string();
        }
    }

} // namespace Wheatear
