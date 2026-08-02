#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"

namespace Wheatear::ArcadeCombatActionCatalog {

    WHEATEAR_API const char* WeaponActionId(ArcadeWeaponType weapon);
    WHEATEAR_API WAO::ActionRecipe BuildWeaponRecipe(ArcadeWeaponType weapon);
    WHEATEAR_API WAO::ActionRecipe BuildBossShotRecipe();
    WHEATEAR_API float PrimaryDamage(const WAO::ActionRecipe& recipe, float fallback);
    WHEATEAR_API void RegisterActionRecipes();

} // namespace Wheatear::ArcadeCombatActionCatalog
