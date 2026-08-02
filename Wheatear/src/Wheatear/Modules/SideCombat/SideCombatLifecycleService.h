#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatLifecycleService {

    WHEATEAR_API void ResetLevelRuntime(Scene* scene, SideCombatLevelComponent& level);
    WHEATEAR_API void ResetCombatants(Scene* scene, SideCombatLevelComponent& level);

} // namespace Wheatear::SideCombatLifecycleService
