#include "wtpch.h"
#include "TacticalCombatFlowService.h"

#include "TacticalCombatAIService.h"
#include "TacticalCombatActionService.h"
#include "TacticalCombatBoardService.h"
#include "TacticalCombatSkillService.h"
#include "TacticalCombatUIService.h"
#include "TacticalCombatVisualService.h"
#include "Wheatear/Modules/Common/GameplayFlowService.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>

namespace Wheatear::TacticalCombatFlowService {

    void ResetLevel(Scene* scene, TacticalCombatLevelComponent& level)
    {
        level.RuntimeElapsed = 0.0f;
        level.RuntimeFadeAlpha = 1.0f;
        level.RuntimePhase = TacticalCombatPhase::Intro;
        level.RuntimeActionReturnPhase = TacticalCombatPhase::PlayerTurn;
        level.RuntimeRound = 1;
        level.RuntimeEnemyCursor = 0;
        level.RuntimeSelectedUnit = 0;
        level.RuntimeCommandMenuPage = "root";
        level.RuntimeSelectedSkillId.clear();
        level.RuntimeActionActor = 0;
        level.RuntimeActionTarget = 0;
        level.RuntimeActionSkillId.clear();
        level.RuntimeMessage = "战棋演示开始。";
        level.RuntimeRequestedCommand.clear();
        level.RuntimeIntroTimer = 0.0f;
        level.RuntimeActionTimer = 0.0f;
        level.RuntimeEnemyStepTimer = 0.0f;
        level.RuntimeResultTimer = 0.0f;
        level.RuntimeInitialized = true;
        level.RuntimeActionApplied = false;
        level.RuntimeResultCommandIssued = false;

        for (Entity unit : TacticalCombatBoardService::CollectUnits(scene))
        {
            auto& tactical = unit.GetComponent<TacticalUnitComponent>();
            tactical.Health = std::clamp(
                tactical.Health <= 0.0f ? tactical.MaxHealth : tactical.Health,
                0.0f,
                tactical.MaxHealth);
            tactical.RuntimeAlive = tactical.Health > 0.0f;
            tactical.RuntimeHasActed = false;
            tactical.RuntimeMoved = false;
            tactical.RuntimeGuarding = false;
            tactical.RuntimeHitFlashTimer = 0.0f;
            tactical.RuntimeStatusEffects.clear();
            tactical.RuntimeVisualClip.clear();
            tactical.RuntimeVisualTimer = 0.0f;
        }

        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, 1.0f);
        UIRuntimeTools::SetImageAlpha(scene, level.ActionEffectEntityName, 0.0f);
    }

    void UpdateLevel(Scene* scene, TacticalCombatLevelComponent& level, float dt)
    {
        if (!level.RuntimeInitialized)
            ResetLevel(scene, level);

        level.RuntimeElapsed += dt;
        if (level.StartFadeDuration <= 0.0f)
            level.RuntimeFadeAlpha = 0.0f;
        else
            level.RuntimeFadeAlpha = std::max(
                0.0f,
                level.RuntimeFadeAlpha - dt / level.StartFadeDuration);
        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, level.RuntimeFadeAlpha);

        for (Entity unit : TacticalCombatBoardService::CollectUnits(scene))
        {
            auto& tactical = unit.GetComponent<TacticalUnitComponent>();
            if (tactical.RuntimeHitFlashTimer > 0.0f)
                tactical.RuntimeHitFlashTimer = std::max(0.0f, tactical.RuntimeHitFlashTimer - dt);
            TacticalCombatVisualService::UpdateUnitVisual(scene, level, unit, tactical, dt);
        }

        switch (level.RuntimePhase)
        {
        case TacticalCombatPhase::Intro:
            level.RuntimeIntroTimer += dt;
            if (level.RuntimeIntroTimer >= level.IntroDuration)
            {
                level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
                level.RuntimeMessage = "我方回合。请选择单位。";
            }
            break;

        case TacticalCombatPhase::Acting:
            level.RuntimeActionTimer += dt;
            TacticalCombatVisualService::UpdateActionEffect(scene, level);
            if (!level.RuntimeActionApplied
                && level.RuntimeActionTimer >= level.ActionDuration * 0.45f)
            {
                TacticalCombatActionService::ApplyAction(scene, level);
                level.RuntimeActionApplied = true;
            }
            if (level.RuntimeActionTimer >= level.ActionDuration)
                TacticalCombatActionService::EndAction(scene, level);
            break;

        case TacticalCombatPhase::EnemyTurn:
            level.RuntimeEnemyStepTimer += dt;
            if (level.RuntimeEnemyStepTimer >= level.EnemyStepDuration)
            {
                level.RuntimeEnemyStepTimer = 0.0f;
                TacticalCombatAIService::ProcessEnemyStep(scene, level);
            }
            break;

        case TacticalCombatPhase::Victory:
            level.RuntimeResultTimer += dt;
            GameplayFlowService::TryIssueDelayedCommand(
                level.RuntimeResultTimer,
                level.VictoryReturnDelay,
                level.RuntimeResultCommandIssued,
                level.RuntimeRequestedCommand,
                level.VictorySceneCommand);
            break;

        case TacticalCombatPhase::Defeat:
            level.RuntimeResultTimer += dt;
            GameplayFlowService::TryIssueDelayedCommand(
                level.RuntimeResultTimer,
                level.DefeatReturnDelay,
                level.RuntimeResultCommandIssued,
                level.RuntimeRequestedCommand,
                level.DefeatSceneCommand);
            break;

        case TacticalCombatPhase::PlayerTurn:
        case TacticalCombatPhase::AwaitCommand:
        case TacticalCombatPhase::Targeting:
            break;
        }

        TacticalCombatUIService::UpdateBattleUI(scene, level);
    }

} // namespace Wheatear::TacticalCombatFlowService
