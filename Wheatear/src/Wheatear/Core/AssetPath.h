#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>

namespace Wheatear {

    class WHEATEAR_API AssetPath
    {
    public:
        static std::filesystem::path DiscoverProjectRoot(const std::filesystem::path& start = {});

        static void SetProjectRoot(const std::filesystem::path& projectRoot);
        static void SetAssetDirectoryName(const std::filesystem::path& directoryName);

        static const std::filesystem::path& GetProjectRoot();
        static std::filesystem::path GetAssetRoot();
        static std::filesystem::path GetResourceRoot();

        static std::filesystem::path Resolve(const std::filesystem::path& path);
        static std::filesystem::path ResolveRuntimeData(const std::filesystem::path& path);
        static std::filesystem::path ResolveAsset(const std::filesystem::path& path);
        static std::filesystem::path ResolveResource(const std::filesystem::path& path);

        static std::filesystem::path ToProjectRelative(const std::filesystem::path& path);
    };

} // namespace Wheatear
