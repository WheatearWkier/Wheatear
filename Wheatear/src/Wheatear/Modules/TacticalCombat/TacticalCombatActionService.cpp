#include "wtpch.h"
#include "TacticalCombatActionService.h"

#include "TacticalCombatActionCatalog.h"
#include "TacticalCombatBoardService.h"
#include "TacticalCombatFeedbackService.h"
#include "TacticalCombatSkillService.h"
#include "TacticalCombatVisualService.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"
#include "Wheatear/Modules/Common/GameplayCombatService.h"
#include "Wheatear/Modules/Common/GameplayEntityService.h"

#include <string>

namespace Wheatear::TacticalCombatActionService {

    namespace {

        static const WAO::ActionRecipe* ResolveRecipe(
            const TacticalCombatSkillService::TacticalSkillDefinition& skill)
        {
            const std::string recipeId = TacticalCombatActionCatalog::ActionRecipeId(skill.Id);
            if (const WAO::ActionRecipe* recipe = WAO::ActionDatabase::Find(recipeId))
                return recipe;

            WAO::ActionDatabase::Register(TacticalCombatActionCatalog::BuildActionRecipe(skill));
            return WAO::ActionDatabase::Find(recipeId);
        }

        static const char* StatusId(TacticalCombatSkillService::TacticalStatusEffectKind effect)
        {
            using TacticalCombatSkillService::TacticalStatusEffectKind;
            switch (effect)
            {
            case TacticalStatusEffectKind::Guard: return WAO::StateIds::Guard;
            case TacticalStatusEffectKind::Regeneration: return WAO::StateIds::Regeneration;
            case TacticalStatusEffectKind::Burn: return WAO::StateIds::Burn;
            case TacticalStatusEffectKind::DefenseDown: return WAO::StateIds::DefenseDown;
            case TacticalStatusEffectKind::Stun: return WAO::StateIds::Stun;
            case TacticalStatusEffectKind::None:
            default: return "";
            }
        }

        static WAO::ActionIntent BuildIntent(Entity actor,
            Entity target,
            const std::string& actionId)
        {
            WAO::ActionIntent intent;
            intent.Actor = actor ? actor.GetUUID() : UUID(0);
            intent.ExplicitTarget = target ? target.GetUUID() : UUID(0);
            intent.ActionId = actionId;
            intent.Source = "TacticalCombat";
            return intent;
        }

        static void RecordTacticalEffect(WAO::EffectLedger& ledger,
            const WAO::ActionIntent& intent,
            WAO::EffectType type,
            UUID target,
            const std::string& detail,
            float value,
            bool applied)
        {
            ledger.Record({
                intent.ActionId,
                type,
                intent.Actor,
                target ? target : intent.ExplicitTarget,
                detail,
                value,
                applied
            });
        }

        static void RecordStatusEffect(WAO::EffectLedger& ledger,
            const WAO::ActionIntent& intent,
            UUID target,
            TacticalCombatSkillService::TacticalStatusEffectKind effect,
            int turns,
            float power,
            bool applied)
        {
            const char* stateId = StatusId(effect);
            if (stateId[0] == '\0' || turns <= 0)
                return;

            RecordTacticalEffect(ledger,
                intent,
                WAO::EffectType::AddState,
                target,
                std::string("AddState ") + stateId,
                power,
                applied);
        }

    } // namespace

    void BeginAction(Scene*,
        TacticalCombatLevelComponent& level,
        Entity actor,
        const std::string& skillId,
        Entity target,
        TacticalCombatPhase returnPhase)
    {
        if (!actor || !actor.HasComponent<TacticalUnitComponent>())
            return;

        level.RuntimePhase = TacticalCombatPhase::Acting;
        level.RuntimeActionReturnPhase = returnPhase;
        level.RuntimeActionActor = actor.GetUUID();
        level.RuntimeActionTarget = target ? target.GetUUID() : actor.GetUUID();
        level.RuntimeActionSkillId = skillId;
        level.RuntimeActionTimer = 0.0f;
        level.RuntimeActionApplied = false;
        level.RuntimeCommandMenuPage = "root";

        const auto& unit = actor.GetComponent<TacticalUnitComponent>();
        const auto* skill = TacticalCombatSkillService::FindSkill(skillId);
        level.RuntimeMessage = unit.DisplayName + " 使用 "
            + (skill ? skill->DisplayName : skillId) + "。";
        if (skill)
            TacticalCombatFeedbackService::PlaySound(skill->SoundPath, 0.50f);
    }

    void FinishSelectedPlayerAction(Scene* scene, TacticalCombatLevelComponent& level)
    {
        Entity selected = GameplayEntityService::Resolve(scene, level.RuntimeSelectedUnit);
        if (selected && selected.HasComponent<TacticalUnitComponent>())
        {
            auto& unit = selected.GetComponent<TacticalUnitComponent>();
            unit.RuntimeHasActed = true;
            unit.RuntimeGuarding = level.RuntimeSelectedSkillId == "guard_wait";
        }

        level.RuntimeSelectedUnit = 0;
        level.RuntimeSelectedSkillId.clear();
        level.RuntimeCommandMenuPage = "root";

        if (TacticalCombatBoardService::AllPlayerUnitsDone(scene))
        {
            level.RuntimePhase = TacticalCombatPhase::EnemyTurn;
            level.RuntimeEnemyCursor = 0;
            level.RuntimeEnemyStepTimer = 0.0f;
            level.RuntimeMessage = "敌方回合。";
        }
        else
        {
            level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
            level.RuntimeMessage = "请选择一个仍能行动的我方单位。";
        }
    }

    void ApplyAction(Scene* scene, TacticalCombatLevelComponent& level)
    {
        Entity actor = GameplayEntityService::Resolve(scene, level.RuntimeActionActor);
        Entity target = GameplayEntityService::Resolve(scene, level.RuntimeActionTarget);
        const auto* skill = TacticalCombatSkillService::FindSkill(level.RuntimeActionSkillId);
        if (!actor || !target || !skill
            || !actor.HasComponent<TacticalUnitComponent>()
            || !target.HasComponent<TacticalUnitComponent>())
        {
            return;
        }

        const WAO::ActionRecipe* recipe = ResolveRecipe(*skill);
        const std::string actionId = recipe
            ? recipe->Id
            : TacticalCombatActionCatalog::ActionRecipeId(level.RuntimeActionSkillId);
        WAO::EffectLedger ledger;
        const WAO::ActionIntent intent = BuildIntent(actor, target, actionId);
        ledger.BeginAction(intent);

        auto& actorUnit = actor.GetComponent<TacticalUnitComponent>();
        auto& targetUnit = target.GetComponent<TacticalUnitComponent>();

        if (skill->Guard)
        {
            actorUnit.RuntimeGuarding = true;
            TacticalCombatSkillService::ApplyStatusEffect(
                actorUnit, skill->AppliedEffect, skill->EffectTurns, skill->EffectPower);
            RecordStatusEffect(ledger,
                intent,
                actor.GetUUID(),
                skill->AppliedEffect,
                skill->EffectTurns,
                skill->EffectPower,
                true);
            WAO::ActionDebugHistory::Record(ledger, true, "Tactical skill applied");
            level.RuntimeMessage = actorUnit.DisplayName + " 进入防御。";
            return;
        }

        if (skill->HealPower > 0.0f)
        {
            const float amount = TacticalCombatSkillService::CalculateHeal(*skill, actorUnit);
            const float healed = GameplayCombatService::ApplyHealing(
                targetUnit.Health, targetUnit.MaxHealth, amount);
            RecordTacticalEffect(ledger,
                intent,
                WAO::EffectType::Heal,
                target.GetUUID(),
                "Heal",
                healed,
                healed > 0.0f);
            TacticalCombatSkillService::ApplyStatusEffect(
                targetUnit, skill->AppliedEffect, skill->EffectTurns, skill->EffectPower);
            RecordStatusEffect(ledger,
                intent,
                target.GetUUID(),
                skill->AppliedEffect,
                skill->EffectTurns,
                skill->EffectPower,
                true);
            targetUnit.RuntimeHitFlashTimer = 0.25f;
            WAO::ActionDebugHistory::Record(ledger, true, "Tactical skill applied");
            level.RuntimeMessage = targetUnit.DisplayName + " 恢复 "
                + std::to_string((int)healed) + " 生命。";
            return;
        }

        const float damage = TacticalCombatSkillService::CalculateDamage(
            *skill, actorUnit, targetUnit);
        const float applied = GameplayCombatService::ApplyDamage(targetUnit.Health, damage);
        RecordTacticalEffect(ledger,
            intent,
            WAO::EffectType::Damage,
            target.GetUUID(),
            targetUnit.Invulnerable ? "Invulnerable" : "Damage",
            applied,
            applied > 0.0f);
        targetUnit.RuntimeHitFlashTimer = 0.30f;
        targetUnit.RuntimeGuarding = false;
        targetUnit.RuntimeAlive = GameplayCombatService::IsAlive(targetUnit.Health);
        if (targetUnit.RuntimeAlive)
        {
            TacticalCombatSkillService::ApplyStatusEffect(
                targetUnit, skill->AppliedEffect, skill->EffectTurns, skill->EffectPower);
            RecordStatusEffect(ledger,
                intent,
                target.GetUUID(),
                skill->AppliedEffect,
                skill->EffectTurns,
                skill->EffectPower,
                true);
        }
        else
        {
            RecordTacticalEffect(ledger,
                intent,
                WAO::EffectType::AddState,
                target.GetUUID(),
                "Dead",
                1.0f,
                true);
        }

        TacticalCombatFeedbackService::PlaySound(
            "assets/vertical_slice/tactical_combat/audio/tac_hit.wav", 0.44f);
        WAO::ActionDebugHistory::Record(ledger, true, "Tactical skill applied");
        level.RuntimeMessage = actorUnit.DisplayName + " 对 " + targetUnit.DisplayName
            + " 造成 " + std::to_string((int)applied) + " 伤害。";
    }

    void EndAction(Scene* scene, TacticalCombatLevelComponent& level)
    {
        TacticalCombatVisualService::HideActionEffect(scene, level);

        if (!TacticalCombatBoardService::HasAliveTeam(
            scene, (int)TacticalCombatTeam::Enemy))
        {
            level.RuntimePhase = TacticalCombatPhase::Victory;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeMessage = "战斗胜利。";
            TacticalCombatFeedbackService::PlaySound(
                "assets/vertical_slice/tactical_combat/audio/tac_victory.wav", 0.46f);
            return;
        }
        if (!TacticalCombatBoardService::HasAliveTeam(
            scene, (int)TacticalCombatTeam::Player))
        {
            level.RuntimePhase = TacticalCombatPhase::Defeat;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeMessage = "队伍被击败。";
            return;
        }

        const TacticalCombatPhase returnPhase = level.RuntimeActionReturnPhase;
        level.RuntimeActionActor = 0;
        level.RuntimeActionTarget = 0;
        level.RuntimeActionSkillId.clear();

        if (returnPhase == TacticalCombatPhase::EnemyTurn)
        {
            level.RuntimePhase = TacticalCombatPhase::EnemyTurn;
            level.RuntimeEnemyCursor += 1;
            level.RuntimeEnemyStepTimer = 0.0f;
            return;
        }

        FinishSelectedPlayerAction(scene, level);
    }

} // namespace Wheatear::TacticalCombatActionService
