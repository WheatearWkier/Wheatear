#pragma once

#include <filesystem>
#include <string>

namespace Wheatear {

    struct PlayerPackageOptions
    {
        std::filesystem::path StartupScene;
        std::filesystem::path OutputDirectory;
        std::string Configuration = "Debug";
        bool EnableScripts = false;
        bool IncludeDebugSymbols = false;
    };

    struct PlayerPackageResult
    {
        bool Success = false;
        std::string Message;
        std::filesystem::path PackageDirectory;
        std::filesystem::path ExecutablePath;
    };

    class PlayerPackager
    {
    public:
        static PlayerPackageResult PackagePlayer(const PlayerPackageOptions& options);
        static void OpenDirectory(const std::filesystem::path& directory);
    };

} // namespace Wheatear
