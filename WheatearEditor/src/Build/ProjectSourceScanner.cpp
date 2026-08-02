#include "wepch.h"
#include "ProjectSourceScanner.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <system_error>
#include <tuple>

namespace Wheatear {

    namespace {

        struct SourceProjectSpec
        {
            const char* Name;
            std::filesystem::path ProjectFile;
            std::filesystem::path SourceRoot;
        };

        std::string NormalizeProjectPath(const std::filesystem::path& path)
        {
            std::filesystem::path normalized = path.lexically_normal();
            std::string text = normalized.generic_string();
            std::transform(text.begin(), text.end(), text.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        bool IsTrackedSourceExtension(const std::filesystem::path& path)
        {
            const std::string extension = NormalizeProjectPath(path.extension());
            return extension == ".cpp" || extension == ".h" || extension == ".hpp" || extension == ".inl";
        }

        bool ReadTextFile(const std::filesystem::path& path, std::string* text)
        {
            if (!text)
                return false;

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            *text = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            return true;
        }

        std::set<std::string> ScanSourceTree(const std::filesystem::path& workspaceRoot,
            const SourceProjectSpec& spec,
            ProjectSourceReport* report)
        {
            std::set<std::string> files;
            const std::filesystem::path absoluteRoot = workspaceRoot / spec.SourceRoot;
            std::error_code error;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(absoluteRoot, error))
            {
                if (error || !entry.is_regular_file() || !IsTrackedSourceExtension(entry.path()))
                    continue;

                const std::filesystem::path relativeToProject =
                    std::filesystem::relative(entry.path(), workspaceRoot / spec.ProjectFile.parent_path(), error);
                if (!error)
                {
                    files.insert(NormalizeProjectPath(relativeToProject));
                    if (report)
                        ++report->ScannedSourceFiles;
                }
            }
            return files;
        }

        std::set<std::string> ScanProjectFile(const std::filesystem::path& workspaceRoot,
            const SourceProjectSpec& spec,
            ProjectSourceReport* report)
        {
            std::set<std::string> entries;
            std::string text;
            if (!ReadTextFile(workspaceRoot / spec.ProjectFile, &text))
                return entries;

            static const std::regex sourceRegex("(?:ClCompile|ClInclude)\\s+Include=\"([^\"]+)\"");
            for (std::sregex_iterator it(text.begin(), text.end(), sourceRegex), end; it != end; ++it)
            {
                const std::filesystem::path entryPath = (*it)[1].str();
                if (!IsTrackedSourceExtension(entryPath))
                    continue;

                const std::string normalized = NormalizeProjectPath(entryPath);
                if (normalized.rfind("src/", 0) != 0)
                    continue;

                entries.insert(normalized);
                if (report)
                    ++report->ScannedProjectEntries;
            }
            return entries;
        }

        void CompareProject(const std::filesystem::path& workspaceRoot,
            const SourceProjectSpec& spec,
            ProjectSourceReport* report)
        {
            if (!report)
                return;

            const std::set<std::string> sourceFiles = ScanSourceTree(workspaceRoot, spec, report);
            const std::set<std::string> projectEntries = ScanProjectFile(workspaceRoot, spec, report);

            for (const std::string& file : sourceFiles)
            {
                if (!projectEntries.count(file))
                    report->MissingFromProject.push_back({ spec.Name, file });
            }

            for (const std::string& file : projectEntries)
            {
                if (!sourceFiles.count(file))
                    report->StaleProjectEntries.push_back({ spec.Name, file });
            }
        }

        std::filesystem::path FindWorkspaceRoot(std::filesystem::path root)
        {
            if (root.empty())
                root = std::filesystem::current_path();

            root = root.lexically_normal();
            if (std::filesystem::exists(root / "Wheatear.sln"))
                return root;
            if (root.filename() == "WheatearEditor" && std::filesystem::exists(root.parent_path() / "Wheatear.sln"))
                return root.parent_path();
            if (root.filename() == "WheatearSandbox" && std::filesystem::exists(root.parent_path() / "Wheatear.sln"))
                return root.parent_path();

            std::filesystem::path cursor = root;
            while (!cursor.empty())
            {
                if (std::filesystem::exists(cursor / "Wheatear.sln"))
                    return cursor;

                const std::filesystem::path parent = cursor.parent_path();
                if (parent == cursor)
                    break;
                cursor = parent;
            }

            return root;
        }

    } // namespace

    ProjectSourceReport ProjectSourceScanner::BuildReport(const std::filesystem::path& workspaceRoot)
    {
        const std::filesystem::path root = FindWorkspaceRoot(workspaceRoot);
        const SourceProjectSpec specs[] = {
            { "Wheatear", "Wheatear/Wheatear.vcxproj", "Wheatear/src" },
            { "WheatearEditor", "WheatearEditor/WheatearEditor.vcxproj", "WheatearEditor/src" },
            { "WheatearSandbox", "WheatearSandbox/WheatearSandbox.vcxproj", "WheatearSandbox/src" },
        };

        ProjectSourceReport report;
        for (const SourceProjectSpec& spec : specs)
            CompareProject(root, spec, &report);

        std::sort(report.MissingFromProject.begin(), report.MissingFromProject.end(),
            [](const ProjectSourceRecord& left, const ProjectSourceRecord& right)
            {
                return std::tie(left.ProjectName, left.File) < std::tie(right.ProjectName, right.File);
            });
        std::sort(report.StaleProjectEntries.begin(), report.StaleProjectEntries.end(),
            [](const ProjectSourceRecord& left, const ProjectSourceRecord& right)
            {
                return std::tie(left.ProjectName, left.File) < std::tie(right.ProjectName, right.File);
            });
        return report;
    }

} // namespace Wheatear
