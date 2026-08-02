#include "wtpch.h"
#include "TurnCombatActionCatalog.h"

#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"

namespace Wheatear::TurnCombatActionCatalog {

    namespace {

        const char* CategoryTag(TurnCombatSkillService::TurnSkillCategory category)
        {
            using TurnCombatSkillService::TurnSkillCategory;
            switch (category)
            {
            case TurnSkillCategory::Attack: return "Category.Attack";
            case TurnSkillCategory::Guard: return "Category.Guard";
            case TurnSkillCategory::Item: return "Category.Item";
            case TurnSkillCategory::Wait: return "Category.Wait";
            case TurnSkillCategory::Skill:
            default: return "Category.Skill";
            }
        }

        const char* TargetTag(TurnTargetRule rule)
        {
            switch (rule)
            {
            case TurnTargetRule::EnemySingle: return "Target.EnemySingle";
            case TurnTargetRule::AllySingle: return "Target.AllySingle";
            case TurnTargetRule::Self: return "Target.Self";
            case TurnTargetRule::EnemyAll: return "Target.EnemyAll";
            case TurnTargetRule::AllyAll: return "Target.AllyAll";
            default: return "Target.Unknown";
            }
        }

        const char* StatusId(TurnCombatSkillService::TurnStatusEffectKind effect)
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

        void AddSkillEffects(WAO::ActionRecipe& recipe,
            const TurnCombatSkillService::TurnSkillDefinition& skill)
        {
            if (skill.ManaCost > 0.0f)
                recipe.ResourceCost["mana"] = skill.ManaCost;

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

            if (std::string(skill.Id) == "focus_wait")
            {
                WAO::EffectSpec restore;
                restore.Type = WAO::EffectType::ModifyAttribute;
                restore.AttributeId = "mana";
                restore.Value = 10.0f;
                recipe.Effects.push_back(restore);
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
        return "turn." + skillId;
    }

    WAO::ActionRecipe BuildActionRecipe(const TurnCombatSkillService::TurnSkillDefinition& skill)
    {
        WAO::ActionRecipe recipe;
        recipe.Id = ActionRecipeId(skill.Id);
        recipe.DisplayName = skill.DisplayName;
        recipe.Description = skill.Description;
        recipe.IconPath = skill.IconPath;
        recipe.AnimationId = std::string("turn_") + skill.Id;
        recipe.SoundPath = skill.SoundPath;
        recipe.EffectPath = skill.EffectPath;
        recipe.Duration = 0.72f;
        recipe.Startup = 0.16f;
        recipe.HitTime = 0.36f;
        recipe.Recovery = 0.20f;
        recipe.MovementScale = 0.0f;
        recipe.Tags = {
            "Gameplay.TurnCombat",
            "Gameplay.Combat",
            CategoryTag(skill.Category),
            TargetTag(skill.TargetRule)
        };
        if (skill.Magic)
            recipe.Tags.push_back("Skill.Magic");
        if (skill.Guard)
            recipe.Tags.push_back("Skill.Guard");
        if (skill.HealPower > 0.0f)
            recipe.Tags.push_back("Skill.Heal");

        AddSkillEffects(recipe, skill);
        recipe.Signals = { "turn.skill.apply", "turn.skill.vfx", "turn.skill.sfx" };
        return recipe;
    }

    void RegisterActionRecipes()
    {
        for (const auto& skill : TurnCombatSkillService::SkillLibrary())
            WAO::ActionDatabase::Register(BuildActionRecipe(skill));
    }

} // namespace Wheatear::TurnCombatActionCatalog
