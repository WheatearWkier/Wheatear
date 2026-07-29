#include "PlayerPackager.h"

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Core/FileSystem.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Core/PlayerConfig.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <queue>
#include <regex>
#include <set>
#include <system_error>
#include <vector>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "GLFW/deps/stb_image_write.h"

#ifdef WT_PLATFORM_WINDOWS
    #include <shellapi.h>
    #include <windows.h>
#endif

namespace Wheatear {

    namespace {

        static std::filesystem::path FindRepositoryRoot()
        {
            std::filesystem::path cursor = FileSystem::Normalize(std::filesystem::current_path());
            while (!cursor.empty())
            {
                if (std::filesystem::exists(cursor / "Wheatear.sln") &&
                    std::filesystem::exists(cursor / "WheatearSandbox" / "premake5.lua"))
                {
                    return cursor;
                }

                const std::filesystem::path parent = cursor.parent_path();
                if (parent == cursor)
                    break;
                cursor = parent;
            }

            const std::filesystem::path projectRoot = AssetPath::GetProjectRoot();
            if (std::filesystem::exists(projectRoot.parent_path() / "Wheatear.sln"))
                return projectRoot.parent_path();

            return {};
        }

        static std::filesystem::path FindPowerShell()
        {
            if (const char* systemRoot = std::getenv("SystemRoot"))
            {
                const std::filesystem::path candidate =
                    std::filesystem::path(systemRoot) /
                    "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe";
                if (std::filesystem::exists(candidate))
                    return candidate;
            }

            return "powershell.exe";
        }

        static std::string Quote(const std::filesystem::path& path)
        {
            return "\"" + path.string() + "\"";
        }

#ifdef WT_PLATFORM_WINDOWS
        static std::wstring QuoteWide(const std::filesystem::path& path)
        {
            return L"\"" + path.wstring() + L"\"";
        }

        static std::wstring QuoteWide(const std::wstring& value)
        {
            return L"\"" + value + L"\"";
        }

        static int RunProcess(const std::filesystem::path& executable,
            const std::wstring& arguments,
            const std::filesystem::path& workingDirectory)
        {
            std::wstring commandLine = QuoteWide(executable) + L" " + arguments;
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};

            std::wstring workingDirectoryText = workingDirectory.wstring();
            const BOOL created = CreateProcessW(
                executable.wstring().c_str(),
                commandLine.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                workingDirectoryText.c_str(),
                &startup,
                &process);

            if (!created)
                return -static_cast<int>(GetLastError());

            WaitForSingleObject(process.hProcess, INFINITE);

            DWORD exitCode = 1;
            GetExitCodeProcess(process.hProcess, &exitCode);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return static_cast<int>(exitCode);
        }
#endif

        static std::string ToOutputDirectoryName(const std::string& configuration)
        {
            return configuration + "-windows-x86_64";
        }

        static constexpr const char* kAssetPackFilename = "content.wtpack";
        static constexpr const char kAssetPackMagic[8] = { 'W', 'T', 'P', 'A', 'C', 'K', '1', '\0' };
        static constexpr uint32_t kAssetPackVersion = 1;
        static constexpr uint32_t kAssetPackMethodStore = 0;
        static constexpr uint32_t kAssetPackMethodZlib = 1;

        static bool CopyRuntimeBinaries(const std::filesystem::path& source,
            const std::filesystem::path& destination,
            bool includeDebugSymbols,
            std::string* errorMessage)
        {
            if (!std::filesystem::exists(source))
            {
                if (errorMessage)
                    *errorMessage = "Runtime binary directory does not exist: " + source.string();
                return false;
            }

            if (!FileSystem::EnsureDirectory(destination, errorMessage))
                return false;

            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(source, error))
            {
                if (error)
                {
                    if (errorMessage)
                        *errorMessage = source.string() + ": " + error.message();
                    return false;
                }
                if (!entry.is_regular_file())
                    continue;

                const std::filesystem::path extension = entry.path().extension();
                const bool shouldCopy =
                    extension == ".exe" ||
                    extension == ".dll" ||
                    (includeDebugSymbols && extension == ".pdb");

                if (!shouldCopy)
                    continue;

                std::filesystem::copy_file(entry.path(), destination / entry.path().filename(),
                    std::filesystem::copy_options::overwrite_existing, error);
                if (error)
                {
                    if (errorMessage)
                        *errorMessage = entry.path().string() + ": " + error.message();
                    return false;
                }
            }

            return true;
        }

        static std::filesystem::path ResolvePackagedStartupScene(const PlayerPackageOptions& options,
            const std::filesystem::path& packageDirectory)
        {
            const std::filesystem::path resolvedScene = AssetPath::Resolve(options.StartupScene);
            const std::filesystem::path relativeScene = AssetPath::ToProjectRelative(resolvedScene);

            if (!relativeScene.empty() && *relativeScene.begin() == "assets")
                return relativeScene.generic_string();

            const std::filesystem::path startupScene = packageDirectory / "assets" / "scenes" /
                (std::string("PackagedStartup") + AssetFileType::SceneExtension);
            std::filesystem::create_directories(startupScene.parent_path());
            std::filesystem::copy_file(resolvedScene, startupScene,
                std::filesystem::copy_options::overwrite_existing);
            return std::filesystem::path("assets") / "scenes" /
                (std::string("PackagedStartup") + AssetFileType::SceneExtension);
        }

        static bool FirstPartEquals(const std::filesystem::path& path, const char* expected)
        {
            for (const auto& part : path)
            {
                const std::string text = part.generic_string();
                if (text.empty() || text == ".")
                    continue;
                return text == expected;
            }
            return false;
        }

        static std::string NormalizeAssetReference(std::string reference)
        {
            std::replace(reference.begin(), reference.end(), '\\', '/');
            while (!reference.empty() &&
                (reference.back() == '.' || reference.back() == ',' || reference.back() == ';'))
            {
                reference.pop_back();
            }
            return std::filesystem::path(reference).lexically_normal().generic_string();
        }

        static bool IsPackableAsset(const std::filesystem::path& relativePath)
        {
            if (relativePath.empty() || relativePath.is_absolute())
                return false;

            if (!FirstPartEquals(relativePath, "assets"))
                return false;

            auto it = relativePath.begin();
            if (it == relativePath.end())
                return false;
            ++it;

            if (it != relativePath.end())
            {
                const std::string secondPart = it->generic_string();
                if (secondPart == "cache" || secondPart == "saves")
                    return false;
                if (secondPart == "game" && relativePath.filename() == "player.config")
                    return false;
            }

            for (const auto& part : relativePath)
            {
                const std::string text = part.generic_string();
                if (text == "..")
                    return false;
            }

            return true;
        }

        static bool ShouldParseDependencies(const std::filesystem::path& relativePath)
        {
            const std::string extension = relativePath.extension().generic_string();
            return extension == AssetFileType::SceneExtension ||
                extension == AssetFileType::PrefabExtension ||
                extension == AssetFileType::MaterialExtension ||
                extension == ".vn" ||
                extension == ".wts" ||
                extension == ".yaml" ||
                extension == ".yml" ||
                extension == ".json";
        }

        static bool ReadTextFile(const std::filesystem::path& path, std::string* text)
        {
            if (!text)
                return false;

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            *text = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            return true;
        }

        static void ExtractAssetReferences(const std::string& text, std::vector<std::string>* references)
        {
            if (!references)
                return;

            static const std::regex assetRegex(R"(assets[\\/][A-Za-z0-9_\-./\\{}]+)");
            for (std::sregex_iterator it(text.begin(), text.end(), assetRegex), end; it != end; ++it)
            {
                const std::string reference = NormalizeAssetReference(it->str());
                if (!reference.empty())
                    references->push_back(reference);
            }
        }

        static bool TryAddPackageAsset(const std::filesystem::path& projectRoot,
            const std::filesystem::path& relativePath,
            std::set<std::string>* assets,
            std::queue<std::string>* parseQueue)
        {
            if (!assets)
                return false;

            const std::string normalized = NormalizeAssetReference(relativePath.generic_string());
            const std::filesystem::path normalizedPath(normalized);
            if (!IsPackableAsset(normalizedPath))
                return false;

            const std::filesystem::path sourcePath = projectRoot / normalizedPath;
            if (!std::filesystem::is_regular_file(sourcePath))
                return false;

            const bool inserted = assets->insert(normalized).second;
            if (inserted && parseQueue && ShouldParseDependencies(normalizedPath))
                parseQueue->push(normalized);
            return inserted;
        }

        static bool EndsWith(const std::string& value, const std::string& suffix)
        {
            return value.size() >= suffix.size() &&
                value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        static void ExpandTemplateReference(const std::filesystem::path& projectRoot,
            const std::string& reference,
            std::set<std::string>* assets,
            std::queue<std::string>* parseQueue)
        {
            const std::string normalized = NormalizeAssetReference(reference);
            const size_t placeholder = normalized.find('{');
            if (placeholder == std::string::npos)
            {
                TryAddPackageAsset(projectRoot, normalized, assets, parseQueue);
                return;
            }

            const std::filesystem::path templatePath(normalized);
            const std::filesystem::path relativeDirectory = templatePath.parent_path();
            const std::filesystem::path sourceDirectory = projectRoot / relativeDirectory;
            if (!std::filesystem::is_directory(sourceDirectory))
                return;

            const std::string filenameTemplate = templatePath.filename().generic_string();
            const size_t localPlaceholder = filenameTemplate.find('{');
            if (localPlaceholder == std::string::npos)
                return;

            const size_t localPlaceholderEnd = filenameTemplate.find('}', localPlaceholder);
            if (localPlaceholderEnd == std::string::npos)
                return;

            const std::string prefix = filenameTemplate.substr(0, localPlaceholder);
            const std::string suffix = filenameTemplate.substr(localPlaceholderEnd + 1);

            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(sourceDirectory, error))
            {
                if (error || !entry.is_regular_file())
                    continue;

                const std::string filename = entry.path().filename().generic_string();
                if (filename.rfind(prefix, 0) != 0 || !EndsWith(filename, suffix))
                    continue;

                TryAddPackageAsset(projectRoot, relativeDirectory / filename, assets, parseQueue);
            }
        }

        static void AddDirectoryFiles(const std::filesystem::path& projectRoot,
            const std::filesystem::path& relativeDirectory,
            std::set<std::string>* assets)
        {
            const std::filesystem::path sourceDirectory = projectRoot / relativeDirectory;
            if (!std::filesystem::is_directory(sourceDirectory))
                return;

            std::error_code error;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDirectory, error))
            {
                if (error || !entry.is_regular_file())
                    continue;

                const std::filesystem::path relativePath =
                    std::filesystem::relative(entry.path(), projectRoot, error);
                if (!error)
                TryAddPackageAsset(projectRoot, relativePath, assets, nullptr);
            }
        }

        static bool IsLooseTuningAsset(const std::filesystem::path& relativePath)
        {
            const std::string extension = relativePath.extension().generic_string();
            if (extension != ".yaml" && extension != ".yml")
                return false;

            const std::string filename = relativePath.filename().generic_string();
            return filename.find("tuning") != std::string::npos;
        }

        static bool CopyLooseTuningAssets(const std::filesystem::path& projectRoot,
            const std::vector<std::filesystem::path>& packageAssets,
            const std::filesystem::path& outputDirectory,
            std::string* errorMessage)
        {
            std::error_code error;
            for (const std::filesystem::path& relativePath : packageAssets)
            {
                if (!IsLooseTuningAsset(relativePath))
                    continue;

                const std::filesystem::path sourcePath = projectRoot / relativePath;
                const std::filesystem::path targetPath = outputDirectory / relativePath;
                std::filesystem::create_directories(targetPath.parent_path(), error);
                if (error)
                {
                    if (errorMessage)
                        *errorMessage = targetPath.parent_path().string() + ": " + error.message();
                    return false;
                }

                std::filesystem::copy_file(sourcePath, targetPath,
                    std::filesystem::copy_options::overwrite_existing, error);
                if (error)
                {
                    if (errorMessage)
                        *errorMessage = sourcePath.string() + ": " + error.message();
                    return false;
                }
            }

            return true;
        }

        static std::vector<std::filesystem::path> CollectPackageAssets(
            const std::filesystem::path& projectRoot,
            const std::filesystem::path& startupScene,
            bool enableScripts)
        {
            std::set<std::string> assets;
            std::queue<std::string> parseQueue;

            TryAddPackageAsset(projectRoot, startupScene, &assets, &parseQueue);

            AddDirectoryFiles(projectRoot, "assets/shaders", &assets);
            TryAddPackageAsset(projectRoot, "assets/fonts/wqy-microhei.ttc", &assets, nullptr);
            TryAddPackageAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/LICENSE_Apache2.txt", &assets, nullptr);
            TryAddPackageAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/LICENSE_GPLv3.txt", &assets, nullptr);
            TryAddPackageAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/README.txt", &assets, nullptr);
            TryAddPackageAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/AUTHORS.txt", &assets, nullptr);
            TryAddPackageAsset(projectRoot, "assets/fonts/NotoSansSC-VF.ttf", &assets, nullptr);
            TryAddPackageAsset(projectRoot, "assets/fonts/Open-Sans-2.ttf", &assets, nullptr);

            if (enableScripts)
            {
                TryAddPackageAsset(projectRoot, EngineInfo::ScriptCoreAssemblyPath, &assets, nullptr);
                TryAddPackageAsset(projectRoot, "assets/scripts/Wheatear-ScriptCore.deps.json", &assets, nullptr);
            }

            while (!parseQueue.empty())
            {
                const std::string current = parseQueue.front();
                parseQueue.pop();

                std::string text;
                if (!ReadTextFile(projectRoot / current, &text))
                    continue;

                std::vector<std::string> references;
                ExtractAssetReferences(text, &references);
                for (const std::string& reference : references)
                    ExpandTemplateReference(projectRoot, reference, &assets, &parseQueue);
            }

            std::vector<std::filesystem::path> result;
            result.reserve(assets.size());
            for (const std::string& asset : assets)
                result.emplace_back(asset);
            return result;
        }

        static void WriteU16(std::ostream& output, uint16_t value)
        {
            output.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void WriteU32(std::ostream& output, uint32_t value)
        {
            output.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static void WriteU64(std::ostream& output, uint64_t value)
        {
            output.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        static bool ReadBinaryFile(const std::filesystem::path& path, std::vector<unsigned char>* bytes)
        {
            if (!bytes)
                return false;

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            *bytes = std::vector<unsigned char>(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
            return true;
        }

        static bool WriteAssetPack(const std::filesystem::path& projectRoot,
            const std::vector<std::filesystem::path>& assets,
            const std::filesystem::path& packPath,
            std::string* errorMessage)
        {
            std::ofstream output(packPath, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                if (errorMessage)
                    *errorMessage = "Could not create asset pack: " + packPath.string();
                return false;
            }

            output.write(kAssetPackMagic, sizeof(kAssetPackMagic));
            WriteU32(output, kAssetPackVersion);
            WriteU32(output, static_cast<uint32_t>(assets.size()));

            for (const auto& relativePath : assets)
            {
                const std::string entryPath = NormalizeAssetReference(relativePath.generic_string());
                if (entryPath.size() > std::numeric_limits<uint16_t>::max())
                {
                    if (errorMessage)
                        *errorMessage = "Asset path is too long for pack: " + entryPath;
                    return false;
                }

                std::vector<unsigned char> sourceBytes;
                if (!ReadBinaryFile(projectRoot / entryPath, &sourceBytes))
                {
                    if (errorMessage)
                        *errorMessage = "Could not read asset for pack: " + entryPath;
                    return false;
                }

                uint32_t method = kAssetPackMethodStore;
                std::vector<unsigned char> payload = sourceBytes;
                if (!sourceBytes.empty() && sourceBytes.size() <= static_cast<size_t>(std::numeric_limits<int>::max()))
                {
                    int compressedSize = 0;
                    unsigned char* compressed = stbi_zlib_compress(
                        sourceBytes.data(),
                        static_cast<int>(sourceBytes.size()),
                        &compressedSize,
                        8);

                    if (compressed && compressedSize > 0 &&
                        static_cast<size_t>(compressedSize) < sourceBytes.size())
                    {
                        method = kAssetPackMethodZlib;
                        payload.assign(compressed, compressed + compressedSize);
                    }

                    if (compressed)
                        std::free(compressed);
                }

                WriteU16(output, static_cast<uint16_t>(entryPath.size()));
                WriteU32(output, method);
                WriteU64(output, static_cast<uint64_t>(sourceBytes.size()));
                WriteU64(output, static_cast<uint64_t>(payload.size()));
                output.write(entryPath.data(), static_cast<std::streamsize>(entryPath.size()));
                if (!payload.empty())
                {
                    output.write(reinterpret_cast<const char*>(payload.data()),
                        static_cast<std::streamsize>(payload.size()));
                }

                if (!output.good())
                {
                    if (errorMessage)
                        *errorMessage = "Failed while writing asset pack entry: " + entryPath;
                    return false;
                }
            }

            return true;
        }

        static PlayerPackageResult Fail(const std::string& message,
            const std::filesystem::path& packageDirectory = {})
        {
            WT_CORE_ERROR("PlayerPackager: {}", message);
            return { false, message, packageDirectory, {} };
        }

    } // namespace

    PlayerPackageResult PlayerPackager::PackagePlayer(const PlayerPackageOptions& options)
    {
#ifndef WT_PLATFORM_WINDOWS
        return Fail("Player packaging currently targets Windows builds.");
#else
        const std::filesystem::path repositoryRoot = FindRepositoryRoot();
        if (repositoryRoot.empty())
            return Fail("Could not locate Wheatear.sln; unable to find repository root.");

        if (options.StartupScene.empty() || !std::filesystem::exists(AssetPath::Resolve(options.StartupScene)))
            return Fail("Startup scene is not saved or does not exist.");

        const std::filesystem::path outputDirectory = options.OutputDirectory.empty()
            ? repositoryRoot / "Builds" / "Windows" / "Player"
            : options.OutputDirectory;
        const std::filesystem::path buildsRoot = repositoryRoot / "Builds";

        if (FileSystem::Normalize(outputDirectory) == FileSystem::Normalize(buildsRoot) ||
            !FileSystem::IsSubPath(outputDirectory, buildsRoot))
        {
            return Fail("Package output directory must be inside Builds and may not be Builds itself.");
        }

        const std::filesystem::path playerProject = repositoryRoot / "WheatearSandbox" / "WheatearSandbox.vcxproj";
        if (!std::filesystem::exists(playerProject))
            return Fail("WheatearSandbox project file was not generated. Run premake first.", outputDirectory);

        const std::filesystem::path buildScript = repositoryRoot / "scripts" / "Build-Windows.ps1";
        if (!std::filesystem::exists(buildScript))
            return Fail("Build-Windows.ps1 was not found; package build cannot start.", outputDirectory);

        const std::filesystem::path powerShell = FindPowerShell();
        const std::filesystem::path projectPath = std::filesystem::path("WheatearSandbox") / "WheatearSandbox.vcxproj";
        const std::string command =
            Quote(powerShell) +
            " -NoProfile -ExecutionPolicy Bypass -File " + Quote(buildScript) +
            " -ProjectPath " + Quote(projectPath) +
            " -Configuration " + options.Configuration +
            " -Platform x64 -Verbosity m -LinkIncremental false -LogFile build-msbuild.log";

        WT_CORE_INFO("PlayerPackager: building WheatearSandbox with '{}'", command);
#ifdef WT_PLATFORM_WINDOWS
        const std::wstring arguments =
            L"-NoProfile -ExecutionPolicy Bypass -File " + QuoteWide(buildScript) +
            L" -ProjectPath " + QuoteWide(projectPath) +
            L" -Configuration " + std::wstring(options.Configuration.begin(), options.Configuration.end()) +
            L" -Platform x64 -Verbosity m -LinkIncremental false -LogFile build-msbuild.log";
        const int buildResult = RunProcess(powerShell, arguments, repositoryRoot);
#else
        const int buildResult = std::system(command.c_str());
#endif
        if (buildResult != 0)
            return Fail("WheatearSandbox project build failed. Check the build output for details.", outputDirectory);

        const std::filesystem::path runtimeBinaryDirectory =
            repositoryRoot / "bin" / ToOutputDirectoryName(options.Configuration) / "WheatearSandbox";
        const std::filesystem::path playerExe = runtimeBinaryDirectory / "WheatearSandbox.exe";
        if (!std::filesystem::exists(playerExe))
            return Fail("WheatearSandbox.exe was not generated; package aborted.", outputDirectory);

        std::error_code error;
        std::filesystem::remove_all(outputDirectory, error);
        if (error)
            return Fail("Failed to clean previous package directory: " + error.message(), outputDirectory);

        std::string errorMessage;
        if (!FileSystem::EnsureDirectory(outputDirectory, &errorMessage))
            return Fail("Failed to create package directory: " + errorMessage, outputDirectory);

        if (!CopyRuntimeBinaries(runtimeBinaryDirectory, outputDirectory,
            options.IncludeDebugSymbols, &errorMessage))
        {
            return Fail("Failed to copy runtime binaries: " + errorMessage, outputDirectory);
        }

        const std::filesystem::path monoSource = AssetPath::GetProjectRoot() / "mono";
        if (options.EnableScripts && !std::filesystem::exists(monoSource))
            return Fail("Scripts are enabled, but the mono runtime directory was not found.", outputDirectory);

        if (options.EnableScripts &&
            std::filesystem::exists(monoSource) &&
            !FileSystem::CopyDirectoryRecursive(monoSource, outputDirectory / "mono", {}, &errorMessage))
        {
            return Fail("Failed to copy mono runtime: " + errorMessage, outputDirectory);
        }

        const std::filesystem::path startupScene = ResolvePackagedStartupScene(options, outputDirectory);
        const std::vector<std::filesystem::path> packageAssets = CollectPackageAssets(
            AssetPath::GetProjectRoot(),
            startupScene,
            options.EnableScripts);
        if (packageAssets.empty())
            return Fail("No assets were collected for the package.", outputDirectory);

        const std::filesystem::path assetPackPath = outputDirectory / kAssetPackFilename;
        if (!WriteAssetPack(AssetPath::GetProjectRoot(), packageAssets, assetPackPath, &errorMessage))
            return Fail("Failed to write asset pack: " + errorMessage, outputDirectory);

        if (!CopyLooseTuningAssets(AssetPath::GetProjectRoot(), packageAssets, outputDirectory, &errorMessage))
            return Fail("Failed to copy loose tuning assets: " + errorMessage, outputDirectory);

        RuntimePlayerConfig playerConfig;
        playerConfig.StartupScene = startupScene;
        playerConfig.EnableScripts = options.EnableScripts;
        if (!SaveRuntimePlayerConfig(outputDirectory / "assets" / "game" / "player.config",
            playerConfig,
            EngineInfo::EditorName))
        {
            return Fail("Failed to write player.config.", outputDirectory);
        }

        const std::filesystem::path executablePath = outputDirectory / "WheatearSandbox.exe";
        WT_CORE_INFO("PlayerPackager: package completed '{}' with {} packed assets",
            outputDirectory.string(),
            packageAssets.size());
        return {
            true,
            "Package completed: " + outputDirectory.string() +
                " (" + std::to_string(packageAssets.size()) + " assets packed into " +
                kAssetPackFilename + ")",
            outputDirectory,
            executablePath
        };
#endif
    }

    void PlayerPackager::OpenDirectory(const std::filesystem::path& directory)
    {
        if (directory.empty() || !std::filesystem::exists(directory))
            return;

#ifdef WT_PLATFORM_WINDOWS
        ShellExecuteA(nullptr, "open", directory.string().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#elif defined(WT_PLATFORM_MACOS)
        (void)std::system(("open " + Quote(directory)).c_str());
#else
        (void)std::system(("xdg-open " + Quote(directory)).c_str());
#endif
    }

} // namespace Wheatear
