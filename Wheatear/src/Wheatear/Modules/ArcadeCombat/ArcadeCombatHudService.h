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

    // On-screen touch controls (joystick / attack / weapon buttons). Movement
    // follows the UI convention (up = -1); {0,0} while untouched so callers
    // fall back to keyboard. GetTouchWeaponPressed returns the weapon type
    // index (0..2) on the frame its button was pressed, otherwise -1.
    WHEATEAR_API glm::vec2 GetTouchMovement();
    WHEATEAR_API bool GetTouchAttackHeld();
    WHEATEAR_API int GetTouchWeaponPressed();
} // namespace Wheatear::ArcadeCombatHudService

