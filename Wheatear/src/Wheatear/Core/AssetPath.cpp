#include "wtpch.h"
#include "AssetPath.h"

#include "Wheatear/Core/FileSystem.h"
#include "Wheatear/Utils/StringUtils.h"

#include <system_error>
#include <vector>

namespace Wheatear {

    using Wheatear::StringUtils::ToLower;

    namespace {

        struct AssetPathState
        {
            std::filesystem::path ProjectRoot;
            std::filesystem::path AssetDirectoryName = "assets";
            bool Initialized = false;
        };

        static AssetPathState s_State;

        static bool FirstPartEquals(const std::filesystem::path& path, const char* expected)
        {
            for (const auto& part : path)
            {
                const std::string text = part.string();
                if (text == "." || text.empty())
                    continue;
                return ToLower(text) == expected;
            }
            return false;
        }

        static bool HasUsableAssetRoot(const std::filesystem::path& projectRoot)
        {
            const std::filesystem::path assets = projectRoot / "assets";
            if (!std::filesystem::is_directory(assets))
                return false;

            if (std::filesystem::exists(assets / "shaders" / "Renderer2D_Quad.glsl"))
                return true;
            if (std::filesystem::exists(assets / "scenes"))
                return true;
            if (std::filesystem::exists(assets / "vn"))
                return true;

            return false;
        }

        static std::filesystem::path FirstExistingOrFallback(
            const std::vector<std::filesystem::path>& candidates)
        {
            for (const auto& candidate : candidates)
            {
                if (std::filesystem::exists(candidate))
                    return FileSystem::Normalize(candidate);
            }

            return candidates.empty() ? std::filesystem::path{} : candidates.front().lexically_normal();
        }

        static std::filesystem::path FindLooseRuntimeData(const std::filesystem::path& path)
        {
            if (path.empty() || path.is_absolute() || !FirstPartEquals(path, "assets"))
                return {};

            std::error_code error;
            std::filesystem::path cursor = std::filesystem::current_path(error);
            while (!error && !cursor.empty())
            {
                const std::filesystem::path candidate = (cursor / path).lexically_normal();
                if (std::filesystem::exists(candidate, error) && !error)
                    return FileSystem::Normalize(candidate);

                error.clear();
                const std::filesystem::path parent = cursor.parent_path();
                if (parent == cursor)
                    break;
                cursor = parent;
            }

            return {};
        }

        static void EnsureInitialized()
        {
            if (s_State.Initialized)
                return;

            s_State.ProjectRoot = AssetPath::DiscoverProjectRoot();
            s_State.Initialized = true;
        }

    } // namespace

    std::filesystem::path AssetPath::DiscoverProjectRoot(const std::filesystem::path& start)
    {
        std::filesystem::path cursor = start.empty()
            ? std::filesystem::current_path()
            : start;
        cursor = FileSystem::Normalize(cursor);

        while (!cursor.empty())
        {
            if (HasUsableAssetRoot(cursor))
                return cursor;

            const std::filesystem::path editorProject = cursor / "WheatearEditor";
            if (HasUsableAssetRoot(editorProject))
                return editorProject;

            const std::filesystem::path runtimeProject = cursor / "WheatearSandbox";
            if (HasUsableAssetRoot(runtimeProject))
                return runtimeProject;

            const std::filesystem::path parent = cursor.parent_path();
            if (parent == cursor)
                break;
            cursor = parent;
        }

        return FileSystem::Normalize(std::filesystem::current_path());
    }

    void AssetPath::SetProjectRoot(const std::filesystem::path& projectRoot)
    {
        s_State.ProjectRoot = FileSystem::Normalize(projectRoot);
        s_State.Initialized = true;
    }

    void AssetPath::SetAssetDirectoryName(const std::filesystem::path& directoryName)
    {
        s_State.AssetDirectoryName = directoryName.empty()
            ? std::filesystem::path("assets")
            : directoryName;
    }

    const std::filesystem::path& AssetPath::GetProjectRoot()
    {
        EnsureInitialized();
        return s_State.ProjectRoot;
    }

    std::filesystem::path AssetPath::GetAssetRoot()
    {
        return GetProjectRoot() / s_State.AssetDirectoryName;
    }

    std::filesystem::path AssetPath::GetResourceRoot()
    {
        return GetProjectRoot() / "Resources";
    }

    std::filesystem::path AssetPath::Resolve(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        if (path.is_absolute())
            return std::filesystem::exists(path) ? FileSystem::Normalize(path) : path.lexically_normal();

        const std::filesystem::path projectRoot = GetProjectRoot();
        const bool startsWithAssets = FirstPartEquals(path, "assets");
        const bool startsWithResources = FirstPartEquals(path, "resources");
        const bool startsWithMono = FirstPartEquals(path, "mono");

        std::vector<std::filesystem::path> candidates;
        if (startsWithAssets || startsWithResources || startsWithMono)
            candidates.push_back(projectRoot / path);

        if (!startsWithAssets && !startsWithResources && !startsWithMono)
            candidates.push_back(GetAssetRoot() / path);

        candidates.push_back(projectRoot / path);
        candidates.push_back(std::filesystem::current_path() / path);

        return FirstExistingOrFallback(candidates);
    }

    std::filesystem::path AssetPath::ResolveRuntimeData(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        if (path.is_absolute())
            return std::filesystem::exists(path) ? FileSystem::Normalize(path) : path.lexically_normal();

        const std::filesystem::path loosePath = FindLooseRuntimeData(path);
        if (!loosePath.empty())
            return loosePath;

        return Resolve(path);
    }

    std::filesystem::path AssetPath::ResolveAsset(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        if (path.is_absolute())
            return std::filesystem::exists(path) ? FileSystem::Normalize(path) : path.lexically_normal();

        if (FirstPartEquals(path, "assets"))
            return Resolve(path);

        return FirstExistingOrFallback({
            GetAssetRoot() / path,
            GetProjectRoot() / path,
            std::filesystem::current_path() / path
        });
    }

    std::filesystem::path AssetPath::ResolveResource(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        if (path.is_absolute())
            return std::filesystem::exists(path) ? FileSystem::Normalize(path) : path.lexically_normal();

        if (FirstPartEquals(path, "resources"))
            return Resolve(path);

        return FirstExistingOrFallback({
            GetResourceRoot() / path,
            GetProjectRoot() / path,
            std::filesystem::current_path() / path
        });
    }

    std::filesystem::path AssetPath::ToProjectRelative(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        if (!path.is_absolute())
        {
            if (FirstPartEquals(path, "assets")
                || FirstPartEquals(path, "resources")
                || FirstPartEquals(path, "mono"))
            {
                return path.lexically_normal();
            }

            const std::filesystem::path assetPath = GetAssetRoot() / path;
            if (std::filesystem::exists(assetPath))
                return std::filesystem::relative(assetPath, GetProjectRoot()).lexically_normal();

            return path.lexically_normal();
        }

        const std::filesystem::path root = GetProjectRoot();
        if (!FileSystem::IsSubPath(path, root))
            return path.lexically_normal();

        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(FileSystem::Normalize(path), root, error);
        return error ? path.lexically_normal() : relative.lexically_normal();
    }

} // namespace Wheatear
