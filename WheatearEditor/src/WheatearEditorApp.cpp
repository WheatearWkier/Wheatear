#include <Wheatear.h>
#include "Assets/AssetRegistry.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/ModeSelectLayer.h"
#include "Editor/CoreEditorComponents.h"
#include "Build/PlayerPackager.h"
#include "Modules/ModuleEditorBootstrap.h"

#include <Wheatear/Core/EngineInfo.h>
#include <Wheatear/Core/EntryPoint.h>

#include <filesystem>
#include <string>

namespace Wheatear
{
    namespace {

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
                if (argument == "--package-player" || argument == "--build-player")
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

        static std::filesystem::path ReadPackageStartupScene(ApplicationCommandLineArgs args)
        {
            for (int i = 1; i < args.Count; ++i)
            {
                const std::string argument = args[i];
                if ((argument == "--package-player" || argument == "--build-player") && i + 1 < args.Count)
                    return args[i + 1];
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

    } // namespace

    class WheatearEditor : public Application
    {
    public:
        WheatearEditor(ApplicationCommandLineArgs args)
            : Application(CreateEditorSpecification(args))
        {
            if (IsPackagePlayerCommand(args))
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

                Close();
                return;
            }

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

            PushLayer(new ModeSelectLayer());
        }

        ~WheatearEditor() = default;
    };

    Application* CreateApplication(ApplicationCommandLineArgs args)
    {
        return new WheatearEditor(args);
    }

} // namespace Wheatear
