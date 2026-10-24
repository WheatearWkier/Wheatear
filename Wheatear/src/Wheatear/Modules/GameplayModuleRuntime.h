#pragma once

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
#include "Wheatear/Gameplay/Behavior/BehaviorAssetLoader.h"
#include "Wheatear/Gameplay/Behavior/BehaviorDatabase.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Modules/GameplayModuleComponents.h"
#include "Wheatear/Scene/Scene.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    namespace Detail {

        template<typename T>
        void DrainRuntimeCommandsFromComponent(Scene* scene, std::vector<std::string>& commands)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<T>())
            {
                auto& component = registry.get<T>(entity);
                if (!component.RuntimeRequestedCommand.empty())
                {
                    commands.push_back(component.RuntimeRequestedCommand);
                    component.RuntimeRequestedCommand.clear();
                }
            }
        }

        template<typename... Components>
        void DrainRuntimeCommandsFromComponents(
            ComponentGroup<Components...>,
            Scene* scene,
            std::vector<std::string>& commands)
        {
            (DrainRuntimeCommandsFromComponent<Components>(scene, commands), ...);
        }

    } // namespace Detail

    inline void DrainGameplayRuntimeCommands(Scene* scene, std::vector<std::string>& commands)
    {
        Detail::DrainRuntimeCommandsFromComponents(
            GameplayRuntimeCommandComponents{},
            scene,
            commands);
    }

    inline std::vector<std::string> DrainGameplayRuntimeCommands(Scene* scene)
    {
        std::vector<std::string> commands;
        DrainGameplayRuntimeCommands(scene, commands);
        return commands;
    }

    // Hot-reloads WAO action recipes while a scene is playing: polls the
    // action_sets manifest (500ms throttle) and reloads on change, so tuning
    // recipes in the editor takes effect without restarting play. Call once
    // per frame from the runtime update path.
    inline void UpdateActionHotReload()
    {
        static auto s_LastCheck = std::chrono::steady_clock::now();
        static uint64_t s_LastStamp = 0;

        const auto now = std::chrono::steady_clock::now();
        if (now - s_LastCheck < std::chrono::milliseconds(500))
            return;
        s_LastCheck = now;

        const std::filesystem::path manifestPath = AssetAliasRegistry::Path(
            "wao.action_sets", "assets/gameplay/actions/action_sets.yaml");
        const std::filesystem::path directory = AssetAliasRegistry::Path(
            "wao.action_directory", "assets/gameplay/actions");
        std::error_code error;
        uint64_t stamp = 1469598103934665603ull;
        const auto addStamp = [&stamp](const std::filesystem::path& file)
        {
            std::error_code fileError;
            const uintmax_t size = std::filesystem::file_size(file, fileError);
            const auto writeTime = std::filesystem::last_write_time(file, fileError)
                .time_since_epoch().count();
            const std::string key = file.generic_string() + ":" +
                std::to_string(size) + ":" + std::to_string(writeTime);
            for (unsigned char character : key)
            {
                stamp ^= character;
                stamp *= 1099511628211ull;
            }
        };
        if (std::filesystem::is_regular_file(manifestPath, error))
            addStamp(manifestPath);
        if (std::filesystem::is_directory(directory, error))
        {
            std::vector<std::filesystem::path> files;
            for (const auto& entry :
                std::filesystem::recursive_directory_iterator(directory, error))
            {
                if (error || !entry.is_regular_file())
                    continue;
                const std::string extension = entry.path().extension().generic_string();
                if (extension == ".yaml" || extension == ".yml")
                    files.push_back(entry.path());
            }
            std::sort(files.begin(), files.end());
            for (const auto& file : files)
                addStamp(file);
        }

        if (stamp == s_LastStamp)
            return;
        s_LastStamp = stamp;

        size_t count = WAO::ActionAssetLoader::ReloadManifest(manifestPath);
        if (count == 0)
        {
            WAO::ActionAssetLoader::ReloadDirectory(AssetAliasRegistry::Path(
                "wao.action_directory", "assets/gameplay/actions"));
        }
        WT_CORE_INFO("WAO: hot-reloaded action recipes ({} recipes)",
            count ? count : WAO::ActionDatabase::All().size());
    }

    inline void UpdateBehaviorHotReload()
    {
        static auto s_LastCheck = std::chrono::steady_clock::now();
        static uint64_t s_LastStamp = 0;

        const auto now = std::chrono::steady_clock::now();
        if (now - s_LastCheck < std::chrono::milliseconds(500))
            return;
        s_LastCheck = now;

        const std::filesystem::path directory = AssetAliasRegistry::Path(
            "wao.behavior_directory",
            "assets/gameplay/behaviors");
        std::error_code error;
        uint64_t stamp = 1469598103934665603ull;
        if (std::filesystem::is_directory(directory, error))
        {
            std::vector<std::filesystem::path> files;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, error))
            {
                if (error || !entry.is_regular_file() || entry.path().extension() != ".wtb")
                    continue;
                files.push_back(entry.path());
            }
            std::sort(files.begin(), files.end());
            for (const auto& file : files)
            {
                std::error_code fileError;
                const uintmax_t size = std::filesystem::file_size(file, fileError);
                const auto writeTime = std::filesystem::last_write_time(file, fileError)
                    .time_since_epoch().count();
                const std::string key = file.generic_string() + ":" +
                    std::to_string(size) + ":" + std::to_string(writeTime);
                for (unsigned char character : key)
                {
                    stamp ^= character;
                    stamp *= 1099511628211ull;
                }
            }
        }

        if (stamp == s_LastStamp)
            return;
        s_LastStamp = stamp;

        const size_t count = WAO::BehaviorAssetLoader::ReloadDirectory(directory);
        WT_CORE_INFO("WAO: hot-reloaded behavior programs ({} entries, revision {})",
            count,
            WAO::BehaviorDatabase::Revision());
    }

} // namespace Wheatear
