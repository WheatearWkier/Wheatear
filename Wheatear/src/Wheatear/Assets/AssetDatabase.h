#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Wheatear {

    // Runtime view of the editor's asset registry. Scenes store UUIDs for
    // model/material/texture references so an asset can be moved or renamed
    // without breaking every scene that uses it; the UUID resolves back to a
    // project-relative path here.
    class WHEATEAR_API AssetDatabase
    {
    public:
        static AssetDatabase& Get();

        // Reads assets/.wheatear/asset_registry.yaml from the current project.
        // Safe to call repeatedly; the registry is loaded lazily the first
        // time a UUID is resolved.
        void Reload();
        bool IsLoaded() const { return m_Loaded; }

        std::string GetPath(UUID id) const;
        std::filesystem::path Resolve(UUID id) const;

        UUID FindByPath(const std::string& projectRelativePath) const;

        // Editor/runtime hook for assets imported after the last registry
        // write. Does not persist; call AssetRegistry::WriteRegistry for that.
        void SetMapping(UUID id, const std::string& projectRelativePath);

        static std::string NormalizePath(const std::string& path);
        static std::filesystem::path GetRegistryPath();

        size_t GetAssetCount() const { return m_ById.size(); }

    private:
        void EnsureLoaded() const;

        mutable bool m_Loaded = false;
        mutable std::unordered_map<uint64_t, std::string> m_ById;
        mutable std::unordered_map<std::string, uint64_t> m_ByPath;
    };

} // namespace Wheatear
