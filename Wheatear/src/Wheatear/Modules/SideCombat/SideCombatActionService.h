#pragma once

#include "SideCombatComponents.h"
#include "SideCombatTuningService.h"
#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::SideCombatActionService {

    WHEATEAR_API float GetActionDuration(const SideCombatTuningService::SideAttackTuning& attack);

    WHEATEAR_API bool IsPlayerActionActive(const SidePlayerControllerComponent& controller);
    WHEATEAR_API bool CanStartPlayerAction(const SidePlayerControllerComponent& controller);
    WHEATEAR_API float GetPlayerActionMovementScale(const SidePlayerControllerComponent& controller);
    WHEATEAR_API void ClearPlayerAction(SidePlayerControllerComponent& controller);
    WHEATEAR_API void BeginPlayerAction(SidePlayerControllerComponent& controller,
        const SideCombatTuningService::SideAttackTuning& attack,
        const std::string& attackId,
        const std::string& recipeId,
        const std::string& entityName,
        SideAttackKind kind);

    WHEATEAR_API bool IsEnemyActionActive(const SideEnemyAIComponent& ai);
    WHEATEAR_API void ClearEnemyAction(SideEnemyAIComponent& ai);
    WHEATEAR_API void BeginEnemyAction(SideEnemyAIComponent& ai,
        const SideCombatTuningService::SideAttackTuning& attack,
        const std::string& attackId,
        const std::string& recipeId,
        const std::string& entityName,
        SideAttackKind kind,
        float facing);

} // namespace Wheatear::SideCombatActionService
