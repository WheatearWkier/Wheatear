#pragma once

#include "ActionRunner.h"
#include "ActionTypes.h"
#include "Wheatear/Core/Core.h"

#include <functional>
#include <string>
#include <vector>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::WAO {

    struct ActionResolveContext
    {
        Scene* SceneContext = nullptr;
        ActionIntent Intent;
        ActionRuntime* Runtime = nullptr;
        const void* TransientPayload = nullptr;
        std::string Detail;
    };

    struct ActionResolveResult
    {
        bool Handled = false;
        bool Success = false;
        EffectBundle AppliedEffects;
        EffectLedger Ledger;
        std::string Detail;
    };

    class WHEATEAR_API ActionResolverRegistry
    {
    public:
        using Resolver = std::function<ActionResolveResult(const ActionResolveContext&, const ActionRecipe&)>;

        static void RegisterResolver(const std::string& actionPrefix, Resolver resolver);
        static ActionResolveResult Resolve(const ActionResolveContext& context, const ActionRecipe& recipe);
        static void ClearResolvers();

    private:
        struct ResolverEntry
        {
            std::string Prefix;
            Resolver Callback;
        };

        static std::vector<ResolverEntry>& Resolvers();
    };

    class WHEATEAR_API ActionOrchestrator
    {
    public:
        static ActionResolveResult Execute(const ActionResolveContext& context);
        static ActionResolveResult ExecuteWithRecipe(const ActionResolveContext& context, const ActionRecipe& recipe);
    };

    WHEATEAR_API ActionResolveResult ResolveRecipePreview(const ActionResolveContext& context,
        const ActionRecipe& recipe,
        const std::string& detail);
    WHEATEAR_API void RegisterRecipePreviewResolver(const std::string& actionPrefix,
        const std::string& detail);

} // namespace Wheatear::WAO
