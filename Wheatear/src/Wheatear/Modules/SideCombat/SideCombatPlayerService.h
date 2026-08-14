#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatPlayerService {

    // Per-frame player input. Edge (pressed-this-frame) semantics come from
    // InputBindingService::IsActionPressed; this struct only carries the
    // analog movement axes.
    struct PlayerInputState
    {
        float Horizontal = 0.0f;
        float Lane = 0.0f;
    };

    WHEATEAR_API void UpdatePlayer(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        float dt,
        const PlayerInputState& input);

} // namespace Wheatear::SideCombatPlayerService
