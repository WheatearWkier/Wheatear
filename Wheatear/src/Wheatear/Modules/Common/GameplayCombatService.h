#pragma once

#include "Wheatear/Core/Core.h"

namespace Wheatear::GameplayCombatService {

    WHEATEAR_API float DamageWithDefense(float rawDamage,
        float defense,
        float minDamage = 1.0f,
        float defenseBase = 100.0f);

    WHEATEAR_API float ApplyDamage(float& health, float damage);
    WHEATEAR_API float ApplyHealing(float& health, float maxHealth, float amount);
    WHEATEAR_API bool IsAlive(float health);

} // namespace Wheatear::GameplayCombatService
