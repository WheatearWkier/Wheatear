#pragma once

#include "Wheatear/Core/UUID.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    enum class EditorAssetKind
    {
        Unknown = 0,
        Scene,
        Texture,
        SpriteSheet,
        Shader,
        Audio,
        Script,
        EventScript,
        Prefab,
        UITemplate,
        Material,
        Data,
        Metadata,
        AnimationClip
    };

    struct TextureImportSettings
    {
        std::string Filter = "Linear";
        float PixelsPerUnit = 100.0f;
        int Columns = 1;
        int Rows = 1;
        int CellWidth = 0;
        int CellHeight = 0;
        int PaddingX = 0;
        int PaddingY = 0;
    };

    struct AudioImportSettings
    {
        std::string Usage = "SFX";
        float DefaultVolume = 0.35f;
        bool Loop = false;
    };

    struct PrefabImportSettings
    {
        std::string Category = "Gameplay";
        std::string TemplateKind;
        std::string Description;
    };

    struct EditorAssetMetadata
    {
        UUID ID = 0;
        std::string RelativePath;
        EditorAssetKind Kind = EditorAssetKind::Unknown;
        std::string DisplayName;
        uintmax_t SizeBytes = 0;
        int64_t LastWriteTime = 0;
        bool Dirty = false;

        TextureImportSettings Texture;
        AudioImportSettings Audio;
        PrefabImportSettings Prefab;

        std::vector<std::string> Tags;
        std::vector<std::string> References;
        std::vector<std::string> ReferencedBy;
    };

    class AssetRegistry
    {
    public:
        static AssetRegistry& Get();

        void LoadCache(const std::filesystem::path& projectRoot = {});
        void Scan(const std::filesystem::path& projectRoot = {});
        bool WriteRegistry() const;
        const std::vector<EditorAssetMetadata>& GetAssets() const { return m_Assets; }
        const EditorAssetMetadata* FindByPath(const std::filesystem::path& relativePath) const;
        EditorAssetMetadata* FindMutableByPath(const std::filesystem::path& relativePath);
        const EditorAssetMetadata* FindByUUID(UUID id) const;

        size_t GetAssetCount() const { return m_Assets.size(); }
        size_t GetReferenceCount() const;
        bool HasScanned() const { return m_HasScanned; }

        static std::string KindToString(EditorAssetKind kind);
        static EditorAssetKind KindFromString(const std::string& value);
        static EditorAssetKind DetectKind(const std::filesystem::path& relativePath);
        static std::string NormalizePath(const std::filesystem::path& path);
        static std::filesystem::path GetRegistryPath(const std::filesystem::path& projectRoot);

    private:
        void LoadRegistryCache();
        void RebuildIndexes();
        void RebuildReverseReferences();
        void PreserveOrCreateMetadata(EditorAssetMetadata& metadata,
            const std::unordered_map<std::string, EditorAssetMetadata>& previousByPath);
        std::vector<std::string> ExtractReferences(const std::filesystem::path& absolutePath,
            const std::filesystem::path& relativePath) const;

    private:
        std::filesystem::path m_ProjectRoot;
        std::vector<EditorAssetMetadata> m_Assets;
        std::unordered_map<std::string, size_t> m_PathToIndex;
        std::unordered_map<uint64_t, size_t> m_UUIDToIndex;
        bool m_HasScanned = false;
    };

} // namespace Wheatear
