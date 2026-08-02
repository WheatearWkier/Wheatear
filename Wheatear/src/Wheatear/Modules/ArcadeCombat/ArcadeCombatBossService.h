#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatBossService {

    WHEATEAR_API void UpdateBoss(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity boss,
        Entity player,
        float dt);

} // namespace Wheatear::ArcadeCombatBossService
