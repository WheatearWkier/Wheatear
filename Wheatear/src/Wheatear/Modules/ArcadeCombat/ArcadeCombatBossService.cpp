#include "wtpch.h"
#include "ArcadeCombatBossService.h"

#include "ArcadeCombatMath.h"
#include "ArcadeCombatProjectileService.h"
#include "ArcadeCombatSignalHandlers.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::ArcadeCombatBossService {

    namespace {

        constexpr float Pi = 3.1415926535f;

        static const WAO::ActionRecipe* ResolveBossShotRecipe()
        {
            return WAO::FindRecipeOrWarn("arcade.boss_bullet", "ArcadeCombat.Boss");
        }

        static void StartBossJump(ArcadeCombatLevelComponent& level,
            ArcadeBossComponent& boss,
            TransformComponent& transform)
        {
            boss.RuntimeJumping = true;
            boss.RuntimeJumpProgress = 0.0f;
            boss.RuntimeJumpTimer = 0.0f;
            boss.RuntimeJumpStart = transform.Translation;

            const float x = std::sin(level.RuntimeElapsed * 1.73f) * 5.4f;
            const float y = 2.0f + std::cos(level.RuntimeElapsed * 1.11f) * 0.65f;
            boss.RuntimeJumpTarget = {
                std::clamp(x, level.ArenaMin.x + 1.2f, level.ArenaMax.x - 1.2f),
                std::clamp(y, level.ArenaMin.y + 1.2f, level.ArenaMax.y - 0.9f),
                transform.Translation.z
            };
        }

    } // namespace

    void UpdateBoss(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity boss,
        Entity player,
        float dt)
    {
        if (!boss || !boss.HasComponent<ArcadeBossComponent>() ||
            !boss.HasComponent<ArcadeCombatantComponent>() ||
            !boss.HasComponent<TransformComponent>())
            return;

        auto& bossComponent = boss.GetComponent<ArcadeBossComponent>();
        auto& combatant = boss.GetComponent<ArcadeCombatantComponent>();
        auto& transform = boss.GetComponent<TransformComponent>();

        if (!combatant.Alive)
        {
            if (!level.RuntimeVictory)
            {
                bossComponent.Active = false;
                bossComponent.RuntimeJumping = false;
                level.RuntimeVictory = true;
                level.RuntimeResultTimer = 0.0f;
                level.RuntimeResultCommandIssued = false;
                level.RuntimePaused = false;

                if (player && player.HasComponent<ArcadeCombatantComponent>())
                    player.GetComponent<ArcadeCombatantComponent>().ControlsLocked = true;

                ArcadeCombatProjectileService::DestroyProjectiles(scene);
            }
            return;
        }

        if (!bossComponent.Active || !player || !player.HasComponent<TransformComponent>())
            return;

        bossComponent.RuntimeJumpTimer += dt;
        if (!bossComponent.RuntimeJumping && bossComponent.RuntimeJumpTimer >= bossComponent.JumpInterval)
            StartBossJump(level, bossComponent, transform);

        if (bossComponent.RuntimeJumping)
        {
            bossComponent.RuntimeJumpProgress += dt / std::max(0.01f, bossComponent.JumpDuration);
            const float t = std::clamp(bossComponent.RuntimeJumpProgress, 0.0f, 1.0f);
            transform.Translation = glm::mix(bossComponent.RuntimeJumpStart, bossComponent.RuntimeJumpTarget, t);
            transform.Translation.y += std::sin(t * Pi) * 0.85f;
            if (t >= 1.0f)
                bossComponent.RuntimeJumping = false;
        }

        bossComponent.RuntimeShootTimer += dt;
        const WAO::ActionRecipe* shotRecipe = ResolveBossShotRecipe();
        if (!shotRecipe)
            return;

        const float shootInterval = bossComponent.ShootInterval > 0.0f
            ? bossComponent.ShootInterval
            : std::max(0.01f, shotRecipe->Cooldown);
        if (bossComponent.RuntimeShootTimer >= shootInterval)
        {
            bossComponent.RuntimeShootTimer = 0.0f;
            const glm::vec2 direction = ArcadeCombatMath::DirectionTo(
                transform.Translation,
                player.GetComponent<TransformComponent>().Translation);

            ArcadeCombatSignalHandlers::ProjectileSpawnPayload payload;
            payload.SceneContext = scene;
            payload.EntityName = "Arcade_BossBullet";
            payload.Position = transform.Translation + glm::vec3(direction * 0.65f, -0.1f);
            payload.Velocity = direction * 4.2f;
            payload.Damage = WAO::PrimaryEffectValue(*shotRecipe, WAO::EffectType::Damage, 12.0f);
            payload.Lifetime = 2.4f;
            payload.Radius = 0.19f;
            payload.Team = (int)ArcadeTeam::Enemy;
            payload.Color = { 0.95f, 0.22f, 0.34f, 1.0f };

            const std::string detail = "fire " + shotRecipe->DisplayName;
            WAO::ActionResolveContext actionContext;
            actionContext.SceneContext = scene;
            actionContext.Intent.Actor = boss.GetUUID();
            actionContext.Intent.ExplicitTarget = player.GetUUID();
            actionContext.Intent.ActionId = shotRecipe->Id;
            actionContext.Intent.Source = "ArcadeCombat.Boss";
            actionContext.Detail = detail;
            actionContext.TransientPayload = &payload;
            WAO::ActionOrchestrator::ExecuteWithRecipe(actionContext, *shotRecipe);
        }
    }

} // namespace Wheatear::ArcadeCombatBossService
