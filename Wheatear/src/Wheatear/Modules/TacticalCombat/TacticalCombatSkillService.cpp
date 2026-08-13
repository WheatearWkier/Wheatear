#include "wtpch.h"
#include "TacticalCombatSkillService.h"

#include "Wheatear/Core/Log.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"
#include "Wheatear/Gameplay/Services/GameplayTextService.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::TacticalCombatSkillService {

    using Wheatear::StringUtils::ToLower;

    namespace {

        static TacticalSkillCategory CategoryFromRecipe(const WAO::ActionRecipe& recipe)
        {
            const std::string category = ToLower(WAO::ParamString(recipe, "category"));
            if (category == "attack" || WAO::HasTag(recipe, "Category.Attack"))
                return TacticalSkillCategory::Attack;
            if (category == "guard" || WAO::HasTag(recipe, "Category.Guard"))
                return TacticalSkillCategory::Guard;
            if (category == "item" || WAO::HasTag(recipe, "Category.Item"))
                return TacticalSkillCategory::Item;
            if (category == "wait" || WAO::HasTag(recipe, "Category.Wait"))
                return TacticalSkillCategory::Wait;
            return TacticalSkillCategory::Skill;
        }

        static TacticalTargetRule TargetFromRecipe(const WAO::ActionRecipe& recipe)
        {
            const std::string target = ToLower(WAO::ParamString(recipe, "targetRule"));
            if (target == "ally" || WAO::HasTag(recipe, "Target.Ally"))
                return TacticalTargetRule::Ally;
            if (target == "self" || WAO::HasTag(recipe, "Target.Self"))
                return TacticalTargetRule::Self;
            return TacticalTargetRule::Enemy;
        }

        static int RangeFromRecipe(const WAO::ActionRecipe& recipe)
        {
            const std::string rangeValue = WAO::ParamString(recipe, "range");
            if (!rangeValue.empty())
                return std::max(0, WAO::ParamInt(recipe, "range", 0));
            if (WAO::HasTag(recipe, "Range.Melee") || WAO::HasTag(recipe, "Range.1"))
                return 1;
            if (WAO::HasTag(recipe, "Range.Three") || WAO::HasTag(recipe, "Range.3"))
                return 3;
            for (const std::string& tag : recipe.Tags)
            {
                if (tag.rfind("Range.", 0) != 0)
                    continue;
                try
                {
                    return std::max(0, std::stoi(tag.substr(6)));
                }
                catch (...) {}
            }
            return 1;
        }

        static TacticalStatusEffectKind StatusFromText(const std::string& value)
        {
            const std::string status = ToLower(value);
            if (status == "guard" || status == WAO::StateIds::Guard)
                return TacticalStatusEffectKind::Guard;
            if (status == "regeneration" || status == "regen" || status == WAO::StateIds::Regeneration)
                return TacticalStatusEffectKind::Regeneration;
            if (status == "burn" || status == WAO::StateIds::Burn)
                return TacticalStatusEffectKind::Burn;
            if (status == "defensedown" || status == "defense_down" || status == "def_down" || status == WAO::StateIds::DefenseDown)
                return TacticalStatusEffectKind::DefenseDown;
            if (status == "stun" || status == WAO::StateIds::Stun)
                return TacticalStatusEffectKind::Stun;
            return TacticalStatusEffectKind::None;
        }

        static TacticalSkillDefinition BuildSkillFromRecipe(const WAO::ActionRecipe& recipe)
        {
            const WAO::EffectSpec* damage = WAO::FirstEffect(recipe, WAO::EffectType::Damage);
            const WAO::EffectSpec* heal = WAO::FirstEffect(recipe, WAO::EffectType::Heal);
            const WAO::EffectSpec* state = WAO::FirstEffect(recipe, WAO::EffectType::AddState);

            TacticalSkillDefinition skill;
            skill.Id = recipe.Id.rfind("tactical.", 0) == 0 ? recipe.Id.substr(9) : recipe.Id;
            skill.DisplayName = recipe.DisplayName;
            skill.Description = recipe.Description;
            skill.IconPath = recipe.IconPath;
            skill.SoundPath = recipe.SoundPath;
            skill.EffectFramePattern = recipe.EffectPath;
            skill.EffectAtlas.SheetPath = WAO::ParamString(recipe, "effectAtlasSheet");
            skill.EffectAtlas.CellWidth = WAO::ParamInt(recipe, "effectAtlasCellWidth", 0);
            skill.EffectAtlas.CellHeight = WAO::ParamInt(recipe, "effectAtlasCellHeight", 0);
            skill.EffectAtlas.Columns = WAO::ParamInt(recipe, "effectAtlasColumns", 0);
            skill.EffectAtlas.StartFrame = WAO::ParamInt(recipe, "effectAtlasStartFrame", 0);
            skill.EffectFrameCount = WAO::ParamInt(recipe, "effectFrameCount", 1);
            skill.EffectFrameRate = WAO::ParamFloat(recipe, "effectFrameRate", 12.0f);
            skill.TargetRule = TargetFromRecipe(recipe);
            skill.Range = RangeFromRecipe(recipe);
            skill.Power = WAO::ParamFloat(recipe, "power", damage ? damage->Value : 0.0f);
            skill.HealPower = WAO::ParamFloat(recipe, "healPower", heal ? heal->Value : 0.0f);
            skill.DefensePierce = WAO::ParamFloat(recipe, "defensePierce", 0.0f);
            skill.Magic = WAO::ParamBool(recipe, "magic",
                WAO::HasTag(recipe, "Skill.Magic")
                || (damage && damage->AttributeId == "magic")
                || (heal && heal->AttributeId == "magic"));
            skill.Guard = WAO::ParamBool(recipe, "guard", WAO::HasTag(recipe, "Skill.Guard"));
            skill.Category = CategoryFromRecipe(recipe);
            skill.AppliedEffect = StatusFromText(WAO::ParamString(recipe, "statusEffect"));
            if (skill.AppliedEffect == TacticalStatusEffectKind::None && state)
                skill.AppliedEffect = StatusFromText(state->StateId);
            skill.EffectTurns = WAO::ParamInt(recipe, "effectTurns", state ? state->Turns : 0);
            skill.EffectPower = WAO::ParamFloat(recipe, "effectPower", state ? state->Value : 0.0f);
            return skill;
        }

        static std::vector<TacticalSkillDefinition> BuildSkillsFromActionDatabase()
        {
            std::vector<TacticalSkillDefinition> skills;
            for (const auto& recipe : WAO::RecipesWithPrefix("tactical."))
                skills.push_back(BuildSkillFromRecipe(recipe));
            return skills;
        }

    } // namespace

    const std::vector<TacticalSkillDefinition>& SkillLibrary()
    {
        static std::vector<TacticalSkillDefinition> skills;
        static uint64_t cachedRevision = 0;
        static bool initialized = false;
        const uint64_t revision = WAO::ActionDatabase::Revision();
        if (!initialized || cachedRevision != revision)
        {
            skills = BuildSkillsFromActionDatabase();
            cachedRevision = revision;
            initialized = true;
        }

        static bool warnedMissingData = false;
        if (skills.empty() && !warnedMissingData)
        {
            WT_CORE_WARN("TacticalCombatSkillService: no tactical.* WAO action recipes loaded. Check assets/gameplay/actions.");
            warnedMissingData = true;
        }
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
