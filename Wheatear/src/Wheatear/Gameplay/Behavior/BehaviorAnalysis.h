#pragma once

#include "BehaviorTypes.h"
#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear::WAO {

    struct BehaviorAnalysisIssue
    {
        bool Error = false;
        std::string BehaviorId;
        std::string EntryId;
        std::string NodeId;
        std::string Message;
    };

    // Static checks over a compiled behavior program: reachability, control
    // flow cycles, branch-local producers and duplicate local writes.
    WHEATEAR_API std::vector<BehaviorAnalysisIssue> AnalyzeProgram(
        const BehaviorProgram& program);

} // namespace Wheatear::WAO
