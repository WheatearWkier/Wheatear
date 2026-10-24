#include "wtpch.h"
#include "BehaviorBuiltinPrimitives.h"

#include "BehaviorPrimitiveRegistry.h"
#include "Wheatear/Gameplay/Action/ActionSignalRouter.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Gameplay/Action/ActionRecipeQueries.h"
#include "Wheatear/Gameplay/Behavior/BehaviorTypes.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"
#include "Wheatear/Modules/SideCombat/SideCombatGuardService.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace Wheatear::WAO {

    namespace {

        static bool Compare(double left,
            const std::string& operation,
            double right)
        {
            if (operation == "=" || operation == "==" || operation == "eq")
                return left == right;
            if (operation == "!=" || operation == "ne")
                return left != right;
            if (operation == ">" || operation == "gt")
                return left > right;
            if (operation == ">=" || operation == "gte")
                return left >= right;
            if (operation == "<" || operation == "lt")
                return left < right;
            if (operation == "<=" || operation == "lte")
                return left <= right;
            return false;
        }

        static std::string Lower(std::string value)
        {
            for (char& character : value)
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            return value;
        }

        static void RecordPrimitiveEffect(BehaviorPrimitiveContext& context,
            EffectType type,
            UUID target,
            const std::string& detail,
            float value,
            bool applied)
        {
            if (!context.Ledger)
                return;
            context.Ledger->Record({
                context.Intent.ActionId,
                type,
                context.Intent.Actor,
                target ? target : context.Intent.ExplicitTarget,
                detail,
                value,
                applied
            });
        }

        static BehaviorPrimitiveDefinition Definition(const char* id,
            const char* displayName,
            BehaviorPrimitiveCategory category,
            BehaviorPrimitiveCallback callback,
            std::vector<BehaviorPrimitivePort> inputs = {},
            std::vector<BehaviorPrimitivePort> outputs = {})
        {
            BehaviorPrimitiveDefinition definition;
            definition.Id = id;
            definition.DisplayName = displayName;
            definition.Category = category;
            definition.Inputs = std::move(inputs);
            definition.Outputs = std::move(outputs);
            definition.Callback = std::move(callback);
            return definition;
        }

        static BehaviorPrimitivePort Port(const char* name,
            BehaviorValueType type,
            bool required = false)
        {
            return { name, type, required };
        }

        static SideCombatantComponent* ResolveSideCombatant(
            BehaviorPrimitiveContext& context)
        {
            if (!context.SceneContext)
                return nullptr;

            const UUID actor = context.Input("actor").AsUUID(context.Intent.Actor);
            if (actor == 0)
                return nullptr;

            Entity entity = SceneQueries::FindEntityByUUID(
                context.SceneContext,
                actor);
            if (!entity || !entity.HasComponent<SideCombatantComponent>())
                return nullptr;
            return &entity.GetComponent<SideCombatantComponent>();
        }

    } // namespace

    void RegisterBuiltinBehaviorPrimitives()
    {
        BehaviorPrimitiveRegistry::Register(Definition(
            "flow.always_true",
            "Always True",
            BehaviorPrimitiveCategory::Query,
            [](BehaviorPrimitiveContext& context)
            {
                context.SetOutput("value", true);
                return true;
            },
            {},
            { Port("value", BehaviorValueType::Bool) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "flow.compare_number",
            "Compare Number",
            BehaviorPrimitiveCategory::Query,
            [](BehaviorPrimitiveContext& context)
            {
                const double left = context.Input("left").AsNumber();
                const double right = context.Input("right").AsNumber();
                const std::string operation = Lower(context.Input("operator", "==").AsString("=="));
                context.SetOutput("value", Compare(left, operation, right));
                return true;
            },
            {
                Port("left", BehaviorValueType::Number, true),
                Port("right", BehaviorValueType::Number, true),
                Port("operator", BehaviorValueType::String)
            },
            { Port("value", BehaviorValueType::Bool) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "wao.recipe_param",
            "Read Recipe Parameter",
            BehaviorPrimitiveCategory::Query,
            [](BehaviorPrimitiveContext& context)
            {
                if (!context.Recipe)
                {
                    context.Fail("Recipe parameter query has no action recipe.");
                    return false;
                }
                const std::string key = context.Input("key").AsString();
                context.SetOutput("value", ParamString(*context.Recipe, key));
                return true;
            },
            { Port("key", BehaviorValueType::String, true) },
            { Port("value", BehaviorValueType::None) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "wao.recipe_param_number",
            "Read Numeric Recipe Parameter",
            BehaviorPrimitiveCategory::Query,
            [](BehaviorPrimitiveContext& context)
            {
                if (!context.Recipe)
                {
                    context.Fail("Recipe parameter query has no action recipe.");
                    return false;
                }
                const std::string key = context.Input("key").AsString();
                context.SetOutput("value",
                    static_cast<double>(ParamFloat(*context.Recipe, key)));
                return true;
            },
            { Port("key", BehaviorValueType::String, true) },
            { Port("value", BehaviorValueType::Number) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "wao.can_activate",
            "Can Activate Action",
            BehaviorPrimitiveCategory::Query,
            [](BehaviorPrimitiveContext& context)
            {
                if (!context.Recipe || !context.Runtime)
                {
                    context.Fail("CanActivate requires an action recipe and runtime.");
                    return false;
                }
                context.SetOutput("value", CanActivate(*context.Recipe, *context.Runtime));
                return true;
            },
            {},
            { Port("value", BehaviorValueType::Bool) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "wao.execute_recipe",
            "Execute WAO Recipe",
            BehaviorPrimitiveCategory::Effect,
            [](BehaviorPrimitiveContext& context)
            {
                if (!context.Recipe || !context.Runtime)
                {
                    context.Fail("Recipe execution requires an action recipe and runtime.");
                    return false;
                }

                const bool actionAlreadyActivated =
                    context.Instance &&
                    context.Instance->ActionActivated &&
                    context.Recipe == &context.Instance->Recipe;
                ActionRuntime* targetRuntime = context.Instance
                    ? context.Instance->TargetRuntimeBinding
                    : nullptr;
                ActionExecutionResult result = actionAlreadyActivated
                    ? ExecuteBody(context.Intent, *context.Recipe,
                        *context.Runtime, targetRuntime, context.Runtime)
                    : Execute(context.Intent, *context.Recipe,
                        *context.Runtime, targetRuntime, context.Runtime);
                context.SetOutput("success", result.Success);
                context.SetOutput("value", result.Success);

                if (context.AppliedEffects)
                {
                    for (const EffectSpec& effect : result.AppliedEffects.Effects)
                        context.AppliedEffects->Add(effect);
                }
                if (context.Ledger)
                {
                    for (const EffectLedgerEntry& entry : result.Ledger.Entries())
                        context.Ledger->Record(entry);
                }
                return true;
            },
            {},
            {
                Port("success", BehaviorValueType::Bool),
                Port("value", BehaviorValueType::Bool)
            }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "wao.start_action",
            "Start Another WAO Action",
            BehaviorPrimitiveCategory::Effect,
            [](BehaviorPrimitiveContext& context)
            {
                const std::string actionId =
                    context.Input("action", context.Input("actionId")).AsString();
                if (actionId.empty())
                {
                    context.Fail("Start action requires an action id.");
                    return false;
                }

                ActionResolveContext resolveContext;
                resolveContext.SceneContext = context.SceneContext;
                resolveContext.Intent = context.Intent;
                resolveContext.Intent.ActionId = actionId;
                resolveContext.Intent.Source = context.Intent.Source.empty()
                    ? "BehaviorVM"
                    : context.Intent.Source;
                resolveContext.Runtime = context.Runtime;
                resolveContext.Detail = context.Input("detail").AsString(
                    "Started by Behavior VM");

                ActionResolveResult result = ActionOrchestrator::Execute(resolveContext);
                context.SetOutput("success", result.Success);
                context.SetOutput("value", result.Success);
                if (!result.Success)
                {
                    context.Fail(result.Detail.empty()
                        ? "Nested action failed: " + actionId
                        : result.Detail);
                    return false;
                }

                if (context.Instance &&
                    context.Input("wait").AsBool(false) &&
                    result.Started &&
                    !result.Completed &&
                    result.Handle != 0)
                {
                    context.Instance->WaitingChildHandle = result.Handle;
                }

                if (context.AppliedEffects)
                {
                    for (const EffectSpec& effect : result.AppliedEffects.Effects)
                        context.AppliedEffects->Add(effect);
                }
                if (context.Ledger)
                {
                    for (const EffectLedgerEntry& entry : result.Ledger.Entries())
                        context.Ledger->Record(entry);
                }
                return true;
            },
            {
                Port("action", BehaviorValueType::String),
                Port("actionId", BehaviorValueType::String),
                Port("detail", BehaviorValueType::String),
                Port("wait", BehaviorValueType::Bool)
            },
            {
                Port("success", BehaviorValueType::Bool),
                Port("value", BehaviorValueType::Bool)
            }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "wao.emit_signal",
            "Emit Action Signal",
            BehaviorPrimitiveCategory::Presentation,
            [](BehaviorPrimitiveContext& context)
            {
                const std::string signal = context.Input("signal", context.Input("signalId")).AsString();
                if (signal.empty())
                {
                    context.Fail("Emit signal requires a signal id.");
                    return false;
                }

                ActionSignalRouter::Emit({
                    context.Intent,
                    context.Intent.ActionId,
                    signal,
                    context.Intent.Source,
                    context.Input("detail").AsString("Behavior VM"),
                    nullptr
                });
                RecordPrimitiveEffect(
                    context,
                    EffectType::EmitSignal,
                    context.Intent.ExplicitTarget,
                    "Emit " + signal,
                    0.0f,
                    true);
                context.SetOutput("value", true);
                return true;
            },
            {
                Port("signal", BehaviorValueType::String),
                Port("signalId", BehaviorValueType::String),
                Port("detail", BehaviorValueType::String)
            },
            { Port("value", BehaviorValueType::Bool) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "behavior.emit_event",
            "Emit Behavior Event",
            BehaviorPrimitiveCategory::Flow,
            [](BehaviorPrimitiveContext& context)
            {
                if (!context.EmitEvent)
                {
                    context.Fail("Behavior event emitter has no runtime queue.");
                    return false;
                }

                const std::string eventId = context.Input("event", context.Input("eventId")).AsString();
                if (eventId.empty())
                {
                    context.Fail("Behavior event requires an event id.");
                    return false;
                }

                BehaviorEvent event;
                event.Id = eventId;
                event.Source = context.Intent.Actor;
                event.Target = context.Intent.ExplicitTarget;
                context.EmitEvent(event);
                context.SetOutput("value", true);
                return true;
            },
            {
                Port("event", BehaviorValueType::String),
                Port("eventId", BehaviorValueType::String)
            },
            { Port("value", BehaviorValueType::Bool) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "combat.open_guard_window",
            "Open Combat Guard Window",
            BehaviorPrimitiveCategory::Effect,
            [](BehaviorPrimitiveContext& context)
            {
                SideCombatantComponent* combatant = ResolveSideCombatant(context);
                if (!combatant)
                {
                    context.Fail("Guard window requires a SideCombatant actor.");
                    return false;
                }

                const float duration = static_cast<float>(
                    std::max(0.0, context.Input("duration", 0.24).AsNumber(0.24)));
                const float parryWindow = static_cast<float>(
                    std::max(0.0, context.Input("parryWindow", 0.08).AsNumber(0.08)));
                const float damageMultiplier = static_cast<float>(
                    std::clamp(
                        context.Input("damageMultiplier", 0.25).AsNumber(0.25),
                        0.0,
                        1.0));
                SideCombatGuardService::OpenGuardWindow(
                    *combatant,
                    duration,
                    parryWindow,
                    damageMultiplier);
                context.SetOutput("value", true);
                return true;
            },
            {
                Port("actor", BehaviorValueType::UUID),
                Port("duration", BehaviorValueType::Number),
                Port("parryWindow", BehaviorValueType::Number),
                Port("damageMultiplier", BehaviorValueType::Number)
            },
            { Port("value", BehaviorValueType::Bool) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "combat.close_guard_window",
            "Close Combat Guard Window",
            BehaviorPrimitiveCategory::Effect,
            [](BehaviorPrimitiveContext& context)
            {
                SideCombatantComponent* combatant = ResolveSideCombatant(context);
                if (!combatant)
                {
                    context.Fail("Guard window requires a SideCombatant actor.");
                    return false;
                }
                SideCombatGuardService::CloseGuardWindow(*combatant);
                context.SetOutput("value", true);
                return true;
            },
            { Port("actor", BehaviorValueType::UUID) },
            { Port("value", BehaviorValueType::Bool) }));

        BehaviorPrimitiveRegistry::Register(Definition(
            "combat.is_guard_window_open",
            "Is Combat Guard Window Open",
            BehaviorPrimitiveCategory::Query,
            [](BehaviorPrimitiveContext& context)
            {
                SideCombatantComponent* combatant = ResolveSideCombatant(context);
                if (!combatant)
                {
                    context.Fail("Guard query requires a SideCombatant actor.");
                    return false;
                }
                context.SetOutput("value",
                    combatant->RuntimeGuardActive &&
                    combatant->RuntimeGuardTimer > 0.0f);
                return true;
            },
            { Port("actor", BehaviorValueType::UUID) },
            { Port("value", BehaviorValueType::Bool) }));
    }

} // namespace Wheatear::WAO
