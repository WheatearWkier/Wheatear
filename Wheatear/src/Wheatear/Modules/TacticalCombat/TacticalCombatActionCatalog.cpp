#include "wtpch.h"
#include "TacticalCombatActionCatalog.h"

#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"

namespace Wheatear::TacticalCombatActionCatalog {

    namespace {

        const char* CategoryTag(TacticalCombatSkillService::TacticalSkillCategory category)
        {
            using TacticalCombatSkillService::TacticalSkillCategory;
            switch (category)
            {
            case TacticalSkillCategory::Attack: return "Category.Attack";
            case TacticalSkillCategory::Guard: return "Category.Guard";
            case TacticalSkillCategory::Item: return "Category.Item";
            case TacticalSkillCategory::Wait: return "Category.Wait";
            case TacticalSkillCategory::Skill:
            default: return "Category.Skill";
            }
        }

        const char* TargetTag(TacticalCombatSkillService::TacticalTargetRule rule)
        {
            using TacticalCombatSkillService::TacticalTargetRule;
            switch (rule)
            {
            case TacticalTargetRule::Enemy: return "Target.Enemy";
            case TacticalTargetRule::Ally: return "Target.Ally";
            case TacticalTargetRule::Self: return "Target.Self";
            default: return "Target.Unknown";
            }
        }

        const char* StatusId(TacticalCombatSkillService::TacticalStatusEffectKind effect)
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

        void AddSkillEffects(WAO::ActionRecipe& recipe,
            const TacticalCombatSkillService::TacticalSkillDefinition& skill)
        {
            if (skill.HealPower > 0.0f)
            {
                WAO::EffectSpec heal;
                heal.Type = WAO::EffectType::Heal;
                heal.AttributeId = "magic";
                heal.Value = skill.HealPower;
                recipe.Effects.push_back(heal);
            }
            else if (skill.Power > 0.0f)
            {
                WAO::EffectSpec damage;
                damage.Type = WAO::EffectType::Damage;
                damage.AttributeId = skill.Magic ? "magic" : "attack";
                damage.Value = skill.Power;
                recipe.Effects.push_back(damage);
            }

            const char* status = StatusId(skill.AppliedEffect);
            if (status[0] != '\0' && skill.EffectTurns > 0)
            {
                WAO::EffectSpec state;
                state.Type = WAO::EffectType::AddState;
                state.StateId = status;
                state.Turns = skill.EffectTurns;
                state.Value = skill.EffectPower;
                state.DurationPolicy = WAO::EffectDurationPolicy::Turns;
                recipe.Effects.push_back(state);
            }
        }

    } // namespace

    std::string ActionRecipeId(const std::string& skillId)
    {
        return "tactical." + skillId;
    }

    WAO::ActionRecipe BuildActionRecipe(const TacticalCombatSkillService::TacticalSkillDefinition& skill)
    {
        WAO::ActionRecipe recipe;
        recipe.Id = ActionRecipeId(skill.Id);
        recipe.DisplayName = skill.DisplayName;
        recipe.Description = skill.Description;
        recipe.IconPath = skill.IconPath;
        recipe.AnimationId = std::string("tactical_") + skill.Id;
        recipe.SoundPath = skill.SoundPath;
        recipe.EffectPath = skill.EffectFramePattern;
        recipe.Duration = 0.62f;
        recipe.Startup = 0.12f;
        recipe.HitTime = 0.30f;
        recipe.Recovery = 0.20f;
        recipe.MovementScale = 0.0f;
        recipe.Tags = {
            "Gameplay.TacticalCombat",
            "Gameplay.Combat",
            "Target.Grid",
            CategoryTag(skill.Category),
            TargetTag(skill.TargetRule),
            "Range." + std::to_string(skill.Range)
        };
        if (skill.Magic)
            recipe.Tags.push_back("Skill.Magic");
        if (skill.Guard)
            recipe.Tags.push_back("Skill.Guard");
        if (skill.HealPower > 0.0f)
            recipe.Tags.push_back("Skill.Heal");

        AddSkillEffects(recipe, skill);
        recipe.Signals = { "tactical.skill.apply", "tactical.skill.vfx", "tactical.skill.sfx" };
        return recipe;
    }

    void RegisterActionRecipes()
    {
        for (const auto& skill : TacticalCombatSkillService::SkillLibrary())
            WAO::ActionDatabase::Register(BuildActionRecipe(skill));
    }

} // namespace Wheatear::TacticalCombatActionCatalog
