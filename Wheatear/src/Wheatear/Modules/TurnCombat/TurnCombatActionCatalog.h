#pragma once

#include "TurnCombatSkillService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"

#include <string>

namespace Wheatear::TurnCombatActionCatalog {

    WHEATEAR_API std::string ActionRecipeId(const std::string& skillId);
    WHEATEAR_API WAO::ActionRecipe BuildActionRecipe(const TurnCombatSkillService::TurnSkillDefinition& skill);
    WHEATEAR_API void RegisterActionRecipes();

} // namespace Wheatear::TurnCombatActionCatalog
