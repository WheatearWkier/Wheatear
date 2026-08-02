#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    struct ProjectSourceRecord
    {
        std::string ProjectName;
        std::filesystem::path File;
    };

    struct ProjectSourceReport
    {
        std::vector<ProjectSourceRecord> MissingFromProject;
        std::vector<ProjectSourceRecord> StaleProjectEntries;
        size_t ScannedSourceFiles = 0;
        size_t ScannedProjectEntries = 0;

        bool Healthy() const
        {
            return MissingFromProject.empty() && StaleProjectEntries.empty();
        }
    };

    class ProjectSourceScanner
    {
    public:
        static ProjectSourceReport BuildReport(const std::filesystem::path& workspaceRoot);
    };

} // namespace Wheatear
