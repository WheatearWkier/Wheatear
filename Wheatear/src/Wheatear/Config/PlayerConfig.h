#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/EngineInfo.h"

#include <filesystem>

namespace Wheatear {

    struct RuntimePlayerConfig
    {
        std::filesystem::path StartupScene = EngineInfo::DefaultStartupScene;
    };

    WHEATEAR_API std::filesystem::path FindRuntimePlayerConfigPath(const std::filesystem::path& start = {});
    WHEATEAR_API RuntimePlayerConfig LoadRuntimePlayerConfig(const std::filesystem::path& configPath = {});
    WHEATEAR_API bool SaveRuntimePlayerConfig(const std::filesystem::path& configPath,
        const RuntimePlayerConfig& config,
        const char* generatorName = EngineInfo::EditorName);

} // namespace Wheatear
