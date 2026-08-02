#pragma once

#include "ActionTypes.h"
#include "StateRegistry.h"
#include "Wheatear/Core/Core.h"

#include <unordered_map>

namespace Wheatear::WAO {

    struct ActionRuntime
    {
        AttributeStore Attributes;
        std::vector<RuntimeState> States;
        std::vector<std::string> Tags;
        std::unordered_map<std::string, float> Cooldowns;
        std::unordered_map<std::string, float> Resources;
    };

    struct ActionExecutionResult
    {
        bool Success = false;
        EffectLedger Ledger;
        EffectBundle AppliedEffects;
    };

    WHEATEAR_API bool CanAfford(const ActionRecipe& recipe, const ActionRuntime& runtime);
    WHEATEAR_API bool CanActivate(const ActionRecipe& recipe, const ActionRuntime& runtime);
    WHEATEAR_API ActionExecutionResult Execute(const ActionIntent& intent,
        const ActionRecipe& recipe,
        ActionRuntime& runtime);

} // namespace Wheatear::WAO
