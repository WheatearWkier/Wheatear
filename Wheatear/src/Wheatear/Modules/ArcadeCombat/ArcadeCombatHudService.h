#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatHudService {

    WHEATEAR_API const char* WeaponName(ArcadeWeaponType weapon);
    WHEATEAR_API void UpdateHUD(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss);

} // namespace Wheatear::ArcadeCombatHudService
