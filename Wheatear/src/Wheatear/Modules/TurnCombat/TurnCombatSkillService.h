#pragma once

#include "TurnCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <optional>
#include <string>
#include <vector>

namespace Wheatear::TurnCombatSkillService {

    enum class TurnSkillCategory
    {
        Attack = 0,
        Guard = 1,
        Skill = 2,
        Item = 3,
        Wait = 4
    };

    enum class TurnStatusEffectKind
    {
        None = 0,
        Guard = 1,
        Regeneration = 2,
        Burn = 3,
        DefenseDown = 4,
        Stun = 5
    };

    struct TurnSkillDefinition
    {
        std::string Id;
        std::string DisplayName;
        std::string Description;
        std::string IconPath;
        std::string SoundPath;
        std::string EffectPath;
        TurnTargetRule TargetRule = TurnTargetRule::EnemySingle;
        float ManaCost = 0.0f;
        float Power = 1.0f;
        float HealPower = 0.0f;
        float DefensePierce = 0.0f;
        bool Magic = false;
        bool Guard = false;
        TurnSkillCategory Category = TurnSkillCategory::Skill;
        TurnStatusEffectKind AppliedEffect = TurnStatusEffectKind::None;
        int EffectTurns = 0;
        float EffectPower = 0.0f;
    };

    WHEATEAR_API const std::vector<TurnSkillDefinition>& SkillLibrary();
    WHEATEAR_API const TurnSkillDefinition* FindSkill(const std::string& id);
    WHEATEAR_API std::optional<std::string> ResolvePlayerSkillId(const TurnCombatantComponent& actor, const std::string& payload);
    WHEATEAR_API std::string ChooseEnemySkill(const TurnCombatantComponent& actor, int round);
    WHEATEAR_API float CalculateDamage(const TurnSkillDefinition& skill,
        const TurnCombatantComponent& actor,
        const TurnCombatantComponent& target);
    WHEATEAR_API float CalculateHeal(const TurnSkillDefinition& skill,
        const TurnCombatantComponent& actor);
    WHEATEAR_API float GetDefenseMultiplier(const TurnCombatantComponent& combatant);
    WHEATEAR_API float GetDamageTakenMultiplier(const TurnCombatantComponent& combatant);
    WHEATEAR_API bool HasStatusEffect(const TurnCombatantComponent& combatant, TurnStatusEffectKind effect);
    WHEATEAR_API std::string FormatStatusEffects(const TurnCombatantComponent& combatant);
    WHEATEAR_API void ApplyStatusEffect(TurnCombatantComponent& target,
        TurnStatusEffectKind effect,
        int turns,
        float power);
    WHEATEAR_API void TickStatusEffects(TurnCombatantComponent& combatant);

} // namespace Wheatear::TurnCombatSkillService
