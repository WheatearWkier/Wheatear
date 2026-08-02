#include "wtpch.h"
#include "TacticalCombatSkillService.h"

#include "Wheatear/Gameplay/Action/StateRegistry.h"
#include "Wheatear/Modules/Common/GameplayTextService.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Wheatear::TacticalCombatSkillService {

    const std::vector<TacticalSkillDefinition>& SkillLibrary()
    {
        static const std::vector<TacticalSkillDefinition> skills = {
            {
                "sword_slash", "魔剑斩", "近战单体斩击。",
                "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_attack.png",
                "assets/vertical_slice/tactical_combat/audio/tac_slash.wav",
                "assets/vertical_slice/tactical_combat/effects/vfx_tac_slash_{frame2}.png",
                5, 14.0f, TacticalTargetRule::Enemy, 1, 1.00f, 0.0f, 0.05f, false, false,
                TacticalSkillCategory::Attack
            },
            {
                "aether_lance", "灵枪", "三格射程的魔法突刺，并施加破甲。",
                "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_magic.png",
                "assets/vertical_slice/tactical_combat/audio/tac_magic.wav",
                "assets/vertical_slice/tactical_combat/effects/vfx_tac_magic_{frame2}.png",
                6, 14.0f, TacticalTargetRule::Enemy, 3, 1.25f, 0.0f, 0.22f, true, false,
                TacticalSkillCategory::Skill, TacticalStatusEffectKind::DefenseDown, 2, 0.22f
            },
            {
                "white_pulse", "白脉", "治疗三格内的一名我方单位，并施加再生。",
                "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_heal.png",
                "assets/vertical_slice/tactical_combat/audio/tac_heal.wav",
                "assets/vertical_slice/tactical_combat/effects/vfx_tac_heal_{frame2}.png",
                6, 12.0f, TacticalTargetRule::Ally, 3, 0.0f, 1.05f, 0.0f, true, false,
                TacticalSkillCategory::Skill, TacticalStatusEffectKind::Regeneration, 2, 8.0f
            },
            {
                "guard_wait", "守备", "结束行动并进入防御姿态。",
                "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_guard.png",
                "assets/vertical_slice/tactical_combat/audio/tac_guard.wav",
                "assets/vertical_slice/tactical_combat/effects/vfx_tac_guard_{frame2}.png",
                4, 10.0f, TacticalTargetRule::Self, 0, 0.0f, 0.0f, 0.0f, false, true,
                TacticalSkillCategory::Guard, TacticalStatusEffectKind::Guard, 1, 0.55f
            },
            {
                "tactical_potion", "恢复药水", "恢复三格内的一名我方单位。",
                "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_item.png",
                "assets/vertical_slice/tactical_combat/audio/tac_heal.wav",
                "assets/vertical_slice/tactical_combat/effects/vfx_tac_heal_{frame2}.png",
                6, 12.0f, TacticalTargetRule::Ally, 3, 0.0f, 0.85f, 0.0f, false, false,
                TacticalSkillCategory::Item
            },
            {
                "enemy_strike", "敌方斩击", "敌方近战攻击。",
                "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_enemy.png",
                "assets/vertical_slice/tactical_combat/audio/tac_hit.wav",
                "assets/vertical_slice/tactical_combat/effects/vfx_tac_slash_{frame2}.png",
                5, 14.0f, TacticalTargetRule::Enemy, 1, 0.95f, 0.0f, 0.0f, false, false,
                TacticalSkillCategory::Attack
            },
            {
                "enemy_dark", "敌方暗术", "敌方远程暗属性魔法，并施加燃烧。",
                "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_enemy_magic.png",
                "assets/vertical_slice/tactical_combat/audio/tac_magic.wav",
                "assets/vertical_slice/tactical_combat/effects/vfx_tac_dark_{frame2}.png",
                6, 14.0f, TacticalTargetRule::Enemy, 3, 1.12f, 0.0f, 0.18f, true, false,
                TacticalSkillCategory::Skill, TacticalStatusEffectKind::Burn, 2, 6.0f
            }
        };
        return skills;
    }

    const TacticalSkillDefinition* FindSkill(const std::string& id)
    {
        const auto& skills = SkillLibrary();
        const auto it = std::find_if(skills.begin(), skills.end(),
            [&](const TacticalSkillDefinition& skill) { return skill.Id == id; });
        return it == skills.end() ? nullptr : &(*it);
    }

    std::optional<std::string> ResolvePlayerSkillId(
        const TacticalUnitComponent& unit,
        const std::string& slot)
    {
        if (slot == "slot0" || slot == "basic")
            return unit.BasicSkillId;
        if (slot == "slot1")
            return unit.Skill1Id;
        if (slot == "slot2")
            return unit.Skill2Id;
        if (slot == "item0" || slot == "potion")
            return std::string("tactical_potion");
        if (slot == "guard" || slot == "wait")
            return std::string("guard_wait");
        if (FindSkill(slot))
            return slot;
        return std::nullopt;
    }

    std::string FormatFramePath(const std::string& pattern, int frameIndex)
    {
        return GameplayTextService::FormatFramePath(pattern, frameIndex + 1);
    }

    float CalculateDamage(const TacticalSkillDefinition& skill,
        const TacticalUnitComponent& actor,
        const TacticalUnitComponent& target)
    {
        const float offense = skill.Magic ? actor.Magic : actor.Attack;
        const float defense = target.Defense
            * GetDefenseMultiplier(target)
            * (skill.Magic ? 0.45f : 1.0f)
            * (1.0f - skill.DefensePierce);
        float damage = std::max(6.0f, offense * skill.Power - defense);
        damage *= GetDamageTakenMultiplier(target);
        if (target.Invulnerable)
            damage = 0.0f;
        return damage;
    }

    float CalculateHeal(const TacticalSkillDefinition& skill,
        const TacticalUnitComponent& actor)
    {
        return std::max(8.0f, actor.Magic * skill.HealPower);
    }

    static const char* StatusEffectId(TacticalStatusEffectKind effect)
    {
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

    bool HasStatusEffect(const TacticalUnitComponent& unit,
        TacticalStatusEffectKind effect)
    {
        return WAO::HasState(unit.RuntimeStatusEffects, StatusEffectId(effect));
    }

    float GetDefenseMultiplier(const TacticalUnitComponent& unit)
    {
        return WAO::CalculateDefenseMultiplier(unit.RuntimeStatusEffects);
    }

    float GetDamageTakenMultiplier(const TacticalUnitComponent& unit)
    {
        return WAO::CalculateDamageTakenMultiplier(
            unit.RuntimeStatusEffects,
            unit.RuntimeGuarding,
            0.55f);
    }

    std::string FormatStatusEffects(const TacticalUnitComponent& unit)
    {
        return WAO::FormatStates(unit.RuntimeStatusEffects);
    }

    void ApplyStatusEffect(TacticalUnitComponent& target,
        TacticalStatusEffectKind effect,
        int turns,
        float power)
    {
        if (effect == TacticalStatusEffectKind::None || turns <= 0)
            return;

        WAO::ApplyState(
            target.RuntimeStatusEffects,
            WAO::MakeState(StatusEffectId(effect), turns, power));
    }

    void TickStatusEffects(TacticalUnitComponent& unit)
    {
        const WAO::StateTickResult result = WAO::TickTurnStates(
            unit.RuntimeStatusEffects,
            unit.Health,
            unit.MaxHealth);
        if (result.Killed)
            unit.RuntimeAlive = false;
    }

} // namespace Wheatear::TacticalCombatSkillService
