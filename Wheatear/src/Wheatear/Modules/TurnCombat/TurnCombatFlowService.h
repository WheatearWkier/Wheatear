#pragma once

#include "TurnCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TurnCombatFlowService {

    WHEATEAR_API void ResetLevel(Scene* scene, TurnCombatLevelComponent& level);
    WHEATEAR_API void BeginNextTurn(Scene* scene, TurnCombatLevelComponent& level);
    WHEATEAR_API void UpdateLevel(Scene* scene, TurnCombatLevelComponent& level, float dt);

} // namespace Wheatear::TurnCombatFlowService
