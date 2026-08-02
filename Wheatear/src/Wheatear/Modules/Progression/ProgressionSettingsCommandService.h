#pragma once

#include "GameProgress.h"
#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::ProgressionSettingsCommandService {

    WHEATEAR_API bool IsSettingsCommand(const std::string& action);
    WHEATEAR_API GameProgress::CommandResult Execute(const std::string& action, GameProgress::State& state);
    WHEATEAR_API void ApplyToRuntime();
    WHEATEAR_API std::string BuildStatusText();

} // namespace Wheatear::ProgressionSettingsCommandService
