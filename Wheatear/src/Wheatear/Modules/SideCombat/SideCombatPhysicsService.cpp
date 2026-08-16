#include "wtpch.h"
#include "SideCombatPhysicsService.h"

#include "SideCombatFeedbackService.h"
#include "SideCombatHitResolutionService.h"
#include "SideCombatTuningService.h"
#include "SideCombatVisualService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>

namespace Wheatear::SideCombatPhysicsService {

    namespace {

        constexpr float GravityDefault = 22.0f;

    } // namespace

    void UpdateCombatants(Scene* scene,
        const SideCombatLevelComponent& level,
        float dt)
    {
        if (!scene)
            return;

        const auto& tuning = SideCombatTuningService::GetTuning(level);
        const float laneMinY = level.LaneMinY < level.LaneMaxY ? level.LaneMinY : tuning.LaneMinY;
        const float laneMaxY = level.LaneMinY < level.LaneMaxY ? level.LaneMaxY : tuning.LaneMaxY;
        // Normalize arena bounds: std::clamp requires lo <= hi (hand-edited
        // scenes may invert ArenaMin/ArenaMax, which would be UB).
        const float arenaLo = std::min(level.ArenaMin.x, level.ArenaMax.x);
        const float arenaHi = std::max(level.ArenaMin.x, level.ArenaMax.x);
        const float arenaMaxX = level.WaveModeEnabled
            ? std::clamp(level.RuntimeWaveRightWall, arenaLo, arenaHi)
            : level.ArenaMax.x;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TransformComponent, SideCombatantComponent>())
        {
            auto& combatant = registry.get<SideCombatantComponent>(e);
            const bool wasOnGround = combatant.RuntimeOnGround;

            combatant.RuntimeHitStun = std::max(0.0f, combatant.RuntimeHitStun - dt);
            combatant.RuntimeInvulnerableTimer = std::max(0.0f, combatant.RuntimeInvulnerableTimer - dt);
            combatant.RuntimeStateTimer = std::max(0.0f, combatant.RuntimeStateTimer - dt);

            if (!combatant.Alive)
            {
                combatant.RuntimeDeathTimer += dt;
                combatant.RuntimeVelocity = { 0.0f, 0.0f };
                float gravity = GravityDefault;
                if (registry.all_of<SidePlayerControllerComponent>(e))
                    gravity = registry.get<SidePlayerControllerComponent>(e).Gravity;
                if (!combatant.RuntimeOnGround)
                {
                    combatant.RuntimeAirVelocity -= gravity * combatant.GravityScale * dt;
                    combatant.RuntimeAirHeight += combatant.RuntimeAirVelocity * dt;
                }
                if (combatant.RuntimeAirHeight <= 0.0f)
                {
                    combatant.RuntimeAirHeight = 0.0f;
                    combatant.RuntimeAirVelocity = 0.0f;
                    combatant.RuntimeOnGround = true;
                }
                SideCombatHitResolutionService::SetCombatState(combatant, SideCombatState::Dead);
                SideCombatVisualService::UpdateCombatantVisual(scene, { e, scene }, level, tuning, dt);
                continue;
            }

            float gravity = GravityDefault;
            if (registry.all_of<SidePlayerControllerComponent>(e))
                gravity = registry.get<SidePlayerControllerComponent>(e).Gravity;

            if (!combatant.RuntimeOnGround)
            {
                combatant.RuntimeAirVelocity -= gravity * combatant.GravityScale * dt;
                combatant.RuntimeAirHeight += combatant.RuntimeAirVelocity * dt;
            }

            if (combatant.RuntimeAirHeight <= 0.0f)
            {
                combatant.RuntimeAirHeight = 0.0f;
                combatant.RuntimeAirVelocity = 0.0f;
                combatant.RuntimeOnGround = true;
                if (!wasOnGround && combatant.Team == (int)SideCombatTeam::Player)
                {
                    SideCombatFeedbackService::PlaySfx(
                        tuning.Feedback.LandSound,
                        tuning.Feedback.LandSoundVolume);
                }
            }
            else
            {
                combatant.RuntimeOnGround = false;
            }

            combatant.RuntimeGroundPosition += combatant.RuntimeVelocity * dt;
            float entityArenaMaxX = arenaMaxX;
            if (registry.all_of<SideEnemyAIComponent>(e) &&
                !registry.get<SideEnemyAIComponent>(e).RuntimeAwake &&
                combatant.Alive)
            {
                entityArenaMaxX = level.ArenaMax.x;
            }
            combatant.RuntimeGroundPosition.x = std::clamp(
                combatant.RuntimeGroundPosition.x,
                level.ArenaMin.x + combatant.CollisionSize.x * 0.5f,
                entityArenaMaxX - combatant.CollisionSize.x * 0.5f);
            combatant.RuntimeGroundPosition.y = std::clamp(
                combatant.RuntimeGroundPosition.y,
                laneMinY + combatant.CollisionSize.y * 0.5f,
                laneMaxY - combatant.CollisionSize.y * 0.5f);
            combatant.RuntimeAirHeight = std::min(combatant.RuntimeAirHeight, std::max(0.0f, level.ArenaMax.y - laneMinY));

            const bool boss = SideCombatHitResolutionService::IsBossEntity(scene, e);
            if (boss)
                combatant.RuntimeProtectionMax = std::max(1.0f, tuning.Protection.BossProtectionMax);

            if (combatant.RuntimeState == SideCombatState::SuperArmor)
            {
                if (combatant.RuntimeProtection > 0.0f)
                {
                    combatant.RuntimeProtection = std::max(
                        0.0f,
                        combatant.RuntimeProtection - tuning.Protection.BossProtectionDecayPerSecond * dt);
                }

                if (combatant.RuntimeProtection <= 0.0f &&
                    combatant.RuntimeStateTimer <= 0.0f &&
                    combatant.RuntimeOnGround)
                {
                    SideCombatHitResolutionService::SetCombatState(combatant, SideCombatState::Recovery, tuning.Protection.GroundResetDelay);
                }
            }
            else if (combatant.RuntimeState == SideCombatState::Recovery ||
                combatant.RuntimeState == SideCombatState::Broken)
            {
                if (combatant.RuntimeStateTimer <= 0.0f)
                {
                    if (combatant.RuntimeHitStun > 0.0f)
                        SideCombatHitResolutionService::SetCombatState(combatant, SideCombatState::HitStun, combatant.RuntimeHitStun);
                    else if (!combatant.RuntimeOnGround)
                        SideCombatHitResolutionService::SetCombatState(combatant, SideCombatState::Launched);
                    else
                        SideCombatHitResolutionService::SetCombatState(combatant, SideCombatState::Normal);
                }
            }
            else if (combatant.RuntimeHitStun > 0.0f)
            {
                combatant.RuntimeState = SideCombatState::HitStun;
                combatant.RuntimeStateTimer = std::max(combatant.RuntimeStateTimer, combatant.RuntimeHitStun);
            }
            else if (!combatant.RuntimeOnGround)
            {
                combatant.RuntimeState = SideCombatState::Launched;
            }
            else
            {
                combatant.RuntimeState = SideCombatState::Normal;
            }

            if (boss &&
                combatant.RuntimeOnGround &&
                combatant.RuntimeState == SideCombatState::Normal &&
                combatant.RuntimeProtection > 0.0f)
            {
                constexpr float NormalBossProtectionDecayScale = 0.18f;
                combatant.RuntimeProtection = std::max(
                    0.0f,
                    combatant.RuntimeProtection - tuning.Protection.BossProtectionDecayPerSecond * NormalBossProtectionDecayScale * dt);
            }

            SideCombatVisualService::UpdateCombatantVisual(scene, { e, scene }, level, tuning, dt);
        }
    }

} // namespace Wheatear::SideCombatPhysicsService
