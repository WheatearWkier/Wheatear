#include "wepch.h"
#include <Wheatear.h>
#include "Assets/AssetRegistry.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/ModeSelectLayer.h"
#include "Editor/CoreEditorComponents.h"
#include "Build/AssetDependencyScanner.h"
#include "Build/PlayerPackager.h"
#include "Build/ProjectSourceScanner.h"
#include "Modules/ModuleEditorBootstrap.h"

#include <Wheatear/Core/EngineInfo.h>
#include <Wheatear/Core/EntryPoint.h>

#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <string>

namespace Wheatear
{
    namespace {

        static bool IsCommandOption(const std::string& argument)
        {
            return argument.rfind("--", 0) == 0;
        }

        static std::filesystem::path GetEditorConfigDirectory()
        {
            if (const char* localAppData = std::getenv("LOCALAPPDATA"))
                return std::filesystem::path(localAppData) / "Wheatear";
            return std::filesystem::temp_directory_path() / "Wheatear";
        }

        static std::filesystem::path ReadLastProject()
        {
            std::ifstream input(GetEditorConfigDirectory() / "last_project.txt");
            std::string line;
            std::getline(input, line);
            if (line.empty() || line[0] == '\0')
                return {};
            return std::filesystem::path(line);
        }

        static void WriteLastProject(const std::filesystem::path& projectRoot)
        {
            std::error_code error;
            const std::filesystem::path configDirectory = GetEditorConfigDirectory();
            std::filesystem::create_directories(configDirectory, error);
            if (error)
                return;
            std::ofstream output(configDirectory / "last_project.txt", std::ios::trunc);
            output << projectRoot.generic_string();
        }

        // Default project when nothing is specified: the repository's
        // Projects/ directory, falling back to automatic discovery.
        static std::filesystem::path DefaultProjectRoot()
        {
            const std::filesystem::path repositoryRoot = AssetPath::GetEngineRoot().parent_path();
            const std::filesystem::path demoProject = repositoryRoot / "Projects" / "WheatearDemo";
            if (std::filesystem::is_directory(demoProject / "assets"))
                return demoProject;
            return {};
        }

        static ApplicationSpecification CreateEditorSpecification(ApplicationCommandLineArgs args)
        {
            ApplicationSpecification specification;
            specification.Name = EngineInfo::EditorName;
            specification.CommandLineArgs = args;

            // Project root precedence: --project <dir> > WHEATEAR_PROJECT env >
            // last opened project > Projects/WheatearDemo > auto discovery.
            std::filesystem::path projectRoot;
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if (argument == "--project" && i + 1 < args.Count && !IsCommandOption(args[i + 1]))
                {
                    projectRoot = std::filesystem::path(args[i + 1]);
                    break;
                }
            }
            if (projectRoot.empty())
            {
                if (const char* env = std::getenv("WHEATEAR_PROJECT"); env && *env)
                    projectRoot = std::filesystem::path(env);
            }
            if (projectRoot.empty())
                projectRoot = ReadLastProject();
            if (projectRoot.empty())
                projectRoot = DefaultProjectRoot();
            specification.ProjectRoot = projectRoot.empty()
                ? AssetPath::DiscoverProjectRoot()
                : std::filesystem::absolute(projectRoot);
            return specification;
        }

        static bool IsPackagePlayerCommand(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if (argument == "--package" || argument == "--package-player" || argument == "--build-player")
                    return true;
            }
            return false;
        }

        static bool IsRefreshAssetRegistryCommand(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if (argument == "--refresh-assets" || argument == "--refresh-asset-registry")
                    return true;
            }
            return false;
        }

        static bool IsProjectHealthCommand(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if (argument == "--health" || argument == "--project-health")
                    return true;
            }
            return false;
        }

        static std::filesystem::path ReadStartupScene(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if ((argument == "--startup" || argument == "--startup-scene")
                    && i + 1 < args.Count
                    && !IsCommandOption(args[i + 1]))
                {
                    return args[i + 1];
                }
            }

            return EngineInfo::DefaultStartupScene;
        }

        static std::filesystem::path ReadPackageStartupScene(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if ((argument == "--package" || argument == "--package-player" || argument == "--build-player")
                    && i + 1 < args.Count
                    && !IsCommandOption(args[i + 1]))
                {
                    return args[i + 1];
                }
            }

            return EngineInfo::DefaultStartupScene;
        }

        static bool ReadIncludeUnusedAssets(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if (argument == "--unused" || argument == "--include-unused")
                    return true;
                if (argument == "--no-unused")
                    return false;
            }

            return false;
        }

        static int RunProjectHealth(ApplicationCommandLineArgs args)
        {
            AssetDependencyScanOptions options;
            options.ProjectRoot = AssetPath::GetProjectRoot();
            options.StartupAsset = ReadStartupScene(args);
            options.IncludeBuiltinAssets = true;
            options.IncludeUnusedAssets = ReadIncludeUnusedAssets(args);

            const AssetDependencyReport dependencyReport = AssetDependencyScanner::BuildReport(options);
            const ProjectSourceReport sourceReport = ProjectSourceScanner::BuildReport(options.ProjectRoot);
            AssetRegistry::Get().Scan(options.ProjectRoot);
            const bool registryWritten = AssetRegistry::Get().WriteRegistry();

            const size_t sourceSyncIssues = sourceReport.MissingFromProject.size()
                + sourceReport.StaleProjectEntries.size();
            const size_t blockingIssues = dependencyReport.MissingReferences.size()
                + dependencyReport.MissingSceneTransitions.size()
                + sourceSyncIssues
                + (registryWritten ? 0 : 1);

            WT_CORE_INFO("Project health startup '{}'", options.StartupAsset.string());
            WT_CORE_INFO("Project health included assets: {}", dependencyReport.IncludedAssets.size());
            WT_CORE_INFO("Project health parsed text assets: {}", dependencyReport.ParsedTextAssets.size());
            WT_CORE_INFO("Project health missing references: {}", dependencyReport.MissingReferences.size());
            WT_CORE_INFO("Project health missing scene transitions: {}", dependencyReport.MissingSceneTransitions.size());
            WT_CORE_INFO("Project health source sync issues: {}", sourceSyncIssues);
            WT_CORE_INFO("Project health registry write: {}", registryWritten ? "ok" : "failed");

            for (const AssetReferenceRecord& record : dependencyReport.MissingReferences)
                WT_CORE_ERROR("Missing asset reference: '{}' from '{}'", record.Reference, record.SourceAsset);
            for (const AssetReferenceRecord& record : dependencyReport.MissingSceneTransitions)
                WT_CORE_ERROR("Missing scene transition: '{}' from '{}'", record.Reference, record.SourceAsset);
            for (const std::string& warning : dependencyReport.Warnings)
                WT_CORE_WARN("Project health warning: {}", warning);
            for (const ProjectSourceRecord& source : sourceReport.MissingFromProject)
                WT_CORE_ERROR("Source file missing from project: '{}' ({})", source.File.string(), source.ProjectName);
            for (const ProjectSourceRecord& source : sourceReport.StaleProjectEntries)
                WT_CORE_ERROR("Stale project entry: '{}' ({})", source.File.string(), source.ProjectName);

            if (blockingIssues == 0)
            {
                WT_CORE_INFO("Project health passed.");
                return 0;
            }

            WT_CORE_ERROR("Project health failed with {} blocking issue(s).", blockingIssues);
            return 1;
        }

        static std::string ReadPackageConfiguration(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                if ((args[i] == "--configuration" || args[i] == "--config")
                    && i + 1 < args.Count
                    && !IsCommandOption(args[i + 1]))
                {
                    const std::string value = args[i + 1];
                    if (value == "Debug" || value == "Release")
                        return value;
                    WT_CORE_WARN("Unknown package configuration '{}'; falling back to Debug.", value);
                    return "Debug";
                }
            }
            return "Debug";
        }

        static int RunPackagePlayer(ApplicationCommandLineArgs args)
        {
            // --project selects which project to package; otherwise the
            // default project (Projects/WheatearDemo) or the engine root.
            bool projectSet = false;
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if (argument == "--project" && i + 1 < args.Count && !IsCommandOption(args[i + 1]))
                {
                    AssetPath::SetProjectRoot(std::filesystem::absolute(args[i + 1]));
                    projectSet = true;
                    break;
                }
            }
            if (!projectSet)
            {
                const std::filesystem::path defaultProject = DefaultProjectRoot();
                AssetPath::SetProjectRoot(defaultProject.empty()
                    ? AssetPath::GetEngineRoot()
                    : defaultProject);
            }

            PlayerPackageOptions options;
            options.StartupScene = ReadPackageStartupScene(args);
            options.Configuration = ReadPackageConfiguration(args);
            options.IncludeDebugSymbols = false;

            const PlayerPackageResult result = PlayerPackager::PackagePlayer(options);
            if (result.Success)
                WT_CORE_INFO("{}", result.Message);
            else
                WT_CORE_ERROR("{}", result.Message);
            return result.Success ? 0 : 1;
        }

    } // namespace

    class WheatearEditor : public Application
    {
    public:
        WheatearEditor(ApplicationCommandLineArgs args)
            : Application(CreateEditorSpecification(args))
        {
            if (IsRefreshAssetRegistryCommand(args))
            {
                const std::filesystem::path projectRoot = GetSpecification().ProjectRoot.empty()
                    ? AssetPath::GetProjectRoot()
                    : GetSpecification().ProjectRoot;
                UITemplateFactory::WriteBuiltinTemplateAssets(projectRoot);
                AssetRegistry::Get().Scan(projectRoot);
                if (AssetRegistry::Get().WriteRegistry())
                    WT_CORE_INFO("AssetRegistry refreshed at '{}'", (projectRoot / "assets" / ".wheatear" / "asset_registry.yaml").string());
                else
                    WT_CORE_ERROR("AssetRegistry failed to write registry under '{}'", projectRoot.string());
                Close();
                return;
            }

            RegisterCoreEditorComponents();
            RegisterDefaultGameplayEditorModules();
            RegisterDefaultGameplayModules();

            const std::filesystem::path projectRoot = GetSpecification().ProjectRoot.empty()
                ? AssetPath::GetProjectRoot()
                : GetSpecification().ProjectRoot;
            UITemplateFactory::WriteBuiltinTemplateAssets(projectRoot);
            AssetRegistry::Get().LoadCache(projectRoot);

            PushLayer(std::make_unique<ModeSelectLayer>());
        }

        ~WheatearEditor() = default;
    };

    Application* CreateApplication(ApplicationCommandLineArgs args)
    {
        if (IsPackagePlayerCommand(args))
            std::exit(RunPackagePlayer(args));
        if (IsProjectHealthCommand(args))
            std::exit(RunProjectHealth(args));

        // Engine built-ins (shaders / fonts / editor Resources) resolve from
        // the engine repository even when --project points elsewhere.
        AssetPath::SetEngineRoot(AssetPath::DiscoverProjectRoot());

        return new WheatearEditor(args);
    }

} // namespace Wheatear
