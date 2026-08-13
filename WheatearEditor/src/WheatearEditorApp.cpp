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
#include <string>

namespace Wheatear
{
    namespace {

        static bool IsCommandOption(const std::string& argument)
        {
            return argument.rfind("--", 0) == 0;
        }

        static ApplicationSpecification CreateEditorSpecification(ApplicationCommandLineArgs args)
        {
            ApplicationSpecification specification;
            specification.Name = EngineInfo::EditorName;
            specification.CommandLineArgs = args;
            specification.ProjectRoot = AssetPath::DiscoverProjectRoot();
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

        static bool ReadPackageEnableScripts(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if (argument == "--scripts" || argument == "--enable-scripts")
                    return true;
                if (argument == "--no-scripts" || argument == "--disable-scripts")
                    return false;
            }

            return false;
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
            options.EnableScripts = ReadPackageEnableScripts(args);
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

        static int RunPackagePlayer(ApplicationCommandLineArgs args)
        {
            PlayerPackageOptions options;
            options.StartupScene = ReadPackageStartupScene(args);
            options.Configuration = "Debug";
            options.EnableScripts = ReadPackageEnableScripts(args);
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

        return new WheatearEditor(args);
    }

} // namespace Wheatear
