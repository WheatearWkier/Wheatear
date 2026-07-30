#include <Wheatear.h>
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

            RegisterCoreEditorComponents();
            RegisterDefaultGameplayEditorModules();
            RegisterDefaultGameplayModules();

            PushLayer(new ModeSelectLayer());
        }

        ~WheatearEditor() = default;
    };

    Application* CreateApplication(ApplicationCommandLineArgs args)
    {
        return new WheatearEditor(args);
    }

} // namespace Wheatear
