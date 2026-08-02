#pragma once

#include "TacticalCombatSkillService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"

#include <string>

namespace Wheatear::TacticalCombatActionCatalog {

    WHEATEAR_API std::string ActionRecipeId(const std::string& skillId);
    WHEATEAR_API WAO::ActionRecipe BuildActionRecipe(const TacticalCombatSkillService::TacticalSkillDefinition& skill);
    WHEATEAR_API void RegisterActionRecipes();

} // namespace Wheatear::TacticalCombatActionCatalog
