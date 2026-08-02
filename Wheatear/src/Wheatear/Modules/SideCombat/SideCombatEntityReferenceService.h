#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

namespace Wheatear {

    class Entity;
    class Scene;

} // namespace Wheatear

namespace Wheatear::SideCombatEntityReferenceService {

    WHEATEAR_API void RefreshLevelReferences(Scene* scene, SideCombatLevelComponent& level);
    WHEATEAR_API Entity ResolvePlayer(Scene* scene, SideCombatLevelComponent& level);
    WHEATEAR_API Entity ResolveBoss(Scene* scene, SideCombatLevelComponent& level);

} // namespace Wheatear::SideCombatEntityReferenceService
