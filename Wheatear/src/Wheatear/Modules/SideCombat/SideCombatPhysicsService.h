#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatPhysicsService {

    WHEATEAR_API void UpdateCombatants(Scene* scene,
        const SideCombatLevelComponent& level,
        float dt);

} // namespace Wheatear::SideCombatPhysicsService
