#include "wtpch.h"
#include "TacticalCombatCommandService.h"

#include "TacticalCombatActionService.h"
#include "TacticalCombatBoardService.h"
#include "TacticalCombatFeedbackService.h"
#include "TacticalCombatSkillService.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Modules/Common/GameplayEntityService.h"
#include "Wheatear/Modules/Common/GameplayTextService.h"

#include <optional>
#include <string>
#include <vector>

namespace Wheatear::TacticalCombatCommandService {

    namespace {

        static void ResetToUnitSelection(TacticalCombatLevelComponent& level)
        {
            level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
            level.RuntimeSelectedUnit = 0;
            level.RuntimeSelectedSkillId.clear();
            level.RuntimeCommandMenuPage = "root";
            level.RuntimeMessage = "请选择一个仍能行动的我方单位。";
        }

        static void ReturnToRootMenu(TacticalCombatLevelComponent& level)
        {
            level.RuntimePhase = TacticalCombatPhase::AwaitCommand;
            level.RuntimeSelectedSkillId.clear();
            level.RuntimeCommandMenuPage = "root";
            level.RuntimeMessage = "请选择移动、攻击、技能、道具或待机。";
        }

        static void HandleCellCommand(Scene* scene,
            TacticalCombatLevelComponent& level,
            int x,
            int y)
        {
            if (!TacticalCombatBoardService::InBounds(level, x, y))
                return;

            if (level.RuntimePhase == TacticalCombatPhase::PlayerTurn)
            {
                Entity occupant = TacticalCombatBoardService::FindUnitAt(scene, x, y);
                if (!occupant || !occupant.HasComponent<TacticalUnitComponent>())
                    return;

                auto& unit = occupant.GetComponent<TacticalUnitComponent>();
                if (unit.Team != (int)TacticalCombatTeam::Player || !unit.Controllable || unit.RuntimeHasActed)
                {
                    level.RuntimeMessage = "这个单位现在不能行动。";
                    return;
                }

                level.RuntimeSelectedUnit = occupant.GetUUID();
                level.RuntimeSelectedSkillId.clear();
                level.RuntimeCommandMenuPage = "root";
                level.RuntimePhase = TacticalCombatPhase::AwaitCommand;
                level.RuntimeMessage = unit.DisplayName + "：请选择行动。";
                TacticalCombatFeedbackService::PlaySound(
                    AssetAliasRegistry::Path("tactical.audio.select"), 0.34f);
                return;
            }

            Entity selected = GameplayEntityService::Resolve(scene, level.RuntimeSelectedUnit);
            if (!selected || !selected.HasComponent<TacticalUnitComponent>())
                return;

            auto& selectedUnit = selected.GetComponent<TacticalUnitComponent>();
            Entity occupant = TacticalCombatBoardService::FindUnitAt(scene, x, y);

            if (level.RuntimePhase == TacticalCombatPhase::AwaitCommand)
            {
                if (level.RuntimeCommandMenuPage == "move")
                {
                    if (!TacticalCombatBoardService::CanMoveTo(scene, level, selectedUnit, x, y))
                    {
                        level.RuntimeMessage = "请选择蓝色范围内的空格。";
                        return;
                    }

                    selectedUnit.GridX = x;
                    selectedUnit.GridY = y;
                    selectedUnit.RuntimeMoved = true;
                    level.RuntimeCommandMenuPage = "root";
                    level.RuntimeMessage = selectedUnit.DisplayName + " 已移动，可以继续攻击或待机。";
                    TacticalCombatFeedbackService::PlaySound(
                        AssetAliasRegistry::Path("tactical.audio.move"), 0.38f);
                    return;
                }

                if (level.RuntimeCommandMenuPage == "attack")
                {
                    if (!occupant || !occupant.HasComponent<TacticalUnitComponent>())
                    {
                        level.RuntimeMessage = "请选择红色攻击范围内的敌人。";
                        return;
                    }

                    auto& targetUnit = occupant.GetComponent<TacticalUnitComponent>();
                    const auto* basic = TacticalCombatSkillService::FindSkill(selectedUnit.BasicSkillId);
                    if (!basic || !TacticalCombatBoardService::IsValidTarget(*basic, selectedUnit, targetUnit))
                    {
                        level.RuntimeMessage = "目标不在普通攻击范围内。";
                        return;
                    }

                    const std::string skillId = basic->Id;
                    TacticalCombatActionService::BeginAction(
                        scene, level, selected, skillId, occupant, TacticalCombatPhase::PlayerTurn);
                    return;
                }

                // Root stays forgiving for keyboard/controller integrations that select a cell
                // without first opening a menu.
                if (level.RuntimeCommandMenuPage == "root")
                {
                    if (occupant && occupant.HasComponent<TacticalUnitComponent>())
                    {
                        auto& targetUnit = occupant.GetComponent<TacticalUnitComponent>();
                        const auto* basic = TacticalCombatSkillService::FindSkill(selectedUnit.BasicSkillId);
                        if (basic && TacticalCombatBoardService::IsValidTarget(*basic, selectedUnit, targetUnit))
                        {
                            const std::string skillId = basic->Id;
                            TacticalCombatActionService::BeginAction(
                                scene, level, selected, skillId, occupant, TacticalCombatPhase::PlayerTurn);
                            return;
                        }
                    }

                    if (TacticalCombatBoardService::CanMoveTo(scene, level, selectedUnit, x, y))
                    {
                        selectedUnit.GridX = x;
                        selectedUnit.GridY = y;
                        selectedUnit.RuntimeMoved = true;
                        level.RuntimeMessage = selectedUnit.DisplayName + " 已移动，可以继续攻击或待机。";
                        TacticalCombatFeedbackService::PlaySound(
                            AssetAliasRegistry::Path("tactical.audio.move"), 0.38f);
                    }
                }
                return;
            }

            if (level.RuntimePhase != TacticalCombatPhase::Targeting)
                return;

            const auto* skill = TacticalCombatSkillService::FindSkill(level.RuntimeSelectedSkillId);
            if (!skill || !occupant || !occupant.HasComponent<TacticalUnitComponent>())
            {
                level.RuntimeMessage = "请选择有效目标。";
                return;
            }

            auto& targetUnit = occupant.GetComponent<TacticalUnitComponent>();
            if (!TacticalCombatBoardService::IsValidTarget(*skill, selectedUnit, targetUnit))
            {
                level.RuntimeMessage = "目标不在技能范围内。";
                return;
            }

            const std::string skillId = skill->Id;
            TacticalCombatActionService::BeginAction(
                scene, level, selected, skillId, occupant, TacticalCombatPhase::PlayerTurn);
        }

        static void HandleSkillCommand(Scene* scene,
            TacticalCombatLevelComponent& level,
            const std::string& slot)
        {
            Entity selected = GameplayEntityService::Resolve(scene, level.RuntimeSelectedUnit);
            if (!selected || !selected.HasComponent<TacticalUnitComponent>())
                return;

            auto& unit = selected.GetComponent<TacticalUnitComponent>();
            if (slot == "cancel")
            {
                if (level.RuntimePhase == TacticalCombatPhase::Targeting
                    || level.RuntimeCommandMenuPage != "root")
                {
                    ReturnToRootMenu(level);
                }
                else
                {
                    ResetToUnitSelection(level);
                }
                return;
            }

            if (slot == "wait" || slot == "guard")
            {
                level.RuntimeSelectedSkillId = "guard_wait";
                TacticalCombatActionService::BeginAction(
                    scene, level, selected, "guard_wait", selected, TacticalCombatPhase::PlayerTurn);
                return;
            }

            const std::optional<std::string> skillId =
                TacticalCombatSkillService::ResolvePlayerSkillId(unit, slot);
            const auto* skill = skillId ? TacticalCombatSkillService::FindSkill(*skillId) : nullptr;
            if (!skill)
            {
                level.RuntimeMessage = "这个技能槽没有可用技能。";
                return;
            }

            level.RuntimeSelectedSkillId = skill->Id;
            if (skill->TargetRule == TacticalCombatSkillService::TacticalTargetRule::Self)
            {
                const std::string actionSkillId = skill->Id;
                TacticalCombatActionService::BeginAction(
                    scene, level, selected, actionSkillId, selected, TacticalCombatPhase::PlayerTurn);
                return;
            }

            level.RuntimePhase = TacticalCombatPhase::Targeting;
            level.RuntimeMessage = "请选择 " + std::string(skill->DisplayName) + " 的目标。";
        }

    } // namespace

    void ProcessCommand(Scene* scene,
        TacticalCombatLevelComponent& level,
        const std::string& command)
    {
        const std::vector<std::string> parts = GameplayTextService::SplitCommand(command);
        if (parts.size() < 2 || parts[0] != "tactic")
            return;

        if (parts[1] == "cell" && parts.size() >= 4)
        {
            try
            {
                HandleCellCommand(scene, level, std::stoi(parts[2]), std::stoi(parts[3]));
            }
            catch (...) {}
            return;
        }

        if (parts[1] == "menu" && parts.size() >= 3
            && level.RuntimePhase == TacticalCombatPhase::AwaitCommand)
        {
            const std::string page = parts[2];
            if (page == "move" || page == "attack" || page == "skills" || page == "items")
            {
                level.RuntimeCommandMenuPage = page;
                level.RuntimeSelectedSkillId.clear();
                level.RuntimeMessage = "请选择" + page + "范围内的行动。";
            }
            return;
        }

        if (parts[1] == "cancel")
        {
            Entity selected = GameplayEntityService::Resolve(scene, level.RuntimeSelectedUnit);
            if (selected && selected.HasComponent<TacticalUnitComponent>())
                HandleSkillCommand(scene, level, "cancel");
            else
                ResetToUnitSelection(level);
            return;
        }

        if (parts[1] == "skill" && parts.size() >= 3
            && (level.RuntimePhase == TacticalCombatPhase::AwaitCommand
                || level.RuntimePhase == TacticalCombatPhase::Targeting))
        {
            HandleSkillCommand(scene, level, parts[2]);
        }
    }

} // namespace Wheatear::TacticalCombatCommandService
