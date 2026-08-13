#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"

#include <optional>
#include <string>
#include <vector>

namespace Wheatear::TacticalCombatSkillService {

    enum class TacticalSkillCategory
    {
        Attack = 0,
        Guard = 1,
        Skill = 2,
        Item = 3,
        Wait = 4
    };

    enum class TacticalStatusEffectKind
    {
        None = 0,
        Guard = 1,
        Regeneration = 2,
        Burn = 3,
        DefenseDown = 4,
        Stun = 5
    };

    enum class TacticalTargetRule
    {
        Enemy = 0,
        Ally = 1,
        Self = 2
    };

    struct TacticalSkillDefinition
    {
        std::string Id;
        std::string DisplayName;
        std::string Description;
        std::string IconPath;
        std::string SoundPath;
        std::string EffectFramePattern;
        GameplayVisualService::TextureAtlasFrameSpec EffectAtlas;
        int EffectFrameCount = 1;
        float EffectFrameRate = 12.0f;
        TacticalTargetRule TargetRule = TacticalTargetRule::Enemy;
        int Range = 1;
        float Power = 1.0f;
        float HealPower = 0.0f;
        float DefensePierce = 0.0f;
        bool Magic = false;
        bool Guard = false;
        TacticalSkillCategory Category = TacticalSkillCategory::Skill;
        TacticalStatusEffectKind AppliedEffect = TacticalStatusEffectKind::None;
        int EffectTurns = 0;
        float EffectPower = 0.0f;
    };

    WHEATEAR_API const std::vector<TacticalSkillDefinition>& SkillLibrary();
    WHEATEAR_API const TacticalSkillDefinition* FindSkill(const std::string& id);
    WHEATEAR_API std::optional<std::string> ResolvePlayerSkillId(const TacticalUnitComponent& unit, const std::string& slot);
    WHEATEAR_API std::string FormatFramePath(const std::string& pattern, int frameIndex);
    WHEATEAR_API float CalculateDamage(const TacticalSkillDefinition& skill,
        const TacticalUnitComponent& actor,
        const TacticalUnitComponent& target);
    WHEATEAR_API float CalculateHeal(const TacticalSkillDefinition& skill,
        const TacticalUnitComponent& actor);
    WHEATEAR_API float GetDefenseMultiplier(const TacticalUnitComponent& unit);
    WHEATEAR_API float GetDamageTakenMultiplier(const TacticalUnitComponent& unit);
    WHEATEAR_API bool HasStatusEffect(const TacticalUnitComponent& unit, TacticalStatusEffectKind effect);
    WHEATEAR_API std::string FormatStatusEffects(const TacticalUnitComponent& unit);
    WHEATEAR_API void ApplyStatusEffect(TacticalUnitComponent& target,
        TacticalStatusEffectKind effect,
        int turns,
        float power);
    WHEATEAR_API void TickStatusEffects(TacticalUnitComponent& unit);

} // namespace Wheatear::TacticalCombatSkillService
