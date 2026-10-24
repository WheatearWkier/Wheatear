#include "wtpch.h"
#include "ActionRunner.h"

#include "ActionDebugHistory.h"
#include "ActionSignalRouter.h"
#include "EffectFormula.h"
#include "EffectRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>

namespace Wheatear::WAO {

    namespace {

        bool Contains(const std::vector<std::string>& values, const std::string& id)
        {
            return std::find(values.begin(), values.end(), id) != values.end();
        }

        std::string ToLowerAscii(const std::string& text)
        {
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return lower;
        }

        bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.size() >= prefix.size()
                && value.compare(0, prefix.size(), prefix) == 0;
        }

        bool IsHealthAttribute(const std::string& id)
        {
            const std::string lower = ToLowerAscii(id);
            return lower == "health" || lower == "hp";
        }

        bool IsMaxHealthAttribute(const std::string& id)
        {
            const std::string lower = ToLowerAscii(id);
            return lower == "maxhealth" || lower == "max_health" || lower == "max_hp";
        }

        float GetAttribute(const ActionRuntime& runtime, const std::string& id,
            float fallback = 0.0f)
        {
            const std::string lower = ToLowerAscii(id);
            if (lower == "health" || lower == "hp")
            {
                if (runtime.Attributes.Values.count("Health"))
                    return runtime.Attributes.Get("Health", fallback);
                return runtime.Attributes.Get("hp", fallback);
            }
            if (IsMaxHealthAttribute(id))
            {
                if (runtime.Attributes.Values.count("MaxHealth"))
                    return runtime.Attributes.Get("MaxHealth", fallback);
                if (runtime.Attributes.Values.count("max_health"))
                    return runtime.Attributes.Get("max_health", fallback);
                return runtime.Attributes.Get("max_hp", fallback);
            }
            if (lower == "atk" || lower == "attack")
            {
                if (runtime.Attributes.Values.count("attack"))
                    return runtime.Attributes.Get("attack", fallback);
                return runtime.Attributes.Get("atk", fallback);
            }
            if (lower == "def" || lower == "defense")
            {
                if (runtime.Attributes.Values.count("defense"))
                    return runtime.Attributes.Get("defense", fallback);
                return runtime.Attributes.Get("def", fallback);
            }
            return runtime.Attributes.Get(id, fallback);
        }

        void SetAttribute(ActionRuntime& runtime, const std::string& id, float value)
        {
            const std::string lower = ToLowerAscii(id);
            if (lower == "health" || lower == "hp")
            {
                runtime.Attributes.Set("Health", value);
                runtime.Attributes.Set("hp", value);
                return;
            }
            if (IsMaxHealthAttribute(id))
            {
                runtime.Attributes.Set("MaxHealth", value);
                runtime.Attributes.Set("max_health", value);
                runtime.Attributes.Set("max_hp", value);
                return;
            }
            if (lower == "atk" || lower == "attack")
            {
                runtime.Attributes.Set("atk", value);
                runtime.Attributes.Set("attack", value);
                return;
            }
            if (lower == "def" || lower == "defense")
            {
                runtime.Attributes.Set("def", value);
                runtime.Attributes.Set("defense", value);
                return;
            }
            runtime.Attributes.Set(id, value);
        }

        ActionRuntime& SelectRuntime(EffectTargetSelector selector,
            ActionRuntime& actor,
            ActionRuntime* target,
            ActionRuntime* source)
        {
            if (selector == EffectTargetSelector::Target && target)
                return *target;
            if (selector == EffectTargetSelector::Source && source)
                return *source;
            return actor;
        }

        void AddAttributeAliases(AttributeStore& vars, const std::string& prefix,
            const ActionRuntime& runtime)
        {
            for (const auto& [id, value] : runtime.Attributes.Values)
            {
                const std::string lower = ToLowerAscii(id);
                vars.Set(prefix + "." + id, value);
                vars.Set(prefix + "." + lower, value);

                if (lower == "health" || lower == "hp")
                {
                    vars.Set(prefix + ".health", value);
                    vars.Set(prefix + ".hp", value);
                }
                if (IsMaxHealthAttribute(id))
                {
                    vars.Set(prefix + ".max_health", value);
                    vars.Set(prefix + ".max_hp", value);
                }
                if (lower == "atk" || lower == "attack")
                {
                    vars.Set(prefix + ".atk", value);
                    vars.Set(prefix + ".attack", value);
                }
                if (lower == "def" || lower == "defense")
                {
                    vars.Set(prefix + ".def", value);
                    vars.Set(prefix + ".defense", value);
                }
            }

            for (const auto& [id, value] : runtime.Resources)
            {
                const std::string lower = ToLowerAscii(id);
                vars.Set(prefix + "." + id, value);
                vars.Set(prefix + "." + lower, value);
            }
        }

        AttributeStore BuildEffectVariables(const ActionRuntime& actor,
            const ActionRuntime* target,
            const ActionRuntime* source)
        {
            AttributeStore vars;
            AddAttributeAliases(vars, "actor", actor);
            AddAttributeAliases(vars, "source", source ? *source : actor);
            AddAttributeAliases(vars, "target", target ? *target : actor);
            AddAttributeAliases(vars, "controller", actor);
            return vars;
        }

        void WriteBackCustomEffect(const AttributeStore& original,
            const AttributeStore& mutated,
            ActionRuntime& actor,
            ActionRuntime* target)
        {
            for (const auto& [key, value] : mutated.Values)
            {
                // The custom handler receives a full attribute snapshot, so
                // only write back keys it actually changed. Writing every
                // seeded alias back would let an unchanged "target.hp" overwrite
                // a changed "target.health" depending on hash iteration order.
                const auto originalIt = original.Values.find(key);
                if (originalIt != original.Values.end()
                    && std::abs(originalIt->second - value) <= 0.0001f)
                {
                    continue;
                }

                if (StartsWith(key, "target."))
                {
                    ActionRuntime& destination = target ? *target : actor;
                    SetAttribute(destination, key.substr(7), value);
                }
                else if (StartsWith(key, "source.") || StartsWith(key, "actor."))
                {
                    SetAttribute(actor, key.substr(key.find('.') + 1), value);
                }
                else if (StartsWith(key, "controller."))
                {
                    const std::string id = key.substr(11);
                    SetAttribute(actor, id, value);

                    const std::string lower = ToLowerAscii(id);
                    if (lower == "mana" || lower == "max_mana"
                        || actor.Resources.count(id) > 0)
                    {
                        actor.Resources[id] = value;
                    }
                }
            }
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

        UUID ResolveSourceId(const ActionIntent& intent, const EffectSpec& effect)
        {
            if (effect.Source != 0)
                return effect.Source;
            if (effect.SourceSelector == EffectTargetSelector::Target
                && intent.ExplicitTarget != 0)
            {
                return intent.ExplicitTarget;
            }
            return intent.Actor;
        }

        UUID ResolveTargetId(const ActionIntent& intent, const EffectSpec& effect)
        {
            if (effect.Target != 0)
                return effect.Target;

            switch (effect.TargetSelector)
            {
            case EffectTargetSelector::Target:
                return intent.ExplicitTarget != 0
                    ? intent.ExplicitTarget
                    : intent.Actor;
            case EffectTargetSelector::Source:
                return ResolveSourceId(intent, effect);
            case EffectTargetSelector::Actor:
            default:
                return intent.Actor;
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
                ResolveSourceId(intent, effect),
                ResolveTargetId(intent, effect),
                detail,
                effect.Value,
                applied
            });
        }

        struct EffectApplication
        {
            EffectSpec Resolved;
            bool Applied = false;
        };

        EffectApplication ApplyEffect(const ActionIntent& intent,
            const EffectSpec& effect,
            ActionRuntime& actorRuntime,
            ActionRuntime* targetRuntime,
            ActionRuntime* sourceRuntime,
            EffectLedger& ledger)
        {
            EffectApplication application;
            application.Resolved = effect;

            const AttributeStore vars = BuildEffectVariables(
                actorRuntime, targetRuntime, sourceRuntime);

            if (!effect.Formula.empty())
            {
                application.Resolved.Value = EvaluateEffectFormula(
                    effect.Formula, vars, effect.Value);
            }

            ActionRuntime& runtime = SelectRuntime(
                effect.TargetSelector,
                actorRuntime,
                targetRuntime,
                sourceRuntime);

            if (!effect.CustomType.empty())
            {
                CustomEffectContext context;
                AttributeStore mutableVars = vars;
                context.Vars = &mutableVars;
                context.InValue = application.Resolved.Value;

                if (!EffectRegistry::Run(effect.CustomType, context))
                {
                    RecordEffect(ledger, intent, application.Resolved, false,
                        "Custom effect declined: " + effect.CustomType);
                    return application;
                }

                WriteBackCustomEffect(vars, mutableVars, actorRuntime, targetRuntime);
                if (context.OutValue != 0.0f)
                    application.Resolved.Value = context.OutValue;

                RecordEffect(ledger, intent, application.Resolved, true,
                    "Custom " + effect.CustomType);
                application.Applied = true;
                return application;
            }

            switch (effect.Type)
            {
            case EffectType::Damage:
            case EffectType::Heal:
            {
                const std::string attributeId = effect.AttributeId.empty()
                    ? "Health"
                    : effect.AttributeId;
                const float magnitude = std::abs(application.Resolved.Value);
                const float delta = effect.Type == EffectType::Damage
                    ? -magnitude
                    : magnitude;

                float before = GetAttribute(runtime, attributeId, 0.0f);
                float after = before + delta;
                if (IsHealthAttribute(attributeId))
                {
                    const float maxHealth = GetAttribute(runtime, "MaxHealth", 0.0f);
                    after = maxHealth > 0.0f
                        ? std::clamp(after, 0.0f, maxHealth)
                        : std::max(0.0f, after);
                }

                SetAttribute(runtime, attributeId, after);
                application.Resolved.Value = delta;
                RecordEffect(ledger, intent, application.Resolved, true,
                    std::string(effect.Type == EffectType::Damage ? "Damage " : "Heal ")
                        + attributeId + " (" + std::to_string(after) + ")");
                application.Applied = true;
                break;
            }
            case EffectType::ModifyAttribute:
                if (effect.AttributeId.empty())
                {
                    RecordEffect(ledger, intent, application.Resolved, false,
                        "ModifyAttribute needs an attribute id");
                    break;
                }
                SetAttribute(runtime, effect.AttributeId,
                    GetAttribute(runtime, effect.AttributeId, 0.0f)
                        + application.Resolved.Value);
                RecordEffect(ledger, intent, application.Resolved, true,
                    "Modify " + effect.AttributeId);
                application.Applied = true;
                break;

            case EffectType::AddState:
            {
                if (effect.StateId.empty())
                {
                    RecordEffect(ledger, intent, application.Resolved, false,
                        "AddState needs a state id");
                    break;
                }

                int turns = effect.Turns;
                float seconds = effect.Seconds;
                bool infinite = false;
                if (effect.DurationPolicy == EffectDurationPolicy::Infinite)
                {
                    infinite = true;
                    turns = 0;
                    seconds = 0.0f;
                }
                else if (effect.DurationPolicy == EffectDurationPolicy::Turns)
                {
                    turns = std::max(1, turns);
                    seconds = 0.0f;
                }
                else if (effect.DurationPolicy == EffectDurationPolicy::Seconds)
                {
                    turns = 0;
                    seconds = std::max(0.0f, seconds);
                }

                if (!infinite && turns <= 0 && seconds <= 0.0f)
                {
                    RecordEffect(ledger, intent, application.Resolved, false,
                        "AddState has no duration");
                    break;
                }

                RuntimeState state = MakeState(
                    effect.StateId,
                    turns,
                    seconds,
                    application.Resolved.Value,
                    ResolveSourceId(intent, effect));
                state.Infinite = infinite;
                ApplyState(runtime.States, state);

                RecordEffect(ledger, intent, application.Resolved, true,
                    "AddState " + effect.StateId);
                application.Applied = true;
                break;
            }
            case EffectType::RemoveState:
                RemoveState(runtime.States, effect.StateId);
                RecordEffect(ledger, intent, application.Resolved, true,
                    "RemoveState " + effect.StateId);
                application.Applied = true;
                break;

            case EffectType::StartCooldown:
            {
                const float seconds = effect.Seconds > 0.0f
                    ? effect.Seconds
                    : effect.Value;
                if (seconds <= 0.0f)
                {
                    RecordEffect(ledger, intent, application.Resolved, false,
                        "StartCooldown needs Seconds or Value");
                    break;
                }

                actorRuntime.Cooldowns[intent.ActionId] = std::max(
                    actorRuntime.Cooldowns[intent.ActionId], seconds);
                application.Resolved.Value = seconds;
                application.Resolved.TargetSelector = EffectTargetSelector::Actor;
                RecordEffect(ledger, intent, application.Resolved, true,
                    "StartCooldown " + std::to_string(seconds) + "s");
                application.Applied = true;
                break;
            }
            case EffectType::ConsumeResource:
            {
                if (effect.AttributeId.empty())
                {
                    RecordEffect(ledger, intent, application.Resolved, false,
                        "ConsumeResource needs a resource id");
                    break;
                }

                const float cost = std::max(0.0f, application.Resolved.Value);
                const float current = runtime.Resources.count(effect.AttributeId) > 0
                    ? runtime.Resources[effect.AttributeId]
                    : 0.0f;
                runtime.Resources[effect.AttributeId] = std::max(0.0f, current - cost);
                RecordEffect(ledger, intent, application.Resolved, true,
                    "Consume " + effect.AttributeId);
                application.Applied = true;
                break;
            }
            case EffectType::EmitSignal:
                RecordEffect(ledger, intent, application.Resolved, true,
                    "Signal " + effect.SignalId);
                ActionSignalRouter::Emit({
                    intent,
                    intent.ActionId,
                    effect.SignalId,
                    intent.Source,
                    "ActionRunner"
                });
                application.Applied = true;
                break;

            case EffectType::HitStun:
            {
                const float seconds = effect.Seconds > 0.0f ? effect.Seconds : 0.2f;
                RuntimeState state = MakeState(
                    StateIds::Stun, 0, seconds, 1.0f,
                    ResolveSourceId(intent, effect));
                ApplyState(runtime.States, state);
                RecordEffect(ledger, intent, application.Resolved, true,
                    "HitStun " + std::to_string(seconds) + "s");
                application.Applied = true;
                break;
            }
            case EffectType::Launch:
            {
                const float seconds = effect.Seconds > 0.0f ? effect.Seconds : 0.3f;
                RuntimeState state = MakeState(
                    "launch", 0, seconds,
                    application.Resolved.Value,
                    ResolveSourceId(intent, effect));
                ApplyState(runtime.States, state);
                RecordEffect(ledger, intent, application.Resolved, true,
                    "Launch " + std::to_string(seconds) + "s");
                application.Applied = true;
                break;
            }
            case EffectType::None:
            default:
                RecordEffect(ledger, intent, application.Resolved, false, "Ignored");
                break;
            }

            return application;
        }

        void AppendLedger(EffectLedger& destination,
            const EffectLedger& source)
        {
            for (const EffectLedgerEntry& entry : source.Entries())
                destination.Record(entry);
        }

        void AppendEffects(EffectBundle& destination,
            const EffectBundle& source)
        {
            for (const EffectSpec& effect : source.Effects)
                destination.Add(effect);
        }

        void ApplyRecipeBody(const ActionIntent& intent,
            const ActionRecipe& recipe,
            ActionRuntime& actorRuntime,
            ActionRuntime* targetRuntime,
            ActionRuntime* sourceRuntime,
            ActionExecutionResult& result)
        {
            for (const EffectSpec& effect : recipe.Effects)
            {
                const EffectApplication application = ApplyEffect(
                    intent, effect, actorRuntime, targetRuntime,
                    sourceRuntime, result.Ledger);
                if (application.Applied)
                    result.AppliedEffects.Add(application.Resolved);
            }

            for (const std::string& signal : recipe.Signals)
            {
                EffectSpec effect;
                effect.Type = EffectType::EmitSignal;
                effect.Source = intent.Actor;
                effect.SignalId = signal;
                const EffectApplication application = ApplyEffect(
                    intent, effect, actorRuntime, targetRuntime,
                    sourceRuntime, result.Ledger);
                if (application.Applied)
                    result.AppliedEffects.Add(application.Resolved);
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

    ActionActivationResult BeginAction(const ActionIntent& intent,
        const ActionRecipe& recipe,
        ActionRuntime& runtime,
        ActionRuntime* targetRuntime,
        ActionRuntime* sourceRuntime)
    {
        ActionActivationResult result;
        result.Ledger.BeginAction(intent);

        if (!CanActivate(recipe, runtime))
        {
            result.Detail = "CanActivate failed";
            result.Ledger.Record({
                intent.ActionId,
                EffectType::None,
                intent.Actor,
                intent.ExplicitTarget,
                result.Detail,
                0.0f,
                false
            });
            return result;
        }

        for (const auto& [id, cost] : recipe.ResourceCost)
        {
            EffectSpec consume;
            consume.Type = EffectType::ConsumeResource;
            consume.Source = intent.Actor;
            consume.TargetSelector = EffectTargetSelector::Actor;
            consume.AttributeId = id;
            consume.Value = cost;

            ActionExecutionResult activationEffect;
            activationEffect.Ledger.BeginAction(intent);
            const EffectApplication application = ApplyEffect(
                intent, consume, runtime, targetRuntime,
                sourceRuntime, activationEffect.Ledger);
            if (application.Applied)
                result.AppliedEffects.Add(application.Resolved);
            AppendLedger(result.Ledger, activationEffect.Ledger);
        }

        if (recipe.Cooldown > 0.0f)
        {
            EffectSpec cooldown;
            cooldown.Type = EffectType::StartCooldown;
            cooldown.Source = intent.Actor;
            cooldown.TargetSelector = EffectTargetSelector::Actor;
            cooldown.Seconds = recipe.Cooldown;
            cooldown.Value = recipe.Cooldown;
            cooldown.DurationPolicy = EffectDurationPolicy::Seconds;

            ActionExecutionResult activationEffect;
            activationEffect.Ledger.BeginAction(intent);
            const EffectApplication application = ApplyEffect(
                intent, cooldown, runtime, targetRuntime,
                sourceRuntime, activationEffect.Ledger);
            if (application.Applied)
                result.AppliedEffects.Add(application.Resolved);
            AppendLedger(result.Ledger, activationEffect.Ledger);
        }

        result.Success = true;
        result.Detail = "Action activated";
        return result;
    }

    ActionExecutionResult ExecuteBody(const ActionIntent& intent,
        const ActionRecipe& recipe,
        ActionRuntime& runtime,
        ActionRuntime* targetRuntime,
        ActionRuntime* sourceRuntime)
    {
        ActionExecutionResult result;
        result.Ledger.BeginAction(intent);
        ApplyRecipeBody(intent, recipe, runtime, targetRuntime,
            sourceRuntime, result);
        result.Success = true;
        return result;
    }

    ActionExecutionResult Execute(const ActionIntent& intent,
        const ActionRecipe& recipe,
        ActionRuntime& runtime,
        ActionRuntime* targetRuntime,
        ActionRuntime* sourceRuntime)
    {
        ActionExecutionResult result;
        ActionActivationResult activation = BeginAction(
            intent, recipe, runtime, targetRuntime, sourceRuntime);
        result.Success = activation.Success;
        result.Ledger = std::move(activation.Ledger);
        result.AppliedEffects = std::move(activation.AppliedEffects);
        if (!activation.Success)
        {
            ActionDebugHistory::Record(result.Ledger,
                false,
                activation.Detail.empty() ? "CanActivate failed" : activation.Detail);
            return result;
        }

        ActionExecutionResult body = ExecuteBody(
            intent, recipe, runtime, targetRuntime, sourceRuntime);
        AppendLedger(result.Ledger, body.Ledger);
        AppendEffects(result.AppliedEffects, body.AppliedEffects);
        result.Success = body.Success;
        ActionDebugHistory::Record(result.Ledger, result.Success, "Executed");
        return result;
    }

} // namespace Wheatear::WAO
