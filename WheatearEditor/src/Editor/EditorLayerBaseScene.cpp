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
        CommandHistory::Get().Clear();
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

    void EditorLayerBase::CommitPendingGizmoEdit()
    {
        if (m_GizmoEditEntity && m_GizmoEditEntity.HasComponent<TransformComponent>())
        {
            const TransformComponent after = m_GizmoEditEntity.GetComponent<TransformComponent>();
            CommandHistory::Get().Push(
                MakeComponentValueCommand(m_GizmoEditEntity, m_GizmoStartTransform, after));
        }

        m_GizmoWasUsing = false;
        m_GizmoEditEntity = {};
    }

    void EditorLayerBase::CommitPendingUIEdit()
    {
        if (m_UIEditEntity && m_UIEditEntity.HasComponent<UIWidgetComponent>())
        {
            const UIWidgetComponent after = m_UIEditEntity.GetComponent<UIWidgetComponent>();
            auto command = std::make_unique<CompositeCommand>();
            command->Add(MakeComponentValueCommand(m_UIEditEntity, m_UIEditStartWidget, after));
            if (m_UIEditStartHadText && m_UIEditEntity.HasComponent<UITextComponent>())
            {
                const UITextComponent afterText = m_UIEditEntity.GetComponent<UITextComponent>();
                command->Add(MakeComponentValueCommand(m_UIEditEntity, m_UIEditStartText, afterText));
            }
            CommandHistory::Get().Push(std::move(command));
        }

        m_UIEditHandle = UIEdit_None;
        m_UIEditSurface = 0;
        m_UIEditEntity = {};
        m_UIEditStartHadText = false;
    }

    void EditorLayerBase::UpdateUITextFontDuringUIResize(Entity entity)
    {
        if (m_UIEditHandle == UIEdit_Move || !m_UIEditStartHadText)
            return;
        if (!entity || !entity.HasComponent<UIWidgetComponent>() || !entity.HasComponent<UITextComponent>())
            return;

        const auto& widget = entity.GetComponent<UIWidgetComponent>();
        auto& text = entity.GetComponent<UITextComponent>();

        const float startArea = std::max(0.000001f, m_UIEditStartWidget.Size.x * m_UIEditStartWidget.Size.y);
        const float currentArea = std::max(0.000001f, widget.Size.x * widget.Size.y);
        const float scale = std::clamp(std::sqrt(currentArea / startArea), 0.2f, 5.0f);
        text.FontSize = std::clamp(m_UIEditStartText.FontSize * scale, 1.0f, 256.0f);
    }

    // =========================================================================
    // 鍦烘櫙鏂囦欢鎿嶄綔
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
            found.GetComponent<TagComponent>().Tag = finalName;

        m_SceneHierarchyPanel.SetSelectedEntity(e);
    }

    void EditorLayerBase::OnDuplicateEntity()
    {
        if (m_SceneState != SceneState::Edit) return;
        if (Entity selected = m_SceneHierarchyPanel.GetSelectedEntity())
        {
            auto command = std::make_unique<EntityDuplicateCommand>(m_EditorScene.get(), selected);
            command->Execute();
            m_SceneHierarchyPanel.SetSelectedEntity(command->GetEntity());
            CommandHistory::Get().Push(std::move(command));
        }
    }

    // =========================================================================
    // 鍏叡 UI
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

} // namespace Wheatear
