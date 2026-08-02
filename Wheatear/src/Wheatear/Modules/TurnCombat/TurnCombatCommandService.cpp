#include "wtpch.h"
#include "TurnCombatCommandService.h"

#include "TurnCombatActionService.h"
#include "TurnCombatSkillService.h"
#include "TurnCombatTargetService.h"
#include "Wheatear/Modules/Common/GameplayEntityService.h"
#include "Wheatear/Modules/Common/GameplayTextService.h"

#include <optional>
#include <vector>

namespace Wheatear::TurnCombatCommandService {

    namespace {

        static void BeginSkillOrTarget(Scene* scene,
            TurnCombatLevelComponent& level,
            Entity actor,
            TurnCombatantComponent& actorCombatant,
            const TurnCombatSkillService::TurnSkillDefinition& skill,
            const std::string& targetMessage)
        {
            if (actorCombatant.Mana < skill.ManaCost)
            {
                level.RuntimeMessage = "魔力不足。";
                return;
            }

            if (skill.TargetRule == TurnTargetRule::Self)
            {
                TurnCombatActionService::BeginAction(scene, level, actor, skill.Id, actor);
                return;
            }

            if (skill.TargetRule == TurnTargetRule::EnemyAll
                || skill.TargetRule == TurnTargetRule::AllyAll)
            {
                TurnCombatActionService::BeginAction(scene, level, actor, skill.Id, {});
                return;
            }

            level.RuntimePhase = TurnCombatPhase::AwaitTarget;
            level.RuntimeSelectedSkillId = skill.Id;
            level.RuntimeMessage = targetMessage;
        }

    } // namespace

    void ProcessCommand(Scene* scene,
        TurnCombatLevelComponent& level,
        const std::string& command)
    {
        const std::vector<std::string> parts = GameplayTextService::SplitCommand(command);
        if (parts.size() < 2 || parts[0] != "turn")
            return;

        Entity actor = GameplayEntityService::Resolve(scene, level.RuntimeActiveActor);
        if (!actor || !actor.HasComponent<TurnCombatantComponent>())
            return;

        auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
        if (actorCombatant.Team != (int)TurnCombatTeam::Player || !actorCombatant.Controllable)
            return;

        if (parts[1] == "menu" && parts.size() >= 3
            && level.RuntimePhase == TurnCombatPhase::AwaitCommand)
        {
            level.RuntimeCommandMenuPage = parts[2];
            level.RuntimeSelectedSkillId.clear();
            level.RuntimeMessage = actorCombatant.DisplayName + "：请选择行动。";
            return;
        }

        if (parts[1] == "cancel")
        {
            if (level.RuntimePhase == TurnCombatPhase::AwaitTarget)
                level.RuntimePhase = TurnCombatPhase::AwaitCommand;
            level.RuntimeCommandMenuPage = "root";
            level.RuntimeSelectedSkillId.clear();
            level.RuntimeMessage = actorCombatant.DisplayName + " 的回合。";
            return;
        }

        if (level.RuntimePhase != TurnCombatPhase::AwaitCommand
            && level.RuntimePhase != TurnCombatPhase::AwaitTarget)
        {
            return;
        }

        if (parts[1] == "wait" && level.RuntimePhase == TurnCombatPhase::AwaitCommand)
        {
            TurnCombatActionService::BeginAction(scene, level, actor, "focus_wait", actor);
            return;
        }

        if (parts[1] == "guard" && level.RuntimePhase == TurnCombatPhase::AwaitCommand)
        {
            TurnCombatActionService::BeginAction(scene, level, actor, "shield_oath", actor);
            return;
        }

        if (parts[1] == "item" && parts.size() >= 3
            && level.RuntimePhase == TurnCombatPhase::AwaitCommand)
        {
            const std::optional<std::string> skillId =
                TurnCombatSkillService::ResolvePlayerSkillId(actorCombatant, parts[2]);
            const auto* item = skillId ? TurnCombatSkillService::FindSkill(*skillId) : nullptr;
            if (!item)
            {
                level.RuntimeMessage = "这个道具栏没有可用道具。";
                return;
            }

            BeginSkillOrTarget(
                scene, level, actor, actorCombatant, *item, "请选择道具目标。");
            return;
        }

        if (parts[1] == "skill" && parts.size() >= 3
            && level.RuntimePhase == TurnCombatPhase::AwaitCommand)
        {
            const std::optional<std::string> skillId =
                TurnCombatSkillService::ResolvePlayerSkillId(actorCombatant, parts[2]);
            const auto* skill = skillId ? TurnCombatSkillService::FindSkill(*skillId) : nullptr;
            if (!skill)
            {
                level.RuntimeMessage = "这个技能槽没有可用技能。";
                return;
            }

            BeginSkillOrTarget(
                scene, level, actor, actorCombatant, *skill, "请选择技能目标。");
            return;
        }

        if (parts[1] == "target" && parts.size() >= 3
            && level.RuntimePhase == TurnCombatPhase::AwaitTarget)
        {
            const auto* skill = TurnCombatSkillService::FindSkill(level.RuntimeSelectedSkillId);
            Entity target = TurnCombatTargetService::FindTurnTarget(scene, parts[2]);
            if (!skill || !target || !target.HasComponent<TurnCombatantComponent>())
                return;

            if (!TurnCombatTargetService::IsValidTarget(
                    *skill, actorCombatant, target.GetComponent<TurnCombatantComponent>()))
            {
                level.RuntimeMessage = "目标无效。";
                return;
            }

            TurnCombatActionService::BeginAction(scene, level, actor, skill->Id, target);
        }
    }

} // namespace Wheatear::TurnCombatCommandService
