#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatEnemyAIService {

    WHEATEAR_API void UpdateEnemies(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt);

} // namespace Wheatear::SideCombatEnemyAIService
