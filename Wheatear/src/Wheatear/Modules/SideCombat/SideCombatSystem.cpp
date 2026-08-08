#include "wtpch.h"
#include "SideCombatSystem.h"

#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/InputBindingService.h"
#include "Wheatear/Core/KeyCodes.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Modules/SideCombat/SideCombatComboService.h"
#include "Wheatear/Modules/SideCombat/SideCombatEnemyAIService.h"
#include "Wheatear/Modules/SideCombat/SideCombatEntityReferenceService.h"
#include "Wheatear/Modules/SideCombat/SideCombatFeedbackService.h"
#include "Wheatear/Modules/SideCombat/SideCombatHitboxService.h"
#include "Wheatear/Modules/SideCombat/SideCombatHudService.h"
#include "Wheatear/Modules/SideCombat/SideCombatLifecycleService.h"
#include "Wheatear/Modules/SideCombat/SideCombatOutcomeService.h"
#include "Wheatear/Modules/SideCombat/SideCombatPhysicsService.h"
#include "Wheatear/Modules/SideCombat/SideCombatPickupService.h"
#include "Wheatear/Modules/SideCombat/SideCombatPlayerService.h"
#include "Wheatear/Modules/SideCombat/SideCombatTuningService.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Wheatear {


    void SideCombatSystem::ResetInputState()
    {
        m_PreviousPausePressed = false;
        m_PreviousJumpPressed = false;
        m_PreviousBasicPressed = false;
        m_PreviousLauncherPressed = false;
        m_PreviousMagicPressed = false;
        m_PreviousSupportPressed = false;
        m_PreviousDashPressed = false;
        m_PreviousBreakLimitPressed = false;
        m_PreviousItem1Pressed = false;
        m_PreviousItem2Pressed = false;
        m_PreviousItem3Pressed = false;
    }

    void SideCombatSystem::OnRuntimeStart(Scene* scene)
    {
        ResetInputState();
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<SideCombatLevelComponent>())
        {
            auto& level = registry.get<SideCombatLevelComponent>(e);
            if (!level.PlayOnStart)
                continue;

            SideCombatLifecycleService::ResetLevelRuntime(scene, level);
            SideCombatLifecycleService::ResetCombatants(scene, level);
        }
    }

    void SideCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        const float dt = std::min(0.05f, ts.GetSeconds());
        auto& registry = scene->GetRegistry();

        const bool pausePressed = InputBindingService::IsActionDown("game.pause");
        const bool downHeld = InputBindingService::IsActionDown("move.down");
        const bool jumpPressed = InputBindingService::IsActionDown("side.jump");
        bool basicPressed = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT) || InputBindingService::IsActionDown("side.basic");
        bool launcherPressed = downHeld && basicPressed;
        bool magicPressed = InputBindingService::IsActionDown("side.magic");
        bool supportPressed = InputBindingService::IsActionDown("side.support");
        bool dashPressed = InputBindingService::IsActionDown("side.dash");
        bool breakLimitPressed = InputBindingService::IsActionDown("side.break_limit");
        bool item1Pressed = InputBindingService::IsActionDown("side.item1") || Input::IsKeyPressed(WT_KEY_1);
        bool item2Pressed = InputBindingService::IsActionDown("side.item2") || Input::IsKeyPressed(WT_KEY_2);
        bool item3Pressed = InputBindingService::IsActionDown("side.item3") || Input::IsKeyPressed(WT_KEY_3);
        for (const std::string& command : CommandBus::DrainGameplayCommands("side:"))
        {
            if (command == "side:item:1")
                item1Pressed = true;
            else if (command == "side:item:2")
                item2Pressed = true;
            else if (command == "side:item:3")
                item3Pressed = true;
            else if (command == "side:basic")
                basicPressed = true;
            else if (command == "side:launcher")
                launcherPressed = true;
            else if (command == "side:magic")
                magicPressed = true;
            else if (command == "side:support")
                supportPressed = true;
            else if (command == "side:dash")
                dashPressed = true;
            else if (command == "side:break_limit")
                breakLimitPressed = true;
        }
        float horizontal = 0.0f;
        if (InputBindingService::IsActionDown("move.left"))
            horizontal -= 1.0f;
        if (InputBindingService::IsActionDown("move.right"))
            horizontal += 1.0f;

        float lane = 0.0f;
        if (InputBindingService::IsActionDown("move.down"))
            lane -= 1.0f;
        if (InputBindingService::IsActionDown("move.up"))
            lane += 1.0f;

        const SideCombatPlayerService::PlayerInputState input{
            jumpPressed,
            basicPressed,
            launcherPressed,
            magicPressed,
            supportPressed,
            dashPressed,
            breakLimitPressed,
            item1Pressed,
            item2Pressed,
            item3Pressed,
            horizontal,
            lane
        };
        const SideCombatPlayerService::PlayerInputState previousInput{
            m_PreviousJumpPressed,
            m_PreviousBasicPressed,
            m_PreviousLauncherPressed,
            m_PreviousMagicPressed,
            m_PreviousSupportPressed,
            m_PreviousDashPressed,
            m_PreviousBreakLimitPressed,
            m_PreviousItem1Pressed,
            m_PreviousItem2Pressed,
            m_PreviousItem3Pressed,
            0.0f,
            0.0f
        };

        for (auto levelEntity : registry.view<SideCombatLevelComponent>())
        {
            auto& level = registry.get<SideCombatLevelComponent>(levelEntity);
            if (!level.PlayOnStart)
                continue;

            level.RuntimeElapsed += dt;
            SideCombatFeedbackService::UpdateStartFade(scene, level, dt);
            SideCombatFeedbackService::UpdateCameraFeedback(scene, level, dt);

            if (pausePressed && !m_PreviousPausePressed)
                level.RuntimePaused = !level.RuntimePaused;

            Entity player = SideCombatEntityReferenceService::ResolvePlayer(scene, level);
            Entity boss = SideCombatEntityReferenceService::ResolveBoss(scene, level);

            if (!level.RuntimePaused)
            {
                const auto& tuning = SideCombatTuningService::GetTuning(level);
                float simulationDt = dt;
                float simulationTimeScale = 1.0f;
                if (level.RuntimeHitPauseTimer > 0.0f)
                {
                    level.RuntimeHitPauseTimer = std::max(0.0f, level.RuntimeHitPauseTimer - dt);
                    simulationTimeScale = std::min(
                        simulationTimeScale,
                        std::clamp(tuning.Feedback.HitPauseTimeScale, 0.0f, 1.0f));
                }
                if (level.RuntimeCinematicTimer > 0.0f)
                {
                    simulationTimeScale = std::min(
                        simulationTimeScale,
                        std::clamp(level.RuntimeCinematicTimeScale, 0.02f, 1.0f));
                }
                simulationDt = dt * simulationTimeScale;

                SideCombatComboService::UpdateCombo(level, simulationDt);
                SideCombatPlayerService::UpdatePlayer(scene, level, player, simulationDt, input, previousInput);
                SideCombatEnemyAIService::UpdateEnemies(scene, level, player, simulationDt);
                SideCombatHitboxService::UpdateHitboxes(scene, level, simulationDt);
                SideCombatPhysicsService::UpdateCombatants(scene, level, simulationDt);
                SideCombatOutcomeService::UpdateDeathsAndVictory(scene, level, player);
                SideCombatPickupService::UpdatePickups(scene, level, player, simulationDt);
                SideCombatOutcomeService::UpdateResultTransition(scene, level, dt);
            }

            SideCombatHudService::UpdateUI(scene, level, player, boss);
        }

        m_PreviousPausePressed = pausePressed;
        m_PreviousJumpPressed = jumpPressed;
        m_PreviousBasicPressed = basicPressed;
        m_PreviousLauncherPressed = launcherPressed;
        m_PreviousMagicPressed = magicPressed;
        m_PreviousSupportPressed = supportPressed;
        m_PreviousDashPressed = dashPressed;
        m_PreviousBreakLimitPressed = breakLimitPressed;
        m_PreviousItem1Pressed = item1Pressed;
        m_PreviousItem2Pressed = item2Pressed;
        m_PreviousItem3Pressed = item3Pressed;
    }

} // namespace Wheatear
