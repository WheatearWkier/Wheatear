#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatLifecycleService {

    WHEATEAR_API void ResetLevelRuntime(Scene* scene, ArcadeCombatLevelComponent& level);
    WHEATEAR_API void ResetCombatants(Scene* scene);
    WHEATEAR_API void ResetBossPresentation(Entity boss);

} // namespace Wheatear::ArcadeCombatLifecycleService
