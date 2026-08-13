#include "wtpch.h"
#include "GameplayCombatService.h"

#include <algorithm>

namespace Wheatear::GameplayCombatService {

    float DamageWithDefense(float rawDamage, float defense, float minDamage, float defenseBase)
    {
        const float safeRaw = std::max(0.0f, rawDamage);
        const float safeDefense = std::max(0.0f, defense);
        const float safeBase = std::max(1.0f, defenseBase);
        const float defenseFactor = safeBase / (safeBase + safeDefense);
        return std::max(std::max(0.0f, minDamage), safeRaw * defenseFactor);
    }

    float ApplyDamage(float& health, float damage)
    {
        const float applied = std::min(std::max(0.0f, health), std::max(0.0f, damage));
        health = std::max(0.0f, health - applied);
        return applied;
    }

    float ApplyHealing(float& health, float maxHealth, float amount)
    {
        const float before = std::clamp(health, 0.0f, std::max(0.0f, maxHealth));
        health = std::min(std::max(0.0f, maxHealth), before + std::max(0.0f, amount));
        return health - before;
    }

    bool IsAlive(float health)
    {
        return health > 0.0f;
    }

} // namespace Wheatear::GameplayCombatService
