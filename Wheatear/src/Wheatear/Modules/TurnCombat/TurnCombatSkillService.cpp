#include "wtpch.h"
#include "TurnCombatSkillService.h"

#include "Wheatear/Gameplay/Action/StateRegistry.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Wheatear::TurnCombatSkillService {

    const std::vector<TurnSkillDefinition>& SkillLibrary()
    {
        static const std::vector<TurnSkillDefinition> skills = {
            {
                "slash", "魔剑斩", "快速的单体魔剑斩击。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_attack.png",
                "assets/vertical_slice/turn_combat/audio/turn_slash.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_slash.png",
                TurnTargetRule::EnemySingle, 0.0f, 1.00f, 0.0f, 0.10f, false, false,
                TurnSkillCategory::Attack
            },
            {
                "aether_edge", "灵素剑锋", "消耗魔力发动更重的剑与魔法合击。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_magic_sword.png",
                "assets/vertical_slice/turn_combat/audio/turn_magic.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_magic_sword.png",
                TurnTargetRule::EnemySingle, 8.0f, 1.45f, 0.0f, 0.22f, true, false,
                TurnSkillCategory::Skill, TurnStatusEffectKind::DefenseDown, 2, 0.22f
            },
            {
                "white_vow", "白誓治愈", "治疗一名我方角色，并施加短暂再生。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_heal.png",
                "assets/vertical_slice/turn_combat/audio/turn_heal.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_heal.png",
                TurnTargetRule::AllySingle, 7.0f, 0.0f, 1.25f, 0.0f, true, false,
                TurnSkillCategory::Skill, TurnStatusEffectKind::Regeneration, 2, 10.0f
            },
            {
                "shield_oath", "守护誓约", "本回合进入防御，降低受到的伤害。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_guard.png",
                "assets/vertical_slice/turn_combat/audio/turn_guard.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_guard.png",
                TurnTargetRule::Self, 4.0f, 0.0f, 0.0f, 0.0f, false, true,
                TurnSkillCategory::Guard, TurnStatusEffectKind::Guard, 1, 0.45f
            },
            {
                "black_flare", "黑炎爆发", "不稳定的暗魔法，攻击所有敌人并施加燃烧。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_dark.png",
                "assets/vertical_slice/turn_combat/audio/turn_magic.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_dark.png",
                TurnTargetRule::EnemyAll, 12.0f, 0.86f, 0.0f, 0.18f, true, false,
                TurnSkillCategory::Skill, TurnStatusEffectKind::Burn, 2, 8.0f
            },
            {
                "healing_potion", "恢复药水", "消耗道具，恢复一名我方角色的生命。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_item_potion.png",
                "assets/vertical_slice/turn_combat/audio/turn_heal.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_heal.png",
                TurnTargetRule::AllySingle, 0.0f, 0.0f, 0.85f, 0.0f, false, false,
                TurnSkillCategory::Item
            },
            {
                "focus_wait", "冥想", "跳过回合并恢复魔力。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_wait.png",
                "assets/vertical_slice/turn_combat/audio/turn_guard.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_focus.png",
                TurnTargetRule::Self, 0.0f, 0.0f, 0.0f, 0.0f, false, false,
                TurnSkillCategory::Wait
            },
            {
                "claw", "爪击", "魔物的近身攻击。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_enemy_claw.png",
                "assets/vertical_slice/turn_combat/audio/turn_hit.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_claw.png",
                TurnTargetRule::EnemySingle, 0.0f, 0.95f, 0.0f, 0.0f, false, false,
                TurnSkillCategory::Attack
            },
            {
                "wild_pounce", "猛扑", "魔物的强力突进攻击。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_enemy_claw.png",
                "assets/vertical_slice/turn_combat/audio/turn_hit.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_claw.png",
                TurnTargetRule::EnemySingle, 0.0f, 1.25f, 0.0f, 0.08f, false, false,
                TurnSkillCategory::Attack
            },
            {
                "dark_orb", "暗影魔弹", "精英法师发射的暗属性魔法弹。",
                "assets/vertical_slice/turn_combat/ui/icons/cmd_enemy_orb.png",
                "assets/vertical_slice/turn_combat/audio/turn_magic.wav",
                "assets/vertical_slice/turn_combat/effects/vfx_turn_dark.png",
                TurnTargetRule::EnemySingle, 0.0f, 1.18f, 0.0f, 0.20f, true, false,
                TurnSkillCategory::Skill, TurnStatusEffectKind::Burn, 2, 7.0f
            }
        };
        return skills;
    }

    const TurnSkillDefinition* FindSkill(const std::string& id)
    {
        const auto& skills = SkillLibrary();
        const auto it = std::find_if(skills.begin(), skills.end(),
            [&](const TurnSkillDefinition& skill) { return skill.Id == id; });
        return it == skills.end() ? nullptr : &(*it);
    }

    std::optional<std::string> ResolvePlayerSkillId(
        const TurnCombatantComponent& actor,
        const std::string& payload)
    {
        if (payload == "basic" || payload == "slot0")
            return actor.BasicSkillId;
        if (payload == "slot1")
            return actor.Skill1Id;
        if (payload == "slot2")
            return actor.Skill2Id;
        if (payload == "slot3")
            return actor.Skill3Id;
        if (payload == "item0" || payload == "potion")
            return std::string("healing_potion");
        if (FindSkill(payload))
            return payload;
        return std::nullopt;
    }

    std::string ChooseEnemySkill(const TurnCombatantComponent& actor, int round)
    {
        if (!actor.Skill2Id.empty() && round % 3 == 0)
            return actor.Skill2Id;
        if (!actor.Skill1Id.empty() && round % 2 == 0)
            return actor.Skill1Id;
        if (!actor.BasicSkillId.empty())
            return actor.BasicSkillId;
        return "claw";
    }

    float CalculateDamage(const TurnSkillDefinition& skill,
        const TurnCombatantComponent& actor,
        const TurnCombatantComponent& target)
    {
        const float offense = skill.Magic ? actor.Magic : actor.Attack;
        const float defense = std::max(
            0.0f,
            target.Defense * GetDefenseMultiplier(target)
                * (1.0f - skill.DefensePierce));
        float damage = offense * skill.Power + 6.0f - defense * 0.62f;
        damage *= GetDamageTakenMultiplier(target);
        return std::max(1.0f, std::round(damage));
    }

    float CalculateHeal(const TurnSkillDefinition& skill,
        const TurnCombatantComponent& actor)
    {
        return std::max(1.0f, std::round(actor.Magic * skill.HealPower + 18.0f));
    }

    static const char* StatusEffectId(TurnStatusEffectKind effect)
    {
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

    bool HasStatusEffect(const TurnCombatantComponent& combatant,
        TurnStatusEffectKind effect)
    {
        return WAO::HasState(combatant.RuntimeStatusEffects, StatusEffectId(effect));
    }

    float GetDefenseMultiplier(const TurnCombatantComponent& combatant)
    {
        return WAO::CalculateDefenseMultiplier(combatant.RuntimeStatusEffects);
    }

    float GetDamageTakenMultiplier(const TurnCombatantComponent& combatant)
    {
        return WAO::CalculateDamageTakenMultiplier(
            combatant.RuntimeStatusEffects,
            combatant.RuntimeGuarding,
            0.45f);
    }

    std::string FormatStatusEffects(const TurnCombatantComponent& combatant)
    {
        return WAO::FormatStates(combatant.RuntimeStatusEffects);
    }

    void ApplyStatusEffect(TurnCombatantComponent& target,
        TurnStatusEffectKind effect,
        int turns,
        float power)
    {
        if (effect == TurnStatusEffectKind::None || turns <= 0)
            return;

        WAO::ApplyState(
            target.RuntimeStatusEffects,
            WAO::MakeState(StatusEffectId(effect), turns, power));
    }

    void TickStatusEffects(TurnCombatantComponent& combatant)
    {
        const WAO::StateTickResult result = WAO::TickTurnStates(
            combatant.RuntimeStatusEffects,
            combatant.Health,
            combatant.MaxHealth);
        if (result.Killed)
            combatant.RuntimeAlive = false;
    }

} // namespace Wheatear::TurnCombatSkillService
