#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::ArcadeCombatPlayerService {

    struct PlayerInputState
    {
        glm::vec2 Movement = { 0.0f, 0.0f };
        bool AttackHeld = false;
        bool Weapon1Pressed = false;
        bool Weapon2Pressed = false;
        bool Weapon3Pressed = false;
    };

    WHEATEAR_API void UpdateWeaponSelection(Entity player,
        const PlayerInputState& input,
        const PlayerInputState& previousInput);
    WHEATEAR_API void UpdatePlayer(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss,
        float dt,
        const PlayerInputState& input);

} // namespace Wheatear::ArcadeCombatPlayerService
