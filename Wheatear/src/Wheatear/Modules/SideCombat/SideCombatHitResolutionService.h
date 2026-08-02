#pragma once

#include "SideCombatComponents.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Core/Core.h"

#include "entt.hpp"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatHitResolutionService {

    struct HitResolutionResult
    {
        float Damage = 0.0f;
        bool TargetDied = false;
        bool PlayerWasHit = false;
        bool BossProtectionTriggered = false;
    };

    WHEATEAR_API float CalculateDamage(float rawDamage,
        float defense,
        const SideCombatTuningService::SideCombatRuleTuning& rules);
    WHEATEAR_API bool IsBossEntity(Scene* scene, entt::entity entity);
    WHEATEAR_API bool IsControlledAirborne(const SideCombatantComponent& combatant);
    WHEATEAR_API bool CanEnemyAct(const SideCombatantComponent& combatant);
    WHEATEAR_API void SetCombatState(SideCombatantComponent& combatant,
        SideCombatState state,
        float duration = 0.0f);
    WHEATEAR_API void EnterBossProtectionRecovery(SideCombatantComponent& boss,
        const SideCombatTuningService::SideProtectionTuning& protection);
    WHEATEAR_API HitResolutionResult ResolveHit(Scene* scene,
        SideCombatLevelComponent& level,
        const SideCombatTuningService::SideCombatTuning& tuning,
        entt::entity targetEntity,
        SideCombatantComponent& target,
        const SideHitboxComponent& hitbox);

} // namespace Wheatear::SideCombatHitResolutionService
