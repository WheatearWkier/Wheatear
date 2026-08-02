#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TacticalCombatFlowService {

    WHEATEAR_API void ResetLevel(Scene* scene, TacticalCombatLevelComponent& level);
    WHEATEAR_API void UpdateLevel(Scene* scene, TacticalCombatLevelComponent& level, float dt);

} // namespace Wheatear::TacticalCombatFlowService
