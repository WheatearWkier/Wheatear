#pragma once

#include "ActionTypes.h"
#include "Wheatear/Core/Core.h"

#include <string>
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

    private:
        static std::vector<ActionRecipe>& Recipes();
    };

} // namespace Wheatear::WAO
