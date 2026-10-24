#pragma once

#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear::Logic {

    enum class DiagnosticSeverity
    {
        Info = 0,
        Warning,
        Error
    };

    struct LogicDiagnostic
    {
        DiagnosticSeverity Severity = DiagnosticSeverity::Warning;
        std::string Source;   // e.g. "event-script", "visual-novel", "wao"
        std::string Message;
    };

    // Shared diagnostic sink for the logic runtime. DSL loaders and
    // evaluators report through this instead of only writing logs, so the
    // editor can present one consolidated list for .wts/.vn/WAO issues.
    class WHEATEAR_API Diagnostics
    {
    public:
        static void Report(DiagnosticSeverity severity,
            const std::string& source,
            const std::string& message);
        static void Info(const std::string& source, const std::string& message);
        static void Warn(const std::string& source, const std::string& message);
        static void Error(const std::string& source, const std::string& message);

        static std::vector<LogicDiagnostic> Snapshot();
        static size_t Count(DiagnosticSeverity severity);
        static size_t TotalCount();
        static void Clear();
    };

} // namespace Wheatear::Logic
