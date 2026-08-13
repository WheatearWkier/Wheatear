#include "wtpch.h"
#include "ScriptSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/ScriptableEntity.h"
#include "Wheatear/Scripting/ScriptEngine.h"

namespace Wheatear {

    void ScriptSystem::OnRuntimeStart(Scene* scene)
    {
        if (!ScriptEngine::IsInitialized())
            return;

        ScriptEngine::OnRuntimeStart(scene);
        for (auto e : scene->GetRegistry().view<ScriptComponent>())
            ScriptEngine::OnCreateEntity({ e, scene });
    }

    void ScriptSystem::OnRuntimeStop(Scene* scene)
    {
        if (!ScriptEngine::IsInitialized())
            return;

        ScriptEngine::OnRuntimeStop();
    }

    void ScriptSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!ScriptEngine::IsInitialized())
            return;

        auto& registry = scene->GetRegistry();
        ScriptEngine::OnRuntimeUpdate(ts);

        for (auto e : registry.view<ScriptComponent>())
            ScriptEngine::OnUpdateEntity({ e, scene }, ts);
    }

    void ScriptSystem::OnEntityCreated(Scene* scene, Entity& entity)
    {
        if (!ScriptEngine::IsInitialized()) return;
        if (!entity.HasComponent<ScriptComponent>()) return;
        ScriptEngine::OnCreateEntity(entity);
    }

    void ScriptSystem::OnEntityDestroy(Scene* scene, Entity& entity)
    {
        if (!ScriptEngine::IsInitialized()) return;
        ScriptEngine::OnDestroyEntity(entity);
    }

} // namespace Wheatear
