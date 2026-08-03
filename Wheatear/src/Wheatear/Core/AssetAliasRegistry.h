#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Wheatear {

    class WHEATEAR_API AssetAliasRegistry
    {
    public:
        static void Load(const std::filesystem::path& path = "assets/gameplay/content_manifest.yaml");
        static void Reload(const std::filesystem::path& path = "assets/gameplay/content_manifest.yaml");
        static std::string Resolve(const std::string& aliasOrPath);
        static std::string Path(const std::string& alias, const std::string& fallback = {});
        static bool Has(const std::string& alias);
        static const std::unordered_map<std::string, std::string>& All();

    private:
        static void EnsureLoaded();
    };

} // namespace Wheatear
