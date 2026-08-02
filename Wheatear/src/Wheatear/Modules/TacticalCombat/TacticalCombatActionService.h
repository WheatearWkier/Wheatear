#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TacticalCombatActionService {

    WHEATEAR_API void BeginAction(Scene* scene,
        TacticalCombatLevelComponent& level,
        Entity actor,
        const std::string& skillId,
        Entity target,
        TacticalCombatPhase returnPhase);
    WHEATEAR_API void FinishSelectedPlayerAction(Scene* scene, TacticalCombatLevelComponent& level);
    WHEATEAR_API void ApplyAction(Scene* scene, TacticalCombatLevelComponent& level);
    WHEATEAR_API void EndAction(Scene* scene, TacticalCombatLevelComponent& level);

} // namespace Wheatear::TacticalCombatActionService
