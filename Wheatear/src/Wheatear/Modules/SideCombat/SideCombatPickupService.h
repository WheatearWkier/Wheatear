#pragma once

#include "SideCombatComponents.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;
    struct TransformComponent;

} // namespace Wheatear

namespace Wheatear::SideCombatPickupService {

    WHEATEAR_API Entity CreatePickup(Scene* scene,
        const std::string& name,
        const glm::vec3& position,
        const std::string& itemId,
        const std::string& displayName,
        int amount,
        const std::string& texturePath,
        const SideCombatTuningService::SidePickupTuning& tuning);
    WHEATEAR_API void SpawnDeathRewards(Scene* scene,
        const SideCombatLevelComponent& level,
        const TransformComponent& transform,
        const SideEnemyAIComponent* ai);
    WHEATEAR_API void UpdatePickups(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt);

} // namespace Wheatear::SideCombatPickupService
