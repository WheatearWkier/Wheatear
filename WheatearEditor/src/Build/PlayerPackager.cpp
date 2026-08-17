#include "wepch.h"
#include "Wheatear/Utils/StringUtils.h"
#include "PlayerPackager.h"

#include "AssetDependencyScanner.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Assets/FileSystem.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Config/PlayerConfig.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "GLFW/deps/stb_image_write.h"

#ifdef WT_PLATFORM_WINDOWS
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

        static std::filesystem::path GetCurrentExecutablePath()
        {
#ifdef WT_PLATFORM_WINDOWS
            std::wstring buffer;
            buffer.resize(32768);
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0 || length >= buffer.size())
                return {};

            buffer.resize(length);
            return std::filesystem::path(buffer);
#else
            return {};
#endif
        }

        static bool IsSameNormalizedPath(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
        {
            if (lhs.empty() || rhs.empty())
                return false;
            return FileSystem::Normalize(lhs) == FileSystem::Normalize(rhs);
        }

        static std::string Quote(const std::filesystem::path& path)
        {
            return "\"" + path.string() + "\"";
        }


        static double ElapsedMilliseconds(std::chrono::steady_clock::time_point startedAt)
        {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startedAt).count();
        }

        static void LogTimedStep(const char* label, std::chrono::steady_clock::time_point startedAt)
        {
            WT_CORE_INFO("PlayerPackager: {} completed in {:.2f} ms", label, ElapsedMilliseconds(startedAt));
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
            const std::filesystem::path& workingDirectory,
            DWORD timeoutMs = 10 * 60 * 1000)
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

            const DWORD waitResult = WaitForSingleObject(process.hProcess, timeoutMs);
            if (waitResult == WAIT_TIMEOUT)
            {
                // A hung build must not hang the packager forever: kill the
                // process tree and report failure.
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, 5000);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                return -static_cast<int>(ERROR_TIMEOUT);
            }

            DWORD exitCode = 1;
            GetExitCodeProcess(process.hProcess, &exitCode);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return static_cast<int>(exitCode);
        }
#endif

        // Package output is a distributable artifact. Runtime extraction
        // caches and player saves must not leak from a previous local run into
        // the next package, otherwise stale data can mask packaging fixes.
        static bool CleanPackageDirectory(const std::filesystem::path& directory, std::string* errorMessage)
        {
            std::error_code error;
            if (!std::filesystem::exists(directory, error))
            {
                if (error)
                {
                    if (errorMessage)
                        *errorMessage = directory.string() + ": " + error.message();
                    return false;
                }
                return true;
            }

            if (!std::filesystem::is_directory(directory, error))
            {
                if (errorMessage)
                    *errorMessage = "Package path is not a directory: " + directory.string();
                return false;
            }

            std::filesystem::remove_all(directory, error);
            if (error)
            {
                if (errorMessage)
                    *errorMessage = "Failed to clean package directory: " + error.message();
                return false;
            }
            return true;
        }

        static std::string ToOutputDirectoryName(const std::string& configuration)
        {
            return configuration + "-windows-x86_64";
        }

        static std::string SanitizePathComponent(std::string value)
        {
            for (char& ch : value)
            {
                switch (ch)
                {
                case '<':
                case '>':
                case ':':
                case '"':
                case '/':
                case '\\':
                case '|':
                case '?':
                case '*':
                    ch = '_';
                    break;
                default:
                    break;
                }
            }

            while (!value.empty() && (value.back() == ' ' || value.back() == '.'))
                value.pop_back();

            return value.empty() ? std::string("Project") : value;
        }

        static uint64_t Hash64(const std::string& text)
        {
            uint64_t hash = 1469598103934665603ull;
            for (unsigned char c : text)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        static std::string ToHex8(uint64_t value)
        {
            static constexpr char digits[] = "0123456789abcdef";
            char buffer[9] = {};
            for (int i = 7; i >= 0; --i)
            {
                buffer[i] = digits[value & 0x0F];
                value >>= 4;
            }
            return std::string(buffer, 8);
        }

        // Package key is the project's path relative to the repository with the
        // repository's project container directory (Projects/) dropped, so
        // Projects/WheatearDemo maps to the short key WheatearDemo while nested
        // projects (Projects/Sub/GameA) stay unique.
        static std::filesystem::path BuildProjectPackageKey(const std::filesystem::path& repositoryRoot,
            const std::filesystem::path& projectRoot)
        {
            const std::filesystem::path normalizedRepositoryRoot = FileSystem::Normalize(repositoryRoot);
            const std::filesystem::path normalizedProjectRoot = FileSystem::Normalize(projectRoot);

            std::error_code error;
            if (FileSystem::IsSubPath(normalizedProjectRoot, normalizedRepositoryRoot))
            {
                const std::filesystem::path relative =
                    std::filesystem::relative(normalizedProjectRoot, normalizedRepositoryRoot, error);
                if (!error && !relative.empty() && relative != ".")
                {
                    std::filesystem::path key;
                    for (const auto& part : relative)
                    {
                        const std::string segment = SanitizePathComponent(part.generic_string());
                        if (segment == "." || segment == "..")
                            continue;
                        if (StringUtils::ToLower(segment) == "projects")
                            continue;
                        key /= segment;
                    }

                    if (!key.empty())
                        return key;
                }
            }

            std::string basename = SanitizePathComponent(normalizedProjectRoot.filename().generic_string());
            const std::string fingerprint = ToHex8(Hash64(normalizedProjectRoot.generic_string()));
            return std::filesystem::path(basename + "-" + fingerprint);
        }

        // The player package directory is the release name: the project's
        // player.config PackageName (e.g. "Demo") when set, otherwise the
        // short project key. Layout: Builds/Windows/Player/<release name>.
        static std::filesystem::path DefaultPlayerPackageDirectory(const std::filesystem::path& repositoryRoot,
            const std::filesystem::path& projectRoot)
        {
            const RuntimePlayerConfig projectConfig =
                LoadRuntimePlayerConfig(projectRoot / "assets" / "game" / "player.config");
            const std::string packageName = SanitizePathComponent(
                projectConfig.PackageName.empty()
                    ? BuildProjectPackageKey(repositoryRoot, projectRoot).generic_string()
                    : projectConfig.PackageName);
            return repositoryRoot
                / "Builds"
                / "Windows"
                / "Player"
                / packageName;
        }

        // The editor package is the tool itself: no per-project nesting.
        // Layout: Builds/Windows/Editor.
        static std::filesystem::path DefaultEditorPackageDirectory(const std::filesystem::path& repositoryRoot)
        {
            return repositoryRoot
                / "Builds"
                / "Windows"
                / "Editor";
        }

        static constexpr const char* kAssetPackFilename = "content.wtpack";
        static constexpr const char kAssetPackMagic[8] = { 'W', 'T', 'P', 'A', 'C', 'K', '1', '\0' };
        static constexpr uint32_t kAssetPackVersion = 1;
        static constexpr uint32_t kAssetPackMethodStore = 0;
        static constexpr uint32_t kAssetPackMethodZlib = 1;

        static bool BuildVisualStudioProject(const std::filesystem::path& repositoryRoot,
            const std::filesystem::path& powerShell,
            const std::filesystem::path& buildScript,
            const std::filesystem::path& projectPath,
            const std::string& configuration,
            const std::string& logFile,
            const std::string& label,
            std::string* errorMessage)
        {
            const std::string command =
                Quote(powerShell) +
                " -NoProfile -ExecutionPolicy Bypass -File " + Quote(buildScript) +
                " -ProjectPath " + Quote(projectPath) +
                " -Configuration " + configuration +
                " -Platform x64 -Verbosity m -LinkIncremental false -LogFile " + logFile;
            WT_CORE_INFO("PlayerPackager: building {} with '{}'", label, command);

#ifdef WT_PLATFORM_WINDOWS
            const std::wstring arguments =
                L"-NoProfile -ExecutionPolicy Bypass -File " + QuoteWide(buildScript) +
                L" -ProjectPath " + QuoteWide(projectPath) +
                L" -Configuration " + std::wstring(configuration.begin(), configuration.end()) +
                L" -Platform x64 -Verbosity m -LinkIncremental false -LogFile " +
                std::wstring(logFile.begin(), logFile.end());
            const int buildResult = RunProcess(powerShell, arguments, repositoryRoot);
#else
            const int buildResult = std::system(command.c_str());
#endif
            if (buildResult != 0)
            {
                if (errorMessage)
                    *errorMessage = label + " project build failed. Check the build output for details.";
                return false;
            }
            return true;
        }

        static bool CopyRuntimeBinaries(const std::filesystem::path& source,
            const std::filesystem::path& destination,
            bool includeDebugSymbols,
            const std::string* skipFileName,
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

                // A running editor executable inside the editor package cannot
                // be overwritten (the image file is locked); leave it in place.
                if (skipFileName &&
                    entry.path().filename().generic_string() == *skipFileName)
                {
                    WT_CORE_INFO("PlayerPackager: keeping running binary '{}' in place",
                        (destination / entry.path().filename()).string());
                    continue;
                }

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
            const std::filesystem::path& packageDirectory,
            std::filesystem::path* sourceStartupScene)
        {
            const std::filesystem::path resolvedScene = AssetPath::Resolve(options.StartupScene);
            if (sourceStartupScene)
                *sourceStartupScene = resolvedScene;
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

        static bool IsLooseRuntimeDataAsset(const std::filesystem::path& relativePath)
        {
            const std::string extension = StringUtils::ToLower(relativePath.extension().generic_string());
            if (extension == ".yaml" || extension == ".yml" || extension == ".json" ||
                extension == ".wt" || extension == ".wts" || extension == ".vn" ||
                extension == ".glsl" || extension == ".vert" || extension == ".frag" ||
                extension == ".geom" || extension == ".comp" || extension == ".hlsl")
            {
                return true;
            }

            if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
                return true;

            if (extension == ".config" && relativePath.generic_string().find("assets/game/") == 0)
                return true;

            return false;
        }

        static bool ShouldTryAssetPackCompression(const std::filesystem::path& relativePath)
        {
            const std::string extension = StringUtils::ToLower(relativePath.extension().generic_string());
            return extension != ".png"
                && extension != ".jpg"
                && extension != ".jpeg"
                && extension != ".webp"
                && extension != ".mp3"
                && extension != ".ogg"
                && extension != ".wav"
                && extension != ".ttf"
                && extension != ".ttc"
                && extension != ".otf"
                && extension != ".woff"
                && extension != ".woff2";
        }

        // Engine built-ins (shaders/fonts/gameplay) live under the engine root
        // while project assets live under the project root; resolve source
        // files project-first, engine-root fallback.
        static std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectRoot,
            const std::filesystem::path& builtinRoot,
            const std::filesystem::path& relativePath,
            const std::unordered_map<std::string, std::filesystem::path>* assetSources)
        {
            const std::string entryPath = AssetDependencyScanner::NormalizeAssetReference(relativePath.generic_string());
            if (assetSources)
            {
                auto it = assetSources->find(entryPath);
                if (it != assetSources->end())
                    return it->second;
            }

            const std::filesystem::path projectSource = projectRoot / entryPath;
            if (std::filesystem::exists(projectSource))
                return projectSource;

            const std::filesystem::path builtinSource = builtinRoot / entryPath;
            if (std::filesystem::exists(builtinSource))
                return builtinSource;

            return projectSource;
        }

        static bool CopyLooseRuntimeDataAssets(const std::filesystem::path& projectRoot,
            const std::filesystem::path& builtinRoot,
            const std::vector<std::filesystem::path>& packageAssets,
            const std::unordered_map<std::string, std::filesystem::path>* assetSources,
            const std::filesystem::path& outputDirectory,
            std::string* errorMessage)
        {
            std::error_code error;
            for (const std::filesystem::path& relativePath : packageAssets)
            {
                if (!IsLooseRuntimeDataAsset(relativePath))
                    continue;

                const std::filesystem::path sourcePath =
                    ResolveSourcePath(projectRoot, builtinRoot, relativePath, assetSources);
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

        // The asset pack format is little-endian by design (Windows x86_64
        // target). Write explicitly so the format stays stable regardless of
        // host byte order.
        static void WriteU16(std::ostream& output, uint16_t value)
        {
            const char bytes[2] = {
                static_cast<char>(value & 0xFF),
                static_cast<char>((value >> 8) & 0xFF)
            };
            output.write(bytes, sizeof(bytes));
        }

        static void WriteU32(std::ostream& output, uint32_t value)
        {
            const char bytes[4] = {
                static_cast<char>(value & 0xFF),
                static_cast<char>((value >> 8) & 0xFF),
                static_cast<char>((value >> 16) & 0xFF),
                static_cast<char>((value >> 24) & 0xFF)
            };
            output.write(bytes, sizeof(bytes));
        }

        static void WriteU64(std::ostream& output, uint64_t value)
        {
            const char bytes[8] = {
                static_cast<char>(value & 0xFF),
                static_cast<char>((value >> 8) & 0xFF),
                static_cast<char>((value >> 16) & 0xFF),
                static_cast<char>((value >> 24) & 0xFF),
                static_cast<char>((value >> 32) & 0xFF),
                static_cast<char>((value >> 40) & 0xFF),
                static_cast<char>((value >> 48) & 0xFF),
                static_cast<char>((value >> 56) & 0xFF)
            };
            output.write(bytes, sizeof(bytes));
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
            const std::filesystem::path& builtinRoot,
            const std::vector<std::filesystem::path>& assets,
            const std::unordered_map<std::string, std::filesystem::path>* assetSources,
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
                const std::string entryPath = AssetDependencyScanner::NormalizeAssetReference(relativePath.generic_string());
                if (entryPath.size() > std::numeric_limits<uint16_t>::max())
                {
                    if (errorMessage)
                        *errorMessage = "Asset path is too long for pack: " + entryPath;
                    return false;
                }

                std::vector<unsigned char> sourceBytes;
                if (!ReadBinaryFile(ResolveSourcePath(projectRoot, builtinRoot, entryPath, assetSources), &sourceBytes))
                {
                    if (errorMessage)
                        *errorMessage = "Could not read asset for pack: " + entryPath;
                    return false;
                }

                uint32_t method = kAssetPackMethodStore;
                std::vector<unsigned char> payload = sourceBytes;
                if (ShouldTryAssetPackCompression(std::filesystem::path(entryPath))
                    && !sourceBytes.empty()
                    && sourceBytes.size() <= static_cast<size_t>(std::numeric_limits<int>::max()))
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

        static bool WritePackageReport(const PlayerPackageOptions& options,
            const AssetDependencyReport& report,
            const std::filesystem::path& reportPath,
            const std::filesystem::path& assetPackPath,
            const std::filesystem::path& editorOutputDirectory,
            std::string* errorMessage)
        {
            std::ofstream output(reportPath, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                if (errorMessage)
                    *errorMessage = "Could not create package report: " + reportPath.string();
                return false;
            }

            std::error_code error;
            const uintmax_t packBytes = std::filesystem::file_size(assetPackPath, error);

            output << "Wheatear Player Package Report\n";
            output << "================================\n\n";
            output << "Startup Scene: " << options.StartupScene.generic_string() << "\n";
            output << "Configuration: " << options.Configuration << "\n";
            output << "Runtime Executable: WheatearSandbox.exe\n";
            output << "Editor Package: " << editorOutputDirectory.generic_string() << "\n";
            output << "Loose Runtime Data: text configs, scenes, scripts, shaders and audio\n";
            output << "Packed Assets: " << report.IncludedAssets.size() << "\n";
            output << "Source Asset Bytes: " << report.IncludedBytes << "\n";
            output << "Asset Pack Bytes: " << (error ? 0 : packBytes) << "\n";
            output << "Parsed Text Assets: " << report.ParsedTextAssets.size() << "\n";
            output << "Scene Transitions: " << report.SceneTransitions.size() << "\n";
            output << "Missing Scene Transitions: " << report.MissingSceneTransitions.size() << "\n";
            output << "Missing References: " << report.MissingReferences.size() << "\n\n";

            if (!report.Warnings.empty())
            {
                output << "Warnings\n";
                output << "--------\n";
                for (const std::string& warning : report.Warnings)
                    output << "- " << warning << "\n";
                output << "\n";
            }

            if (!report.MissingReferences.empty())
            {
                output << "Missing References\n";
                output << "------------------\n";
                for (const auto& missing : report.MissingReferences)
                    output << "- " << missing.Reference << "  (from " << missing.SourceAsset << ")\n";
                output << "\n";
            }

            if (!report.MissingSceneTransitions.empty())
            {
                output << "Missing Scene Transitions\n";
                output << "-------------------------\n";
                for (const auto& missing : report.MissingSceneTransitions)
                    output << "- " << missing.Reference << "  (from " << missing.SourceAsset << ")\n";
                output << "\n";
            }

            if (!report.SceneTransitions.empty())
            {
                output << "Scene Transition List\n";
                output << "---------------------\n";
                for (const auto& transition : report.SceneTransitions)
                    output << "- " << transition.Reference << "  (from " << transition.SourceAsset << ")\n";
                output << "\n";
            }

            output << "Packed Asset List\n";
            output << "-----------------\n";
            for (const auto& asset : report.IncludedAssets)
                output << "- " << asset.generic_string() << "\n";

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
        const auto packageStarted = std::chrono::steady_clock::now();
        const std::filesystem::path repositoryRoot = FindRepositoryRoot();
        if (repositoryRoot.empty())
            return Fail("Could not locate Wheatear.sln; unable to find repository root.");

        if (options.StartupScene.empty() || !std::filesystem::exists(AssetPath::Resolve(options.StartupScene)))
            return Fail("Startup scene is not saved or does not exist.");

        const std::filesystem::path projectRoot = AssetPath::GetProjectRoot();
        const std::filesystem::path outputDirectory = options.OutputDirectory.empty()
            ? DefaultPlayerPackageDirectory(repositoryRoot, projectRoot)
            : options.OutputDirectory;
        const std::filesystem::path editorOutputDirectory = options.EditorOutputDirectory.empty()
            ? DefaultEditorPackageDirectory(repositoryRoot)
            : options.EditorOutputDirectory;
        const std::filesystem::path buildsRoot = repositoryRoot / "Builds";

        if (FileSystem::Normalize(outputDirectory) == FileSystem::Normalize(buildsRoot) ||
            !FileSystem::IsSubPath(outputDirectory, buildsRoot))
        {
            return Fail("Package output directory must be inside Builds and may not be Builds itself.");
        }
        if (FileSystem::Normalize(editorOutputDirectory) == FileSystem::Normalize(buildsRoot) ||
            !FileSystem::IsSubPath(editorOutputDirectory, buildsRoot))
        {
            return Fail("Editor package output directory must be inside Builds and may not be Builds itself.");
        }
        if (FileSystem::Normalize(outputDirectory) == FileSystem::Normalize(editorOutputDirectory))
            return Fail("Player and editor package output directories must be separate.", outputDirectory);

        const std::filesystem::path playerProject = repositoryRoot / "WheatearSandbox" / "WheatearSandbox.vcxproj";
        if (!std::filesystem::exists(playerProject))
            return Fail("WheatearSandbox project file was not generated. Run premake first.", outputDirectory);

        const std::filesystem::path editorProject = repositoryRoot / "WheatearEditor" / "WheatearEditor.vcxproj";
        if (!std::filesystem::exists(editorProject))
            return Fail("WheatearEditor project file was not generated. Run premake first.", outputDirectory);

        const std::filesystem::path buildScript = repositoryRoot / "scripts" / "Build-Windows.ps1";
        if (!std::filesystem::exists(buildScript))
            return Fail("Build-Windows.ps1 was not found; package build cannot start.", outputDirectory);

        const std::filesystem::path runtimeBinaryDirectory =
            repositoryRoot / "bin" / ToOutputDirectoryName(options.Configuration) / "WheatearSandbox";
        const std::filesystem::path editorBinaryDirectory =
            repositoryRoot / "bin" / ToOutputDirectoryName(options.Configuration) / "WheatearEditor";
        const std::filesystem::path playerExe = runtimeBinaryDirectory / "WheatearSandbox.exe";
        const std::filesystem::path editorExe = editorBinaryDirectory / "WheatearEditor.exe";

        const std::filesystem::path powerShell = FindPowerShell();
        std::string errorMessage;
        const auto sandboxBuildStarted = std::chrono::steady_clock::now();
        if (!BuildVisualStudioProject(repositoryRoot,
            powerShell,
            buildScript,
            std::filesystem::path("WheatearSandbox") / "WheatearSandbox.vcxproj",
            options.Configuration,
            "build-sandbox-msbuild.log",
            "WheatearSandbox",
            &errorMessage))
            {
                return Fail(errorMessage, outputDirectory);
            }
        LogTimedStep("WheatearSandbox build", sandboxBuildStarted);

        const std::filesystem::path currentExecutable = GetCurrentExecutablePath();
        const auto editorBuildStarted = std::chrono::steady_clock::now();
        if (IsSameNormalizedPath(currentExecutable, editorExe))
        {
            WT_CORE_INFO("PlayerPackager: using running editor executable '{}'; skipping locked self-rebuild",
                editorExe.string());
        }
        else if (!BuildVisualStudioProject(repositoryRoot,
            powerShell,
            buildScript,
            std::filesystem::path("WheatearEditor") / "WheatearEditor.vcxproj",
            options.Configuration,
            "build-editor-msbuild.log",
            "WheatearEditor",
            &errorMessage))
        {
            return Fail(errorMessage, outputDirectory);
        }
        else
        {
            LogTimedStep("WheatearEditor build", editorBuildStarted);
        }

        if (!std::filesystem::exists(playerExe))
            return Fail("WheatearSandbox.exe was not generated; package aborted.", outputDirectory);
        if (!std::filesystem::exists(editorExe))
            return Fail("WheatearEditor.exe was not generated; package aborted.", outputDirectory);

        std::error_code error;
        std::string cleanError;
        if (!CleanPackageDirectory(outputDirectory, &cleanError))
            return Fail("Failed to clean previous package directory: " + cleanError, outputDirectory);
        error.clear();

        // "Self-package" case: the packaged editor is running from inside its
        // own editor package directory. Windows locks the image file of the
        // running executable, so the directory cannot be removed wholesale and
        // that executable cannot be overwritten. Keep the directory and the
        // running exe in place (it is the very binary the user launched) and
        // refresh the rest of the package.
        const bool runningInsideEditorPackage =
            !currentExecutable.empty() &&
            FileSystem::IsSubPath(currentExecutable, editorOutputDirectory);
        if (!runningInsideEditorPackage)
        {
            std::filesystem::remove_all(editorOutputDirectory, error);
            if (error)
                return Fail("Failed to clean previous editor package directory: " + error.message(), editorOutputDirectory);
        }
        else
        {
            WT_CORE_INFO("PlayerPackager: running editor executable lives inside the editor package '{}'; keeping it in place",
                editorOutputDirectory.string());
        }

        if (!FileSystem::EnsureDirectory(outputDirectory, &errorMessage))
            return Fail("Failed to create package directory: " + errorMessage, outputDirectory);
        if (!FileSystem::EnsureDirectory(editorOutputDirectory, &errorMessage))
            return Fail("Failed to create editor package directory: " + errorMessage, editorOutputDirectory);

        if (!CopyRuntimeBinaries(runtimeBinaryDirectory, outputDirectory,
            options.IncludeDebugSymbols, nullptr, &errorMessage))
        {
            return Fail("Failed to copy runtime binaries: " + errorMessage, outputDirectory);
        }
        const std::string runningEditorFileName =
            runningInsideEditorPackage ? currentExecutable.filename().generic_string() : std::string();
        if (!CopyRuntimeBinaries(editorBinaryDirectory, editorOutputDirectory,
            options.IncludeDebugSymbols,
            runningEditorFileName.empty() ? nullptr : &runningEditorFileName,
            &errorMessage))
        {
            return Fail("Failed to copy editor binaries: " + errorMessage, editorOutputDirectory);
        }

        const std::filesystem::path editorResources = repositoryRoot / "WheatearEditor" / "Resources";
        if (std::filesystem::exists(editorResources) &&
            !FileSystem::CopyDirectoryRecursive(editorResources, editorOutputDirectory / "Resources", {}, &errorMessage))
        {
            return Fail("Failed to copy editor resources: " + errorMessage, editorOutputDirectory);
        }

        std::filesystem::path startupSceneSource;
        const std::filesystem::path startupScene = ResolvePackagedStartupScene(options, outputDirectory, &startupSceneSource);
        const std::filesystem::path engineRoot = AssetPath::GetEngineRoot();
        const auto dependencyScanStarted = std::chrono::steady_clock::now();
        AssetDependencyScanOptions scanOptions;
        scanOptions.ProjectRoot = AssetPath::GetProjectRoot();
        scanOptions.BuiltinRoot = engineRoot;
        scanOptions.StartupAsset = startupScene;
        scanOptions.StartupSourceAsset = startupSceneSource;
        scanOptions.IncludeBuiltinAssets = true;
        scanOptions.IncludeUnusedAssets = false;
        const AssetDependencyReport dependencyReport = AssetDependencyScanner::BuildReport(scanOptions);
        LogTimedStep("Dependency scan", dependencyScanStarted);
        if (!dependencyReport.MissingReferences.empty() || !dependencyReport.MissingSceneTransitions.empty())
        {
            const std::filesystem::path preflightReportPath = outputDirectory / "package_preflight_report.txt";
            (void)WritePackageReport(options,
                dependencyReport,
                preflightReportPath,
                {},
                editorOutputDirectory,
                &errorMessage);

            return Fail("Package preflight failed: " +
                std::to_string(dependencyReport.MissingReferences.size()) + " missing asset reference(s), " +
                std::to_string(dependencyReport.MissingSceneTransitions.size()) + " missing scene transition(s). See package_preflight_report.txt.",
                outputDirectory);
        }

        const std::vector<std::filesystem::path>& packageAssets = dependencyReport.IncludedAssets;
        if (packageAssets.empty())
            return Fail("No assets were collected for the package.", outputDirectory);

        const std::filesystem::path assetPackPath = outputDirectory / kAssetPackFilename;
        const auto writePackStarted = std::chrono::steady_clock::now();
        if (!WriteAssetPack(AssetPath::GetProjectRoot(), engineRoot, packageAssets, &dependencyReport.AssetSources, assetPackPath, &errorMessage))
            return Fail("Failed to write asset pack: " + errorMessage, outputDirectory);
        LogTimedStep("Asset pack write", writePackStarted);

        const auto copyLooseStarted = std::chrono::steady_clock::now();
        if (!CopyLooseRuntimeDataAssets(AssetPath::GetProjectRoot(), engineRoot, packageAssets, &dependencyReport.AssetSources, outputDirectory, &errorMessage))
            return Fail("Failed to copy loose runtime data assets: " + errorMessage, outputDirectory);
        LogTimedStep("Loose runtime data copy", copyLooseStarted);

        const std::filesystem::path reportPath = outputDirectory / "package_report.txt";
        const auto reportStarted = std::chrono::steady_clock::now();
        if (!WritePackageReport(options, dependencyReport, reportPath, assetPackPath, editorOutputDirectory, &errorMessage))
            return Fail("Failed to write package report: " + errorMessage, outputDirectory);
        LogTimedStep("Package report write", reportStarted);

        RuntimePlayerConfig playerConfig;
        playerConfig.StartupScene = startupScene;
        const auto configStarted = std::chrono::steady_clock::now();
        if (!SaveRuntimePlayerConfig(outputDirectory / "assets" / "game" / "player.config",
            playerConfig,
            EngineInfo::EditorName))
        {
            return Fail("Failed to write player.config.", outputDirectory);
        }
        LogTimedStep("Player config write", configStarted);

        const std::filesystem::path executablePath = outputDirectory / "WheatearSandbox.exe";
        const std::filesystem::path editorExecutablePath = editorOutputDirectory / "WheatearEditor.exe";
        WT_CORE_INFO("PlayerPackager: package completed '{}' with {} packed assets",
            outputDirectory.string(),
            packageAssets.size());
        LogTimedStep("Full package", packageStarted);
        std::error_code sizeError;
        PlayerPackageResult result;
        result.Success = true;
        result.Message = "Package completed: Player=" + outputDirectory.string() +
                ", Editor=" + editorOutputDirectory.string() +
                " (Sandbox uses " + kAssetPackFilename +
                ", Editor uses WheatearEditor/assets)";
        result.PackageDirectory = outputDirectory;
        result.EditorPackageDirectory = editorOutputDirectory;
        result.ExecutablePath = executablePath;
        result.EditorExecutablePath = editorExecutablePath;
        result.AssetPackPath = assetPackPath;
        result.ReportPath = reportPath;
        result.PackedAssetCount = packageAssets.size();
        result.PackedAssetBytes = dependencyReport.IncludedBytes;
        result.AssetPackBytes = std::filesystem::file_size(assetPackPath, sizeError);
        return result;
#endif
    }

} // namespace Wheatear
