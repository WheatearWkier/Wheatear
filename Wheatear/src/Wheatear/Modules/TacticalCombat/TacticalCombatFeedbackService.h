#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::TacticalCombatFeedbackService {

    WHEATEAR_API void PlaySound(const std::string& path, float volume = 0.48f);

} // namespace Wheatear::TacticalCombatFeedbackService
