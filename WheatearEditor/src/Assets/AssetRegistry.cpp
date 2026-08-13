#include "wepch.h"
#include "AssetRegistry.h"

#include "Build/AssetDependencyScanner.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string_view>
#include <system_error>

namespace Wheatear {

    namespace {

        static bool IsHiddenEditorDirectory(const std::filesystem::path& path)
        {
            for (const auto& part : path)
            {
                const std::string value = part.generic_string();
                if (value == ".wheatear" || value == "cache" || value == "saves" || value == "previews" || value == "source_frames")
                    return true;
                if (value.rfind("_backup_before_", 0) == 0)
                    return true;
            }
            return false;
        }

        static bool IsEditorOnlyOrIntermediateAsset(const std::filesystem::path& path)
        {
            if (IsHiddenEditorDirectory(path))
                return true;

            const std::string extension = path.extension().generic_string();
            return extension == AssetFileType::MetadataExtension
                || extension == ".rar"
                || extension == ".zip"
                || extension == ".7z";
        }

        static int64_t ToStableWriteTime(std::filesystem::file_time_type time)
        {
            return static_cast<int64_t>(time.time_since_epoch().count());
        }

        static bool ReadTextFile(const std::filesystem::path& path, std::string* text)
        {
            if (!text)
                return false;

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            *text = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            return true;
        }

        static void SortUnique(std::vector<std::string>* values)
        {
            if (!values)
                return;
            std::sort(values->begin(), values->end());
            values->erase(std::unique(values->begin(), values->end()), values->end());
        }

        static std::string Trim(std::string value)
        {
            size_t first = 0;
            while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0)
                ++first;

            size_t last = value.size();
            while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
                --last;

            if (first >= last)
                return {};
            return value.substr(first, last - first);
        }

        static bool StartsWith(const std::string& value, const char* prefix)
        {
            const std::string_view view(value);
            const std::string_view expected(prefix);
            return view.size() >= expected.size() && view.substr(0, expected.size()) == expected;
        }

        static std::string Unquote(std::string value)
        {
            value = Trim(std::move(value));
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                return value.substr(1, value.size() - 2);
            return value;
        }

        static std::string ScalarAfter(const std::string& line, const char* key)
        {
            const std::string trimmed = Trim(line);
            if (!StartsWith(trimmed, key))
                return {};
            return Unquote(trimmed.substr(std::strlen(key)));
        }

        static std::string NormalizeAssetPathString(std::string normalized)
        {
            std::replace(normalized.begin(), normalized.end(), '\\', '/');

            while (normalized.rfind("./", 0) == 0)
                normalized.erase(0, 2);

            size_t doubleSlash = normalized.find("//");
            while (doubleSlash != std::string::npos)
            {
                normalized.replace(doubleSlash, 2, "/");
                doubleSlash = normalized.find("//");
            }

            const std::string currentDirectory = "/./";
            size_t current = normalized.find(currentDirectory);
            while (current != std::string::npos)
            {
                normalized.replace(current, currentDirectory.size(), "/");
                current = normalized.find(currentDirectory);
            }

            return normalized;
        }

        static int ParseInt(std::string value, int fallback)
        {
            try { return std::stoi(value); }
            catch (...) { return fallback; }
        }

        static uint64_t ParseUInt64(std::string value, uint64_t fallback)
        {
            try { return std::stoull(value); }
            catch (...) { return fallback; }
        }

        static int64_t ParseInt64(std::string value, int64_t fallback)
        {
            try { return std::stoll(value); }
            catch (...) { return fallback; }
        }

        static uintmax_t ParseSize(std::string value, uintmax_t fallback)
        {
            try { return static_cast<uintmax_t>(std::stoull(value)); }
            catch (...) { return fallback; }
        }

        static float ParseFloat(std::string value, float fallback)
        {
            try { return std::stof(value); }
            catch (...) { return fallback; }
        }

        static bool ParseBool(std::string value, bool fallback)
        {
            value = Trim(std::move(value));
            if (value == "true") return true;
            if (value == "false") return false;
            return fallback;
        }

        static void SerializeStringList(YAML::Emitter& out,
            const char* key,
            const std::vector<std::string>& values)
        {
            out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
            for (const std::string& value : values)
                out << YAML::DoubleQuoted << value;
            out << YAML::EndSeq;
        }

        static bool ReadUITemplateFile(const std::filesystem::path& path,
            std::string* displayName,
            PrefabImportSettings* prefab)
        {
            if (!prefab)
                return false;

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            bool insideTemplate = false;
            std::string line;
            while (std::getline(input, line))
            {
                const std::string trimmed = Trim(line);
                if (trimmed == "UITemplate:")
                {
                    insideTemplate = true;
                    continue;
                }

                if (!insideTemplate)
                    continue;

                if (StartsWith(trimmed, "Kind:"))
                    prefab->TemplateKind = ScalarAfter(trimmed, "Kind:");
                else if (StartsWith(trimmed, "DisplayName:") && displayName)
                    *displayName = ScalarAfter(trimmed, "DisplayName:");
                else if (StartsWith(trimmed, "Category:"))
                    prefab->Category = ScalarAfter(trimmed, "Category:");
                else if (StartsWith(trimmed, "Description:"))
                    prefab->Description = ScalarAfter(trimmed, "Description:");
            }

            return true;
        }

        static void LoadRegistryCacheFile(const std::filesystem::path& path,
            std::vector<EditorAssetMetadata>* assets)
        {
            if (!assets || !std::filesystem::is_regular_file(path))
                return;

            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return;

            EditorAssetMetadata current;
            bool hasAsset = false;
            std::string listSection;
            std::string importSection;

            auto pushCurrent = [&]()
            {
                if (hasAsset && !current.RelativePath.empty() && static_cast<uint64_t>(current.ID) != 0)
                {
                    SortUnique(&current.Tags);
                    assets->push_back(current);
                }
                current = {};
                hasAsset = false;
                listSection.clear();
                importSection.clear();
            };

            std::string line;
            while (std::getline(input, line))
            {
                const std::string trimmed = Trim(line);
                if (trimmed.empty())
                    continue;

                if (StartsWith(trimmed, "- ID:"))
                {
                    pushCurrent();
                    hasAsset = true;
                    current.ID = UUID(ParseUInt64(trimmed.substr(5), 0));
                    continue;
                }

                if (!hasAsset)
                    continue;

                if (StartsWith(trimmed, "Path:")) current.RelativePath = NormalizeAssetPathString(ScalarAfter(trimmed, "Path:"));
                else if (StartsWith(trimmed, "Type:")) current.Kind = AssetRegistry::KindFromString(ScalarAfter(trimmed, "Type:"));
                else if (StartsWith(trimmed, "DisplayName:")) current.DisplayName = ScalarAfter(trimmed, "DisplayName:");
                else if (StartsWith(trimmed, "SizeBytes:")) current.SizeBytes = ParseSize(trimmed.substr(10), current.SizeBytes);
                else if (StartsWith(trimmed, "LastWriteTime:")) current.LastWriteTime = ParseInt64(trimmed.substr(14), current.LastWriteTime);
                else if (StartsWith(trimmed, "Tags:")) { listSection = "Tags"; importSection.clear(); }
                else if (StartsWith(trimmed, "References:")) { listSection = "References"; importSection.clear(); }
                else if (StartsWith(trimmed, "ReferencedBy:")) { listSection = "ReferencedBy"; importSection.clear(); }
                else if (StartsWith(trimmed, "Import:")) { listSection.clear(); importSection.clear(); }
                else if (trimmed == "Texture:" || trimmed == "Audio:" || trimmed == "Prefab:")
                {
                    importSection = trimmed.substr(0, trimmed.size() - 1);
                    listSection.clear();
                }
                else if (StartsWith(trimmed, "- "))
                {
                    if (listSection == "Tags")
                        current.Tags.push_back(Unquote(trimmed.substr(2)));
                    else if (listSection == "References")
                        current.References.push_back(NormalizeAssetPathString(Unquote(trimmed.substr(2))));
                    else if (listSection == "ReferencedBy")
                        current.ReferencedBy.push_back(NormalizeAssetPathString(Unquote(trimmed.substr(2))));
                }
                else if (importSection == "Texture")
                {
                    if (StartsWith(trimmed, "Filter:")) current.Texture.Filter = ScalarAfter(trimmed, "Filter:");
                    else if (StartsWith(trimmed, "PixelsPerUnit:")) current.Texture.PixelsPerUnit = ParseFloat(trimmed.substr(14), current.Texture.PixelsPerUnit);
                    else if (StartsWith(trimmed, "Columns:")) current.Texture.Columns = ParseInt(trimmed.substr(8), current.Texture.Columns);
                    else if (StartsWith(trimmed, "Rows:")) current.Texture.Rows = ParseInt(trimmed.substr(5), current.Texture.Rows);
                    else if (StartsWith(trimmed, "CellWidth:")) current.Texture.CellWidth = ParseInt(trimmed.substr(10), current.Texture.CellWidth);
                    else if (StartsWith(trimmed, "CellHeight:")) current.Texture.CellHeight = ParseInt(trimmed.substr(11), current.Texture.CellHeight);
                    else if (StartsWith(trimmed, "PaddingX:")) current.Texture.PaddingX = ParseInt(trimmed.substr(9), current.Texture.PaddingX);
                    else if (StartsWith(trimmed, "PaddingY:")) current.Texture.PaddingY = ParseInt(trimmed.substr(9), current.Texture.PaddingY);
                }
                else if (importSection == "Audio")
                {
                    if (StartsWith(trimmed, "Usage:")) current.Audio.Usage = ScalarAfter(trimmed, "Usage:");
                    else if (StartsWith(trimmed, "DefaultVolume:")) current.Audio.DefaultVolume = ParseFloat(trimmed.substr(14), current.Audio.DefaultVolume);
                    else if (StartsWith(trimmed, "Loop:")) current.Audio.Loop = ParseBool(trimmed.substr(5), current.Audio.Loop);
                }
                else if (importSection == "Prefab")
                {
                    if (StartsWith(trimmed, "Category:")) current.Prefab.Category = ScalarAfter(trimmed, "Category:");
                    else if (StartsWith(trimmed, "TemplateKind:")) current.Prefab.TemplateKind = ScalarAfter(trimmed, "TemplateKind:");
                    else if (StartsWith(trimmed, "Description:")) current.Prefab.Description = ScalarAfter(trimmed, "Description:");
                }
            }

            pushCurrent();
        }

        static void SerializeImportSettings(YAML::Emitter& out, const EditorAssetMetadata& metadata)
        {
            out << YAML::Key << "Import" << YAML::Value << YAML::BeginMap;

            if (metadata.Kind == EditorAssetKind::Texture || metadata.Kind == EditorAssetKind::SpriteSheet)
            {
                out << YAML::Key << "Texture" << YAML::Value << YAML::BeginMap;
                out << YAML::Key << "Filter" << YAML::Value << YAML::DoubleQuoted << metadata.Texture.Filter;
                out << YAML::Key << "PixelsPerUnit" << YAML::Value << metadata.Texture.PixelsPerUnit;
                out << YAML::Key << "Columns" << YAML::Value << metadata.Texture.Columns;
                out << YAML::Key << "Rows" << YAML::Value << metadata.Texture.Rows;
                out << YAML::Key << "CellWidth" << YAML::Value << metadata.Texture.CellWidth;
                out << YAML::Key << "CellHeight" << YAML::Value << metadata.Texture.CellHeight;
                out << YAML::Key << "PaddingX" << YAML::Value << metadata.Texture.PaddingX;
                out << YAML::Key << "PaddingY" << YAML::Value << metadata.Texture.PaddingY;
                out << YAML::EndMap;
            }

            if (metadata.Kind == EditorAssetKind::Audio)
            {
                out << YAML::Key << "Audio" << YAML::Value << YAML::BeginMap;
                out << YAML::Key << "Usage" << YAML::Value << YAML::DoubleQuoted << metadata.Audio.Usage;
                out << YAML::Key << "DefaultVolume" << YAML::Value << metadata.Audio.DefaultVolume;
                out << YAML::Key << "Loop" << YAML::Value << metadata.Audio.Loop;
                out << YAML::EndMap;
            }

            if (metadata.Kind == EditorAssetKind::Prefab || metadata.Kind == EditorAssetKind::UITemplate)
            {
                out << YAML::Key << "Prefab" << YAML::Value << YAML::BeginMap;
                out << YAML::Key << "Category" << YAML::Value << YAML::DoubleQuoted << metadata.Prefab.Category;
                out << YAML::Key << "TemplateKind" << YAML::Value << YAML::DoubleQuoted << metadata.Prefab.TemplateKind;
                out << YAML::Key << "Description" << YAML::Value << YAML::DoubleQuoted << metadata.Prefab.Description;
                out << YAML::EndMap;
            }

            out << YAML::EndMap;
        }

    } // namespace

    AssetRegistry& AssetRegistry::Get()
    {
        static AssetRegistry registry;
        return registry;
    }

    void AssetRegistry::LoadCache(const std::filesystem::path& projectRoot)
    {
        m_ProjectRoot = projectRoot.empty() ? AssetPath::GetProjectRoot() : projectRoot;
        m_ProjectRoot = m_ProjectRoot.lexically_normal();

        LoadRegistryCache();
        RebuildReverseReferences();
        m_HasScanned = true;
        WT_CORE_INFO("AssetRegistry: loaded cache with {} asset record(s)", m_Assets.size());
    }

    void AssetRegistry::Scan(const std::filesystem::path& projectRoot)
    {
        m_ProjectRoot = projectRoot.empty() ? AssetPath::GetProjectRoot() : projectRoot;
        m_ProjectRoot = m_ProjectRoot.lexically_normal();
        const auto startedAt = std::chrono::steady_clock::now();
        WT_CORE_INFO("AssetRegistry: scanning '{}'", (m_ProjectRoot / "assets").string());

        std::unordered_map<std::string, EditorAssetMetadata> previousByPath;
        WT_CORE_INFO("AssetRegistry: loading registry cache");
        LoadRegistryCache();
        WT_CORE_INFO("AssetRegistry: loaded {} cached asset record(s)", m_Assets.size());
        for (const auto& metadata : m_Assets)
            previousByPath[metadata.RelativePath] = metadata;

        m_Assets.clear();
        const std::filesystem::path assetRoot = m_ProjectRoot / "assets";
        if (!std::filesystem::is_directory(assetRoot))
        {
            RebuildIndexes();
            m_HasScanned = true;
            return;
        }

        std::error_code error;
        size_t visitedFiles = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(assetRoot, error))
        {
            if (error || !entry.is_regular_file())
                continue;

            const std::filesystem::path absolutePath = entry.path();
            const std::filesystem::path relativePath = std::filesystem::relative(absolutePath, m_ProjectRoot, error);
            if (error || IsHiddenEditorDirectory(relativePath))
                continue;

            if (IsEditorOnlyOrIntermediateAsset(relativePath))
                continue;

            ++visitedFiles;
            if ((visitedFiles % 250) == 0)
                WT_CORE_INFO("AssetRegistry: visited {} asset file(s), current '{}'", visitedFiles, relativePath.generic_string());

            EditorAssetMetadata metadata;
            metadata.RelativePath = NormalizePath(relativePath);
            metadata.Kind = DetectKind(relativePath);
            metadata.DisplayName = relativePath.filename().generic_string();
            metadata.SizeBytes = std::filesystem::file_size(absolutePath, error);
            metadata.LastWriteTime = ToStableWriteTime(std::filesystem::last_write_time(absolutePath, error));
            auto previous = previousByPath.find(metadata.RelativePath);
            const bool canReuseReferences = previous != previousByPath.end()
                && previous->second.SizeBytes == metadata.SizeBytes
                && previous->second.LastWriteTime == metadata.LastWriteTime;
            PreserveOrCreateMetadata(metadata, previousByPath);
            metadata.References = canReuseReferences
                ? previous->second.References
                : ExtractReferences(absolutePath, relativePath);
            metadata.References.erase(std::remove_if(metadata.References.begin(), metadata.References.end(), [](const std::string& reference)
            {
                return IsEditorOnlyOrIntermediateAsset(std::filesystem::path(reference));
            }), metadata.References.end());

            m_Assets.push_back(std::move(metadata));
        }
        WT_CORE_INFO("AssetRegistry: indexed {} visible file(s)", visitedFiles);

        std::sort(m_Assets.begin(), m_Assets.end(), [](const auto& left, const auto& right)
        {
            return left.RelativePath < right.RelativePath;
        });

        RebuildIndexes();
        RebuildReverseReferences();
        m_HasScanned = true;
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startedAt).count();
        WT_CORE_INFO("AssetRegistry: scanned {} asset(s), {} reference(s) in {:.2f} ms",
            m_Assets.size(), GetReferenceCount(), elapsed);
    }

    bool AssetRegistry::WriteRegistry() const
    {
        const std::filesystem::path registryPath = GetRegistryPath(m_ProjectRoot.empty() ? AssetPath::GetProjectRoot() : m_ProjectRoot);
        std::filesystem::create_directories(registryPath.parent_path());

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "AssetRegistry" << YAML::Value << "Wheatear";
        out << YAML::Key << "Version" << YAML::Value << 1;
        out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;
        for (const auto& metadata : m_Assets)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "ID" << YAML::Value << static_cast<uint64_t>(metadata.ID);
            out << YAML::Key << "Path" << YAML::Value << YAML::DoubleQuoted << metadata.RelativePath;
            out << YAML::Key << "Type" << YAML::Value << YAML::DoubleQuoted << KindToString(metadata.Kind);
            out << YAML::Key << "DisplayName" << YAML::Value << YAML::DoubleQuoted << metadata.DisplayName;
            out << YAML::Key << "SizeBytes" << YAML::Value << metadata.SizeBytes;
            out << YAML::Key << "LastWriteTime" << YAML::Value << metadata.LastWriteTime;
            SerializeStringList(out, "Tags", metadata.Tags);
            SerializeStringList(out, "References", metadata.References);
            SerializeStringList(out, "ReferencedBy", metadata.ReferencedBy);
            SerializeImportSettings(out, metadata);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream output(registryPath, std::ios::binary);
        if (!output.is_open())
            return false;
        output << out.c_str();
        return true;
    }

    const EditorAssetMetadata* AssetRegistry::FindByPath(const std::filesystem::path& relativePath) const
    {
        const std::string normalized = NormalizePath(relativePath);
        auto it = m_PathToIndex.find(normalized);
        return it == m_PathToIndex.end() ? nullptr : &m_Assets[it->second];
    }

    EditorAssetMetadata* AssetRegistry::FindMutableByPath(const std::filesystem::path& relativePath)
    {
        const std::string normalized = NormalizePath(relativePath);
        auto it = m_PathToIndex.find(normalized);
        return it == m_PathToIndex.end() ? nullptr : &m_Assets[it->second];
    }

    const EditorAssetMetadata* AssetRegistry::FindByUUID(UUID id) const
    {
        auto it = m_UUIDToIndex.find(static_cast<uint64_t>(id));
        return it == m_UUIDToIndex.end() ? nullptr : &m_Assets[it->second];
    }

    size_t AssetRegistry::GetReferenceCount() const
    {
        size_t count = 0;
        for (const auto& metadata : m_Assets)
            count += metadata.References.size();
        return count;
    }

    std::string AssetRegistry::KindToString(EditorAssetKind kind)
    {
        switch (kind)
        {
        case EditorAssetKind::Scene: return "Scene";
        case EditorAssetKind::Texture: return "Texture";
        case EditorAssetKind::SpriteSheet: return "SpriteSheet";
        case EditorAssetKind::Shader: return "Shader";
        case EditorAssetKind::Audio: return "Audio";
        case EditorAssetKind::Script: return "Script";
        case EditorAssetKind::EventScript: return "EventScript";
        case EditorAssetKind::Prefab: return "Prefab";
        case EditorAssetKind::UITemplate: return "UITemplate";
        case EditorAssetKind::Material: return "Material";
        case EditorAssetKind::Data: return "Data";
        case EditorAssetKind::Metadata: return "Metadata";
        case EditorAssetKind::AnimationClip: return "AnimationClip";
        default: return "Unknown";
        }
    }

    EditorAssetKind AssetRegistry::KindFromString(const std::string& value)
    {
        if (value == "Scene") return EditorAssetKind::Scene;
        if (value == "Texture") return EditorAssetKind::Texture;
        if (value == "SpriteSheet") return EditorAssetKind::SpriteSheet;
        if (value == "Shader") return EditorAssetKind::Shader;
        if (value == "Audio") return EditorAssetKind::Audio;
        if (value == "Script") return EditorAssetKind::Script;
        if (value == "EventScript") return EditorAssetKind::EventScript;
        if (value == "Prefab") return EditorAssetKind::Prefab;
        if (value == "UITemplate") return EditorAssetKind::UITemplate;
        if (value == "Material") return EditorAssetKind::Material;
        if (value == "Data") return EditorAssetKind::Data;
        if (value == "Metadata") return EditorAssetKind::Metadata;
        if (value == "AnimationClip") return EditorAssetKind::AnimationClip;
        return EditorAssetKind::Unknown;
    }

    EditorAssetKind AssetRegistry::DetectKind(const std::filesystem::path& relativePath)
    {
        const std::string extension = relativePath.extension().generic_string();
        if (extension == AssetFileType::SceneExtension) return EditorAssetKind::Scene;
        if (extension == AssetFileType::PrefabExtension) return EditorAssetKind::Prefab;
        if (extension == AssetFileType::UITemplateExtension) return EditorAssetKind::UITemplate;
        if (extension == AssetFileType::MaterialExtension) return EditorAssetKind::Material;
        if (extension == AssetFileType::MetadataExtension) return EditorAssetKind::Metadata;
        if (extension == AssetFileType::AnimationClipExtension) return EditorAssetKind::AnimationClip;
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".webp")
            return EditorAssetKind::Texture;
        if (extension == ".glsl" || extension == ".hlsl") return EditorAssetKind::Shader;
        if (extension == ".mp3" || extension == ".wav" || extension == ".ogg") return EditorAssetKind::Audio;
        if (extension == ".wts") return EditorAssetKind::EventScript;
        if (extension == ".lua" || extension == ".cs" || extension == ".vn") return EditorAssetKind::Script;
        if (extension == ".yaml" || extension == ".yml" || extension == ".json" || extension == ".txt")
            return EditorAssetKind::Data;
        return EditorAssetKind::Unknown;
    }

    std::string AssetRegistry::NormalizePath(const std::filesystem::path& path)
    {
        return NormalizeAssetPathString(path.generic_string());
    }

    std::filesystem::path AssetRegistry::GetRegistryPath(const std::filesystem::path& projectRoot)
    {
        return projectRoot / "assets" / ".wheatear" / "asset_registry.yaml";
    }

    void AssetRegistry::LoadRegistryCache()
    {
        m_Assets.clear();

        const std::filesystem::path registryPath = GetRegistryPath(m_ProjectRoot.empty() ? AssetPath::GetProjectRoot() : m_ProjectRoot);
        LoadRegistryCacheFile(registryPath, &m_Assets);

        WT_CORE_INFO("AssetRegistry: refreshing UI template descriptors");
        for (auto& metadata : m_Assets)
        {
            if (metadata.Kind != EditorAssetKind::UITemplate)
                continue;

            metadata.Prefab.Category = "UI Template";
            ReadUITemplateFile(m_ProjectRoot / metadata.RelativePath, &metadata.DisplayName, &metadata.Prefab);
            if (metadata.Prefab.TemplateKind.empty())
                metadata.Prefab.TemplateKind = metadata.DisplayName;
        }
        WT_CORE_INFO("AssetRegistry: UI template descriptors refreshed");

        RebuildIndexes();
    }

    void AssetRegistry::RebuildIndexes()
    {
        m_PathToIndex.clear();
        m_UUIDToIndex.clear();
        for (size_t i = 0; i < m_Assets.size(); ++i)
        {
            m_PathToIndex[m_Assets[i].RelativePath] = i;
            m_UUIDToIndex[static_cast<uint64_t>(m_Assets[i].ID)] = i;
        }
    }

    void AssetRegistry::RebuildReverseReferences()
    {
        for (auto& metadata : m_Assets)
            metadata.ReferencedBy.clear();

        for (const auto& metadata : m_Assets)
        {
            for (const std::string& reference : metadata.References)
            {
                EditorAssetMetadata* target = FindMutableByPath(reference);
                if (target)
                    target->ReferencedBy.push_back(metadata.RelativePath);
            }
        }

        for (auto& metadata : m_Assets)
            SortUnique(&metadata.ReferencedBy);
    }

    void AssetRegistry::PreserveOrCreateMetadata(EditorAssetMetadata& metadata,
        const std::unordered_map<std::string, EditorAssetMetadata>& previousByPath)
    {
        auto previous = previousByPath.find(metadata.RelativePath);
        if (previous != previousByPath.end())
        {
            const EditorAssetMetadata& stored = previous->second;
            metadata.ID = stored.ID;
            metadata.Tags = stored.Tags;
            metadata.Texture = stored.Texture;
            metadata.Audio = stored.Audio;
            metadata.Prefab = stored.Prefab;
        }

        if (static_cast<uint64_t>(metadata.ID) == 0)
        {
            metadata.ID = UUID();
            metadata.Dirty = true;
        }

        if (metadata.Kind == EditorAssetKind::UITemplate)
        {
            metadata.Prefab.Category = "UI Template";
            ReadUITemplateFile(m_ProjectRoot / metadata.RelativePath, &metadata.DisplayName, &metadata.Prefab);

            if (metadata.Prefab.TemplateKind.empty())
                metadata.Prefab.TemplateKind = metadata.DisplayName;
        }
    }

    std::vector<std::string> AssetRegistry::ExtractReferences(const std::filesystem::path& absolutePath,
        const std::filesystem::path& relativePath) const
    {
        std::vector<std::string> references;
        if (!AssetDependencyScanner::ShouldParseDependencies(relativePath))
            return references;

        std::string text;
        if (!ReadTextFile(absolutePath, &text))
            return references;

        AssetDependencyScanner::ExtractAssetReferences(text, &references);
        AssetDependencyScanner::ExtractSceneTransitionReferences(text, &references);
        references.erase(std::remove_if(references.begin(), references.end(), [](const std::string& reference)
        {
            return IsEditorOnlyOrIntermediateAsset(std::filesystem::path(reference));
        }), references.end());
        SortUnique(&references);
        return references;
    }

} // namespace Wheatear
