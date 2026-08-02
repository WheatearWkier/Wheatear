#pragma once

#include "SideCombatComponents.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatVisualService {

    WHEATEAR_API void UpdateCombatantVisual(Scene* scene,
        Entity entity,
        const SideCombatLevelComponent& level,
        const SideCombatTuningService::SideCombatTuning& tuning,
        float dt);

} // namespace Wheatear::SideCombatVisualService
