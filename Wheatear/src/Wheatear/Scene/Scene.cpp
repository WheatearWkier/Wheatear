#include "wtpch.h"
#include "Scene.h"
#include "Components.h"
#include "ComponentLifecycleRegistry.h"
#include "Entity.h"
#include "SceneSerializer.h"

#include "Wheatear/Core/AssetPath.h"

#include "Wheatear/Scene/SceneSystemRegistry.h"
#include "Wheatear/Systems/PhysicsSystem.h"
#include "Wheatear/Systems/ScriptSystem.h"
#include "Wheatear/Systems/AnimationSystem.h"
#include "Wheatear/Systems/AudioSystem.h"
#include "Wheatear/Systems/UISystem.h"
#include "Wheatear/Systems/RenderSystem.h"

#include <glm/glm.hpp>


namespace Wheatear {



    Scene::Scene()
    {
        RegisterCoreComponentLifecycles();
        ComponentLifecycleRegistry::BindScene(*this);
    }

    Scene::~Scene()
    {
        ComponentLifecycleRegistry::UnbindScene(*this);
    }



    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<TagComponent>().Tag = name.empty() ? "Entity" : name;
        return entity;
    }

    void Scene::DestroyEntityImmediate(Entity entity)
    {
        m_Registry.destroy(entity);
    }

    void Scene::DestroyEntity(Entity entity)
    {
        m_DestroyQueue.insert(static_cast<entt::entity>(entity));
    }

    Entity Scene::InstantiateFromPrefab(const std::filesystem::path& prefabPath,
        const glm::vec3& position)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(prefabPath);
        const std::vector<Entity> entities = SceneSerializer::DeserializePrefabEntities(resolvedPath, this);
        Entity entity = entities.empty() ? Entity{} : entities.front();
        if (!entity)
        {
            WT_CORE_WARN("InstantiateFromPrefab: failed to load '{}'", resolvedPath.string());
            return {};
        }

        if (entity.HasComponent<TransformComponent>())
            entity.GetComponent<TransformComponent>().Translation = position;

        for (Entity prefabEntity : entities)
        {
            for (auto& system : m_Systems)
                system->OnEntityCreated(this, prefabEntity);
        }

        return entity;
    }


    void Scene::FlushDestroyQueueEditor()
    {
        for (entt::entity e : m_DestroyQueue)
            if (m_Registry.valid(e))
                m_Registry.destroy(e);
        m_DestroyQueue.clear();
    }

    void Scene::FlushDestroyQueue()
    {
        for (entt::entity e : m_DestroyQueue)
        {
            if (!m_Registry.valid(e)) continue;
            Entity entity = { e, this };

            for (auto& system : m_Systems)
                system->OnEntityDestroy(this, entity);

            m_Registry.destroy(e);
        }
        m_DestroyQueue.clear();
    }


    void Scene::ConfigureRuntimeSystems()
    {
        RegisterSystem<PhysicsSystem>();
        RegisterSystem<ScriptSystem>();
        RegisterSystem<AnimationSystem>();
        RegisterSystem<AudioSystem>();
        RegisterSystem<UISystem>();

        SceneSystemRegistry::ForEachRuntimeSystem(
            [this](const std::string&, const SceneSystemRegistry::SystemFactory& factory)
            {
                m_Systems.push_back(factory());
            });

        RegisterSystem<RenderSystem>();
    }

    void Scene::ConfigureEditorSystems()
    {
        RegisterSystem<AnimationSystem>();
        RegisterSystem<UISystem>();
        RegisterSystem<RenderSystem>();
    }

    void Scene::StartSystems(SceneExecutionMode mode)
    {
        m_ExecutionMode = mode;

        for (auto& system : m_Systems)
        {
            if (m_ExecutionMode == SceneExecutionMode::Edit)
                system->OnEditorStart(this);
            else if (m_ExecutionMode == SceneExecutionMode::Runtime)
                system->OnRuntimeStart(this);
        }
    }

    void Scene::StopSystems()
    {
        if (m_Systems.empty())
        {
            m_ExecutionMode = SceneExecutionMode::None;
            return;
        }

        for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
        {
            if (m_ExecutionMode == SceneExecutionMode::Edit)
                (*it)->OnEditorStop(this);
            else if (m_ExecutionMode == SceneExecutionMode::Runtime)
                (*it)->OnRuntimeStop(this);
        }

        m_Systems.clear();
        m_ExecutionMode = SceneExecutionMode::None;
    }

    void Scene::OnRuntimeStart()
    {
        StopSystems();
        ConfigureRuntimeSystems();
        StartSystems(SceneExecutionMode::Runtime);
    }

    void Scene::OnRuntimeStop()
    {
        StopSystems();
        m_DestroyQueue.clear();
    }

    void Scene::OnUpdateRuntime(Timestep ts)
    {
        FlushDestroyQueue();

        for (auto& system : m_Systems)
            system->OnUpdateRuntime(this, ts);
    }

    void Scene::OnEditorStart()
    {
        StopSystems();
        ConfigureEditorSystems();
        StartSystems(SceneExecutionMode::Edit);
    }

    void Scene::OnEditorStop()
    {
        StopSystems();
        m_DestroyQueue.clear();
    }

    void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera)
    {
        FlushDestroyQueueEditor();

        for (auto& system : m_Systems)
            system->OnUpdateEditor(this, ts);

        if (auto* render = GetSystem<RenderSystem>())
            render->RenderWithEditorCamera(this, camera, false);
    }

    void Scene::RenderWithSceneCamera(const Camera& camera,
        const glm::mat4& cameraTransform,
        bool includeUI)
    {
        if (auto* render = GetSystem<RenderSystem>())
            render->RenderWithSceneCamera(this, camera, cameraTransform, includeUI);
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        for (auto e : m_Registry.view<CameraComponent>())
        {
            auto& cc = m_Registry.get<CameraComponent>(e);
            if (!cc.FixedAspectRatio)
                cc.Camera.SetViewportSize(width, height);
        }
    }

    void Scene::SetViewportOffset(float x, float y)
    {
        m_ViewportOffset = { x, y };

        if (auto* ui = GetSystem<UISystem>())
            ui->SetViewportOffset(x, y);
    }


    void Scene::SetAnimationEditorPreviewActive(bool active)
    {
        if (auto* anim = GetSystem<AnimationSystem>())
            anim->SetEditorPreviewActive(active);
    }


    Entity Scene::GetPrimaryCameraEntity()
    {
        for (auto e : m_Registry.view<CameraComponent>())
            if (m_Registry.get<CameraComponent>(e).Primary)
                return { e, this };
        return {};
    }

    Entity Scene::GetEntityByUUID(UUID uuid)
    {
        for (auto e : m_Registry.view<IDComponent>())
            if (m_Registry.get<IDComponent>(e).ID == uuid)
                return { e, this };
        WT_CORE_WARN("GetEntityByUUID: entity {} not found", (uint64_t)uuid);
        return {};
    }

    Entity Scene::GetEntityByName(const std::string& name)
    {
        for (auto e : m_Registry.view<TagComponent>())
            if (m_Registry.get<TagComponent>(e).Tag == name)
                return { e, this };
        return {};
    }
} // namespace Wheatear
