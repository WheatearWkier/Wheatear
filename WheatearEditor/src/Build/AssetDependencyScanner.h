#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    struct AssetReferenceRecord
    {
        std::string SourceAsset;
        std::string Reference;
    };

    struct AssetDependencyScanOptions
    {
        std::filesystem::path ProjectRoot;
        // Root that provides engine built-ins (shaders / fonts / gameplay
        // recipes). Defaults to ProjectRoot; multi-project setups pass the
        // engine root here.
        std::filesystem::path BuiltinRoot;
        std::filesystem::path StartupAsset;
        bool IncludeBuiltinAssets = true;
        bool IncludeUnusedAssets = true;
    };

    struct AssetDependencyReport
    {
        std::vector<std::filesystem::path> IncludedAssets;
        std::vector<AssetReferenceRecord> SceneTransitions;
        std::vector<AssetReferenceRecord> MissingReferences;
        std::vector<AssetReferenceRecord> MissingSceneTransitions;
        std::vector<std::filesystem::path> UnusedAssets;
        std::vector<std::filesystem::path> ParsedTextAssets;
        std::vector<std::string> Warnings;
        uintmax_t IncludedBytes = 0;
        uintmax_t PackableBytes = 0;
        size_t PackableAssetCount = 0;
    };

    class AssetDependencyScanner
    {
    public:
        static AssetDependencyReport BuildReport(const AssetDependencyScanOptions& options);
        static std::string NormalizeAssetReference(std::string reference);
        static bool IsPackableAsset(const std::filesystem::path& relativePath);
        static bool ShouldParseDependencies(const std::filesystem::path& relativePath);
        static void ExtractAssetReferences(const std::string& text, std::vector<std::string>* references);
        static void ExtractSceneTransitionReferences(const std::string& text, std::vector<std::string>* references);
    };

} // namespace Wheatear
