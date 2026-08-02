#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatComboService {

    WHEATEAR_API void RegisterPlayerHit(SideCombatLevelComponent& level);
    WHEATEAR_API void ResetCombo(SideCombatLevelComponent& level);
    WHEATEAR_API void UpdateCombo(SideCombatLevelComponent& level, float dt);
    WHEATEAR_API void ApplyPlayerAirHitReward(Scene* scene,
        SideCombatLevelComponent& level,
        const SideHitboxComponent& hitbox,
        SideCombatantComponent& target);
    WHEATEAR_API void AwardMagicSwordGauge(Scene* scene,
        SideCombatLevelComponent& level,
        const SideHitboxComponent& hitbox,
        const SideCombatantComponent& target);

} // namespace Wheatear::SideCombatComboService
