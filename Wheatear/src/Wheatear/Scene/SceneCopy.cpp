#include "wtpch.h"
#include "Scene.h"
#include "Components.h"
#include "ComponentGroup.h"
#include "Entity.h"
#include "SceneComponentGroups.h"

namespace Wheatear {

    namespace {

        template<typename... Ts>
        void CopyComponents(ComponentGroup<Ts...>,
            entt::registry& dst, entt::registry& src,
            const std::unordered_map<UUID, entt::entity>& map)
        {
            ([&] {
                for (auto e : src.view<Ts>())
                {
                    // Entities without an IDComponent never make it into the
                    // copy; skip them instead of reading a missing component.
                    const IDComponent* id = src.try_get<IDComponent>(e);
                    if (!id)
                        continue;
                    UUID uuid = id->ID;
                    WT_CORE_ASSERT(map.count(uuid), "Entity UUID not found in map");
                    dst.emplace_or_replace<Ts>(map.at(uuid), src.get<Ts>(e));
                }
                }(), ...);
        }

        template<typename... Ts>
        void CopyComponentsIfExist(ComponentGroup<Ts...>, Entity dst, Entity src)
        {
            ([&] {
                if (src.HasComponent<Ts>())
                    dst.AddOrReplaceComponent<Ts>(src.GetComponent<Ts>());
                }(), ...);
        }

    }

    Ref<Scene> Scene::Copy(Ref<Scene> other)
    {
        Ref<Scene> newScene = CreateRef<Scene>();
        newScene->m_ViewportWidth = other->m_ViewportWidth;
        newScene->m_ViewportHeight = other->m_ViewportHeight;
        newScene->m_ViewportOffset = other->m_ViewportOffset;
        newScene->m_SavePolicy = other->m_SavePolicy;

        auto& src = other->m_Registry;
        auto& dst = newScene->m_Registry;

        std::unordered_map<UUID, entt::entity> enttMap;
        for (auto e : src.view<IDComponent, TagComponent>())
        {
            UUID uuid = src.get<IDComponent>(e).ID;
            const auto& name = src.get<TagComponent>(e).Tag;
            enttMap[uuid] = static_cast<entt::entity>(
                newScene->CreateEntityWithUUID(uuid, name));
        }

        CopyComponents(AllCopyableSceneComponents{}, dst, src, enttMap);
        return newScene;
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
        if (!entity)
            return {};

        const std::vector<Entity> sources = CollectSubtree(entity);
        if (sources.empty())
            return {};

        std::unordered_map<UUID, UUID> idMap;
        std::vector<std::pair<Entity, Entity>> copies;
        copies.reserve(sources.size());

        Entity newRoot;
        for (Entity source : sources)
        {
            if (!source)
                continue;

            Entity copy = CreateEntity(source.GetName());
            CopyComponentsIfExist(AllCopyableSceneComponents{}, copy, source);
            if (!newRoot)
                newRoot = copy;

            idMap[source.GetUUID()] = copy.GetUUID();
            copies.emplace_back(source, copy);

            // Runtime physics pointers must never be copied: sharing a b2Body
            // between two entities leads to a double DestroyBody on teardown.
            if (copy.HasComponent<Rigidbody2DComponent>())
                copy.GetComponent<Rigidbody2DComponent>().RuntimeBody = nullptr;
            if (copy.HasComponent<BoxCollider2DComponent>())
                copy.GetComponent<BoxCollider2DComponent>().RuntimeFixture = nullptr;
            if (copy.HasComponent<CircleCollider2DComponent>())
                copy.GetComponent<CircleCollider2DComponent>().RuntimeFixture = nullptr;
        }

        for (auto& [source, copy] : copies)
        {
            if (!copy)
                continue;

            const bool isRoot = source == entity;
            if (copy.HasComponent<TransformComponent>()
                && source.HasComponent<TransformComponent>())
            {
                auto& transform = copy.GetComponent<TransformComponent>();
                if (!isRoot)
                {
                    const auto it = idMap.find(
                        source.GetComponent<TransformComponent>().Parent);
                    if (it != idMap.end())
                        transform.Parent = it->second;
                    else
                        transform.Parent = UUID(0);
                }
            }

            if (copy.HasComponent<UIWidgetComponent>()
                && source.HasComponent<UIWidgetComponent>()
                && !isRoot)
            {
                auto& widget = copy.GetComponent<UIWidgetComponent>();
                const auto it = idMap.find(
                    source.GetComponent<UIWidgetComponent>().ParentEntity);
                if (it != idMap.end())
                    widget.ParentEntity = it->second;
                else
                    widget.ParentEntity = UUID(0);
            }
        }

        return newRoot;
    }

} // namespace Wheatear
