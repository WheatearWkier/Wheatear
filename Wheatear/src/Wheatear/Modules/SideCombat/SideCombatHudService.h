#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatHudService {

    WHEATEAR_API void UpdateUI(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        Entity boss);

} // namespace Wheatear::SideCombatHudService
