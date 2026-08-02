#include "wtpch.h"
#include "ActionDatabase.h"

#include <algorithm>

namespace Wheatear::WAO {

    std::vector<ActionRecipe>& ActionDatabase::Recipes()
    {
        static std::vector<ActionRecipe> recipes;
        return recipes;
    }

    void ActionDatabase::Register(const ActionRecipe& recipe)
    {
        if (recipe.Id.empty())
            return;

        auto& recipes = Recipes();
        const auto it = std::find_if(recipes.begin(),
            recipes.end(),
            [&](const ActionRecipe& current)
            {
                return current.Id == recipe.Id;
            });
        if (it != recipes.end())
        {
            *it = recipe;
            return;
        }

        recipes.push_back(recipe);
    }

    const ActionRecipe* ActionDatabase::Find(const std::string& id)
    {
        auto& recipes = Recipes();
        const auto it = std::find_if(recipes.begin(),
            recipes.end(),
            [&](const ActionRecipe& current)
            {
                return current.Id == id;
            });
        return it == recipes.end() ? nullptr : &(*it);
    }

    bool ActionDatabase::Has(const std::string& id)
    {
        return Find(id) != nullptr;
    }

    std::vector<ActionRecipe> ActionDatabase::All()
    {
        return Recipes();
    }

    void ActionDatabase::Clear()
    {
        Recipes().clear();
    }

} // namespace Wheatear::WAO
