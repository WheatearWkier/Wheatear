#include "wtpch.h"
#include "SpriteSheetSystem.h"

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

namespace Wheatear {

    namespace {

        template<typename T>
        void ResolveComponent(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<T>())
            {
                T& component = registry.get<T>(entity);
                if (component.SpriteSheet.empty())
                    continue;

                // Shared cached resolution (hot-reloads on sheet file change).
                // A named sub-rect takes priority over a grid cell index.
                SpriteSheetAsset::ResolvedCell resolved;
                const bool ok = !component.SubRect.empty()
                    ? SpriteSheetAsset::ResolveCell(component.SpriteSheet, component.SubRect, resolved)
                    : (component.CellIndex >= 0
                        && SpriteSheetAsset::ResolveCell(component.SpriteSheet, component.CellIndex, resolved));
                if (!ok)
                    continue;

                component.Texture = resolved.Texture;
                component.UVMin = resolved.UVMin;
                component.UVMax = resolved.UVMax;

                // Sprite entities may drive their collider from the cell box.
                if constexpr (std::is_same_v<T, SpriteRendererComponent>)
                {
                    Entity entityHandle = { entity, scene };
                    SpriteSheetSystem::ApplyColliderToEntity(entityHandle, resolved);
                }
            }
        }

    } // namespace

    void SpriteSheetSystem::ApplyColliderToEntity(Entity entity, const SpriteSheetAsset::ResolvedCell& resolved)
    {
        if (Scene* scene = entity.GetScene())
            ApplyColliderToEntity(scene->GetRegistry(), entity, resolved);
    }

    void SpriteSheetSystem::ApplyColliderToEntity(entt::registry& registry, entt::entity entity,
        const SpriteSheetAsset::ResolvedCell& resolved)
    {
        if (entity == entt::null || !resolved.HasCollider)
            return;
        if (!registry.all_of<BoxCollider2DComponent>(entity))
            return;
        auto& box = registry.get<BoxCollider2DComponent>(entity);
        if (!box.FollowAnimation)
            return;
        if (!registry.all_of<SpriteRendererComponent>(entity))
            return;
        const float ppu = registry.get<SpriteRendererComponent>(entity).PixelsPerUnit;
        if (ppu <= 0.0f)
            return;

        box.Size = { resolved.ColliderWidth / ppu, resolved.ColliderHeight / ppu };

        // Collider center offset relative to the rendered content (trim) box
        // center, in world units (world Y points up, content Y points down).
        const float dx = resolved.ColliderLeft + resolved.ColliderWidth * 0.5f
            - (resolved.ContentX + resolved.ContentWidth * 0.5f);
        const float dy = resolved.ColliderTop + resolved.ColliderHeight * 0.5f
            - (resolved.ContentY + resolved.ContentHeight * 0.5f);
        box.Offset = { dx / ppu, -dy / ppu };
    }

    void SpriteSheetSystem::ResolveSheets(Scene* scene)
    {
        if (!scene)
            return;

        ResolveComponent<SpriteRendererComponent>(scene);
        ResolveComponent<UIImageComponent>(scene);
    }

} // namespace Wheatear
