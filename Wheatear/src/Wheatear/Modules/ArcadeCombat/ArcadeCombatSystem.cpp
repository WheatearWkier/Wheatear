#include "wtpch.h"
#include "ArcadeCombatSystem.h"

#include "ArcadeCombatBossService.h"
#include "ArcadeCombatHudService.h"
#include "ArcadeCombatLifecycleService.h"
#include "ArcadeCombatPlayerService.h"
#include "ArcadeCombatPresentationService.h"
#include "ArcadeCombatProjectileService.h"
#include "ArcadeCombatOutcomeService.h"
#include "ArcadeCombatTuningService.h"
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

            input.AttackHeld = InputBindingService::IsActionDown("arcade.attack")
                || ArcadeCombatHudService::GetTouchAttackHeld();

            // On-screen joystick takes over while dragged; its y-axis follows
            // the UI convention (up = -1), so it is flipped to match the world
            // convention (up = +1) used by PlayerInputState.
            const glm::vec2 touch = ArcadeCombatHudService::GetTouchMovement();
            if (touch.x != 0.0f || touch.y != 0.0f)
            {
                input.Movement.x = touch.x;
                input.Movement.y = -touch.y;
            }
            return input;
        }

    } // namespace


    void ArcadeCombatSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        ArcadeCombatHudService::ResetTouchControls();
        ArcadeCombatLifecycleService::ResetCombatants(scene);

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<ArcadeCombatLevelComponent>())
        {
            auto& level = registry.get<ArcadeCombatLevelComponent>(e);

            // Data-driven global tuning overrides the scene-authored flow /
            // boss / player values (hot-reloaded; component fields remain the
            // per-scene fallback).
            const auto& tuning = ArcadeCombatTuningService::GetTuning(level);
            ArcadeCombatTuningService::ApplyLevelTuning(tuning, level);
            ArcadeCombatTuningService::ApplyBossTuning(tuning, scene, level);
            ArcadeCombatTuningService::ApplyPlayerTuning(tuning, scene, level);

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
        const auto input = SamplePlayerInput();
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

            if (InputBindingService::IsActionPressed("game.pause") && !level.RuntimeVictory && !level.RuntimeDefeat)
                level.RuntimePaused = !level.RuntimePaused;

            ArcadeCombatPlayerService::UpdateWeaponSelection(player, input);

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

            ArcadeCombatHudService::UpdateHUD(scene, level, player, boss, dt);
        }
    }

} // namespace Wheatear
