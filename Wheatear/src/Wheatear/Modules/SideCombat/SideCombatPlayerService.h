#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatPlayerService {

    struct PlayerInputState
    {
        bool JumpPressed = false;
        bool BasicPressed = false;
        bool LauncherPressed = false;
        bool MagicPressed = false;
        bool SupportPressed = false;
        bool DashPressed = false;
        bool BreakLimitPressed = false;
        bool Item1Pressed = false;
        bool Item2Pressed = false;
        bool Item3Pressed = false;
        float Horizontal = 0.0f;
        float Lane = 0.0f;
    };

    WHEATEAR_API void UpdatePlayer(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt,
        const PlayerInputState& input,
        const PlayerInputState& previousInput);

} // namespace Wheatear::SideCombatPlayerService
