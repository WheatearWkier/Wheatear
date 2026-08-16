#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {
    class Scene;
}

namespace Wheatear::SideCombatHudPreset {

    // Captures the on-scene UI widget positions/sizes back into the level's
    // layout fields at runtime start, keeping the component fields in sync
    // with what the canvas editor actually placed.
    WHEATEAR_API int CaptureSceneLayout(SideCombatLevelComponent& level,
        Scene* scene);

} // namespace Wheatear::SideCombatHudPreset
