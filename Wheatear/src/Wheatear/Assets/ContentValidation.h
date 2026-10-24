#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    enum class ContentIssueSeverity
    {
        Warning = 0,
        Error
    };

    struct ContentIssue
    {
        std::filesystem::path Asset;
        int SourceLine = 0;
        ContentIssueSeverity Severity = ContentIssueSeverity::Warning;
        std::string Message;
    };

    struct ContentValidationReport
    {
        std::vector<ContentIssue> Issues;
        size_t FilesChecked = 0;

        bool HasErrors() const;
        size_t ErrorCount() const;
        size_t WarningCount() const;
    };

    // Validates authored content without entering Play mode. This is the
    // single entry point used by editor health checks, packaging and the
    // `--validate-content` CLI so all three agree on what "broken" means.
    class WHEATEAR_API ContentValidator
    {
    public:
        static ContentValidationReport ValidateFile(const std::filesystem::path& path);
        static ContentValidationReport ValidateDirectory(const std::filesystem::path& path);
    };

} // namespace Wheatear
