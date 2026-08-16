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
        Entity newEntity = CreateEntity(entity.GetName());
        CopyComponentsIfExist(AllCopyableSceneComponents{}, newEntity, entity);

        // Runtime physics pointers must never be copied: sharing a b2Body
        // between two entities leads to a double DestroyBody on teardown.
        // The duplicated entity gets fresh bodies on its next physics step.
        if (newEntity.HasComponent<Rigidbody2DComponent>())
            newEntity.GetComponent<Rigidbody2DComponent>().RuntimeBody = nullptr;
        if (newEntity.HasComponent<BoxCollider2DComponent>())
            newEntity.GetComponent<BoxCollider2DComponent>().RuntimeFixture = nullptr;
        if (newEntity.HasComponent<CircleCollider2DComponent>())
            newEntity.GetComponent<CircleCollider2DComponent>().RuntimeFixture = nullptr;

        return newEntity;
    }

} // namespace Wheatear
