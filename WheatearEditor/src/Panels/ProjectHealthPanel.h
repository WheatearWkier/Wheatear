#pragma once

#include "Build/AssetDependencyScanner.h"
#include "Build/ProjectSourceScanner.h"
#include "Editor/EditorToolRegistry.h"
#include "Editor/YamlAssetDocument.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    struct AssetDuplicateGroup
    {
        std::string Hash;
        uintmax_t SizeBytes = 0;
        std::vector<std::filesystem::path> Assets;
    };

    struct AssetSizeRecord
    {
        std::filesystem::path Asset;
        uintmax_t SizeBytes = 0;
        bool Packed = false;
        bool Unused = false;
    };

    struct AssetAliasIssue
    {
        std::string Alias;
        std::string Target;
        std::string Issue;
    };

    struct HardcodedAssetPathRecord
    {
        std::filesystem::path SourceFile;
        std::string Reference;
    };

    struct AssetHygieneReport
    {
        std::vector<AssetDuplicateGroup> DuplicateGroups;
        std::vector<AssetSizeRecord> LargestAssets;
        std::vector<AssetAliasIssue> AliasIssues;
        std::vector<HardcodedAssetPathRecord> HardcodedAssetPaths;
        size_t DuplicateExtraAssetCount = 0;
        uintmax_t DuplicateExtraBytes = 0;
    };

    enum class AssetHygieneActionType
    {
        ArchiveUnusedAsset = 0,
        ArchiveDuplicateAsset,
        ReviewDuplicateAsset,
        ReplacePathWithAlias
    };

    struct AssetHygieneAction
    {
        AssetHygieneActionType Type = AssetHygieneActionType::ArchiveUnusedAsset;
        bool Selected = false;
        bool CanApply = true;
        std::string Title;
        std::string Detail;
        std::filesystem::path SourceAsset;
        std::filesystem::path DestinationAsset;
        std::filesystem::path SourceFile;
        std::string SearchText;
        std::string ReplacementText;
        uintmax_t SizeBytes = 0;
    };

    class ProjectHealthPanel
    {
    public:
        void Open(const EditorToolContext& context);
        void OnImGuiRender();

    private:
        void Refresh();
        void DrawSummary() const;
        void DrawAssetRegistry() const;
        void DrawMissingReferences() const;
        void DrawSceneTransitions() const;
        void DrawEntityBindings() const;
        void ScanEntityBindings();
        void DrawAssetHygiene();
        void DrawHygieneCleanupPlan();
        void DrawAliasManifestEditor();
        void DrawSourceSync() const;
        void LoadAliasManifest();
        void SaveAliasManifest();
        void SaveStartupScene();
        void BuildHygieneCleanupPlan();
        bool ApplySelectedHygieneActions();
        void DrawAssetList(const char* tableId,
            const std::vector<std::filesystem::path>& assets,
            size_t maxRows = 500) const;

    private:
        bool m_Open = false;
        bool m_IncludeUnusedAssets = true;
        std::string m_StartupScene;
        std::string m_AliasManifestPath = "assets/gameplay/content_manifest.yaml";
        std::string m_SelectedAlias;
        std::string m_NewAliasName;
        std::string m_NewAliasTarget = "assets/";
        AssetDependencyReport m_Report;
        ProjectSourceReport m_SourceReport;
        AssetHygieneReport m_HygieneReport;
        std::vector<AssetHygieneAction> m_HygieneActions;
        // Dangling entity-name / @UUID bindings found inside scene files
        // (Reference = bound value, SourceAsset = scene :: component.field).
        std::vector<AssetReferenceRecord> m_MissingEntityBindings;
        EditorDocuments::YamlAssetDocument m_AliasManifestDocument;
        std::string m_Status;
        std::string m_AliasStatus;
        std::string m_HygieneActionStatus;
    };

} // namespace Wheatear
