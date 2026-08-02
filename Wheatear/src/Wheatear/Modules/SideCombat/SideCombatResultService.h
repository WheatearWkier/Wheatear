#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::SideCombatResultService {

    WHEATEAR_API std::string CalculateGrade(int bestCombo, int hitsTaken, float elapsedSeconds);
    WHEATEAR_API int GetGradeExperienceBonus(const std::string& grade, bool firstClear);
    WHEATEAR_API void RefreshResult(SideCombatLevelComponent& level);

} // namespace Wheatear::SideCombatResultService
