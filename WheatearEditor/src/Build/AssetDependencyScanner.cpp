#include "wepch.h"
#include "AssetDependencyScanner.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <queue>
#include <regex>
#include <set>
#include <system_error>
#include <tuple>

namespace Wheatear {

    namespace {

        static bool FirstPartEquals(const std::filesystem::path& path, const char* expected)
        {
            for (const auto& part : path)
            {
                const std::string text = part.generic_string();
                if (text.empty() || text == ".")
                    continue;
                return text == expected;
            }
            return false;
        }

        static bool EndsWith(const std::string& value, const std::string& suffix)
        {
            return value.size() >= suffix.size()
                && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
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

        static std::filesystem::path ToProjectRelative(const std::filesystem::path& projectRoot,
            const std::filesystem::path& path)
        {
            if (path.empty())
                return {};

            if (!path.is_absolute())
                return std::filesystem::path(AssetDependencyScanner::NormalizeAssetReference(path.generic_string()));

            std::error_code error;
            std::filesystem::path relative = std::filesystem::relative(path, projectRoot, error);
            if (error)
                return {};
            return std::filesystem::path(AssetDependencyScanner::NormalizeAssetReference(relative.generic_string()));
        }

        static bool TryAddAsset(const std::filesystem::path& projectRoot,
            const std::filesystem::path& relativePath,
            const std::string& sourceAsset,
            std::set<std::string>* assets,
            std::queue<std::string>* parseQueue,
            std::vector<AssetReferenceRecord>* missingReferences)
        {
            if (!assets)
                return false;

            const std::string normalized = AssetDependencyScanner::NormalizeAssetReference(relativePath.generic_string());
            const std::filesystem::path normalizedPath(normalized);
            if (!AssetDependencyScanner::IsPackableAsset(normalizedPath))
                return false;

            const std::filesystem::path sourcePath = projectRoot / normalizedPath;
            if (std::filesystem::is_directory(sourcePath))
            {
                bool addedAny = false;
                std::error_code error;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(sourcePath, error))
                {
                    if (error || !entry.is_regular_file())
                        continue;

                    const std::filesystem::path childPath = std::filesystem::relative(entry.path(), projectRoot, error);
                    if (!error)
                        addedAny = TryAddAsset(projectRoot, childPath, sourceAsset, assets, parseQueue, missingReferences) || addedAny;
                }
                return addedAny;
            }

            if (!std::filesystem::is_regular_file(sourcePath))
            {
                if (missingReferences)
                    missingReferences->push_back({ sourceAsset, normalized });
                return false;
            }

            const bool inserted = assets->insert(normalized).second;
            if (inserted && parseQueue && AssetDependencyScanner::ShouldParseDependencies(normalizedPath))
                parseQueue->push(normalized);
            return inserted;
        }

        static void ExpandTemplateReference(const std::filesystem::path& projectRoot,
            const std::string& reference,
            const std::string& sourceAsset,
            std::set<std::string>* assets,
            std::queue<std::string>* parseQueue,
            std::vector<AssetReferenceRecord>* missingReferences)
        {
            const std::string normalized = AssetDependencyScanner::NormalizeAssetReference(reference);
            const size_t placeholder = normalized.find('{');
            if (placeholder == std::string::npos)
            {
                TryAddAsset(projectRoot, normalized, sourceAsset, assets, parseQueue, missingReferences);
                return;
            }

            const std::filesystem::path templatePath(normalized);
            const std::filesystem::path relativeDirectory = templatePath.parent_path();
            const std::filesystem::path sourceDirectory = projectRoot / relativeDirectory;
            if (!std::filesystem::is_directory(sourceDirectory))
            {
                if (missingReferences)
                    missingReferences->push_back({ sourceAsset, normalized });
                return;
            }

            const std::string filenameTemplate = templatePath.filename().generic_string();
            const size_t localPlaceholder = filenameTemplate.find('{');
            const size_t localPlaceholderEnd = filenameTemplate.find('}', localPlaceholder);
            if (localPlaceholder == std::string::npos || localPlaceholderEnd == std::string::npos)
                return;

            const std::string prefix = filenameTemplate.substr(0, localPlaceholder);
            const std::string suffix = filenameTemplate.substr(localPlaceholderEnd + 1);

            bool matchedAny = false;
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(sourceDirectory, error))
            {
                if (error || !entry.is_regular_file())
                    continue;

                const std::string filename = entry.path().filename().generic_string();
                if (filename.rfind(prefix, 0) != 0 || !EndsWith(filename, suffix))
                    continue;

                matchedAny = true;
                TryAddAsset(projectRoot, relativeDirectory / filename, sourceAsset, assets, parseQueue, missingReferences);
            }

            if (!matchedAny && missingReferences)
                missingReferences->push_back({ sourceAsset, normalized });
        }

        static void AddDirectoryFiles(const std::filesystem::path& projectRoot,
            const std::filesystem::path& relativeDirectory,
            std::set<std::string>* assets,
            std::queue<std::string>* parseQueue = nullptr,
            std::vector<AssetReferenceRecord>* missingReferences = nullptr)
        {
            const std::filesystem::path sourceDirectory = projectRoot / relativeDirectory;
            if (!std::filesystem::is_directory(sourceDirectory))
                return;

            std::error_code error;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDirectory, error))
            {
                if (error || !entry.is_regular_file())
                    continue;

                const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), projectRoot, error);
                if (!error)
                    TryAddAsset(projectRoot, relativePath, {}, assets, parseQueue, missingReferences);
            }
        }

        static void AddBuiltinAssets(const std::filesystem::path& projectRoot,
            bool enableScripts,
            std::set<std::string>* assets,
            std::queue<std::string>* parseQueue,
            std::vector<AssetReferenceRecord>* missingReferences)
        {
            AddDirectoryFiles(projectRoot, "assets/shaders", assets);
            AddDirectoryFiles(projectRoot, "assets/gameplay/actions", assets, parseQueue, missingReferences);
            AddDirectoryFiles(projectRoot, "assets/gameplay/progression", assets, parseQueue, missingReferences);
            TryAddAsset(projectRoot, "assets/gameplay/content_manifest.yaml", {}, assets, parseQueue, missingReferences);
            TryAddAsset(projectRoot, AssetAliasRegistry::Path("font.ui_default", "assets/fonts/wqy-microhei.ttc"), {}, assets, nullptr, nullptr);
            TryAddAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/LICENSE_Apache2.txt", {}, assets, nullptr, nullptr);
            TryAddAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/LICENSE_GPLv3.txt", {}, assets, nullptr, nullptr);
            TryAddAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/README.txt", {}, assets, nullptr, nullptr);
            TryAddAsset(projectRoot, "assets/fonts/licenses/WenQuanYiMicroHei/AUTHORS.txt", {}, assets, nullptr, nullptr);
            TryAddAsset(projectRoot, AssetAliasRegistry::Path("font.ui_fallback_sc", "assets/fonts/NotoSansSC-VF.ttf"), {}, assets, nullptr, nullptr);
            TryAddAsset(projectRoot, AssetAliasRegistry::Path("font.latin", "assets/fonts/Open-Sans-2.ttf"), {}, assets, nullptr, nullptr);

            if (enableScripts)
            {
                TryAddAsset(projectRoot, EngineInfo::ScriptCoreAssemblyPath, {}, assets, nullptr, nullptr);
                TryAddAsset(projectRoot, "assets/scripts/Wheatear-ScriptCore.deps.json", {}, assets, nullptr, nullptr);
            }
        }

        static void SortAndUnique(std::vector<std::filesystem::path>* paths)
        {
            if (!paths)
                return;

            std::sort(paths->begin(), paths->end());
            paths->erase(std::unique(paths->begin(), paths->end()), paths->end());
        }

        static void SortAndUniqueRecords(std::vector<AssetReferenceRecord>* records)
        {
            if (!records)
                return;

            std::sort(records->begin(), records->end(),
                [](const AssetReferenceRecord& left, const AssetReferenceRecord& right)
                {
                    return std::tie(left.SourceAsset, left.Reference)
                        < std::tie(right.SourceAsset, right.Reference);
                });
            records->erase(
                std::unique(records->begin(), records->end(),
                    [](const AssetReferenceRecord& left, const AssetReferenceRecord& right)
                    {
                        return left.SourceAsset == right.SourceAsset && left.Reference == right.Reference;
                    }),
                records->end());
        }

        static void AddLegacyReferenceWarnings(
            const std::string& sourceAsset,
            const std::string& text,
            std::vector<std::string>* warnings)
        {
            if (!warnings)
                return;

            if (text.find("ParentTag:") != std::string::npos)
                warnings->push_back(sourceAsset + ": legacy UI ParentTag field found; use ParentEntity UUID.");
            if (text.find("PagerTag:") != std::string::npos)
                warnings->push_back(sourceAsset + ": legacy UI PagerTag field found; use PagerEntity UUID.");

            static const std::regex targetedEventByName(R"(event:([A-Za-z_][A-Za-z0-9_]*):([A-Za-z_][A-Za-z0-9_]*))");
            if (std::regex_search(text, targetedEventByName))
                warnings->push_back(sourceAsset + ": legacy targeted event by name found; use event:name or event:@UUID:name.");

            static const std::regex pagerByName(R"(ui:pager:([A-Za-z_][A-Za-z0-9_]*):)");
            if (std::regex_search(text, pagerByName))
                warnings->push_back(sourceAsset + ": legacy UI pager command by name found; use ui:pager:@UUID:action.");
        }

    } // namespace

    AssetDependencyReport AssetDependencyScanner::BuildReport(const AssetDependencyScanOptions& options)
    {
        AssetDependencyReport report;
        const std::filesystem::path projectRoot = options.ProjectRoot.empty()
            ? AssetPath::GetProjectRoot()
            : options.ProjectRoot;

        std::set<std::string> assets;
        std::queue<std::string> parseQueue;
        const std::filesystem::path startupAsset = ToProjectRelative(projectRoot, options.StartupAsset);

        if (startupAsset.empty())
        {
            report.Warnings.push_back("Startup asset is empty or outside the project root.");
        }
        else
        {
            TryAddAsset(projectRoot, startupAsset, {}, &assets, &parseQueue, &report.MissingReferences);
        }

        if (options.IncludeBuiltinAssets)
            AddBuiltinAssets(projectRoot, options.EnableScripts, &assets, &parseQueue, &report.MissingReferences);

        while (!parseQueue.empty())
        {
            const std::string current = parseQueue.front();
            parseQueue.pop();

            std::string text;
            if (!ReadTextFile(projectRoot / current, &text))
            {
                report.Warnings.push_back("Could not parse dependency text asset: " + current);
                continue;
            }

            report.ParsedTextAssets.emplace_back(current);
            AddLegacyReferenceWarnings(current, text, &report.Warnings);
            std::vector<std::string> references;
            ExtractAssetReferences(text, &references);
            for (const std::string& reference : references)
                ExpandTemplateReference(projectRoot, reference, current, &assets, &parseQueue, &report.MissingReferences);

            std::vector<std::string> sceneTransitions;
            ExtractSceneTransitionReferences(text, &sceneTransitions);
            for (const std::string& sceneTransition : sceneTransitions)
            {
                const std::string normalized = NormalizeAssetReference(sceneTransition);
                if (normalized.empty())
                    continue;

                report.SceneTransitions.push_back({ current, normalized });
                const std::filesystem::path transitionPath(normalized);
                if (transitionPath.extension() != AssetFileType::SceneExtension)
                    report.Warnings.push_back(current + ": scene transition does not target a .wt scene: " + normalized);

                if (!std::filesystem::is_regular_file(projectRoot / transitionPath))
                    report.MissingSceneTransitions.push_back({ current, normalized });

                ExpandTemplateReference(projectRoot, normalized, current, &assets, &parseQueue, &report.MissingReferences);
            }
        }

        report.IncludedAssets.reserve(assets.size());
        for (const std::string& asset : assets)
        {
            report.IncludedAssets.emplace_back(asset);
            std::error_code error;
            report.IncludedBytes += std::filesystem::file_size(projectRoot / asset, error);
        }

        if (options.IncludeUnusedAssets)
        {
            const std::filesystem::path assetRoot = projectRoot / "assets";
            std::error_code error;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(assetRoot, error))
            {
                if (error || !entry.is_regular_file())
                    continue;

                const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), projectRoot, error);
                if (error || !IsPackableAsset(relativePath))
                    continue;

                ++report.PackableAssetCount;
                report.PackableBytes += std::filesystem::file_size(entry.path(), error);
                const std::string normalized = NormalizeAssetReference(relativePath.generic_string());
                if (!assets.count(normalized))
                    report.UnusedAssets.emplace_back(normalized);
            }
        }

        SortAndUnique(&report.IncludedAssets);
        SortAndUnique(&report.ParsedTextAssets);
        SortAndUnique(&report.UnusedAssets);
        std::sort(report.Warnings.begin(), report.Warnings.end());
        report.Warnings.erase(std::unique(report.Warnings.begin(), report.Warnings.end()), report.Warnings.end());
        SortAndUniqueRecords(&report.SceneTransitions);
        SortAndUniqueRecords(&report.MissingReferences);
        SortAndUniqueRecords(&report.MissingSceneTransitions);

        return report;
    }

    std::string AssetDependencyScanner::NormalizeAssetReference(std::string reference)
    {
        std::replace(reference.begin(), reference.end(), '\\', '/');
        while (!reference.empty()
            && (reference.back() == '.' || reference.back() == ',' || reference.back() == ';' || reference.back() == '"' || reference.back() == '\''))
        {
            reference.pop_back();
        }
        return std::filesystem::path(reference).lexically_normal().generic_string();
    }

    bool AssetDependencyScanner::IsPackableAsset(const std::filesystem::path& relativePath)
    {
        if (relativePath.empty() || relativePath.is_absolute())
            return false;

        if (!FirstPartEquals(relativePath, "assets"))
            return false;

        auto it = relativePath.begin();
        if (it == relativePath.end())
            return false;
        ++it;

        if (it != relativePath.end())
        {
            const std::string secondPart = it->generic_string();
            if (secondPart == ".wheatear")
                return false;
            if (secondPart == "archive" || secondPart == "cache" || secondPart == "saves")
                return false;
            if (secondPart == "game" && relativePath.filename() == "player.config")
                return false;
        }

        const std::string extension = relativePath.extension().generic_string();
        if (extension == AssetFileType::MetadataExtension || extension == AssetFileType::UITemplateExtension)
            return false;
        if (extension == ".rar" || extension == ".zip" || extension == ".7z")
            return false;

        for (const auto& part : relativePath)
        {
            const std::string partText = part.generic_string();
            if (partText == "..")
                return false;
            if (partText == "previews" || partText == "source_frames")
                return false;
            if (partText.rfind("_backup_before_", 0) == 0)
                return false;
        }

        return true;
    }

    bool AssetDependencyScanner::ShouldParseDependencies(const std::filesystem::path& relativePath)
    {
        const std::string extension = relativePath.extension().generic_string();
        return extension == AssetFileType::SceneExtension
            || extension == AssetFileType::PrefabExtension
            || extension == AssetFileType::MaterialExtension
            || extension == AssetFileType::AnimationClipExtension
            || extension == ".vn"
            || extension == ".wts"
            || extension == ".yaml"
            || extension == ".yml"
            || extension == ".json";
    }

    void AssetDependencyScanner::ExtractAssetReferences(const std::string& text,
        std::vector<std::string>* references)
    {
        if (!references)
            return;

        static const std::regex assetRegex(R"(assets[\\/][A-Za-z0-9_\-./\\{}]+)");
        for (std::sregex_iterator it(text.begin(), text.end(), assetRegex), end; it != end; ++it)
        {
            const std::string reference = NormalizeAssetReference(it->str());
            if (!reference.empty())
                references->push_back(reference);
        }
    }

    void AssetDependencyScanner::ExtractSceneTransitionReferences(const std::string& text,
        std::vector<std::string>* references)
    {
        if (!references)
            return;

        static const std::regex sceneRegex(R"(\b(scene|newgame|loadgame):\s*(assets[\\/][A-Za-z0-9_\-./\\{}]+))");
        for (std::sregex_iterator it(text.begin(), text.end(), sceneRegex), end; it != end; ++it)
        {
            const std::string reference = NormalizeAssetReference((*it)[2].str());
            if (!reference.empty())
                references->push_back(reference);
        }
    }

} // namespace Wheatear
