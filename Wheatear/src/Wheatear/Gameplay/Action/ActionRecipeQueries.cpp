#include "wtpch.h"
#include "ActionRecipeQueries.h"

#include "ActionDatabase.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <unordered_set>

namespace Wheatear::WAO {

    using Wheatear::StringUtils::ToLower;

    namespace {

        static std::unordered_set<std::string>& MissingRecipeWarnings()
        {
            static std::unordered_set<std::string> warnings;
            return warnings;
        }

    } // namespace

    std::string ComposeActionId(std::string_view prefix, std::string_view localId)
    {
        if (prefix.empty())
            return std::string(localId);

        std::string result(prefix);
        if (!result.empty() && result.back() != '.')
            result.push_back('.');
        result.append(localId.data(), localId.size());
        return result;
    }

    const ActionRecipe* FindRecipeOrWarn(const std::string& recipeId, const char* owner)
    {
        if (recipeId.empty())
            return nullptr;

        if (const ActionRecipe* recipe = ActionDatabase::Find(recipeId))
            return recipe;

        const std::string source = owner && owner[0] ? owner : "WAO";
        const std::string key = source + ":" + recipeId;
        if (MissingRecipeWarnings().insert(key).second)
            WT_CORE_WARN("{}: missing WAO action recipe '{}'. Check editable action data.", source, recipeId);
        return nullptr;
    }

    std::vector<ActionRecipe> RecipesWithPrefix(std::string_view prefix)
    {
        std::vector<ActionRecipe> recipes;
        const std::string prefixText(prefix);
        for (const ActionRecipe& recipe : ActionDatabase::All())
        {
            if (prefixText.empty() || recipe.Id.rfind(prefixText, 0) == 0)
                recipes.push_back(recipe);
        }
        std::sort(recipes.begin(),
            recipes.end(),
            [](const ActionRecipe& a, const ActionRecipe& b)
            {
                return a.Id < b.Id;
            });
        return recipes;
    }

    const EffectSpec* FirstEffect(const ActionRecipe& recipe, EffectType type)
    {
        for (const EffectSpec& effect : recipe.Effects)
        {
            if (effect.Type == type)
                return &effect;
        }
        return nullptr;
    }

    std::string ParamString(const ActionRecipe& recipe,
        const std::string& key,
        const std::string& fallback)
    {
        if (auto it = recipe.Params.find(key); it != recipe.Params.end())
            return it->second;
        return fallback;
    }

    float ParamFloat(const ActionRecipe& recipe,
        const std::string& key,
        float fallback)
    {
        try
        {
            const std::string value = ParamString(recipe, key);
            return value.empty() ? fallback : std::stof(value);
        }
        catch (...)
        {
            return fallback;
        }
    }

    int ParamInt(const ActionRecipe& recipe,
        const std::string& key,
        int fallback)
    {
        try
        {
            const std::string value = ParamString(recipe, key);
            return value.empty() ? fallback : std::stoi(value);
        }
        catch (...)
        {
            return fallback;
        }
    }

    bool ParamBool(const ActionRecipe& recipe,
        const std::string& key,
        bool fallback)
    {
        const std::string value = ToLower(ParamString(recipe, key));
        if (value == "1" || value == "true" || value == "yes")
            return true;
        if (value == "0" || value == "false" || value == "no")
            return false;
        return fallback;
    }

    float PrimaryEffectValue(const ActionRecipe& recipe, EffectType type, float fallback)
    {
        const EffectSpec* effect = FirstEffect(recipe, type);
        return effect ? effect->Value : fallback;
    }

    float ResourceCost(const ActionRecipe& recipe, const std::string& resourceId, float fallback)
    {
        const auto it = recipe.ResourceCost.find(resourceId);
        return it != recipe.ResourceCost.end() ? it->second : fallback;
    }

    bool HasTag(const ActionRecipe& recipe, std::string_view tag)
    {
        return std::find_if(recipe.Tags.begin(), recipe.Tags.end(),
            [&](const std::string& current)
            {
                return current == tag;
            }) != recipe.Tags.end();
    }

} // namespace Wheatear::WAO
