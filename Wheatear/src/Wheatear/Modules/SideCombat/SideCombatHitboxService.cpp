#include "wtpch.h"
#include "SideCombatHitboxService.h"

#include "SideCombatComboService.h"
#include "SideCombatFeedbackService.h"
#include "SideCombatHitResolutionService.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Modules/Common/GameplayTextService.h"
#include "Wheatear/Modules/Common/GameplayVisualService.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Wheatear::SideCombatHitboxService {

    namespace {

        static std::string FormatFramePath(const std::string& pattern, int frame)
        {
            return GameplayTextService::FormatFramePath(pattern, frame);
        }

        static std::string ResolveAttackTexture(SideAttackKind kind, int team)
        {
            if (team == (int)SideCombatTeam::Enemy)
            {
                if (kind == SideAttackKind::EnemyProjectile || kind == SideAttackKind::EnemyShockwave)
                    return AssetAliasRegistry::Path("side.vfx.enemy_projectile");
                return AssetAliasRegistry::Path("side.vfx.enemy_claw");
            }

            switch (kind)
            {
            case SideAttackKind::Launcher:
                return AssetAliasRegistry::Path("side.vfx.launcher_slash");
            case SideAttackKind::MagicBolt:
                return AssetAliasRegistry::Path("side.vfx.magic_bolt");
            case SideAttackKind::AllySupport:
                return AssetAliasRegistry::Path("side.vfx.ally_support");
            case SideAttackKind::BreakLimit:
                return AssetAliasRegistry::Path("side.vfx.ally_support");
            case SideAttackKind::Dash:
                return AssetAliasRegistry::Path("side.vfx.launcher_slash");
            case SideAttackKind::Basic:
            default:
                return AssetAliasRegistry::Path("side.vfx.basic_slash");
            }
        }

        static bool ShouldKeepVisualAfterHit(const SideHitboxComponent& hitbox)
        {
            return hitbox.AttackKind == SideAttackKind::Launcher &&
                !hitbox.TextureFramePattern.empty() &&
                hitbox.TextureFrameCount > 1;
        }

    } // namespace

    Entity CreateHitbox(Scene* scene,
        const std::string& name,
        entt::entity ownerEntity,
        const glm::vec2& sourceGroundPosition,
        float sourceAirHeight,
        float facing,
        SideAttackKind kind,
        const std::string& actionRecipeId,
        int team,
        const SideCombatTuningService::SideAttackTuning& tuning,
        float damage,
        const SideCombatTuningService::SideCombatTuning& combatTuning)
    {
        Entity hitbox = scene->CreateEntity(name);
        const glm::vec2 groundPosition = {
            sourceGroundPosition.x + facing * tuning.Offset.x,
            sourceGroundPosition.y + tuning.Offset.y
        };
        const float airHeight = std::max(0.0f, sourceAirHeight + tuning.AirHeight);

        auto& transform = hitbox.GetComponent<TransformComponent>();
        transform.Translation = {
            groundPosition.x,
            groundPosition.y + airHeight,
            SideCombatTuningService::CalculateSortZ(groundPosition.y, combatTuning) + 0.04f
        };
        transform.Scale = { tuning.Size.x, std::max(tuning.AirRange, tuning.Size.y), 1.0f };

        auto& sprite = hitbox.AddComponent<SpriteRendererComponent>();
        sprite.Color = { 1.0f, 1.0f, 1.0f, 0.88f };
        sprite.FlipX = facing < 0.0f;

        auto& component = hitbox.AddComponent<SideHitboxComponent>();
        component.RuntimeOwnerEntity = static_cast<uint32_t>(ownerEntity);
        component.Team = team;
        component.AttackKind = kind;
        component.ActionRecipeId = actionRecipeId;
        component.Size = tuning.Size;
        component.Velocity = { facing * tuning.Velocity.x, tuning.Velocity.y };
        component.Damage = damage;
        component.Lifetime = tuning.Lifetime;
        component.HitStun = tuning.HitStun;
        component.LaunchVelocity = { facing * tuning.LaunchVelocity.x, tuning.LaunchVelocity.y };
        component.AttackerAirImpulse = tuning.AttackerAirImpulse;
        component.AttackerAirFallStep = tuning.AttackerAirFallStep;
        component.TargetAirFallStep = tuning.TargetAirFallStep;
        component.ProtectionGain = tuning.ProtectionGain;
        component.AirHeight = airHeight;
        component.AirRange = tuning.AirRange;
        component.DestroyOnHit = tuning.DestroyOnHit;
        component.TextureFramePattern = tuning.TextureFramePattern;
        component.TextureFrameCount = std::max(1, tuning.TextureFrameCount);
        component.TextureFrameRate = std::max(1.0f, tuning.TextureFrameRate);
        component.HitSound = tuning.HitSound;
        component.HitSoundVolume = tuning.SoundVolume;
        component.HitPause = tuning.HitPause;
        component.CameraShake = tuning.CameraShake;
        component.CameraShakeDuration = tuning.CameraShakeDuration;
        component.RuntimeGroundPosition = groundPosition;
        ApplyFrameTexture(sprite, component);
        if (!sprite.Texture)
        {
            if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(ResolveAttackTexture(kind, team)))
                sprite.Texture = texture;
        }
        return hitbox;
    }

    void DestroyOwnedHitboxes(Scene* scene, entt::entity ownerEntity)
    {
        if (!scene || ownerEntity == entt::null)
            return;

        const uint32_t owner = static_cast<uint32_t>(ownerEntity);
        auto& registry = scene->GetRegistry();
        std::vector<entt::entity> hitboxes;
        for (auto e : registry.view<SideHitboxComponent>())
        {
            if (registry.get<SideHitboxComponent>(e).RuntimeOwnerEntity == owner)
                hitboxes.push_back(e);
        }

        for (auto e : hitboxes)
        {
            if (registry.valid(e))
                scene->DestroyEntity({ e, scene });
        }
    }

    void ApplyFrameTexture(SpriteRendererComponent& sprite, const SideHitboxComponent& hitbox)
    {
        const int frameCount = std::max(1, hitbox.TextureFrameCount);
        const float frameRate = std::max(1.0f, hitbox.TextureFrameRate);
        const int frame = 1 + std::min(frameCount - 1, (int)std::floor(hitbox.RuntimeAge * frameRate));
        const std::string path = FormatFramePath(hitbox.TextureFramePattern, frame);
        if (path.empty())
            return;

        if (Ref<Texture2D> texture = GameplayVisualService::LoadTextureCached(path))
            sprite.Texture = texture;
    }

    bool OverlapsHitbox(const SideHitboxComponent& hitbox, const SideCombatantComponent& target)
    {
        const glm::vec2 groundDelta = glm::abs(hitbox.RuntimeGroundPosition - target.RuntimeGroundPosition);
        const bool overlapsGround =
            groundDelta.x <= (hitbox.Size.x + target.CollisionSize.x) * 0.5f &&
            groundDelta.y <= (hitbox.Size.y + target.CollisionSize.y) * 0.5f;
        if (!overlapsGround)
            return false;

        const float hitMin = hitbox.AirHeight - hitbox.AirRange * 0.5f;
        const float hitMax = hitbox.AirHeight + hitbox.AirRange * 0.5f;
        const float targetMin = target.RuntimeAirHeight;
        const float targetMax = target.RuntimeAirHeight + std::max(0.1f, target.CollisionHeight);
        return hitMax >= targetMin && hitMin <= targetMax;
    }

    void UpdateHitboxes(Scene* scene,
        SideCombatLevelComponent& level,
        float dt)
    {
        if (!scene)
            return;

        const auto& tuning = SideCombatTuningService::GetTuning(level);
        auto& registry = scene->GetRegistry();
        std::vector<entt::entity> hitboxesToDestroy;

        for (auto e : registry.view<TransformComponent, SideHitboxComponent>())
        {
            auto& hitboxTransform = registry.get<TransformComponent>(e);
            auto& hitbox = registry.get<SideHitboxComponent>(e);

            hitbox.RuntimeAge += dt;
            hitbox.Lifetime -= dt;
            hitbox.RuntimeGroundPosition += hitbox.Velocity * dt;
            hitboxTransform.Translation = {
                hitbox.RuntimeGroundPosition.x,
                hitbox.RuntimeGroundPosition.y + hitbox.AirHeight,
                SideCombatTuningService::CalculateSortZ(hitbox.RuntimeGroundPosition.y, tuning) + 0.04f
            };
            if (registry.all_of<SpriteRendererComponent>(e))
                ApplyFrameTexture(registry.get<SpriteRendererComponent>(e), hitbox);

            if (hitbox.Lifetime <= 0.0f)
            {
                hitboxesToDestroy.push_back(e);
                continue;
            }

            if (hitbox.RuntimeHitSomething && hitbox.DestroyOnHit && ShouldKeepVisualAfterHit(hitbox))
                continue;

            for (auto targetEntity : registry.view<TransformComponent, SideCombatantComponent>())
            {
                auto& target = registry.get<SideCombatantComponent>(targetEntity);
                if (!target.Alive || target.Team == hitbox.Team || target.Team == (int)SideCombatTeam::Neutral)
                    continue;
                if (target.Invulnerable || target.RuntimeInvulnerableTimer > 0.0f)
                    continue;
                if (!OverlapsHitbox(hitbox, target))
                    continue;

                if (!hitbox.RuntimeHitSomething)
                    SideCombatFeedbackService::TriggerHitFeedback(scene, level, hitbox);

                const auto hitResult = SideCombatHitResolutionService::ResolveHit(
                    scene,
                    level,
                    tuning,
                    targetEntity,
                    target,
                    hitbox);
                SideCombatComboService::ApplyPlayerAirHitReward(scene, level, hitbox, target);
                SideCombatComboService::AwardMagicSwordGauge(scene, level, hitbox, target);

                if (hitbox.Team == (int)SideCombatTeam::Player && !hitResult.BossProtectionTriggered)
                    SideCombatComboService::RegisterPlayerHit(level);
                else
                    SideCombatComboService::ResetCombo(level);

                hitbox.RuntimeHitSomething = true;
                if (hitbox.DestroyOnHit)
                {
                    if (ShouldKeepVisualAfterHit(hitbox))
                        break;

                    hitboxesToDestroy.push_back(e);
                    break;
                }
            }
        }

        for (auto e : hitboxesToDestroy)
        {
            if (registry.valid(e))
                scene->DestroyEntity({ e, scene });
        }
    }

} // namespace Wheatear::SideCombatHitboxService
