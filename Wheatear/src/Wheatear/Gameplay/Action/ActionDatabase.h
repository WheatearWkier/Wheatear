#pragma once

#include "ActionTypes.h"
#include "Wheatear/Core/Core.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::WAO {

    class WHEATEAR_API ActionDatabase
    {
    public:
        static void Register(const ActionRecipe& recipe);
        static const ActionRecipe* Find(const std::string& id);
        static bool Has(const std::string& id);
        static std::vector<ActionRecipe> All();
        static void Clear();
        static uint64_t Revision();

    private:
        // Keyed by recipe Id for O(1) lookups; node addresses are stable across
        // insert/erase, so Find()'s returned pointer stays valid.
        static std::unordered_map<std::string, ActionRecipe>& Recipes();
        static uint64_t& RevisionCounter();
    };

} // namespace Wheatear::WAO
