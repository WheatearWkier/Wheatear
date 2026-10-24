#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/EngineInfo.h"

#include <filesystem>

namespace Wheatear {

    struct RuntimePlayerConfig
    {
        std::string ProjectName;
        std::string ProjectVersion = "0.1.0";
        std::string Description;
        std::filesystem::path StartupScene = EngineInfo::DefaultStartupScene;

        // Preferred editor workspace: "2D" or "3D". The launcher uses it for
        // the project badge and the mode-specific Open button.
        std::string Workspace = "2D";

        // Optional release name for the packaged player directory. Empty
        // falls back to the project key.
        std::string PackageName;
    };

    inline constexpr const char* ProjectManifestExtension = ".wtproject";

    WHEATEAR_API std::filesystem::path FindRuntimePlayerConfigPath(const std::filesystem::path& start = {});
    WHEATEAR_API std::filesystem::path GetProjectConfigPath(const std::filesystem::path& projectRoot);
    WHEATEAR_API std::filesystem::path GetProjectManifestPath(const std::filesystem::path& projectRoot);
    WHEATEAR_API RuntimePlayerConfig LoadRuntimePlayerConfig(const std::filesystem::path& configPath = {});
    WHEATEAR_API bool SaveRuntimePlayerConfig(const std::filesystem::path& configPath,
        const RuntimePlayerConfig& config,
        const char* generatorName = EngineInfo::EditorName);
    WHEATEAR_API bool SaveProjectConfig(const std::filesystem::path& projectRoot,
        const RuntimePlayerConfig& config,
        const char* generatorName = EngineInfo::EditorName);

} // namespace Wheatear
