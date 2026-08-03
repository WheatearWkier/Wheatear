#include "wtpch.h"
#include "TacticalCombatUIService.h"

#include "TacticalCombatBoardService.h"
#include "TacticalCombatSkillService.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Modules/Common/GameplayEntityService.h"
#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <array>
#include <sstream>
#include <string>

namespace Wheatear::TacticalCombatUIService {

    namespace {

        struct CommandSlot
        {
            std::string Label;
            std::string Icon;
            std::string Command;
            bool Enabled = true;
        };

        static std::string MoveIcon() { return AssetAliasRegistry::Path("tactical.icon.move"); }
        static std::string AttackIcon() { return AssetAliasRegistry::Path("tactical.icon.attack"); }
        static std::string SkillIcon() { return AssetAliasRegistry::Path("tactical.icon.magic"); }
        static std::string ItemIcon() { return AssetAliasRegistry::Path("tactical.icon.item"); }
        static std::string WaitIcon() { return AssetAliasRegistry::Path("tactical.icon.guard"); }
        static std::string EmptyIcon() { return AssetAliasRegistry::Path("tactical.icon.empty"); }
        static std::string CancelIcon() { return AssetAliasRegistry::Path("tactical.icon.cancel"); }
        static std::string ButtonPanelPath() { return AssetAliasRegistry::Path("tactical.panel.button"); }

        static void EnsureWaitButton(Scene* scene)
        {
            Entity root = GameplayUILayoutService::EnsureUIWidget(
                scene,
                "TK_Command_5_Root",
                "TK_CommandPanel",
                { 0.05f, 0.845f },
                { 0.90f, 0.14f },
                24,
                true);
            if (!root)
                return;

            auto& image = root.HasComponent<UIImageComponent>()
                ? root.GetComponent<UIImageComponent>()
                : root.AddComponent<UIImageComponent>();
            UIRuntimeTools::SetImageTexture(
                scene,
                "TK_Command_5_Root",
                ButtonPanelPath(),
                true);

            auto& button = root.HasComponent<UIButtonComponent>()
                ? root.GetComponent<UIButtonComponent>()
                : root.AddComponent<UIButtonComponent>();
            button.NormalColor = { 0.06f, 0.12f, 0.15f, 0.08f };
            button.HoverColor = { 0.20f, 0.42f, 0.46f, 0.35f };
            button.PressedColor = { 0.08f, 0.20f, 0.24f, 0.45f };

            Entity icon = GameplayUILayoutService::EnsureUIWidget(
                scene, "TK_Command_5_Icon", "TK_Command_5_Root",
                { 0.08f, 0.08f }, { 0.18f, 0.84f }, 27, true);
            if (icon && !icon.HasComponent<UIImageComponent>())
                icon.AddComponent<UIImageComponent>();
            UIRuntimeTools::SetImageTexture(
                scene,
                "TK_Command_5_Icon",
                WaitIcon(),
                true);

            GameplayUILayoutService::EnsureText(
                scene, "TK_Command_5_Text", "TK_Command_5_Root",
                { 0.32f, 0.20f }, { 0.58f, 0.60f }, 28,
                "待机", 15.0f, { 0.92f, 0.94f, 0.86f, 1.0f });
        }

        static void SetCommandSlot(Scene* scene,
            int index,
            const CommandSlot& slot,
            bool visible)
        {
            const std::string prefix = "TK_Command_" + std::to_string(index);
            const bool show = visible && slot.Enabled && !slot.Command.empty();
            UIRuntimeTools::SetWidgetVisible(scene, prefix + "_Root", show);
            UIRuntimeTools::SetWidgetVisible(scene, prefix + "_Icon", show);
            UIRuntimeTools::SetWidgetVisible(scene, prefix + "_Text", show);
            if (!show)
                return;

            GameplayUILayoutService::SetButtonCommand(scene, prefix + "_Root", slot.Command);
            UIRuntimeTools::SetText(scene, prefix + "_Text", slot.Label);
            UIRuntimeTools::SetImageTexture(scene, prefix + "_Icon", slot.Icon, true);
        }

        static std::array<CommandSlot, 4> BuildRootSlots()
        {
            return {
                CommandSlot{ "移动", MoveIcon(), "tactic:menu:move" },
                CommandSlot{ "攻击", AttackIcon(), "tactic:menu:attack" },
                CommandSlot{ "技能", SkillIcon(), "tactic:menu:skills" },
                CommandSlot{ "道具", ItemIcon(), "tactic:menu:items" }
            };
        }

        static std::array<CommandSlot, 4> BuildSkillSlots(const TacticalUnitComponent& unit)
        {
            std::array<CommandSlot, 4> slots{};
            const std::string ids[] = { unit.BasicSkillId, unit.Skill1Id, unit.Skill2Id, "guard_wait" };
            const std::string commands[] = {
                "tactic:skill:slot0",
                "tactic:skill:slot1",
                "tactic:skill:slot2",
                "tactic:skill:wait"
            };

            for (int i = 0; i < 4; ++i)
            {
                const auto* skill = TacticalCombatSkillService::FindSkill(ids[i]);
                if (!skill)
                    continue;
                slots[i] = { skill->DisplayName, skill->IconPath, commands[i] };
            }
            return slots;
        }

        static std::array<CommandSlot, 4> BuildItemSlots()
        {
            return {
                CommandSlot{
                    "恢复药水",
                    ItemIcon(),
                    "tactic:skill:item0"
                },
                CommandSlot{
                    "空",
                    EmptyIcon(),
                    "",
                    false
                },
                CommandSlot{
                    "空",
                    EmptyIcon(),
                    "",
                    false
                },
                CommandSlot{
                    "返回",
                    CancelIcon(),
                    "tactic:cancel"
                }
            };
        }

        static void UpdateStatusPanel(Scene* scene, TacticalCombatLevelComponent& level)
        {
            for (Entity unitEntity : TacticalCombatBoardService::CollectUnits(scene))
            {
                auto& unit = unitEntity.GetComponent<TacticalUnitComponent>();
                UIRuntimeTools::SetProgress(scene, unit.HealthBarEntityName, unit.Health, unit.MaxHealth);

                std::ostringstream status;
                status << unit.DisplayName << " " << (int)unit.Health << "/" << (int)unit.MaxHealth;
                if (unit.RuntimeGuarding)
                    status << " 防御";
                const std::string effects = TacticalCombatSkillService::FormatStatusEffects(unit);
                if (!effects.empty())
                    status << " " << effects;
                if (unit.RuntimeHasActed && unit.RuntimeAlive)
                    status << " 已行动";
                if (!unit.RuntimeAlive)
                    status << " 倒下";
                UIRuntimeTools::SetText(scene, unit.StatusTextEntityName, status.str());
            }

            std::string phase = "战棋演示";
            switch (level.RuntimePhase)
            {
            case TacticalCombatPhase::Intro: phase = "准备"; break;
            case TacticalCombatPhase::PlayerTurn: phase = "我方回合"; break;
            case TacticalCombatPhase::AwaitCommand: phase = "选择行动"; break;
            case TacticalCombatPhase::Targeting: phase = "选择目标"; break;
            case TacticalCombatPhase::Acting: phase = "行动演出"; break;
            case TacticalCombatPhase::EnemyTurn: phase = "敌方回合"; break;
            case TacticalCombatPhase::Victory: phase = "胜利"; break;
            case TacticalCombatPhase::Defeat: phase = "失败"; break;
            }
            UIRuntimeTools::SetText(
                scene,
                level.PhaseTextEntityName,
                phase + "  第" + std::to_string(level.RuntimeRound) + "回合");
            UIRuntimeTools::SetText(scene, level.MessageTextEntityName, level.RuntimeMessage);
        }

        static bool IsAttackCell(const TacticalCombatLevelComponent& level,
            const TacticalUnitComponent& unit,
            int x,
            int y,
            int range)
        {
            return TacticalCombatBoardService::InBounds(level, x, y)
                && TacticalCombatBoardService::Distance(unit.GridX, unit.GridY, x, y) <= range;
        }

        static void UpdateCommandPanel(Scene* scene, TacticalCombatLevelComponent& level)
        {
            const bool commandVisible = level.RuntimePhase == TacticalCombatPhase::AwaitCommand
                || level.RuntimePhase == TacticalCombatPhase::Targeting;
            UIRuntimeTools::SetWidgetVisible(scene, level.CommandPanelEntityName, commandVisible);
            EnsureWaitButton(scene);

            Entity selected = GameplayEntityService::Resolve(scene, level.RuntimeSelectedUnit);
            if (!selected || !selected.HasComponent<TacticalUnitComponent>())
            {
                UIRuntimeTools::SetWidgetVisible(scene, "TK_CancelButton", false);
                UIRuntimeTools::SetText(scene, level.DetailTextEntityName, "请选择一个我方单位。");
                return;
            }

            const auto& unit = selected.GetComponent<TacticalUnitComponent>();
            std::array<CommandSlot, 4> slots = BuildRootSlots();
            if (level.RuntimeCommandMenuPage == "skills")
                slots = BuildSkillSlots(unit);
            else if (level.RuntimeCommandMenuPage == "items")
                slots = BuildItemSlots();
            if (level.RuntimePhase == TacticalCombatPhase::Targeting)
                slots = {};

            for (int i = 0; i < 4; ++i)
                SetCommandSlot(scene, i + 1, slots[i], commandVisible);

            const bool cancelVisible = commandVisible;
            Entity cancelButton = SceneQueries::FindEntityByName(scene, "TK_CancelButton");
            if (cancelButton)
            {
                Entity cancelIcon = GameplayUILayoutService::EnsureUIWidget(
                    scene,
                    "TK_CancelIcon",
                    "TK_CancelButton",
                    { 0.08f, 0.10f },
                    { 0.18f, 0.80f },
                    27,
                    cancelVisible);
                if (cancelIcon && !cancelIcon.HasComponent<UIImageComponent>())
                    cancelIcon.AddComponent<UIImageComponent>();
            }
            UIRuntimeTools::SetWidgetVisible(scene, "TK_CancelButton", cancelVisible);
            GameplayUILayoutService::SetButtonCommand(
                scene,
                "TK_CancelButton",
                "tactic:cancel");
            UIRuntimeTools::SetText(
                scene,
                "TK_CancelText",
                level.RuntimeCommandMenuPage == "root" ? "取消" : "返回");
            UIRuntimeTools::SetImageTexture(scene, "TK_CancelIcon", CancelIcon(), true);
            UIRuntimeTools::SetWidgetVisible(
                scene,
                "TK_Command_5_Root",
                commandVisible
                    && level.RuntimePhase == TacticalCombatPhase::AwaitCommand
                    && level.RuntimeCommandMenuPage == "root");
            UIRuntimeTools::SetWidgetVisible(
                scene,
                "TK_Command_5_Icon",
                commandVisible
                    && level.RuntimePhase == TacticalCombatPhase::AwaitCommand
                    && level.RuntimeCommandMenuPage == "root");
            UIRuntimeTools::SetWidgetVisible(
                scene,
                "TK_Command_5_Text",
                commandVisible
                    && level.RuntimePhase == TacticalCombatPhase::AwaitCommand
                    && level.RuntimeCommandMenuPage == "root");
            GameplayUILayoutService::SetButtonCommand(
                scene,
                "TK_Command_5_Root",
                "tactic:skill:wait");

            std::ostringstream detail;
            detail << unit.DisplayName << " / " << unit.ClassName << "\n";
            if (level.RuntimeCommandMenuPage == "move")
                detail << "移动范围 " << unit.MoveRange << "\n请选择蓝色格子。";
            else if (level.RuntimeCommandMenuPage == "attack")
            {
                const auto* basic = TacticalCombatSkillService::FindSkill(unit.BasicSkillId);
                detail << "攻击范围 " << (basic ? basic->Range : unit.AttackRange)
                    << "\n请选择红色格子。";
            }
            else if (level.RuntimePhase == TacticalCombatPhase::Targeting)
            {
                const auto* skill = TacticalCombatSkillService::FindSkill(level.RuntimeSelectedSkillId);
                if (skill)
                    detail << skill->DisplayName << "  范围 " << skill->Range
                        << "\n" << skill->Description;
            }
            else
            {
                detail << "请选择移动、攻击、技能、道具或待机。";
            }
            UIRuntimeTools::SetText(scene, level.DetailTextEntityName, detail.str());
        }

    } // namespace

    void UpdateTiles(Scene* scene, TacticalCombatLevelComponent& level)
    {
        Entity selected = GameplayEntityService::Resolve(scene, level.RuntimeSelectedUnit);
        const TacticalUnitComponent* selectedUnit = selected
            && selected.HasComponent<TacticalUnitComponent>()
            ? &selected.GetComponent<TacticalUnitComponent>()
            : nullptr;
        const auto* selectedSkill = TacticalCombatSkillService::FindSkill(level.RuntimeSelectedSkillId);

        for (int y = 0; y < level.GridHeight; ++y)
        {
            for (int x = 0; x < level.GridWidth; ++x)
            {
                glm::vec4 color = level.TileNormalColor;
                if (selectedUnit)
                {
                    if (x == selectedUnit->GridX && y == selectedUnit->GridY)
                    {
                        color = level.TileSelectedColor;
                    }
                    else if (level.RuntimePhase == TacticalCombatPhase::Targeting && selectedSkill)
                    {
                        Entity occupant = TacticalCombatBoardService::FindUnitAt(scene, x, y);
                        if (occupant && occupant.HasComponent<TacticalUnitComponent>()
                            && TacticalCombatBoardService::IsValidTarget(
                                *selectedSkill,
                                *selectedUnit,
                                occupant.GetComponent<TacticalUnitComponent>()))
                        {
                            color = selectedSkill->TargetRule
                                == TacticalCombatSkillService::TacticalTargetRule::Ally
                                ? level.TileMoveColor
                                : level.TileAttackColor;
                        }
                    }
                    else if (level.RuntimePhase == TacticalCombatPhase::AwaitCommand)
                    {
                        if (level.RuntimeCommandMenuPage == "move"
                            && TacticalCombatBoardService::CanMoveTo(
                                scene, level, *selectedUnit, x, y))
                        {
                            color = level.TileMoveColor;
                        }
                        else if (level.RuntimeCommandMenuPage == "attack")
                        {
                            const auto* basic =
                                TacticalCombatSkillService::FindSkill(selectedUnit->BasicSkillId);
                            const int range = basic ? basic->Range : selectedUnit->AttackRange;
                            if (IsAttackCell(level, *selectedUnit, x, y, range))
                                color = level.TileAttackColor;
                        }
                        else if (level.RuntimeCommandMenuPage == "root")
                        {
                            if (TacticalCombatBoardService::CanMoveTo(
                                scene, level, *selectedUnit, x, y))
                                color = level.TileMoveColor;

                            const auto* basic =
                                TacticalCombatSkillService::FindSkill(selectedUnit->BasicSkillId);
                            const int range = basic ? basic->Range : selectedUnit->AttackRange;
                            if (IsAttackCell(level, *selectedUnit, x, y, range))
                                color = level.TileAttackColor;
                        }
                    }
                }

                UIRuntimeTools::SetImageColor(
                    scene, TacticalCombatBoardService::CellTag(level, x, y), color);
            }
        }
    }

    void UpdateBattleUI(Scene* scene, TacticalCombatLevelComponent& level)
    {
        UpdateTiles(scene, level);
        UpdateStatusPanel(scene, level);
        UpdateCommandPanel(scene, level);
    }

    void UpdateStatusUI(Scene* scene, TacticalCombatLevelComponent& level)
    {
        UpdateStatusPanel(scene, level);
    }

    void UpdateCommandUI(Scene* scene, TacticalCombatLevelComponent& level)
    {
        UpdateCommandPanel(scene, level);
    }

} // namespace Wheatear::TacticalCombatUIService
