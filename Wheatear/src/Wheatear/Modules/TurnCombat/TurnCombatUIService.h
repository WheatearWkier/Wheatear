#pragma once

#include "TurnCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TurnCombatUIService {

    WHEATEAR_API void UpdateBattleUI(Scene* scene, TurnCombatLevelComponent& level);

} // namespace Wheatear::TurnCombatUIService
