#include "wtpch.h"
#include "ActionResolver.h"

#include "ActionDebugHistory.h"
#include "ActionRecipeQueries.h"
#include "ActionSignalRouter.h"

#include <algorithm>
#include <utility>

namespace Wheatear::WAO {

    namespace {

        bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return prefix.empty() || value.rfind(prefix, 0) == 0;
        }

        ActionResolveResult MakeFailure(const ActionResolveContext& context,
            const std::string& detail)
        {
            ActionResolveResult result;
            result.Handled = false;
            result.Success = false;
            result.Detail = detail;
            result.Ledger.BeginAction(context.Intent);
            result.Ledger.Record({
                context.Intent.ActionId,
                EffectType::None,
                context.Intent.Actor,
                context.Intent.ExplicitTarget,
                detail,
                0.0f,
                false
            });
            ActionDebugHistory::Record(result.Ledger, false, detail);
            return result;
        }

        void RecordEffect(EffectLedger& ledger,
            const ActionIntent& intent,
            EffectType type,
            UUID target,
            const std::string& detail,
            float value,
            bool applied)
        {
            ledger.Record({
                intent.ActionId,
                type,
                intent.Actor,
                target ? target : intent.ExplicitTarget,
                detail,
                value,
                applied
            });
        }

    } // namespace

    std::vector<ActionResolverRegistry::ResolverEntry>& ActionResolverRegistry::Resolvers()
    {
        static std::vector<ResolverEntry> resolvers;
        return resolvers;
    }

    void ActionResolverRegistry::RegisterResolver(const std::string& actionPrefix, Resolver resolver)
    {
        if (!resolver)
            return;

        auto& resolvers = Resolvers();
        const auto it = std::find_if(resolvers.begin(), resolvers.end(),
            [&](const ResolverEntry& entry)
            {
                return entry.Prefix == actionPrefix;
            });

        if (it != resolvers.end())
        {
            it->Callback = std::move(resolver);
            return;
        }

        resolvers.push_back({ actionPrefix, std::move(resolver) });
    }

    ActionResolveResult ActionResolverRegistry::Resolve(const ActionResolveContext& context,
        const ActionRecipe& recipe)
    {
        for (const ResolverEntry& entry : Resolvers())
        {
            if (!StartsWith(recipe.Id, entry.Prefix))
                continue;

            ActionResolveResult result = entry.Callback(context, recipe);
            if (result.Handled)
                return result;
        }

        return {};
    }

    void ActionResolverRegistry::ClearResolvers()
    {
        Resolvers().clear();
    }

    ActionResolveResult ActionOrchestrator::Execute(const ActionResolveContext& context)
    {
        const ActionRecipe* recipe = FindRecipeOrWarn(context.Intent.ActionId, "ActionOrchestrator");
        if (!recipe)
            return MakeFailure(context, "Action recipe not found");

        return ExecuteWithRecipe(context, *recipe);
    }

    ActionResolveResult ActionOrchestrator::ExecuteWithRecipe(const ActionResolveContext& context,
        const ActionRecipe& recipe)
    {
        ActionResolveContext resolvedContext = context;
        if (resolvedContext.Intent.ActionId.empty())
            resolvedContext.Intent.ActionId = recipe.Id;

        ActionResolveResult resolved = ActionResolverRegistry::Resolve(resolvedContext, recipe);
        if (resolved.Handled)
        {
            if (!resolved.Ledger.Entries().empty())
            {
                ActionDebugHistory::Record(resolved.Ledger,
                    resolved.Success,
                    resolved.Detail.empty() ? "Resolved" : resolved.Detail);
            }
            return resolved;
        }

        if (resolvedContext.Runtime)
        {
            ActionExecutionResult executed = WAO::Execute(resolvedContext.Intent, recipe, *resolvedContext.Runtime);
            ActionResolveResult result;
            result.Handled = true;
            result.Success = executed.Success;
            result.AppliedEffects = executed.AppliedEffects;
            result.Ledger = executed.Ledger;
            result.Detail = executed.Success ? "Executed by ActionRunner" : "ActionRunner failed";
            return result;
        }

        return MakeFailure(resolvedContext, "No resolver or runtime for action");
    }

    ActionResolveResult ResolveRecipePreview(const ActionResolveContext& context,
        const ActionRecipe& recipe,
        const std::string& detail)
    {
        ActionResolveContext resolvedContext = context;
        if (resolvedContext.Intent.ActionId.empty())
            resolvedContext.Intent.ActionId = recipe.Id;

        ActionResolveResult result;
        result.Handled = true;
        result.Success = true;
        result.Detail = detail.empty() ? "Recipe resolved" : detail;
        result.Ledger.BeginAction(resolvedContext.Intent);

        RecordEffect(result.Ledger,
            resolvedContext.Intent,
            EffectType::None,
            resolvedContext.Intent.ExplicitTarget,
            result.Detail,
            0.0f,
            true);

        for (const auto& [resource, cost] : recipe.ResourceCost)
        {
            RecordEffect(result.Ledger,
                resolvedContext.Intent,
                EffectType::ConsumeResource,
                resolvedContext.Intent.Actor,
                "Recipe cost " + resource,
                cost,
                false);
        }

        if (recipe.Cooldown > 0.0f)
        {
            RecordEffect(result.Ledger,
                resolvedContext.Intent,
                EffectType::StartCooldown,
                resolvedContext.Intent.Actor,
                "Recipe cooldown",
                recipe.Cooldown,
                false);
        }

        for (const EffectSpec& effect : recipe.Effects)
        {
            RecordEffect(result.Ledger,
                resolvedContext.Intent,
                effect.Type,
                effect.Target,
                "Recipe effect preview",
                effect.Value,
                false);
            result.AppliedEffects.Add(effect);
        }

        for (const std::string& signal : recipe.Signals)
        {
            ActionSignalContext signalContext;
            signalContext.Intent = resolvedContext.Intent;
            signalContext.ActionId = recipe.Id;
            signalContext.SignalId = signal;
            signalContext.Source = resolvedContext.Intent.Source;
            signalContext.Detail = resolvedContext.Detail;
            signalContext.TransientPayload = resolvedContext.TransientPayload;
            ActionSignalRouter::Emit(signalContext);
            RecordEffect(result.Ledger,
                resolvedContext.Intent,
                EffectType::EmitSignal,
                resolvedContext.Intent.ExplicitTarget,
                "Emit " + signal,
                0.0f,
                true);
        }

        return result;
    }

    void RegisterRecipePreviewResolver(const std::string& actionPrefix,
        const std::string& detail)
    {
        ActionResolverRegistry::RegisterResolver(actionPrefix,
            [detail](const ActionResolveContext& context, const ActionRecipe& recipe)
            {
                return ResolveRecipePreview(context, recipe, detail);
            });
    }

} // namespace Wheatear::WAO
