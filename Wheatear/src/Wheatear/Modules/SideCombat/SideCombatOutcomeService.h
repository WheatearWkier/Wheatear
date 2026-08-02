#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatOutcomeService {

    WHEATEAR_API void UpdateDeathsAndVictory(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player);
    WHEATEAR_API void UpdateResultTransition(Scene* scene,
        SideCombatLevelComponent& level,
        float dt);

} // namespace Wheatear::SideCombatOutcomeService
