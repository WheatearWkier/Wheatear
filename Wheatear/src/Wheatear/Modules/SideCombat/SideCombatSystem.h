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
        bool m_PreviousBreakLimitPressed = false;
    };

} // namespace Wheatear
