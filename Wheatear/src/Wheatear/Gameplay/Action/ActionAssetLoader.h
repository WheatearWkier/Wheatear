#pragma once

#include "Wheatear/Core/Core.h"

#include <cstddef>
#include <filesystem>

namespace Wheatear::WAO {

    class WHEATEAR_API ActionAssetLoader
    {
    public:
        static size_t LoadFile(const std::filesystem::path& path);
        static size_t LoadDirectory(const std::filesystem::path& path);
        static size_t LoadManifest(const std::filesystem::path& path);
        static size_t ReloadDirectory(const std::filesystem::path& path);
        static size_t ReloadManifest(const std::filesystem::path& path);
    };

} // namespace Wheatear::WAO
