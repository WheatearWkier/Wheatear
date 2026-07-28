#pragma once

#include "Wheatear/Systems/ISystem.h"

namespace Wheatear {

    /// @brief 轻量 2D 动作战斗系统
    ///
    /// 该系统只消费 Arcade* 组件，不依赖具体游戏脚本。视觉小说、普通 2D
    /// 关卡和播放器运行时都可以通过场景数据复用这套战斗能力。
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
