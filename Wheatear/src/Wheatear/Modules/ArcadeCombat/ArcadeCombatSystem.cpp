#include "wtpch.h"
#include "ArcadeCombatSystem.h"

#include "ArcadeCombatBossService.h"
#include "ArcadeCombatHudService.h"
#include "ArcadeCombatLifecycleService.h"
#include "ArcadeCombatPlayerService.h"
#include "ArcadeCombatPresentationService.h"
#include "ArcadeCombatProjectileService.h"
#include "ArcadeCombatOutcomeService.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/InputBindingService.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"

namespace Wheatear {

    namespace {

        static ArcadeCombatPlayerService::PlayerInputState SamplePlayerInput()
        {
            ArcadeCombatPlayerService::PlayerInputState input;
            if (InputBindingService::IsActionDown("move.left"))
                input.Movement.x -= 1.0f;
            if (InputBindingService::IsActionDown("move.right"))
                input.Movement.x += 1.0f;
            if (InputBindingService::IsActionDown("move.up"))
                input.Movement.y += 1.0f;
            if (InputBindingService::IsActionDown("move.down"))
                input.Movement.y -= 1.0f;

            input.AttackHeld =
                Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT) ||
                InputBindingService::IsActionDown("arcade.attack");
            input.Weapon1Pressed = InputBindingService::IsActionDown("arcade.weapon1");
            input.Weapon2Pressed = InputBindingService::IsActionDown("arcade.weapon2");
            input.Weapon3Pressed = InputBindingService::IsActionDown("arcade.weapon3");
            return input;
        }

    } // namespace

    void ArcadeCombatSystem::ResetInputState()
    {
        m_PreviousPausePressed = false;
        m_PreviousWeapon1Pressed = false;
        m_PreviousWeapon2Pressed = false;
        m_PreviousWeapon3Pressed = false;
        m_PreviousAttackPressed = false;
    }

    void ArcadeCombatSystem::OnRuntimeStart(Scene* scene)
    {
        ResetInputState();
        if (!scene)
            return;

        ArcadeCombatLifecycleService::ResetCombatants(scene);

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<ArcadeCombatLevelComponent>())
        {
            auto& level = registry.get<ArcadeCombatLevelComponent>(e);
            ArcadeCombatLifecycleService::ResetLevelRuntime(scene, level);
            Entity boss = SceneQueries::FindEntityByName(scene, level.BossEntityName);
            ArcadeCombatLifecycleService::ResetBossPresentation(boss);
            if (boss && boss.HasComponent<ArcadeBossComponent>() &&
                boss.GetComponent<ArcadeBossComponent>().Active)
            {
                level.RuntimeBossIntroStarted = true;
                level.RuntimeBossIntroFinished = true;
            }
        }
    }

    void ArcadeCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        const float dt = ts.GetSeconds();
        const bool pausePressed = InputBindingService::IsActionDown("game.pause");
        const auto input = SamplePlayerInput();
        const ArcadeCombatPlayerService::PlayerInputState previousInput{
            {},
            m_PreviousAttackPressed,
            m_PreviousWeapon1Pressed,
            m_PreviousWeapon2Pressed,
            m_PreviousWeapon3Pressed
        };

        auto& registry = scene->GetRegistry();
        for (auto levelEntity : registry.view<ArcadeCombatLevelComponent>())
        {
            auto& level = registry.get<ArcadeCombatLevelComponent>(levelEntity);
            if (!level.PlayOnStart)
                continue;

            level.RuntimeElapsed += dt;
            ArcadeCombatPresentationService::UpdateStartFade(scene, level, dt);
            ArcadeCombatPresentationService::UpdateTriggerGlow(scene, level);

            Entity player = SceneQueries::FindEntityByName(scene, level.PlayerEntityName);
            Entity boss = SceneQueries::FindEntityByName(scene, level.BossEntityName);

            if (pausePressed && !m_PreviousPausePressed && !level.RuntimeVictory && !level.RuntimeDefeat)
                level.RuntimePaused = !level.RuntimePaused;

            ArcadeCombatPlayerService::UpdateWeaponSelection(player, input, previousInput);

            if (!level.RuntimePaused)
            {
                if (!level.RuntimeBossIntroStarted)
                    ArcadeCombatPresentationService::UpdateIntroTrigger(scene, level, player, boss);

                if (level.RuntimeBossIntroStarted && !level.RuntimeBossIntroFinished)
                    ArcadeCombatPresentationService::UpdateBossIntro(scene, level, player, boss, dt);

                if (level.RuntimeBossIntroFinished)
                    ArcadeCombatBossService::UpdateBoss(scene, level, boss, player, dt);

                if (!level.RuntimeVictory && !level.RuntimeDefeat)
                {
                    ArcadeCombatPlayerService::UpdatePlayer(scene, level, player, boss, dt, input);
                    ArcadeCombatProjectileService::UpdateProjectiles(scene, dt);
                }

                ArcadeCombatOutcomeService::UpdateResultTransition(scene, level, player, boss, dt);
            }

            ArcadeCombatHudService::UpdateHUD(scene, level, player, boss);
        }

        m_PreviousPausePressed = pausePressed;
        m_PreviousWeapon1Pressed = input.Weapon1Pressed;
        m_PreviousWeapon2Pressed = input.Weapon2Pressed;
        m_PreviousWeapon3Pressed = input.Weapon3Pressed;
        m_PreviousAttackPressed = input.AttackHeld;
    }

} // namespace Wheatear
