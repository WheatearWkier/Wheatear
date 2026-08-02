#pragma once

#include "TurnCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TurnCombatCommandService {

    WHEATEAR_API void ProcessCommand(Scene* scene,
        TurnCombatLevelComponent& level,
        const std::string& command);

} // namespace Wheatear::TurnCombatCommandService
