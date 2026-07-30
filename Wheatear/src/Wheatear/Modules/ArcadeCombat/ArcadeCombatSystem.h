#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    ///
    class ArcadeCombatSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;

    private:
        void ResetInputState();

    private:
        bool m_PreviousPausePressed = false;
        bool m_PreviousWeapon1Pressed = false;
        bool m_PreviousWeapon2Pressed = false;
        bool m_PreviousWeapon3Pressed = false;
        bool m_PreviousAttackPressed = false;
    };

} // namespace Wheatear
