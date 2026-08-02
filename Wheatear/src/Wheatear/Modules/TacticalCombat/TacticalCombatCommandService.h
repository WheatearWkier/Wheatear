#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TacticalCombatCommandService {

    WHEATEAR_API void ProcessCommand(Scene* scene,
        TacticalCombatLevelComponent& level,
        const std::string& command);

} // namespace Wheatear::TacticalCombatCommandService
