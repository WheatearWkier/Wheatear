#pragma once

#include "SideCombatComponents.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"

#include <string>

namespace Wheatear::SideCombatActionCatalog {

    WHEATEAR_API std::string ActionRecipeId(const std::string& attackId);
    WHEATEAR_API WAO::ActionRecipe BuildActionRecipe(const std::string& attackId,
        const SideCombatTuningService::SideAttackTuning& attack,
        SideAttackKind kind,
        const std::string& displayName,
        const std::string& description,
        float cooldown,
        float resourceCost = 0.0f);
    WHEATEAR_API void RegisterActionRecipes();

} // namespace Wheatear::SideCombatActionCatalog
