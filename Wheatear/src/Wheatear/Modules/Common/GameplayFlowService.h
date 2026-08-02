#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::GameplayFlowService {

    WHEATEAR_API bool TryIssueDelayedCommand(float elapsed,
        float delay,
        bool& issued,
        std::string& requestedCommand,
        const std::string& command);

} // namespace Wheatear::GameplayFlowService
