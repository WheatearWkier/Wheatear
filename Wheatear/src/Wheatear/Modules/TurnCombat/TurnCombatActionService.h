#pragma once

#include "TurnCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TurnCombatActionService {

    WHEATEAR_API void BeginAction(Scene* scene,
        TurnCombatLevelComponent& level,
        Entity actor,
        const std::string& skillId,
        Entity target);
    WHEATEAR_API void ApplySkill(Scene* scene, TurnCombatLevelComponent& level);

} // namespace Wheatear::TurnCombatActionService
