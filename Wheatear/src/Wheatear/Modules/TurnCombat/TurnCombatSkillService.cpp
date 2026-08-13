#include "wtpch.h"
#include "TurnCombatSkillService.h"

#include "Wheatear/Core/Log.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Action/StateRegistry.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::TurnCombatSkillService {

    using Wheatear::StringUtils::ToLower;

    namespace {

        static TurnSkillCategory CategoryFromRecipe(const WAO::ActionRecipe& recipe)
        {
            const std::string category = ToLower(WAO::ParamString(recipe, "category"));
            if (category == "attack" || WAO::HasTag(recipe, "Category.Attack"))
                return TurnSkillCategory::Attack;
            if (category == "guard" || WAO::HasTag(recipe, "Category.Guard"))
                return TurnSkillCategory::Guard;
            if (category == "item" || WAO::HasTag(recipe, "Category.Item"))
                return TurnSkillCategory::Item;
            if (category == "wait" || WAO::HasTag(recipe, "Category.Wait"))
                return TurnSkillCategory::Wait;
            return TurnSkillCategory::Skill;
        }

        static TurnTargetRule TargetFromRecipe(const WAO::ActionRecipe& recipe)
        {
            const std::string target = ToLower(WAO::ParamString(recipe, "targetRule"));
            if (target == "allysingle" || WAO::HasTag(recipe, "Target.AllySingle"))
                return TurnTargetRule::AllySingle;
            if (target == "self" || WAO::HasTag(recipe, "Target.Self"))
                return TurnTargetRule::Self;
            if (target == "enemyall" || WAO::HasTag(recipe, "Target.EnemyAll"))
                return TurnTargetRule::EnemyAll;
            if (target == "allyall" || WAO::HasTag(recipe, "Target.AllyAll"))
                return TurnTargetRule::AllyAll;
            return TurnTargetRule::EnemySingle;
        }

        static TurnStatusEffectKind StatusFromText(const std::string& value)
        {
            const std::string status = ToLower(value);
            if (status == "guard" || status == WAO::StateIds::Guard)
                return TurnStatusEffectKind::Guard;
            if (status == "regeneration" || status == "regen" || status == WAO::StateIds::Regeneration)
                return TurnStatusEffectKind::Regeneration;
            if (status == "burn" || status == WAO::StateIds::Burn)
                return TurnStatusEffectKind::Burn;
            if (status == "defensedown" || status == "defense_down" || status == "def_down" || status == WAO::StateIds::DefenseDown)
                return TurnStatusEffectKind::DefenseDown;
            if (status == "stun" || status == WAO::StateIds::Stun)
                return TurnStatusEffectKind::Stun;
            return TurnStatusEffectKind::None;
        }

        static TurnSkillDefinition BuildSkillFromRecipe(const WAO::ActionRecipe& recipe)
        {
            const WAO::EffectSpec* damage = WAO::FirstEffect(recipe, WAO::EffectType::Damage);
            const WAO::EffectSpec* heal = WAO::FirstEffect(recipe, WAO::EffectType::Heal);
            const WAO::EffectSpec* state = WAO::FirstEffect(recipe, WAO::EffectType::AddState);

            TurnSkillDefinition skill;
            skill.Id = recipe.Id.rfind("turn.", 0) == 0 ? recipe.Id.substr(5) : recipe.Id;
            skill.DisplayName = recipe.DisplayName;
            skill.Description = recipe.Description;
            skill.IconPath = recipe.IconPath;
            skill.SoundPath = recipe.SoundPath;
            skill.EffectPath = recipe.EffectPath;
            skill.EffectAtlas.SheetPath = WAO::ParamString(recipe, "effectAtlasSheet");
            skill.EffectAtlas.CellWidth = WAO::ParamInt(recipe, "effectAtlasCellWidth", 0);
            skill.EffectAtlas.CellHeight = WAO::ParamInt(recipe, "effectAtlasCellHeight", 0);
            skill.EffectAtlas.Columns = WAO::ParamInt(recipe, "effectAtlasColumns", 0);
            skill.EffectAtlas.StartFrame = WAO::ParamInt(recipe, "effectAtlasStartFrame", 0);
            skill.EffectFrameCount = WAO::ParamInt(recipe, "effectFrameCount", 1);
            skill.EffectFrameRate = WAO::ParamFloat(recipe, "effectFrameRate", 12.0f);
            skill.TargetRule = TargetFromRecipe(recipe);
            skill.ManaCost = WAO::ParamFloat(recipe, "manaCost", WAO::ResourceCost(recipe, "mana"));
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
            if (skill.AppliedEffect == TurnStatusEffectKind::None && state)
                skill.AppliedEffect = StatusFromText(state->StateId);
            skill.EffectTurns = WAO::ParamInt(recipe, "effectTurns", state ? state->Turns : 0);
            skill.EffectPower = WAO::ParamFloat(recipe, "effectPower", state ? state->Value : 0.0f);
            return skill;
        }

        static std::vector<TurnSkillDefinition> BuildSkillsFromActionDatabase()
        {
            std::vector<TurnSkillDefinition> skills;
            for (const auto& recipe : WAO::RecipesWithPrefix("turn."))
                skills.push_back(BuildSkillFromRecipe(recipe));
            return skills;
        }

        static std::string StripTurnPrefix(const std::string& id)
        {
            return id.rfind("turn.", 0) == 0 ? id.substr(5) : id;
        }

    } // namespace

    const std::vector<TurnSkillDefinition>& SkillLibrary()
    {
        static std::vector<TurnSkillDefinition> skills;
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
            WT_CORE_WARN("TurnCombatSkillService: no turn.* WAO action recipes loaded. Check assets/gameplay/actions.");
            warnedMissingData = true;
        }
        return skills;
    }

    const TurnSkillDefinition* FindSkill(const std::string& id)
    {
        const auto& skills = SkillLibrary();
        const std::string normalizedId = StripTurnPrefix(id);
        const auto it = std::find_if(skills.begin(), skills.end(),
            [&](const TurnSkillDefinition& skill) { return skill.Id == normalizedId; });
        return it == skills.end() ? nullptr : &(*it);
    }

    std::optional<std::string> ResolvePlayerSkillId(
        const TurnCombatantComponent& actor,
        const std::string& payload)
    {
        if (payload == "basic" || payload == "slot0")
            return StripTurnPrefix(actor.BasicSkillId);
        if (payload == "slot1")
            return StripTurnPrefix(actor.Skill1Id);
        if (payload == "slot2")
            return StripTurnPrefix(actor.Skill2Id);
        if (payload == "slot3")
            return StripTurnPrefix(actor.Skill3Id);
        if (payload == "item0" || payload == "potion")
            return std::string("healing_potion");
        const std::string normalizedPayload = StripTurnPrefix(payload);
        if (FindSkill(normalizedPayload))
            return normalizedPayload;
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
