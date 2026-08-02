#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TacticalCombatUIService {

    WHEATEAR_API void UpdateTiles(Scene* scene, TacticalCombatLevelComponent& level);
    WHEATEAR_API void UpdateStatusUI(Scene* scene, TacticalCombatLevelComponent& level);
    WHEATEAR_API void UpdateCommandUI(Scene* scene, TacticalCombatLevelComponent& level);
    WHEATEAR_API void UpdateBattleUI(Scene* scene, TacticalCombatLevelComponent& level);

} // namespace Wheatear::TacticalCombatUIService
