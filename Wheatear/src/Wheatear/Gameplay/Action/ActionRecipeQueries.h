#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Wheatear::WAO {

    WHEATEAR_API std::string ComposeActionId(std::string_view prefix, std::string_view localId);
    WHEATEAR_API const ActionRecipe* FindRecipeOrWarn(const std::string& recipeId, const char* owner);
    WHEATEAR_API std::vector<ActionRecipe> RecipesWithPrefix(std::string_view prefix);
    WHEATEAR_API const EffectSpec* FirstEffect(const ActionRecipe& recipe, EffectType type);
    WHEATEAR_API std::string ParamString(const ActionRecipe& recipe,
        const std::string& key,
        const std::string& fallback = {});
    WHEATEAR_API float ParamFloat(const ActionRecipe& recipe,
        const std::string& key,
        float fallback = 0.0f);
    WHEATEAR_API int ParamInt(const ActionRecipe& recipe,
        const std::string& key,
        int fallback = 0);
    WHEATEAR_API bool ParamBool(const ActionRecipe& recipe,
        const std::string& key,
        bool fallback = false);
    WHEATEAR_API float PrimaryEffectValue(const ActionRecipe& recipe, EffectType type, float fallback = 0.0f);
    WHEATEAR_API float ResourceCost(const ActionRecipe& recipe, const std::string& resourceId, float fallback = 0.0f);
    WHEATEAR_API bool HasTag(const ActionRecipe& recipe, std::string_view tag);

} // namespace Wheatear::WAO
