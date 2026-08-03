#include "wtpch.h"
#include "ArcadeCombatProjectileService.h"

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Modules/Common/GameplayCombatService.h"
#include "Wheatear/Modules/Common/GameplayVisualService.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <cmath>
#include <vector>

namespace Wheatear::ArcadeCombatProjectileService {

    namespace {

        static glm::vec2 ToVec2(const glm::vec3& value)
        {
            return { value.x, value.y };
        }

        static float Distance2D(const glm::vec3& a, const glm::vec3& b)
        {
            return glm::length(ToVec2(a) - ToVec2(b));
        }

        static void ApplyDamage(ArcadeCombatantComponent& target, float damage)
        {
            if (!target.Alive || target.Invulnerable)
                return;

            GameplayCombatService::ApplyDamage(target.Health, damage);
            target.Alive = GameplayCombatService::IsAlive(target.Health);
        }

        static std::string ResolveProjectileTexturePath(int team, bool heavy, bool melee)
        {
            if (melee)
                return AssetAliasRegistry::Path("arcade.vfx.katana_slash");
            if (team == (int)ArcadeTeam::Enemy)
                return AssetAliasRegistry::Path("arcade.projectile.boss_orb");
            return heavy
                ? AssetAliasRegistry::Path("arcade.vfx.cannon_blast")
                : AssetAliasRegistry::Path("arcade.projectile.player_bolt");
        }

        static bool ProjectileBlockedByCover(Scene* scene,
            Entity projectileEntity,
            ArcadeProjectileComponent& projectile,
            const TransformComponent& projectileTransform)
        {
            if (projectile.Melee)
                return false;

            auto& registry = scene->GetRegistry();
            for (auto coverEntity : registry.view<TransformComponent, ArcadeCoverComponent>())
            {
                auto& coverTransform = registry.get<TransformComponent>(coverEntity);
                auto& cover = registry.get<ArcadeCoverComponent>(coverEntity);
                if (!cover.BlocksProjectiles || cover.Health <= 0.0f)
                    continue;

                if (Distance2D(projectileTransform.Translation, coverTransform.Translation) <= projectile.Radius + cover.Radius)
                {
                    GameplayCombatService::ApplyDamage(
                        cover.Health,
                        projectile.Damage * (projectile.Heavy ? 0.75f : 0.35f));
                    if (auto* sprite = registry.try_get<SpriteRendererComponent>(coverEntity))
                    {
                        const float normalized = cover.MaxHealth <= 0.0f ? 0.0f : cover.Health / cover.MaxHealth;
                        sprite->Color.a = 0.22f + normalized * 0.78f;
                    }
                    scene->DestroyEntity(projectileEntity);
                    return true;
                }
            }

            return false;
        }

        static bool ProjectileHitCombatant(Scene* scene,
            Entity projectileEntity,
            ArcadeProjectileComponent& projectile,
            const TransformComponent& projectileTransform)
        {
            auto& registry = scene->GetRegistry();
            for (auto targetEntity : registry.view<TransformComponent, ArcadeCombatantComponent>())
            {
                auto& targetTransform = registry.get<TransformComponent>(targetEntity);
                auto& target = registry.get<ArcadeCombatantComponent>(targetEntity);
                if (!target.Alive || target.Team == projectile.Team || target.Team == (int)ArcadeTeam::Neutral)
                    continue;

                if (Distance2D(projectileTransform.Translation, targetTransform.Translation) <= projectile.Radius + target.CollisionRadius)
                {
                    ApplyDamage(target, projectile.Damage);
                    scene->DestroyEntity(projectileEntity);
                    return true;
                }
            }
            return false;
        }

    } // namespace

    void CreateProjectile(Scene* scene,
        const std::string& name,
        const glm::vec3& position,
        const glm::vec2& velocity,
        float damage,
        float lifetime,
        float radius,
        int team,
        const glm::vec4& color,
        bool heavy,
        bool melee)
    {
        if (!scene)
            return;

        Entity projectile = scene->CreateEntity(name);
        auto& transform = projectile.GetComponent<TransformComponent>();
        transform.Translation = position;
        transform.Scale = melee
            ? glm::vec3(radius * 1.65f, radius * 0.36f, 1.0f)
            : glm::vec3(radius * 2.0f, radius * 2.0f, 1.0f);

        if (melee)
            transform.Rotation.z = std::atan2(velocity.y, velocity.x);

        auto& sprite = projectile.AddComponent<SpriteRendererComponent>();
        sprite.Color = color;
        if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(ResolveProjectileTexturePath(team, heavy, melee)))
        {
            sprite.Texture = texture;
            sprite.Color = { 1.0f, 1.0f, 1.0f, color.a };
        }

        auto& projectileComponent = projectile.AddComponent<ArcadeProjectileComponent>();
        projectileComponent.Velocity = velocity;
        projectileComponent.Damage = damage;
        projectileComponent.Lifetime = lifetime;
        projectileComponent.Radius = radius;
        projectileComponent.Team = team;
        projectileComponent.Heavy = heavy;
        projectileComponent.Melee = melee;
    }

    void DestroyProjectiles(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        std::vector<entt::entity> projectiles;
        for (auto e : registry.view<ArcadeProjectileComponent>())
            projectiles.push_back(e);

        for (auto e : projectiles)
        {
            if (registry.valid(e))
                scene->DestroyEntity({ e, scene });
        }
    }

    void UpdateProjectiles(Scene* scene, float dt)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        std::vector<entt::entity> projectiles;
        for (auto e : registry.view<TransformComponent, ArcadeProjectileComponent>())
            projectiles.push_back(e);

        for (auto e : projectiles)
        {
            if (!registry.valid(e))
                continue;

            Entity projectileEntity{ e, scene };
            auto& transform = registry.get<TransformComponent>(e);
            auto& projectile = registry.get<ArcadeProjectileComponent>(e);

            projectile.Lifetime -= dt;
            transform.Translation += glm::vec3(projectile.Velocity * dt, 0.0f);

            if (projectile.Lifetime <= 0.0f)
            {
                scene->DestroyEntity(projectileEntity);
                continue;
            }

            if (ProjectileBlockedByCover(scene, projectileEntity, projectile, transform))
                continue;

            ProjectileHitCombatant(scene, projectileEntity, projectile, transform);
        }
    }

} // namespace Wheatear::ArcadeCombatProjectileService
