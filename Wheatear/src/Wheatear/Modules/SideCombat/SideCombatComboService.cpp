#include "wtpch.h"
#include "SideCombatComboService.h"

#include "SideCombatEntityReferenceService.h"
#include "SideCombatHitResolutionService.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Scene/Entity.h"

#include <algorithm>

namespace Wheatear::SideCombatComboService {

    void RegisterPlayerHit(SideCombatLevelComponent& level)
    {
        ++level.RuntimeComboCount;
        level.RuntimeBestCombo = std::max(level.RuntimeBestCombo, level.RuntimeComboCount);
        level.RuntimeComboTimer = level.ComboDropDelay;
    }

    void ResetCombo(SideCombatLevelComponent& level)
    {
        level.RuntimeComboCount = 0;
        level.RuntimeComboTimer = 0.0f;
    }

    void UpdateCombo(SideCombatLevelComponent& level, float dt)
    {
        if (level.RuntimeComboTimer <= 0.0f)
            return;

        level.RuntimeComboTimer = std::max(0.0f, level.RuntimeComboTimer - dt);
        if (level.RuntimeComboTimer <= 0.0f)
            level.RuntimeComboCount = 0;
    }

    void ApplyPlayerAirHitReward(Scene* scene,
        SideCombatLevelComponent& level,
        const SideHitboxComponent& hitbox,
        SideCombatantComponent& target)
    {
        if (hitbox.Team != (int)SideCombatTeam::Player)
            return;

        Entity player = SideCombatEntityReferenceService::ResolvePlayer(scene, level);
        if (!player || !player.HasComponent<SideCombatantComponent>())
            return;

        auto& attacker = player.GetComponent<SideCombatantComponent>();
        if (attacker.Alive && !attacker.RuntimeOnGround)
        {
            if (hitbox.AttackerAirImpulse > 0.0f)
                attacker.RuntimeAirVelocity = std::max(attacker.RuntimeAirVelocity, hitbox.AttackerAirImpulse);
            if (hitbox.AttackerAirFallStep > 0.0f)
                attacker.RuntimeAirHeight = std::max(0.05f, attacker.RuntimeAirHeight - hitbox.AttackerAirFallStep);
        }

        if (target.RuntimeAirHeight > 0.0f && hitbox.TargetAirFallStep > 0.0f)
            target.RuntimeAirHeight = std::max(0.05f, target.RuntimeAirHeight - hitbox.TargetAirFallStep);
    }

    void AwardMagicSwordGauge(Scene* scene,
        SideCombatLevelComponent& level,
        const SideHitboxComponent& hitbox,
        const SideCombatantComponent& target)
    {
        if (hitbox.Team != (int)SideCombatTeam::Player ||
            hitbox.AttackKind == SideAttackKind::BreakLimit)
        {
            return;
        }

        Entity player = SideCombatEntityReferenceService::ResolvePlayer(scene, level);
        if (!player || !player.HasComponent<SidePlayerControllerComponent>())
            return;

        const auto& tuning = SideCombatTuningService::GetTuning(level);
        auto& controller = player.GetComponent<SidePlayerControllerComponent>();
        const float gain = SideCombatHitResolutionService::IsControlledAirborne(target)
            ? tuning.AirCombo.GaugeGainAirHit
            : tuning.AirCombo.GaugeGainGroundHit;
        controller.RuntimeMagicSwordGauge = std::clamp(
            controller.RuntimeMagicSwordGauge + gain,
            0.0f,
            controller.RuntimeMagicSwordGaugeMax);
    }

} // namespace Wheatear::SideCombatComboService
