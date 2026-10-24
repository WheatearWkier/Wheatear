#include <Wheatear.h>
#include <Wheatear/Core/EntryPoint.h>
#include <Wheatear/Runtime/RuntimePatchManager.h>

#include "RTSGameLayer.h"
#include "RuntimeSceneLayer.h"

#include <Wheatear/Assets/ContentValidation.h>
#include <Wheatear/Renderer/Model.h>

int RunWAOSelfTests();
int RunLogicSelfTests();

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace {

	constexpr const char* kAssetPackFilename = "content.wtpack";

	static std::filesystem::path GetExecutableDirectory()
	{
#ifdef WT_PLATFORM_WINDOWS
		wchar_t buffer[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		if (length > 0 && length < MAX_PATH)
			return std::filesystem::path(buffer).parent_path();
#endif
		return std::filesystem::current_path();
	}

	static std::filesystem::path GetRuntimeCacheRoot()
	{
		// Prefer a cache directory next to the executable so packaged games do
		// not consume the system drive; fall back to the user profile when the
		// install directory is not writable (e.g. Program Files).
		const std::filesystem::path exeCache =
			GetExecutableDirectory() / "Saved" / "Cache" / "Runtime";
		{
			std::error_code error;
			if (std::filesystem::create_directories(exeCache, error) || !error)
				return exeCache;
		}

#ifdef WT_PLATFORM_WINDOWS
		if (const char* localAppData = std::getenv("LOCALAPPDATA"))
			return std::filesystem::path(localAppData) / "Wheatear" / "PackageCache";
#endif
		if (const char* home = std::getenv("HOME"))
			return std::filesystem::path(home) / ".cache" / "Wheatear" / "PackageCache";
		return std::filesystem::temp_directory_path() / "Wheatear" / "PackageCache";
	}

	static std::filesystem::path FindRuntimeAssetPack()
	{
		// Packaged output is self-contained: prefer the pack next to the
		// executable, independent of the shell's current directory.
		const std::filesystem::path exeDirectory = GetExecutableDirectory();
		const std::filesystem::path exeCandidates[] = {
			exeDirectory / "Content" / "Paks" / kAssetPackFilename,
			exeDirectory / kAssetPackFilename,
			exeDirectory / "assets" / "game" / kAssetPackFilename
		};
		for (const auto& candidate : exeCandidates)
		{
			if (std::filesystem::is_regular_file(candidate))
				return candidate;
		}

		std::filesystem::path cursor = std::filesystem::current_path();
		while (!cursor.empty())
		{
			const std::filesystem::path candidates[] = {
				cursor / "Content" / "Paks" / kAssetPackFilename,
				cursor / kAssetPackFilename,
				cursor / "assets" / "game" / kAssetPackFilename
			};
			for (const auto& candidate : candidates)
			{
				if (std::filesystem::is_regular_file(candidate))
					return candidate;
			}

			const std::filesystem::path parent = cursor.parent_path();
			if (parent == cursor)
				break;
			cursor = parent;
		}

		return {};
	}

	static std::filesystem::path PreparePackagedAssetRoot()
	{
		const std::filesystem::path packPath = FindRuntimeAssetPack();
		if (packPath.empty())
			return {};

		// Packaged games run with their own directory as the working root so
		// Config/Saved and relative content consistently resolve beside the exe.
		{
			std::error_code cwdError;
			std::filesystem::current_path(GetExecutableDirectory(), cwdError);
		}

		std::string recoveryError;
		const std::filesystem::path executableDirectory = GetExecutableDirectory();
		if (!Wheatear::RuntimePatchManager::RecoverInterruptedActivation(
			executableDirectory,
			&recoveryError) ||
			!Wheatear::RuntimePatchManager::RecoverFailedLaunch(
				executableDirectory,
				&recoveryError))
		{
			WT_CORE_ERROR(
				"Failed to recover runtime patch state: {}",
				recoveryError);
			return {};
		}

		const Wheatear::RuntimePatchSet activePatches =
			Wheatear::RuntimePatchManager::DiscoverActivePatches(executableDirectory);
		const std::string fingerprint =
			Wheatear::RuntimePatchManager::BuildContentFingerprint(packPath, activePatches);
		const std::filesystem::path cacheRoot = GetRuntimeCacheRoot() / fingerprint;
		const std::filesystem::path marker = cacheRoot / ".complete";
		if (std::filesystem::exists(marker) &&
			std::filesystem::exists(cacheRoot / "assets" / "shaders" / "Renderer2D_Quad.glsl"))
		{
			// Trust the cache only when the marker names this exact pack
			// (fingerprint) and carries a sane entry count; a partially
			// deleted or stale cache re-extracts.
			std::ifstream markerInput(marker);
			std::string markerText;
			std::getline(markerInput, markerText);
			const size_t colon = markerText.find(':');
			const std::string markerFingerprint =
				colon == std::string::npos ? markerText : markerText.substr(0, colon);
			const bool saneCount = colon != std::string::npos
				&& markerText.size() > colon + 1
				&& std::all_of(markerText.begin() + static_cast<std::ptrdiff_t>(colon + 1),
					markerText.end(),
					[](unsigned char c) { return c >= '0' && c <= '9'; });
			if (markerFingerprint == fingerprint && saneCount)
			{
				std::string launchError;
				if (!Wheatear::RuntimePatchManager::BeginLaunch(
					executableDirectory,
					fingerprint,
					&launchError))
				{
					WT_CORE_WARN(
						"Could not write runtime launch marker: {}",
						launchError);
				}
				return cacheRoot;
			}
		}

		std::string materializeError;
		if (!Wheatear::RuntimePatchManager::MaterializeRuntimeRoot(
			packPath,
			activePatches,
			cacheRoot,
			&materializeError))
		{
			WT_CORE_ERROR("Failed to materialize runtime content: {}", materializeError);
			std::error_code removeError;
			std::filesystem::remove_all(cacheRoot, removeError);
			return {};
		}

		// UUID mappings are kept outside the pack (the pack only accepts
		// assets/ entries); copy the project registry into the materialized
		// root so AssetDatabase can resolve serialized UUID references.
		const std::filesystem::path packageRegistry =
			GetExecutableDirectory() / "Intermediate" / "AssetRegistry.yaml";
		if (std::filesystem::exists(packageRegistry))
		{
			std::error_code copyError;
			std::filesystem::create_directories(cacheRoot / "Intermediate", copyError);
			std::filesystem::copy_file(packageRegistry,
				cacheRoot / "Intermediate" / "AssetRegistry.yaml",
				std::filesystem::copy_options::overwrite_existing, copyError);
		}

		// The target is committed only after the base pack, every verified
		// overlay, and every deletion marker have completed successfully.
		{
			std::ofstream markerOutput(
				cacheRoot / ".complete",
				std::ios::binary | std::ios::trunc);
			markerOutput << fingerprint << ':' << activePatches.Patches.size();
			if (!markerOutput.good())
			{
				WT_CORE_ERROR("Failed to write runtime content marker '{}'",
					(cacheRoot / ".complete").string());
				return {};
			}
		}

		std::string launchError;
		if (!Wheatear::RuntimePatchManager::BeginLaunch(
			executableDirectory,
			fingerprint,
			&launchError))
		{
			WT_CORE_WARN(
				"Could not write runtime launch marker: {}",
				launchError);
		}

		// Prune stale fingerprint directories from previous packs so the
		// cache cannot grow unbounded across repacks.
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(GetRuntimeCacheRoot(), error))
		{
			if (error)
				break;
			if (entry.is_directory() && entry.path().filename() != cacheRoot.filename())
				std::filesystem::remove_all(entry.path(), error);
		}

		WT_CORE_INFO("Runtime asset pack extracted to '{}'", cacheRoot.string());
		return cacheRoot;
	}

} // namespace

static std::filesystem::path ReadProjectArgument(const Wheatear::ApplicationCommandLineArgs& args)
{
	for (int i = 1; i < args.Count; ++i)
	{
		const std::string argument = args[i];
		const char* projectArgument = i + 1 < args.Count ? args[i + 1] : nullptr;
		if (argument == "--project" && i + 1 < args.Count
			&& projectArgument && projectArgument[0] != '\0'
			&& projectArgument[0] != '-')
		{
			return std::filesystem::path(projectArgument);
		}
	}
	return {};
}

static bool HasArgument(const Wheatear::ApplicationCommandLineArgs& args, const char* flag)
{
	for (int i = 1; i < args.Count; ++i)
	{
		if (std::string(args[i]) == flag)
			return true;
	}
	return false;
}

// Default project when running loose from the engine repository (no wtpack,
// no --project): Projects/WheatearDemo, falling back to discovery.
static std::filesystem::path DefaultProjectRoot()
{
	const std::filesystem::path repositoryRoot = Wheatear::AssetPath::GetEngineRoot().parent_path();
	const std::filesystem::path demoProject = repositoryRoot / "Projects" / "WheatearDemo";
	if (std::filesystem::is_directory(demoProject / "assets"))
		return demoProject;
	return {};
}

static Wheatear::ApplicationSpecification CreateWheatearSandboxSpecification(
	const Wheatear::ApplicationCommandLineArgs& args)
{
	Wheatear::ApplicationSpecification specification;
	specification.Name = "Wheatear Sandbox";
	specification.CommandLineArgs = args;

	if (HasArgument(args, "--fixed-step"))
	{
		specification.UseFixedTimestep = true;
		for (int i = 1; i + 1 < args.Count; ++i)
		{
			if (std::string(args[i]) == "--fixed-step-hz")
			{
				try
				{
					specification.FixedTimestepHz = std::max(
						1.0f, std::stof(args[i + 1]));
				}
				catch (...)
				{
				}
			}
		}
	}

	// --project <dir> runs any project's loose assets straight from the
	// engine repository (developer mode); otherwise the packaged wtpack
	// extraction path applies (packaged games, engine root = extracted root).
	const std::filesystem::path explicitProject = ReadProjectArgument(args);
	if (!explicitProject.empty())
	{
		specification.ProjectRoot = std::filesystem::absolute(explicitProject);
		Wheatear::AssetPath::SetEngineRoot(Wheatear::AssetPath::DiscoverProjectRoot());
		return specification;
	}

	const std::filesystem::path packagedAssetRoot = PreparePackagedAssetRoot();
	if (!packagedAssetRoot.empty())
	{
		specification.ProjectRoot = packagedAssetRoot;
		// Packaged games write saves/settings next to the executable (the
		// extracted cache is regenerated and must stay disposable).
		Wheatear::AssetPath::SetWritableRoot(GetExecutableDirectory());
		return specification;
	}

	const std::filesystem::path defaultProject = DefaultProjectRoot();
	specification.ProjectRoot = !defaultProject.empty()
		? defaultProject
		: Wheatear::AssetPath::DiscoverProjectRoot();
	return specification;
}

class WheatearSandbox : public Wheatear::Application
{
public:
	WheatearSandbox(const Wheatear::ApplicationCommandLineArgs& args)
		: Wheatear::Application(CreateWheatearSandboxSpecification(args))
	{
		for (int i = 1; i + 1 < args.Count; ++i)
		{
			if (std::string(args[i]) == "--cook-model")
			{
				const bool cooked = Wheatear::Model::Cook(args[i + 1]);
				std::exit(cooked ? 0 : 1);
			}
		}

		const bool rtsMode = HasArgument(args, "--rts");
		if (!rtsMode)
			Wheatear::RegisterDefaultGameplayModules();

		if (rtsMode)
			PushLayer(std::make_unique<Wheatear::RTSGameLayer>());
		else
			PushLayer(std::make_unique<RuntimeSceneLayer>());
	}

	~WheatearSandbox()
	{
	}
};

Wheatear::Application* Wheatear::CreateApplication(Wheatear::ApplicationCommandLineArgs args)
{
	if (HasArgument(args, "--test-wao"))
	{
		const int result = RunWAOSelfTests();
		std::exit(result);
	}

	if (HasArgument(args, "--test-logic"))
	{
		const int result = RunLogicSelfTests();
		std::exit(result);
	}

	if (HasArgument(args, "--validate-content"))
	{
		std::filesystem::path target = "assets";
		for (int i = 1; i + 1 < args.Count; ++i)
		{
			if (std::string(args[i]) == "--validate-content")
			{
				const std::string value = args[i + 1];
				if (!value.empty() && value.front() != '-')
					target = value;
				break;
			}
		}

		const ContentValidationReport report =
			ContentValidator::ValidateDirectory(target);
		for (const ContentIssue& issue : report.Issues)
		{
			std::ostream& stream = issue.Severity == ContentIssueSeverity::Error
				? std::cerr
				: std::cout;
			stream << (issue.Severity == ContentIssueSeverity::Error
					? "[ERROR] " : "[WARN] ")
				<< issue.Asset.generic_string();
			if (issue.SourceLine > 0)
				stream << ":" << issue.SourceLine;
			stream << ": " << issue.Message << '\n';
		}
		std::cout << "Validated " << report.FilesChecked << " file(s): "
			<< report.ErrorCount() << " error(s), "
			<< report.WarningCount() << " warning(s).\n";
		std::exit(report.HasErrors() ? 1 : 0);
	}

	return new WheatearSandbox(args);
}
