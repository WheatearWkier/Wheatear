#include "wtpch.h"
#include "ComponentLifecycleRegistry.h"

#include "Components.h"
#include "Scene.h"

#include <algorithm>

namespace Wheatear {

    std::vector<ComponentLifecycleRegistry::Binding>& ComponentLifecycleRegistry::Bindings()
    {
        static std::vector<Binding> bindings;
        return bindings;
    }

    std::unordered_map<entt::registry*, Scene*>& ComponentLifecycleRegistry::SceneByRegistry()
    {
        static std::unordered_map<entt::registry*, Scene*> scenes;
        return scenes;
    }

    std::unordered_map<Scene*, ComponentLifecycleRegistry::SceneConnectionState>&
        ComponentLifecycleRegistry::SceneConnections()
    {
        static std::unordered_map<Scene*, SceneConnectionState> connections;
        return connections;
    }

    ComponentLifecycleRegistry::Binding& ComponentLifecycleRegistry::FindOrCreateBinding(
        std::type_index type,
        std::string typeName,
        SignalConnector connectAdded,
        SignalConnector connectUpdated,
        SignalConnector connectRemoved)
    {
        auto& bindings = Bindings();
        auto it = std::find_if(bindings.begin(), bindings.end(),
            [type](const Binding& binding) { return binding.Type == type; });

        if (it != bindings.end())
            return *it;

        bindings.push_back(Binding{
            type,
            std::move(typeName),
            std::move(connectAdded),
            std::move(connectUpdated),
            std::move(connectRemoved),
            {},
            {},
            {}
            });

        return bindings.back();
    }

    void ComponentLifecycleRegistry::AddCallback(Binding& binding, EventKind kind,
        const std::string& name, RawCallback callback)
    {
        if (name.empty() || !callback)
            return;

        auto* callbacks = &binding.AddedCallbacks;
        if (kind == EventKind::Updated)
            callbacks = &binding.UpdatedCallbacks;
        else if (kind == EventKind::Removed)
            callbacks = &binding.RemovedCallbacks;

        auto it = std::find_if(callbacks->begin(), callbacks->end(),
            [&name](const NamedCallback& registered) { return registered.Name == name; });

        if (it != callbacks->end())
        {
            it->Callback = std::move(callback);
            return;
        }

        callbacks->push_back({ name, std::move(callback) });
        ConnectBindingToActiveScenes(binding, kind);
    }

    void ComponentLifecycleRegistry::BindScene(Scene& scene)
    {
        SceneByRegistry()[&scene.GetRegistry()] = &scene;
        SceneConnections().try_emplace(&scene);

        for (auto& binding : Bindings())
        {
            if (!binding.AddedCallbacks.empty())
                ConnectBindingToScene(scene, binding, EventKind::Added);
            if (!binding.UpdatedCallbacks.empty())
                ConnectBindingToScene(scene, binding, EventKind::Updated);
            if (!binding.RemovedCallbacks.empty())
                ConnectBindingToScene(scene, binding, EventKind::Removed);
        }
    }

    void ComponentLifecycleRegistry::UnbindScene(Scene& scene)
    {
        SceneByRegistry().erase(&scene.GetRegistry());
        SceneConnections().erase(&scene);
    }

    void ComponentLifecycleRegistry::Dispatch(EventKind kind, std::type_index type,
        entt::registry& registry, entt::entity handle, void* component)
    {
        auto sceneIt = SceneByRegistry().find(&registry);
        if (sceneIt == SceneByRegistry().end() || !sceneIt->second)
            return;

        auto bindingIt = std::find_if(Bindings().begin(), Bindings().end(),
            [type](const Binding& binding) { return binding.Type == type; });
        if (bindingIt == Bindings().end())
            return;

        auto* callbacks = &bindingIt->AddedCallbacks;
        if (kind == EventKind::Updated)
            callbacks = &bindingIt->UpdatedCallbacks;
        else if (kind == EventKind::Removed)
            callbacks = &bindingIt->RemovedCallbacks;

        if (callbacks->empty())
            return;

        Scene& scene = *sceneIt->second;
        Entity entity{ handle, &scene };
        for (const auto& callback : *callbacks)
            callback.Callback(scene, entity, component);
    }

    void ComponentLifecycleRegistry::ConnectBindingToActiveScenes(Binding& binding, EventKind kind)
    {
        for (auto& [scene, state] : SceneConnections())
        {
            if (scene)
                ConnectBindingToScene(*scene, binding, kind);
        }
    }

    void ComponentLifecycleRegistry::ConnectBindingToScene(Scene& scene, Binding& binding, EventKind kind)
    {
        auto& state = SceneConnections()[&scene];

        if (kind == EventKind::Added)
        {
            if (state.AddedTypes.insert(binding.Type).second && binding.ConnectAdded)
                binding.ConnectAdded(scene);
            return;
        }

        if (kind == EventKind::Updated)
        {
            if (state.UpdatedTypes.insert(binding.Type).second && binding.ConnectUpdated)
                binding.ConnectUpdated(scene);
            return;
        }

        if (state.RemovedTypes.insert(binding.Type).second && binding.ConnectRemoved)
            binding.ConnectRemoved(scene);
    }

    namespace {

        void ApplyCameraViewport(Scene& scene, Entity, CameraComponent& component)
        {
            if (scene.GetViewportWidth() > 0 && scene.GetViewportHeight() > 0)
                component.Camera.SetViewportSize(scene.GetViewportWidth(), scene.GetViewportHeight());
        }

    } // namespace

    void RegisterCoreComponentLifecycles()
    {
        ComponentLifecycleRegistry::RegisterOnAdded<CameraComponent>(
            "Core.Camera.ApplyViewportOnAdded", ApplyCameraViewport);
        ComponentLifecycleRegistry::RegisterOnUpdated<CameraComponent>(
            "Core.Camera.ApplyViewportOnUpdated", ApplyCameraViewport);
    }

} // namespace Wheatear
