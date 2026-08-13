#include "wtpch.h"
#include "ActionDatabase.h"

namespace Wheatear::WAO {

    std::unordered_map<std::string, ActionRecipe>& ActionDatabase::Recipes()
    {
        static std::unordered_map<std::string, ActionRecipe> recipes;
        return recipes;
    }

    uint64_t& ActionDatabase::RevisionCounter()
    {
        static uint64_t revision = 0;
        return revision;
    }

    void ActionDatabase::Register(const ActionRecipe& recipe)
    {
        if (recipe.Id.empty())
            return;

        Recipes()[recipe.Id] = recipe;
        ++RevisionCounter();
    }

    const ActionRecipe* ActionDatabase::Find(const std::string& id)
    {
        const auto& recipes = Recipes();
        const auto it = recipes.find(id);
        return it == recipes.end() ? nullptr : &(it->second);
    }

    bool ActionDatabase::Has(const std::string& id)
    {
        return Recipes().count(id) > 0;
    }

    std::vector<ActionRecipe> ActionDatabase::All()
    {
        std::vector<ActionRecipe> all;
        all.reserve(Recipes().size());
        for (const auto& [id, recipe] : Recipes())
            all.push_back(recipe);
        return all;
    }

    void ActionDatabase::Clear()
    {
        Recipes().clear();
        ++RevisionCounter();
    }

    uint64_t ActionDatabase::Revision()
    {
        return RevisionCounter();
    }

} // namespace Wheatear::WAO
