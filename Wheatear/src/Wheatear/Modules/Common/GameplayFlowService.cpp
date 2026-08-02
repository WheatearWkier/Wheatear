#include "wtpch.h"
#include "GameplayFlowService.h"

#include <algorithm>

namespace Wheatear::GameplayFlowService {

    bool TryIssueDelayedCommand(float elapsed,
        float delay,
        bool& issued,
        std::string& requestedCommand,
        const std::string& command)
    {
        if (issued || command.empty() || elapsed < std::max(0.0f, delay))
            return false;

        requestedCommand = command;
        issued = true;
        return true;
    }

} // namespace Wheatear::GameplayFlowService
