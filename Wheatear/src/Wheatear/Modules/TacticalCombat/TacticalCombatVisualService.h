#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TacticalCombatVisualService {

    WHEATEAR_API void UpdateUnitVisual(Scene* scene,
        const TacticalCombatLevelComponent& level,
        Entity entity,
        TacticalUnitComponent& unit,
        float dt);
    WHEATEAR_API void UpdateActionEffect(Scene* scene, TacticalCombatLevelComponent& level);
    WHEATEAR_API void HideActionEffect(Scene* scene, TacticalCombatLevelComponent& level);

} // namespace Wheatear::TacticalCombatVisualService
