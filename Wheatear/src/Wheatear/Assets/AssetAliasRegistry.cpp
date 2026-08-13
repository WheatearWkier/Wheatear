#include "wtpch.h"
#include "AssetAliasRegistry.h"

#include "Wheatear/Assets/AssetPath.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>

namespace Wheatear {

    namespace {

        struct AliasStorage
        {
            bool Loaded = false;
            std::filesystem::path SourcePath = "assets/gameplay/content_manifest.yaml";
            std::unordered_map<std::string, std::string> Aliases;
        };

        static AliasStorage& Storage()
        {
            static AliasStorage storage;
            return storage;
        }

        static bool LooksLikeAssetPath(const std::string& value)
        {
            return value.rfind("assets/", 0) == 0
                || value.rfind("assets\\", 0) == 0
                || value.rfind("./", 0) == 0
                || value.rfind("../", 0) == 0;
        }

        static std::string NormalizeAssetPath(std::string value)
        {
            std::replace(value.begin(), value.end(), '\\', '/');
            return value;
        }

        static void ReadAliases(const YAML::Node& node,
            const std::string& prefix,
            std::unordered_map<std::string, std::string>* aliases)
        {
            if (!node || !aliases)
                return;

            if (node.IsMap())
            {
                for (const auto& entry : node)
                {
                    if (!entry.first.IsScalar())
                        continue;

                    const std::string key = entry.first.as<std::string>();
                    const std::string nextPrefix = prefix.empty() ? key : prefix + "." + key;
                    ReadAliases(entry.second, nextPrefix, aliases);
                }
                return;
            }

            if (!prefix.empty() && node.IsScalar())
            {
                const std::string value = NormalizeAssetPath(node.as<std::string>(""));
                if (!value.empty())
                    (*aliases)[prefix] = value;
            }
        }

    } // namespace

    void AssetAliasRegistry::Load(const std::filesystem::path& path)
    {
        AliasStorage& storage = Storage();
        if (storage.Loaded && storage.SourcePath == path)
            return;

        Reload(path);
    }

    void AssetAliasRegistry::Reload(const std::filesystem::path& path)
    {
        AliasStorage& storage = Storage();
        storage.SourcePath = path;
        storage.Aliases.clear();
        storage.Loaded = true;

        const std::filesystem::path resolved = AssetPath::ResolveRuntimeData(path);
        if (!std::filesystem::is_regular_file(resolved))
        {
            WT_CORE_WARN("AssetAliasRegistry: manifest not found '{}'", resolved.string());
            return;
        }

        try
        {
            const YAML::Node root = YAML::LoadFile(resolved.string());
            ReadAliases(root["aliases"] ? root["aliases"] : root, {}, &storage.Aliases);
            WT_CORE_INFO("AssetAliasRegistry: loaded {} alias(es) from '{}'", storage.Aliases.size(), resolved.string());
        }
        catch (const YAML::Exception& exception)
        {
            storage.Aliases.clear();
            WT_CORE_WARN("AssetAliasRegistry: failed to load '{}': {}", resolved.string(), exception.what());
        }
    }

    std::string AssetAliasRegistry::Resolve(const std::string& aliasOrPath)
    {
        if (aliasOrPath.empty() || LooksLikeAssetPath(aliasOrPath))
            return NormalizeAssetPath(aliasOrPath);

        EnsureLoaded();
        const auto& aliases = Storage().Aliases;
        if (auto it = aliases.find(aliasOrPath); it != aliases.end())
            return it->second;

        return aliasOrPath;
    }

    std::string AssetAliasRegistry::Path(const std::string& alias, const std::string& fallback)
    {
        EnsureLoaded();
        const auto& aliases = Storage().Aliases;
        if (auto it = aliases.find(alias); it != aliases.end())
            return it->second;
        return NormalizeAssetPath(fallback);
    }

    bool AssetAliasRegistry::Has(const std::string& alias)
    {
        EnsureLoaded();
        return Storage().Aliases.find(alias) != Storage().Aliases.end();
    }

    const std::unordered_map<std::string, std::string>& AssetAliasRegistry::All()
    {
        EnsureLoaded();
        return Storage().Aliases;
    }

    void AssetAliasRegistry::EnsureLoaded()
    {
        if (!Storage().Loaded)
            Load(Storage().SourcePath);
    }

} // namespace Wheatear
