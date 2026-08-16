#include "wtpch.h"
#include "TurnCombatFlowService.h"

#include "TurnCombatActionService.h"
#include "TurnCombatSkillService.h"
#include "TurnCombatTargetService.h"
#include "TurnCombatUIService.h"
#include "TurnCombatVisualService.h"
#include "Wheatear/Gameplay/Services/GameplayEntityService.h"
#include "Wheatear/Gameplay/Services/GameplayFlowService.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>

namespace Wheatear::TurnCombatFlowService {

    void BeginNextTurn(Scene* scene, TurnCombatLevelComponent& level)
    {
        if (!TurnCombatTargetService::HasAliveTeam(
                scene, (int)TurnCombatTeam::Player))
        {
            level.RuntimePhase = TurnCombatPhase::Defeat;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeMessage = "队伍被击败。";
            return;
        }

        if (!TurnCombatTargetService::HasAliveTeam(
                scene, (int)TurnCombatTeam::Enemy))
        {
            level.RuntimePhase = TurnCombatPhase::Victory;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeMessage = "战斗胜利。";
            return;
        }

        if (level.RuntimeTurnQueue.empty()
            || level.RuntimeTurnIndex >= (int)level.RuntimeTurnQueue.size())
        {
            TurnCombatTargetService::BuildTurnQueue(scene, level);
            ++level.RuntimeRound;
        }

        int safety = 0;
        while (safety++ < 64 && !level.RuntimeTurnQueue.empty())
        {
            if (level.RuntimeTurnIndex >= (int)level.RuntimeTurnQueue.size())
            {
                TurnCombatTargetService::BuildTurnQueue(scene, level);
                ++level.RuntimeRound;
            }

            Entity actor = GameplayEntityService::Resolve(
                scene, level.RuntimeTurnQueue[level.RuntimeTurnIndex]);
            ++level.RuntimeTurnIndex;
            if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                continue;

            auto& combatant = actor.GetComponent<TurnCombatantComponent>();
            if (!combatant.RuntimeAlive)
                continue;

            TurnCombatSkillService::TickStatusEffects(combatant);
            if (!combatant.RuntimeAlive)
                continue;

            combatant.RuntimeGuarding = TurnCombatSkillService::HasStatusEffect(
                combatant,
                TurnCombatSkillService::TurnStatusEffectKind::Guard);
            level.RuntimeActiveActor = actor.GetUUID();
            level.RuntimeCommandMenuPage = "root";

            if (combatant.Team == (int)TurnCombatTeam::Player
                && combatant.Controllable)
            {
                level.RuntimePhase = TurnCombatPhase::AwaitCommand;
                level.RuntimeSelectedSkillId.clear();
                level.RuntimeMessage = combatant.DisplayName + " 的回合。";
                return;
            }

            const std::string skillId = TurnCombatSkillService::ChooseEnemySkill(
                combatant, level.RuntimeRound);
            const auto* skill = TurnCombatSkillService::FindSkill(skillId);
            if (!skill)
            {
                // Missing skill data (empty BasicSkillId with no 'claw'
                // fallback in the action database) must not soft-lock the
                // battle in the intro/acting phase; skip this enemy's turn.
                WT_CORE_WARN("TurnCombat: skill '{}' not found for '{}'; skipping its turn.",
                    skillId, combatant.DisplayName);
                continue;
            }
            Entity target = TurnCombatTargetService::ChooseTargetForAI(scene, actor, *skill);
            TurnCombatActionService::BeginAction(scene, level, actor, skillId, target);
            return;
        }

        // No combatant could act this pass (e.g. every enemy's skill is
        // missing from the action database). Fall back to the command phase so
        // the battle cannot hang permanently.
        WT_CORE_WARN("TurnCombat: no combatant could act; falling back to the command phase.");
        level.RuntimePhase = TurnCombatPhase::AwaitCommand;
        level.RuntimeCommandMenuPage = "root";
        level.RuntimeSelectedSkillId.clear();
        level.RuntimeActiveActor = 0;
    }

    void ResetLevel(Scene* scene, TurnCombatLevelComponent& level)
    {
        level.RuntimeElapsed = 0.0f;
        level.RuntimeFadeAlpha = 1.0f;
        level.RuntimePhase = TurnCombatPhase::Intro;
        level.RuntimeRound = 1;
        level.RuntimeTurnIndex = 0;
        level.RuntimeTurnQueue.clear();
        level.RuntimeActiveActor = 0;
        level.RuntimeCommandMenuPage = "root";
        level.RuntimeSelectedSkillId.clear();
        level.RuntimeActionActor = 0;
        level.RuntimeActionTarget = 0;
        level.RuntimeActionSkillId.clear();
        level.RuntimeMessage = "回合制战斗开始。";
        level.RuntimeRequestedCommand.clear();
        level.RuntimeIntroTimer = 0.0f;
        level.RuntimeActionTimer = 0.0f;
        level.RuntimeResultTimer = 0.0f;
        level.RuntimeInitialized = true;
        level.RuntimeActionApplied = false;
        level.RuntimeResultCommandIssued = false;

        for (Entity entity : TurnCombatTargetService::CollectCombatants(scene))
        {
            auto& combatant = entity.GetComponent<TurnCombatantComponent>();
            combatant.Health = std::clamp(
                combatant.Health <= 0.0f ? combatant.MaxHealth : combatant.Health,
                0.0f,
                combatant.MaxHealth);
            combatant.Mana = std::clamp(
                combatant.Mana <= 0.0f ? combatant.MaxMana : combatant.Mana,
                0.0f,
                combatant.MaxMana);
            if (combatant.Team == (int)TurnCombatTeam::Player)
                combatant.Controllable = true;
            combatant.RuntimeAlive = combatant.Health > 0.0f;
            combatant.RuntimeGuarding = false;
            combatant.RuntimeSelectedTarget = false;
            combatant.RuntimeHitFlashTimer = 0.0f;
            combatant.RuntimeStatusEffects.clear();
            combatant.RuntimeAnimationClip.clear();
            combatant.RuntimeAnimationTimer = 0.0f;
            TurnCombatVisualService::CacheVisuals(entity);
            TurnCombatVisualService::RestoreVisual(entity);
        }

        UIRuntimeTools::SetImageAlpha(scene, level.FadeEntityName, 1.0f);
        UIRuntimeTools::SetImageAlpha(scene, level.ActionFlashEntityName, 0.0f);
        TurnCombatTargetService::BuildTurnQueue(scene, level);
    }

    void UpdateLevel(Scene* scene, TurnCombatLevelComponent& level, float dt)
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

        switch (level.RuntimePhase)
        {
        case TurnCombatPhase::Intro:
            level.RuntimeIntroTimer += dt;
            if (level.RuntimeIntroTimer >= level.IntroDuration)
                BeginNextTurn(scene, level);
            break;

        case TurnCombatPhase::Acting:
            level.RuntimeActionTimer += dt;
            if (!level.RuntimeActionApplied
                && level.RuntimeActionTimer >= level.ActionDuration * 0.42f)
            {
                TurnCombatActionService::ApplySkill(scene, level);
                level.RuntimeActionApplied = true;
            }
            if (level.RuntimeActionTimer >= level.ActionDuration)
                BeginNextTurn(scene, level);
            break;

        case TurnCombatPhase::Victory:
            level.RuntimeResultTimer += dt;
            UIRuntimeTools::SetWidgetVisible(
                scene, level.CommandPanelEntityName, false);
            GameplayFlowService::TryIssueDelayedCommand(
                level.RuntimeResultTimer,
                level.VictoryReturnDelay,
                level.RuntimeResultCommandIssued,
                level.RuntimeRequestedCommand,
                level.VictorySceneCommand);
            break;

        case TurnCombatPhase::Defeat:
            level.RuntimeResultTimer += dt;
            UIRuntimeTools::SetWidgetVisible(
                scene, level.CommandPanelEntityName, false);
            GameplayFlowService::TryIssueDelayedCommand(
                level.RuntimeResultTimer,
                level.DefeatReturnDelay,
                level.RuntimeResultCommandIssued,
                level.RuntimeRequestedCommand,
                level.DefeatSceneCommand);
            break;

        case TurnCombatPhase::AwaitCommand:
        case TurnCombatPhase::AwaitTarget:
            break;
        }

        TurnCombatVisualService::UpdateVisuals(scene, level, dt);
        TurnCombatUIService::UpdateBattleUI(scene, level);
    }

} // namespace Wheatear::TurnCombatFlowService
