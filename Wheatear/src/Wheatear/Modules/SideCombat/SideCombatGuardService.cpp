#include "wtpch.h"
#include "SideCombatGuardService.h"

#include <algorithm>

namespace Wheatear::SideCombatGuardService {

    void OpenGuardWindow(SideCombatantComponent& combatant,
        float duration,
        float parryWindow,
        float damageMultiplier)
    {
        if (!combatant.Alive)
            return;

        combatant.RuntimeGuardActive = duration > 0.0f;
        combatant.RuntimeGuardTimer = std::max(0.0f, duration);
        combatant.RuntimeParryTimer = std::clamp(
            parryWindow,
            0.0f,
            combatant.RuntimeGuardTimer);
        combatant.RuntimeGuardDamageMultiplier = std::clamp(
            damageMultiplier,
            0.0f,
            1.0f);
    }

    void CloseGuardWindow(SideCombatantComponent& combatant)
    {
        combatant.RuntimeGuardActive = false;
        combatant.RuntimeGuardTimer = 0.0f;
        combatant.RuntimeParryTimer = 0.0f;
    }

    void UpdateGuardWindow(SideCombatantComponent& combatant,
        float deltaSeconds)
    {
        if (!combatant.RuntimeGuardActive)
            return;

        const float dt = std::max(0.0f, deltaSeconds);
        combatant.RuntimeGuardTimer = std::max(
            0.0f,
            combatant.RuntimeGuardTimer - dt);
        combatant.RuntimeParryTimer = std::max(
            0.0f,
            combatant.RuntimeParryTimer - dt);
        if (combatant.RuntimeGuardTimer <= 0.0f)
            CloseGuardWindow(combatant);
    }

    GuardDecision ResolveIncomingHit(SideCombatantComponent& combatant,
        const SideHitboxComponent& hitbox)
    {
        GuardDecision decision;
        if (!combatant.RuntimeGuardActive ||
            combatant.RuntimeGuardTimer <= 0.0f ||
            hitbox.Team == combatant.Team ||
            hitbox.Team == (int)SideCombatTeam::Neutral)
        {
            return decision;
        }

        decision.DamageMultiplier = std::clamp(
            combatant.RuntimeGuardDamageMultiplier,
            0.0f,
            1.0f);
        decision.Result = combatant.RuntimeParryTimer > 0.0f
            ? GuardResult::Parried
            : GuardResult::Blocked;

        // A guard window is a one-hit defensive action. The behavior graph
        // receives the outcome event and decides what happens next.
        CloseGuardWindow(combatant);
        return decision;
    }

} // namespace Wheatear::SideCombatGuardService
