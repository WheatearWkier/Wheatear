#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatOutcomeService {

    WHEATEAR_API void UpdateResultTransition(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss,
        float dt);

} // namespace Wheatear::ArcadeCombatOutcomeService
