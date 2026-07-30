#pragma once

#include "entt.hpp"

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <cstdint>
#include <functional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Wheatear {

    class WHEATEAR_API ComponentLifecycleRegistry
    {
    public:
        template<typename Component>
        using ComponentCallback = std::function<void(Scene&, Entity, Component&)>;

        template<typename Component>
        static void RegisterOnAdded(const std::string& name, ComponentCallback<Component> callback)
        {
            auto& binding = EnsureBinding<Component>();
            AddCallback(binding, EventKind::Added, name,
                [callback = std::move(callback)](Scene& scene, Entity entity, void* component)
                {
                    if (callback)
                        callback(scene, entity, *static_cast<Component*>(component));
                });
        }

        template<typename Component>
        static void RegisterOnUpdated(const std::string& name, ComponentCallback<Component> callback)
        {
            auto& binding = EnsureBinding<Component>();
            AddCallback(binding, EventKind::Updated, name,
                [callback = std::move(callback)](Scene& scene, Entity entity, void* component)
                {
                    if (callback)
                        callback(scene, entity, *static_cast<Component*>(component));
                });
        }

        template<typename Component>
        static void RegisterOnRemoved(const std::string& name, ComponentCallback<Component> callback)
        {
            auto& binding = EnsureBinding<Component>();
            AddCallback(binding, EventKind::Removed, name,
                [callback = std::move(callback)](Scene& scene, Entity entity, void* component)
                {
                    if (callback)
                        callback(scene, entity, *static_cast<Component*>(component));
                });
        }

        static void BindScene(Scene& scene);
        static void UnbindScene(Scene& scene);

    private:
        enum class EventKind : uint8_t
        {
            Added = 0,
            Updated,
            Removed
        };

        using RawCallback = std::function<void(Scene&, Entity, void*)>;
        using SignalConnector = std::function<void(Scene&)>;

        struct NamedCallback
        {
            std::string Name;
            RawCallback Callback;
        };

        struct Binding
        {
            std::type_index Type;
            std::string TypeName;
            SignalConnector ConnectAdded;
            SignalConnector ConnectUpdated;
            SignalConnector ConnectRemoved;
            std::vector<NamedCallback> AddedCallbacks;
            std::vector<NamedCallback> UpdatedCallbacks;
            std::vector<NamedCallback> RemovedCallbacks;
        };

        struct SceneConnectionState
        {
            std::unordered_set<std::type_index> AddedTypes;
            std::unordered_set<std::type_index> UpdatedTypes;
            std::unordered_set<std::type_index> RemovedTypes;
        };

        template<typename Component>
        static Binding& EnsureBinding()
        {
            return FindOrCreateBinding(
                std::type_index(typeid(Component)),
                typeid(Component).name(),
                [](Scene& scene)
                {
                    scene.GetRegistry().template on_construct<Component>()
                        .template connect<&ComponentLifecycleRegistry::DispatchAdded<Component>>();
                },
                [](Scene& scene)
                {
                    scene.GetRegistry().template on_update<Component>()
                        .template connect<&ComponentLifecycleRegistry::DispatchUpdated<Component>>();
                },
                [](Scene& scene)
                {
                    scene.GetRegistry().template on_destroy<Component>()
                        .template connect<&ComponentLifecycleRegistry::DispatchRemoved<Component>>();
                });
        }

        template<typename Component>
        static void DispatchAdded(entt::registry& registry, entt::entity handle)
        {
            if (!registry.valid(handle) || !registry.all_of<Component>(handle))
                return;

            Dispatch(EventKind::Added, std::type_index(typeid(Component)),
                registry, handle, &registry.get<Component>(handle));
        }

        template<typename Component>
        static void DispatchUpdated(entt::registry& registry, entt::entity handle)
        {
            if (!registry.valid(handle) || !registry.all_of<Component>(handle))
                return;

            Dispatch(EventKind::Updated, std::type_index(typeid(Component)),
                registry, handle, &registry.get<Component>(handle));
        }

        template<typename Component>
        static void DispatchRemoved(entt::registry& registry, entt::entity handle)
        {
            if (!registry.valid(handle) || !registry.all_of<Component>(handle))
                return;

            Dispatch(EventKind::Removed, std::type_index(typeid(Component)),
                registry, handle, &registry.get<Component>(handle));
        }

        static Binding& FindOrCreateBinding(std::type_index type,
            std::string typeName,
            SignalConnector connectAdded,
            SignalConnector connectUpdated,
            SignalConnector connectRemoved);
        static void AddCallback(Binding& binding, EventKind kind,
            const std::string& name, RawCallback callback);
        static void Dispatch(EventKind kind, std::type_index type,
            entt::registry& registry, entt::entity handle, void* component);

        static void ConnectBindingToActiveScenes(Binding& binding, EventKind kind);
        static void ConnectBindingToScene(Scene& scene, Binding& binding, EventKind kind);

        static std::vector<Binding>& Bindings();
        static std::unordered_map<entt::registry*, Scene*>& SceneByRegistry();
        static std::unordered_map<Scene*, SceneConnectionState>& SceneConnections();
    };

    WHEATEAR_API void RegisterCoreComponentLifecycles();

} // namespace Wheatear
