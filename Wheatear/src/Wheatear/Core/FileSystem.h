#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <functional>
#include <string>

namespace Wheatear::FileSystem {

    using DirectoryCopyFilter = std::function<bool(const std::filesystem::path& relativePath, bool isDirectory)>;

    WHEATEAR_API std::filesystem::path Normalize(const std::filesystem::path& path);
    WHEATEAR_API bool IsSubPath(const std::filesystem::path& child, const std::filesystem::path& parent);
    WHEATEAR_API bool EnsureDirectory(const std::filesystem::path& directory, std::string* errorMessage = nullptr);
    WHEATEAR_API bool CopyDirectoryRecursive(const std::filesystem::path& source,
        const std::filesystem::path& destination,
        const DirectoryCopyFilter& shouldSkip = {},
        std::string* errorMessage = nullptr);
    WHEATEAR_API bool WriteTextFile(const std::filesystem::path& path,
        const std::string& contents,
        std::string* errorMessage = nullptr);

} // namespace Wheatear::FileSystem
