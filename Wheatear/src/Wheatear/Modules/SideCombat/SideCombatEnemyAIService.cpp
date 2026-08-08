#include "wtpch.h"
#include "SideCombatEnemyAIService.h"

#include "SideCombatActionService.h"
#include "SideCombatHitboxService.h"
#include "SideCombatHitResolutionService.h"
#include "SideCombatMath.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::SideCombatEnemyAIService {

    namespace {

        using SideCombatActionService::BeginEnemyAction;
        using SideCombatActionService::ClearEnemyAction;
        using SideCombatActionService::IsEnemyActionActive;
        using SideCombatHitboxService::CreateHitbox;
        using SideCombatHitboxService::DestroyOwnedHitboxes;
        using SideCombatHitResolutionService::CanEnemyAct;
        using SideCombatTuningService::ApplyBearBossTuning;
        using SideCombatTuningService::GetAttack;
        using SideCombatTuningService::GetTuning;

        static std::string ActionRecipeId(const std::string& attackId)
        {
            return WAO::ComposeActionId("side", attackId);
        }

        static bool UpdateEnemyAction(Scene* scene,
            SideCombatLevelComponent& level,
            Entity enemy,
            SideCombatantComponent& combatant,
            SideEnemyAIComponent& ai,
            const glm::vec2& playerPosition,
            float dt)
        {
            if (!IsEnemyActionActive(ai))
            {
                ClearEnemyAction(ai);
                return false;
            }

            const auto& tuning = GetTuning(level);
            const std::string attackId = ai.RuntimeActionAttackId;
            const auto& attack = GetAttack(tuning, attackId);
            ai.RuntimeActionTimer += dt;

            if (attackId == "bear_charge")
            {
                if (ai.RuntimeActionTimer < ai.RuntimeActionHitboxTime)
                {
                    combatant.RuntimeVelocity.x = 0.0f;
                    combatant.RuntimeVelocity.y = 0.0f;
                }
                else
                {
                    if (!ai.RuntimeActionHitboxSpawned)
                    {
                        const float chargeFacing = SideCombatMath::SignNonZero(playerPosition.x - combatant.RuntimeGroundPosition.x);
                        if (chargeFacing != 0.0f)
                        {
                            ai.RuntimeActionFacing = chargeFacing;
                            combatant.RuntimeFacing = chargeFacing;
                        }

                        combatant.RuntimeVelocity.x = ai.RuntimeActionFacing * tuning.Enemy.BearBossChargeSpeed;

                        CreateHitbox(scene,
                            ai.RuntimeActionEntityName.empty() ? "Side_EnemyAction" : ai.RuntimeActionEntityName,
                            static_cast<entt::entity>(enemy),
                            combatant.RuntimeGroundPosition,
                            combatant.RuntimeAirHeight,
                            ai.RuntimeActionFacing,
                            ai.RuntimeActionKind,
                            ai.RuntimeActionRecipeId,
                            (int)SideCombatTeam::Enemy,
                            attack,
                            combatant.Attack * attack.DamageScale + attack.DamageFlat,
                            tuning);
                        ai.RuntimeActionHitboxSpawned = true;
                    }

                    combatant.RuntimeVelocity.x = ai.RuntimeActionFacing * tuning.Enemy.BearBossChargeSpeed;
                    combatant.RuntimeVelocity.y = 0.0f;
                }
            }
            else
            {
                const float movementScale = std::clamp(ai.RuntimeActionMovementScale, 0.0f, 1.0f);
                combatant.RuntimeVelocity.x = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.x,
                    combatant.RuntimeVelocity.x * movementScale,
                    tuning.Enemy.XBrakeAcceleration * dt);
                combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.y,
                    combatant.RuntimeVelocity.y * movementScale,
                    tuning.Enemy.LaneBrakeAcceleration * dt);

                if (!ai.RuntimeActionHitboxSpawned &&
                    ai.RuntimeActionTimer >= ai.RuntimeActionHitboxTime)
                {
                    CreateHitbox(scene,
                        ai.RuntimeActionEntityName.empty() ? "Side_EnemyAction" : ai.RuntimeActionEntityName,
                        static_cast<entt::entity>(enemy),
                        combatant.RuntimeGroundPosition,
                        combatant.RuntimeAirHeight,
                        ai.RuntimeActionFacing,
                        ai.RuntimeActionKind,
                        ai.RuntimeActionRecipeId,
                        (int)SideCombatTeam::Enemy,
                        attack,
                        combatant.Attack * attack.DamageScale + attack.DamageFlat,
                        tuning);
                    ai.RuntimeActionHitboxSpawned = true;
                }
            }

            if (ai.RuntimeActionTimer >= ai.RuntimeActionDuration)
            {
                if (attackId == "bear_charge")
                    ai.RuntimeAttackTimer = 0.0f;
                ClearEnemyAction(ai);
                return false;
            }

            return true;
        }

        static void IssueEnemyAttack(Scene* scene,
            Entity enemy,
            Entity player,
            SideCombatLevelComponent& level,
            SideEnemyAIComponent& ai,
            SideCombatantComponent& combatant)
        {
            if (!enemy || !player)
                return;
            if (!CanEnemyAct(combatant))
                return;

            auto& playerCombatant = player.GetComponent<SideCombatantComponent>();
            const glm::vec2 playerPosition = playerCombatant.RuntimeGroundPosition;
            combatant.RuntimeFacing = SideCombatMath::SignNonZero(playerPosition.x - combatant.RuntimeGroundPosition.x);
            const float facing = combatant.RuntimeFacing;
            const float healthRatio = combatant.MaxHealth > 0.0f ? combatant.Health / combatant.MaxHealth : 0.0f;
            const auto& tuning = GetTuning(level);

            if (ai.Kind == SideEnemyKind::BearBoss)
            {
                const bool lowHealth = healthRatio <= tuning.Enemy.BearBossLowHealthThreshold;
                const bool midHealth = healthRatio <= tuning.Enemy.BearBossMidHealthThreshold;
                const float distance = std::abs(playerPosition.x - combatant.RuntimeGroundPosition.x);
                ai.RuntimeAttackTimer = lowHealth
                    ? tuning.Enemy.BearBossLowAttackInterval
                    : (midHealth ? tuning.Enemy.BearBossMidAttackInterval : ai.AttackInterval);

                const bool farRange = distance >= tuning.Enemy.BearBossShockwaveDistance;
                const bool closeRange = distance <= tuning.Enemy.BearBossChargeDistance;
                const bool alternate = (ai.RuntimeActionSequence % 2u) == 0u;

                const char* attackId = "bear_charge";
                if (farRange)
                {
                    attackId = alternate ? "bear_shockwave" : "bear_charge";
                    if (ai.RuntimeLastActionAttackId == attackId)
                        attackId = alternate ? "bear_charge" : "bear_shockwave";
                }
                else if (closeRange)
                {
                    attackId = alternate ? "enemy_claw" : "bear_charge";
                    if (ai.RuntimeLastActionAttackId == attackId)
                        attackId = alternate ? "bear_charge" : "enemy_claw";
                }
                else if (ai.RuntimeLastActionAttackId == attackId)
                {
                    return;
                }

                const std::string selectedAttackId = attackId;
                const char* entityName = selectedAttackId == "bear_shockwave"
                    ? "Side_BearShockwave"
                    : (selectedAttackId == "enemy_claw" ? "Side_BearClaw" : "Side_BearCharge");
                const SideAttackKind attackKind = selectedAttackId == "bear_shockwave"
                    ? SideAttackKind::EnemyShockwave
                    : SideAttackKind::EnemyMelee;

                const auto& attack = GetAttack(tuning, attackId);
                const std::string recipeId = ActionRecipeId(selectedAttackId);
                if (attackKind != SideAttackKind::EnemyShockwave)
                    combatant.RuntimeVelocity = { 0.0f, 0.0f };
                BeginEnemyAction(ai, attack, selectedAttackId, recipeId, entityName, attackKind, facing);
                ai.RuntimeLastActionAttackId = selectedAttackId;
                return;
            }

            ai.RuntimeAttackTimer = ai.AttackInterval;
            const auto& attack = GetAttack(tuning, "enemy_claw");
            const std::string recipeId = ActionRecipeId("enemy_claw");
            BeginEnemyAction(ai, attack, "enemy_claw", recipeId, "Side_EnemyClaw", SideAttackKind::EnemyMelee, facing);
            ai.RuntimeLastActionAttackId = "enemy_claw";
        }

    } // namespace

    void UpdateEnemies(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt)
    {
        if (!scene || !player || !player.HasComponent<TransformComponent>() || !player.HasComponent<SideCombatantComponent>())
            return;

        const glm::vec2 playerPosition = player.GetComponent<SideCombatantComponent>().RuntimeGroundPosition;
        auto& registry = scene->GetRegistry();
        const auto& tuning = GetTuning(level);

        for (auto e : registry.view<TransformComponent, SideCombatantComponent, SideEnemyAIComponent>())
        {
            Entity enemy = { e, scene };
            auto& combatant = registry.get<SideCombatantComponent>(e);
            auto& ai = registry.get<SideEnemyAIComponent>(e);
            ApplyBearBossTuning(tuning, combatant, ai);

            if (combatant.Team != (int)SideCombatTeam::Enemy)
                continue;
            if (!combatant.Alive)
            {
                ClearEnemyAction(ai);
                DestroyOwnedHitboxes(scene, e);
                combatant.RuntimeVelocity = { 0.0f, 0.0f };
                continue;
            }
            if (!ai.RuntimeAwake)
                continue;
            if (level.RuntimeVictory || level.RuntimeDefeat)
                continue;

            if (!CanEnemyAct(combatant))
            {
                ClearEnemyAction(ai);
                DestroyOwnedHitboxes(scene, e);
                combatant.RuntimeVelocity.x = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.x,
                    0.0f,
                    tuning.Enemy.XBrakeAcceleration * dt);
                combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.y,
                    0.0f,
                    tuning.Enemy.LaneBrakeAcceleration * dt);
                continue;
            }

            if (IsEnemyActionActive(ai))
            {
                const bool actionStillActive = UpdateEnemyAction(scene, level, enemy, combatant, ai, playerPosition, dt);
                if (enemy.HasComponent<SpriteRendererComponent>())
                    enemy.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
                if (actionStillActive)
                    continue;

                if (ai.Kind != SideEnemyKind::BearBoss)
                    continue;
            }

            const glm::vec2 delta = playerPosition - combatant.RuntimeGroundPosition;
            const float distanceX = delta.x;
            const float distanceY = delta.y;
            const float distanceAbs = std::abs(distanceX);
            const float laneAbs = std::abs(distanceY);
            if (distanceAbs > ai.AggroRange)
                continue;

            combatant.RuntimeFacing = SideCombatMath::SignNonZero(distanceX);
            ai.RuntimeAttackTimer = std::max(0.0f, ai.RuntimeAttackTimer - dt);

            const float preferred = ai.Kind == SideEnemyKind::BearBoss
                ? ai.PreferredRange + tuning.Enemy.BossPreferredRangeBonus
                : ai.PreferredRange;
            if (distanceAbs > preferred)
            {
                const float speedScale = ai.Kind == SideEnemyKind::BearBoss
                    ? tuning.Enemy.BossMoveSpeedScale
                    : tuning.Enemy.GruntMoveSpeedScale;
                combatant.RuntimeVelocity.x = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.x,
                    combatant.RuntimeFacing * combatant.MoveSpeed * speedScale,
                    tuning.Enemy.XApproachAcceleration * dt);
            }
            else
            {
                combatant.RuntimeVelocity.x = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.x,
                    0.0f,
                    tuning.Enemy.XBrakeAcceleration * dt);
            }

            if (laneAbs > ai.LaneTolerance)
            {
                const float laneDirection = SideCombatMath::SignNonZero(distanceY);
                const float laneSpeedScale = ai.Kind == SideEnemyKind::BearBoss
                    ? tuning.Enemy.BossLaneSpeedScale
                    : tuning.Enemy.GruntLaneSpeedScale;
                combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.y,
                    laneDirection * combatant.MoveSpeed * laneSpeedScale,
                    tuning.Enemy.LaneApproachAcceleration * dt);
            }
            else
            {
                combatant.RuntimeVelocity.y = SideCombatMath::Approach(
                    combatant.RuntimeVelocity.y,
                    0.0f,
                    tuning.Enemy.LaneBrakeAcceleration * dt);
            }

            const bool laneReady = laneAbs <= ai.LaneTolerance + tuning.Enemy.LaneAttackPadding;
            const bool meleeReady = distanceAbs <= ai.AttackRange + tuning.Enemy.AttackRangePadding;
            const bool bossSpecialReady = ai.Kind == SideEnemyKind::BearBoss &&
                distanceAbs > tuning.Enemy.BearBossChargeDistance;

            if (ai.RuntimeAttackTimer <= 0.0f &&
                laneReady &&
                (meleeReady || bossSpecialReady))
            {
                IssueEnemyAttack(scene, enemy, player, level, ai, combatant);
            }

            if (enemy.HasComponent<SpriteRendererComponent>())
                enemy.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
        }
    }

} // namespace Wheatear::SideCombatEnemyAIService
