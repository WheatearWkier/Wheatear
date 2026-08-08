#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    class SideCombatSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;

    private:
        void ResetInputState();

    private:
        bool m_PreviousPausePressed = false;
        bool m_PreviousJumpPressed = false;
        bool m_PreviousBasicPressed = false;
        bool m_PreviousLauncherPressed = false;
        bool m_PreviousMagicPressed = false;
        bool m_PreviousSupportPressed = false;
        bool m_PreviousDashPressed = false;
        bool m_PreviousBreakLimitPressed = false;
        bool m_PreviousItem1Pressed = false;
        bool m_PreviousItem2Pressed = false;
        bool m_PreviousItem3Pressed = false;
    };

} // namespace Wheatear
