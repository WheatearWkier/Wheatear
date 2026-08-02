#pragma once

#include "TurnCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::TurnCombatVisualService {

    WHEATEAR_API void CacheVisuals(Entity entity);
    WHEATEAR_API void RestoreVisual(Entity entity);
    WHEATEAR_API void MarkHit(Entity entity);
    WHEATEAR_API void UpdateVisuals(Scene* scene, TurnCombatLevelComponent& level, float dt);

} // namespace Wheatear::TurnCombatVisualService
