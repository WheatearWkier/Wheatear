#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear::SideCombatGuardService {

    enum class GuardResult
    {
        None = 0,
        Blocked,
        Parried
    };

    struct GuardDecision
    {
        GuardResult Result = GuardResult::None;
        float DamageMultiplier = 1.0f;

        bool IsBlocked() const { return Result == GuardResult::Blocked; }
        bool IsParried() const { return Result == GuardResult::Parried; }
    };

    WHEATEAR_API void OpenGuardWindow(SideCombatantComponent& combatant,
        float duration,
        float parryWindow,
        float damageMultiplier = 0.25f);
    WHEATEAR_API void CloseGuardWindow(SideCombatantComponent& combatant);
    WHEATEAR_API void UpdateGuardWindow(SideCombatantComponent& combatant,
        float deltaSeconds);
    WHEATEAR_API GuardDecision ResolveIncomingHit(
        SideCombatantComponent& combatant,
        const SideHitboxComponent& hitbox);

} // namespace Wheatear::SideCombatGuardService
