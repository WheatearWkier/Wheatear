#include "wtpch.h"
#include "ActionRunner.h"

#include "ActionDebugHistory.h"
#include "ActionSignalRouter.h"

#include <algorithm>

namespace Wheatear::WAO {

    namespace {

        bool Contains(const std::vector<std::string>& values, const std::string& id)
        {
            return std::find(values.begin(), values.end(), id) != values.end();
        }

        std::string EffectName(EffectType type)
        {
            switch (type)
            {
            case EffectType::Damage: return "Damage";
            case EffectType::Heal: return "Heal";
            case EffectType::ModifyAttribute: return "ModifyAttribute";
            case EffectType::AddState: return "AddState";
            case EffectType::RemoveState: return "RemoveState";
            case EffectType::StartCooldown: return "StartCooldown";
            case EffectType::ConsumeResource: return "ConsumeResource";
            case EffectType::Launch: return "Launch";
            case EffectType::HitStun: return "HitStun";
            case EffectType::EmitSignal: return "EmitSignal";
            case EffectType::None:
            default: return "None";
            }
        }

        void RecordEffect(EffectLedger& ledger,
            const ActionIntent& intent,
            const EffectSpec& effect,
            bool applied,
            const std::string& detail)
        {
            ledger.Record({
                intent.ActionId,
                effect.Type,
                effect.Source ? effect.Source : intent.Actor,
                effect.Target ? effect.Target : intent.ExplicitTarget,
                detail,
                effect.Value,
                applied
            });
        }

        void ApplyEffect(const ActionIntent& intent,
            const EffectSpec& effect,
            ActionRuntime& runtime,
            EffectLedger& ledger)
        {
            switch (effect.Type)
            {
            case EffectType::Damage:
                runtime.Attributes.Modify("Health", -std::max(0.0f, effect.Value));
                RecordEffect(ledger, intent, effect, true, "Damage Health");
                break;
            case EffectType::Heal:
                runtime.Attributes.Modify("Health", std::max(0.0f, effect.Value));
                RecordEffect(ledger, intent, effect, true, "Heal Health");
                break;
            case EffectType::ModifyAttribute:
                runtime.Attributes.Modify(effect.AttributeId, effect.Value);
                RecordEffect(ledger, intent, effect, true, "Modify " + effect.AttributeId);
                break;
            case EffectType::AddState:
                ApplyState(runtime.States,
                    MakeState(effect.StateId, effect.Turns, effect.Value, effect.Source ? effect.Source : intent.Actor));
                RecordEffect(ledger, intent, effect, true, "AddState " + effect.StateId);
                break;
            case EffectType::RemoveState:
                RemoveState(runtime.States, effect.StateId);
                RecordEffect(ledger, intent, effect, true, "RemoveState " + effect.StateId);
                break;
            case EffectType::StartCooldown:
                runtime.Cooldowns[intent.ActionId] = std::max(0.0f, effect.Value);
                RecordEffect(ledger, intent, effect, true, "StartCooldown");
                break;
            case EffectType::ConsumeResource:
                runtime.Resources[effect.AttributeId] -= std::max(0.0f, effect.Value);
                RecordEffect(ledger, intent, effect, true, "Consume " + effect.AttributeId);
                break;
            case EffectType::EmitSignal:
                RecordEffect(ledger, intent, effect, true, "Signal " + effect.SignalId);
                ActionSignalRouter::Emit({
                    intent,
                    intent.ActionId,
                    effect.SignalId,
                    intent.Source,
                    "ActionRunner"
                });
                break;
            case EffectType::Launch:
            case EffectType::HitStun:
                RecordEffect(ledger, intent, effect, true, EffectName(effect.Type));
                break;
            case EffectType::None:
            default:
                RecordEffect(ledger, intent, effect, false, "Ignored");
                break;
            }
        }

    } // namespace

    bool CanAfford(const ActionRecipe& recipe, const ActionRuntime& runtime)
    {
        for (const auto& [id, cost] : recipe.ResourceCost)
        {
            const auto it = runtime.Resources.find(id);
            const float available = it == runtime.Resources.end() ? 0.0f : it->second;
            if (available + 0.001f < cost)
                return false;
        }
        return true;
    }

    bool CanActivate(const ActionRecipe& recipe, const ActionRuntime& runtime)
    {
        const auto cooldownIt = runtime.Cooldowns.find(recipe.Id);
        if (cooldownIt != runtime.Cooldowns.end() && cooldownIt->second > 0.0f)
            return false;

        if (!CanAfford(recipe, runtime))
            return false;

        for (const std::string& id : recipe.RequiredStates)
        {
            if (!HasState(runtime.States, id))
                return false;
        }

        for (const std::string& id : recipe.BlockedStates)
        {
            if (HasState(runtime.States, id))
                return false;
        }

        for (const std::string& id : recipe.RequiredTags)
        {
            if (!Contains(runtime.Tags, id))
                return false;
        }

        for (const std::string& id : recipe.BlockedTags)
        {
            if (Contains(runtime.Tags, id))
                return false;
        }

        return true;
    }

    ActionExecutionResult Execute(const ActionIntent& intent,
        const ActionRecipe& recipe,
        ActionRuntime& runtime)
    {
        ActionExecutionResult result;
        result.Ledger.BeginAction(intent);

        if (!CanActivate(recipe, runtime))
        {
            result.Ledger.Record({
                intent.ActionId,
                EffectType::None,
                intent.Actor,
                intent.ExplicitTarget,
                "CanActivate failed",
                0.0f,
                false
            });
            ActionDebugHistory::Record(result.Ledger, false, "CanActivate failed");
            return result;
        }

        for (const auto& [id, cost] : recipe.ResourceCost)
        {
            EffectSpec consume;
            consume.Type = EffectType::ConsumeResource;
            consume.Source = intent.Actor;
            consume.AttributeId = id;
            consume.Value = cost;
            ApplyEffect(intent, consume, runtime, result.Ledger);
            result.AppliedEffects.Add(consume);
        }

        if (recipe.Cooldown > 0.0f)
        {
            EffectSpec cooldown;
            cooldown.Type = EffectType::StartCooldown;
            cooldown.Source = intent.Actor;
            cooldown.Value = recipe.Cooldown;
            ApplyEffect(intent, cooldown, runtime, result.Ledger);
            result.AppliedEffects.Add(cooldown);
        }

        for (const EffectSpec& effect : recipe.Effects)
        {
            ApplyEffect(intent, effect, runtime, result.Ledger);
            result.AppliedEffects.Add(effect);
        }

        for (const std::string& signal : recipe.Signals)
        {
            EffectSpec effect;
            effect.Type = EffectType::EmitSignal;
            effect.Source = intent.Actor;
            effect.SignalId = signal;
            ApplyEffect(intent, effect, runtime, result.Ledger);
            result.AppliedEffects.Add(effect);
        }

        result.Success = true;
        ActionDebugHistory::Record(result.Ledger, true, "Executed");
        return result;
    }

} // namespace Wheatear::WAO
