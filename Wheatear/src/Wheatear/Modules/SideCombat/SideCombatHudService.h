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

    // Current on-screen joystick input direction, quantized to the 8-way
    // grid ({-1,0,1}^2, non-zero while the player drags the stick; y follows
    // the UI convention, i.e. up = -1). {0,0} when the stick is untouched,
    // so callers can fall back to keyboard input.
    WHEATEAR_API glm::vec2 GetJoystickInputDirection();

} // namespace Wheatear::SideCombatHudService
