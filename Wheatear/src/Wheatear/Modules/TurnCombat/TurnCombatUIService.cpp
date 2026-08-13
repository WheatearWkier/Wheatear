#include "wtpch.h"
#include "TurnCombatUIService.h"

#include "TurnCombatSkillService.h"
#include "TurnCombatTargetService.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Services/GameplayEntityService.h"
#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <array>
#include <sstream>
#include <string>

namespace Wheatear::TurnCombatUIService {

    namespace {

        struct CommandSlot
        {
            std::string Label;
            std::string Icon;
            std::string Command;
            bool Enabled = true;
        };

        static std::string AttackIcon() { return AssetAliasRegistry::Path("turn.icon.attack"); }
        static std::string GuardIcon() { return AssetAliasRegistry::Path("turn.icon.guard"); }
        static std::string SkillIcon() { return AssetAliasRegistry::Path("turn.icon.magic_sword"); }
        static std::string ItemIcon() { return AssetAliasRegistry::Path("turn.icon.item_potion"); }
        static std::string EmptyItemIcon() { return AssetAliasRegistry::Path("turn.icon.item_empty"); }
        static std::string CancelIcon() { return AssetAliasRegistry::Path("turn.icon.cancel"); }
        static std::string WaitIcon() { return AssetAliasRegistry::Path("turn.icon.wait"); }

        static void SetCommandSlot(Scene* scene, int index, const CommandSlot& slot, bool visible)
        {
            const std::string prefix = "TC_Command_" + std::to_string(index);
            const bool show = visible && slot.Enabled && !slot.Command.empty();
            UIRuntimeTools::SetWidgetVisible(scene, prefix + "_Root", show);
            UIRuntimeTools::SetWidgetVisible(scene, prefix + "_Icon", show);
            UIRuntimeTools::SetWidgetVisible(scene, prefix + "_Text", show);
            if (!show)
                return;

            GameplayUILayoutService::SetButtonCommand(scene, prefix + "_Root", slot.Command);
            UIRuntimeTools::SetText(scene, prefix + "_Text", slot.Label);
            UIRuntimeTools::SetImageTexture(scene, prefix + "_Icon", slot.Icon, true);

            Entity icon = SceneQueries::FindEntityByName(scene, prefix + "_Icon");
            if (icon && icon.HasComponent<UIImageComponent>())
                icon.GetComponent<UIImageComponent>().Color = slot.Enabled
                    ? glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
                    : glm::vec4{ 0.35f, 0.38f, 0.42f, 0.82f };
        }

        static void UpdateStatusUI(Scene* scene, TurnCombatLevelComponent& level)
        {
            Entity active = GameplayEntityService::Resolve(scene, level.RuntimeActiveActor);
            const auto* selectedSkill =
                TurnCombatSkillService::FindSkill(level.RuntimeSelectedSkillId);
            const TurnCombatantComponent* activeCombatant =
                (active && active.HasComponent<TurnCombatantComponent>())
                ? &active.GetComponent<TurnCombatantComponent>()
                : nullptr;

            for (Entity entity : TurnCombatTargetService::CollectCombatants(scene))
            {
                auto& combatant = entity.GetComponent<TurnCombatantComponent>();
                UIRuntimeTools::SetProgress(
                    scene, combatant.HealthBarEntityName, combatant.Health, combatant.MaxHealth);
                UIRuntimeTools::SetProgress(
                    scene, combatant.ManaBarEntityName, combatant.Mana, combatant.MaxMana);

                std::ostringstream status;
                status << combatant.DisplayName << "  生命 "
                    << (int)combatant.Health << "/" << (int)combatant.MaxHealth;
                if (combatant.Team == (int)TurnCombatTeam::Player)
                    status << "  魔力 " << (int)combatant.Mana << "/" << (int)combatant.MaxMana;
                if (combatant.RuntimeGuarding)
                    status << "  防御";
                const std::string effects =
                    TurnCombatSkillService::FormatStatusEffects(combatant);
                if (!effects.empty())
                    status << "  " << effects;
                if (!combatant.RuntimeAlive)
                    status << "  倒下";
                UIRuntimeTools::SetText(scene, combatant.StatusTextEntityName, status.str());

                const bool targetVisible =
                    level.RuntimePhase == TurnCombatPhase::AwaitTarget
                    && selectedSkill
                    && activeCombatant
                    && TurnCombatTargetService::IsValidTarget(
                        *selectedSkill,
                        *activeCombatant,
                        combatant);
                UIRuntimeTools::SetWidgetVisible(
                    scene, combatant.TargetButtonEntityName, targetVisible);
                UIRuntimeTools::SetWidgetVisible(
                    scene, combatant.TargetMarkerEntityName, targetVisible);
            }
        }

        static std::array<CommandSlot, 5> BuildRootSlots()
        {
            return {
                CommandSlot{ "攻击", AttackIcon(), "turn:skill:basic" },
                CommandSlot{ "防守", GuardIcon(), "turn:guard" },
                CommandSlot{ "技能", SkillIcon(), "turn:menu:skills" },
                CommandSlot{ "道具", ItemIcon(), "turn:menu:items" },
                CommandSlot{ "取消", CancelIcon(), "turn:cancel" }
            };
        }

        static std::array<CommandSlot, 5> BuildSkillSlots(const TurnCombatantComponent& combatant)
        {
            const std::string ids[] = {
                combatant.Skill1Id,
                combatant.Skill2Id,
                combatant.Skill3Id,
                combatant.BasicSkillId
            };

            std::array<CommandSlot, 5> slots{};
            for (int i = 0; i < 4; ++i)
            {
                const auto* skill = TurnCombatSkillService::FindSkill(ids[i]);
                if (!skill)
                    continue;
                slots[i] = {
                    skill->DisplayName,
                    skill->IconPath,
                    "turn:skill:" + ids[i],
                    combatant.Mana >= skill->ManaCost
                };
            }
            slots[4] = { "返回", CancelIcon(), "turn:cancel" };
            return slots;
        }

        static std::array<CommandSlot, 5> BuildItemSlots()
        {
            return {
                CommandSlot{ "恢复药水", ItemIcon(), "turn:item:potion" },
                CommandSlot{
                    "空",
                    EmptyItemIcon(),
                    "",
                    false
                },
                CommandSlot{
                    "空",
                    EmptyItemIcon(),
                    "",
                    false
                },
                CommandSlot{ "冥想", WaitIcon(), "turn:wait" },
                CommandSlot{ "返回", CancelIcon(), "turn:cancel" }
            };
        }

        static void UpdateCommandUI(Scene* scene, TurnCombatLevelComponent& level)
        {
            const bool commandVisible = level.RuntimePhase == TurnCombatPhase::AwaitCommand
                || level.RuntimePhase == TurnCombatPhase::AwaitTarget;
            UIRuntimeTools::SetWidgetVisible(scene, level.CommandPanelEntityName, commandVisible);
            UIRuntimeTools::SetWidgetVisible(
                scene,
                level.TargetHintTextEntityName,
                level.RuntimePhase == TurnCombatPhase::AwaitTarget);

            Entity actor = GameplayEntityService::Resolve(scene, level.RuntimeActiveActor);
            if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                return;

            const auto& combatant = actor.GetComponent<TurnCombatantComponent>();
            std::array<CommandSlot, 5> slots = BuildRootSlots();
            if (level.RuntimePhase == TurnCombatPhase::AwaitTarget)
            {
                slots = {};
                slots[4] = { "返回", CancelIcon(), "turn:cancel" };
            }
            else if (level.RuntimeCommandMenuPage == "skills")
            {
                slots = BuildSkillSlots(combatant);
            }
            else if (level.RuntimeCommandMenuPage == "items")
            {
                slots = BuildItemSlots();
            }

            for (int i = 0; i < 5; ++i)
                SetCommandSlot(scene, i + 1, slots[i], commandVisible);

            const auto* selected = TurnCombatSkillService::FindSkill(
                level.RuntimeSelectedSkillId.empty()
                    ? combatant.BasicSkillId
                    : level.RuntimeSelectedSkillId);
            if (selected)
            {
                std::ostringstream details;
                details << selected->DisplayName << "  消耗魔力 "
                    << (int)selected->ManaCost << "\n"
                    << selected->Description;
                UIRuntimeTools::SetText(scene, level.SkillDetailTextEntityName, details.str());
            }
        }

    } // namespace

    void UpdateBattleUI(Scene* scene, TurnCombatLevelComponent& level)
    {
        UIRuntimeTools::SetText(scene, level.MessageTextEntityName, level.RuntimeMessage);

        Entity active = GameplayEntityService::Resolve(scene, level.RuntimeActiveActor);
        if (active && active.HasComponent<TurnCombatantComponent>())
        {
            const auto& combatant = active.GetComponent<TurnCombatantComponent>();
            UIRuntimeTools::SetText(
                scene,
                level.ActiveActorTextEntityName,
                "行动中：" + combatant.DisplayName);
        }
        else
        {
            UIRuntimeTools::SetText(scene, level.ActiveActorTextEntityName, "行动中：-");
        }

        UIRuntimeTools::SetText(
            scene,
            level.TurnOrderTextEntityName,
            TurnCombatTargetService::JoinTurnOrder(scene, level));
        UpdateStatusUI(scene, level);
        UpdateCommandUI(scene, level);
    }

} // namespace Wheatear::TurnCombatUIService
