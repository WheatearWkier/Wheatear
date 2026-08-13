#include "wtpch.h"
#include "TurnCombatActionService.h"

#include "TurnCombatSkillService.h"
#include "TurnCombatTargetService.h"
#include "TurnCombatVisualService.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"
#include "Wheatear/Gameplay/Services/GameplayAudioService.h"
#include "Wheatear/Gameplay/Services/GameplayCombatService.h"
#include "Wheatear/Gameplay/Services/GameplayEntityService.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace Wheatear::TurnCombatActionService {

    namespace {

        static void PlayTurnSound(const std::string& path, float volume = 0.55f)
        {
            if (!path.empty())
                GameplayAudioService::PlaySFX(path, volume);
        }

        static std::string ResolveSkillSound(
            const TurnCombatSkillService::TurnSkillDefinition& skill,
            const TurnCombatantComponent& actor)
        {
            if (!skill.SoundPath.empty())
                return skill.SoundPath;

            if (actor.Team == (int)TurnCombatTeam::Enemy)
            {
                if (skill.Magic)
                    return AssetAliasRegistry::Path("turn.audio.enemy_dark");
                if (skill.Power >= 1.15f)
                    return AssetAliasRegistry::Path("turn.audio.enemy_pounce");
                return AssetAliasRegistry::Path("turn.audio.enemy_claw");
            }

            if (skill.HealPower > 0.0f)
                return AssetAliasRegistry::Path("turn.audio.heal");
            if (skill.Guard)
                return AssetAliasRegistry::Path("turn.audio.guard");
            if (skill.Magic)
                return AssetAliasRegistry::Path("turn.audio.magic");
            return AssetAliasRegistry::Path("turn.audio.slash");
        }

        static const WAO::ActionRecipe* ResolveRecipe(
            const TurnCombatSkillService::TurnSkillDefinition& skill)
        {
            return WAO::FindRecipeOrWarn(WAO::ComposeActionId("turn", skill.Id), "TurnCombat");
        }

        static const char* StatusId(TurnCombatSkillService::TurnStatusEffectKind effect)
        {
            using TurnCombatSkillService::TurnStatusEffectKind;
            switch (effect)
            {
            case TurnStatusEffectKind::Guard: return WAO::StateIds::Guard;
            case TurnStatusEffectKind::Regeneration: return WAO::StateIds::Regeneration;
            case TurnStatusEffectKind::Burn: return WAO::StateIds::Burn;
            case TurnStatusEffectKind::DefenseDown: return WAO::StateIds::DefenseDown;
            case TurnStatusEffectKind::Stun: return WAO::StateIds::Stun;
            case TurnStatusEffectKind::None:
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
            intent.Source = "TurnCombat";
            return intent;
        }

        static void RecordTurnEffect(WAO::EffectLedger& ledger,
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
            TurnCombatSkillService::TurnStatusEffectKind effect,
            int turns,
            float power,
            bool applied)
        {
            const char* stateId = StatusId(effect);
            if (stateId[0] == '\0' || turns <= 0)
                return;

            RecordTurnEffect(ledger,
                intent,
                WAO::EffectType::AddState,
                target,
                std::string("AddState ") + stateId,
                power,
                applied);
        }

    } // namespace

    void BeginAction(Scene* scene,
        TurnCombatLevelComponent& level,
        Entity actor,
        const std::string& skillId,
        Entity target)
    {
        const auto* skill = TurnCombatSkillService::FindSkill(skillId);
        if (!actor || !actor.HasComponent<TurnCombatantComponent>() || !skill)
            return;

        auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
        level.RuntimePhase = TurnCombatPhase::Acting;
        level.RuntimeActionTimer = 0.0f;
        level.RuntimeActionApplied = false;
        level.RuntimeActionActor = actor.GetUUID();
        level.RuntimeActionTarget = target ? target.GetUUID() : UUID(0);
        level.RuntimeActionSkillId = skillId;
        level.RuntimeSelectedSkillId.clear();
        level.RuntimeCommandMenuPage = "root";

        level.RuntimeMessage = actorCombatant.DisplayName + " 准备使用 " + skill->DisplayName + "。";
        UIRuntimeTools::SetWidgetVisible(scene, level.CommandPanelEntityName, false);
    }

    void ApplySkill(Scene* scene, TurnCombatLevelComponent& level)
    {
        Entity actor = GameplayEntityService::Resolve(scene, level.RuntimeActionActor);
        Entity explicitTarget = GameplayEntityService::Resolve(scene, level.RuntimeActionTarget);
        const auto* skill = TurnCombatSkillService::FindSkill(level.RuntimeActionSkillId);
        if (!actor || !actor.HasComponent<TurnCombatantComponent>() || !skill)
            return;

        const WAO::ActionRecipe* recipe = ResolveRecipe(*skill);
        const std::string actionId = recipe
            ? recipe->Id
            : WAO::ComposeActionId("turn", level.RuntimeActionSkillId);
        WAO::EffectLedger ledger;
        const WAO::ActionIntent intent = BuildIntent(actor, explicitTarget, actionId);
        ledger.BeginAction(intent);

        auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
        actorCombatant.Mana = std::max(0.0f, actorCombatant.Mana - skill->ManaCost);
        actorCombatant.RuntimeGuarding = false;
        if (skill->ManaCost > 0.0f)
        {
            RecordTurnEffect(ledger,
                intent,
                WAO::EffectType::ConsumeResource,
                actor.GetUUID(),
                "Consume mana",
                skill->ManaCost,
                true);
        }

        if (std::string(skill->Id) == "focus_wait")
        {
            const float before = actorCombatant.Mana;
            actorCombatant.Mana = std::min(actorCombatant.MaxMana, actorCombatant.Mana + 10.0f);
            RecordTurnEffect(ledger,
                intent,
                WAO::EffectType::ModifyAttribute,
                actor.GetUUID(),
                "Restore mana",
                actorCombatant.Mana - before,
                true);
            level.RuntimeMessage = actorCombatant.DisplayName + " 冥想并恢复魔力。";
            PlayTurnSound(ResolveSkillSound(*skill, actorCombatant), 0.42f);
            WAO::ActionDebugHistory::Record(ledger, true, "Turn skill applied");
            return;
        }

        if (skill->Guard)
        {
            actorCombatant.RuntimeGuarding = true;
            TurnCombatSkillService::ApplyStatusEffect(
                actorCombatant,
                skill->AppliedEffect,
                skill->EffectTurns,
                skill->EffectPower);
            RecordStatusEffect(ledger,
                intent,
                actor.GetUUID(),
                skill->AppliedEffect,
                skill->EffectTurns,
                skill->EffectPower,
                true);
            actorCombatant.Mana = std::min(actorCombatant.MaxMana, actorCombatant.Mana + 3.0f);
            RecordTurnEffect(ledger,
                intent,
                WAO::EffectType::ModifyAttribute,
                actor.GetUUID(),
                "Guard mana refund",
                3.0f,
                true);
            level.RuntimeMessage = actorCombatant.DisplayName + " 进入防御。";
            PlayTurnSound(ResolveSkillSound(*skill, actorCombatant), 0.45f);
            WAO::ActionDebugHistory::Record(ledger, true, "Turn skill applied");
            return;
        }

        std::vector<Entity> targets = TurnCombatTargetService::ResolveTargets(
            scene, *skill, actor, explicitTarget);
        if (targets.empty())
        {
            RecordTurnEffect(ledger,
                intent,
                WAO::EffectType::None,
                0,
                "No targets",
                0.0f,
                false);
            WAO::ActionDebugHistory::Record(ledger, false, "Turn skill had no targets");
            return;
        }

        float total = 0.0f;
        for (Entity target : targets)
        {
            const UUID targetId = target.GetUUID();
            auto& targetCombatant = target.GetComponent<TurnCombatantComponent>();
            if (skill->HealPower > 0.0f)
            {
                const float heal = TurnCombatSkillService::CalculateHeal(*skill, actorCombatant);
                const float applied = GameplayCombatService::ApplyHealing(
                    targetCombatant.Health,
                    targetCombatant.MaxHealth,
                    heal);
                total += applied;
                RecordTurnEffect(ledger,
                    intent,
                    WAO::EffectType::Heal,
                    targetId,
                    "Heal",
                    applied,
                    applied > 0.0f);
                TurnCombatSkillService::ApplyStatusEffect(
                    targetCombatant,
                    skill->AppliedEffect,
                    skill->EffectTurns,
                    skill->EffectPower);
                RecordStatusEffect(ledger,
                    intent,
                    targetId,
                    skill->AppliedEffect,
                    skill->EffectTurns,
                    skill->EffectPower,
                    true);
                TurnCombatVisualService::MarkHit(target);
            }
            else
            {
                if (targetCombatant.Invulnerable)
                {
                    RecordTurnEffect(ledger,
                        intent,
                        WAO::EffectType::Damage,
                        targetId,
                        "Invulnerable",
                        0.0f,
                        false);
                    continue;
                }

                const float damage = TurnCombatSkillService::CalculateDamage(
                    *skill,
                    actorCombatant,
                    targetCombatant);
                const float applied = GameplayCombatService::ApplyDamage(
                    targetCombatant.Health,
                    damage);
                total += applied;
                RecordTurnEffect(ledger,
                    intent,
                    WAO::EffectType::Damage,
                    targetId,
                    "Damage",
                    applied,
                    applied > 0.0f);
                targetCombatant.RuntimeAlive =
                    GameplayCombatService::IsAlive(targetCombatant.Health);
                if (targetCombatant.RuntimeAlive)
                {
                    TurnCombatSkillService::ApplyStatusEffect(
                        targetCombatant,
                        skill->AppliedEffect,
                        skill->EffectTurns,
                        skill->EffectPower);
                    RecordStatusEffect(ledger,
                        intent,
                        targetId,
                        skill->AppliedEffect,
                        skill->EffectTurns,
                        skill->EffectPower,
                        true);
                }
                else
                {
                    RecordTurnEffect(ledger,
                        intent,
                        WAO::EffectType::AddState,
                        targetId,
                        "Dead",
                        1.0f,
                        true);
                }
                TurnCombatVisualService::MarkHit(target);
                PlayTurnSound(
                    AssetAliasRegistry::Path("turn.audio.hit"),
                    actorCombatant.Team == (int)TurnCombatTeam::Enemy ? 0.88f : 1.00f);
            }
        }

        std::ostringstream stream;
        stream << actorCombatant.DisplayName << " 使用 " << skill->DisplayName;
        if (skill->HealPower > 0.0f)
            stream << "，恢复 " << (int)total << " 生命";
        else
            stream << "，造成 " << (int)total << " 伤害";
        level.RuntimeMessage = stream.str();
        if (skill->HealPower > 0.0f)
            PlayTurnSound(ResolveSkillSound(*skill, actorCombatant), 0.56f);
        WAO::ActionDebugHistory::Record(ledger, true, "Turn skill applied");
    }

} // namespace Wheatear::TurnCombatActionService
