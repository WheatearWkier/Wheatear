#include "wtpch.h"
#include "AssetDatabase.h"

#include "Wheatear/Assets/AssetPath.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Wheatear {

    AssetDatabase& AssetDatabase::Get()
    {
        static AssetDatabase instance;
        return instance;
    }

    std::string AssetDatabase::NormalizePath(const std::string& path)
    {
        if (path.empty())
            return {};

        std::string normalized = path;
        for (char& ch : normalized)
        {
            if (ch == '\\')
                ch = '/';
        }

        std::filesystem::path candidate(normalized);
        normalized = candidate.lexically_normal().generic_string();
        if (normalized.rfind("./", 0) == 0)
            normalized.erase(0, 2);
        return normalized;
    }

    std::filesystem::path AssetDatabase::GetRegistryPath()
    {
        const std::filesystem::path standard =
            AssetPath::GetProjectRoot() / "Intermediate" / "AssetRegistry.yaml";
        if (std::filesystem::exists(standard))
            return standard;

        const std::filesystem::path legacy =
            AssetPath::GetAssetRoot() / ".wheatear" / "asset_registry.yaml";
        if (std::filesystem::exists(legacy))
            return legacy;

        return standard;
    }

    void AssetDatabase::Reload()
    {
        m_ById.clear();
        m_ByPath.clear();
        m_Loaded = true;

        const std::filesystem::path registryPath = GetRegistryPath();
        std::error_code existsError;
        if (!std::filesystem::exists(registryPath, existsError))
        {
            WT_CORE_WARN("AssetDatabase: registry not found at '{}'; UUID references "
                "will fall back to serialized paths", registryPath.string());
            return;
        }

        try
        {
            const YAML::Node root = YAML::LoadFile(registryPath.string());
            const YAML::Node assets = root["Assets"];
            if (!assets || !assets.IsSequence())
            {
                WT_CORE_WARN("AssetDatabase: '{}' has no Assets sequence", registryPath.string());
                return;
            }

            for (const YAML::Node& asset : assets)
            {
                if (!asset["ID"] || !asset["Path"])
                    continue;

                const uint64_t id = asset["ID"].as<uint64_t>(0);
                const std::string path = NormalizePath(asset["Path"].as<std::string>(""));
                if (id == 0 || path.empty())
                    continue;

                m_ById[id] = path;
                m_ByPath[path] = id;
            }

            WT_CORE_INFO("AssetDatabase: loaded {} UUID mapping(s) from '{}'",
                m_ById.size(), registryPath.string());
        }
        catch (const std::exception& error)
        {
            WT_CORE_ERROR("AssetDatabase: failed to read '{}': {}",
                registryPath.string(), error.what());
        }
    }

    void AssetDatabase::EnsureLoaded() const
    {
        if (!m_Loaded)
            const_cast<AssetDatabase*>(this)->Reload();
    }

    std::string AssetDatabase::GetPath(UUID id) const
    {
        if (static_cast<uint64_t>(id) == 0)
            return {};

        EnsureLoaded();
        const auto it = m_ById.find(static_cast<uint64_t>(id));
        return it == m_ById.end() ? std::string{} : it->second;
    }

    std::filesystem::path AssetDatabase::Resolve(UUID id) const
    {
        const std::string path = GetPath(id);
        return path.empty() ? std::filesystem::path{} : AssetPath::Resolve(path);
    }

    UUID AssetDatabase::FindByPath(const std::string& projectRelativePath) const
    {
        const std::string normalized = NormalizePath(projectRelativePath);
        if (normalized.empty())
            return UUID(0);

        EnsureLoaded();
        const auto it = m_ByPath.find(normalized);
        return it == m_ByPath.end() ? UUID(0) : UUID(it->second);
    }

    void AssetDatabase::SetMapping(UUID id, const std::string& projectRelativePath)
    {
        const std::string normalized = NormalizePath(projectRelativePath);
        if (static_cast<uint64_t>(id) == 0 || normalized.empty())
            return;

        EnsureLoaded();
        m_ById[static_cast<uint64_t>(id)] = normalized;
        m_ByPath[normalized] = static_cast<uint64_t>(id);
    }

} // namespace Wheatear
