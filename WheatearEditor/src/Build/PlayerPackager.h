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
        std::filesystem::path AssetPackPath;
        std::filesystem::path ReportPath;
        size_t PackedAssetCount = 0;
        uintmax_t PackedAssetBytes = 0;
        uintmax_t AssetPackBytes = 0;
    };

    class PlayerPackager
    {
    public:
        static PlayerPackageResult PackagePlayer(const PlayerPackageOptions& options);
        static void OpenDirectory(const std::filesystem::path& directory);
    };

} // namespace Wheatear
