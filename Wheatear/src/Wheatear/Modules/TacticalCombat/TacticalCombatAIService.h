#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TacticalCombatAIService {

    WHEATEAR_API void ProcessEnemyStep(Scene* scene, TacticalCombatLevelComponent& level);

} // namespace Wheatear::TacticalCombatAIService
