#pragma once

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"
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
        static std::filesystem::file_time_type s_LastWrite{};

        const auto now = std::chrono::steady_clock::now();
        if (now - s_LastCheck < std::chrono::milliseconds(500))
            return;
        s_LastCheck = now;

        const std::filesystem::path manifestPath = AssetAliasRegistry::Path(
            "wao.action_sets", "assets/gameplay/actions/action_sets.yaml");
        std::error_code error;
        const auto writeTime = std::filesystem::exists(manifestPath, error)
            ? std::filesystem::last_write_time(manifestPath, error)
            : std::filesystem::file_time_type{};
        if (writeTime == s_LastWrite)
            return;
        s_LastWrite = writeTime;

        size_t count = WAO::ActionAssetLoader::ReloadManifest(manifestPath);
        if (count == 0)
        {
            WAO::ActionAssetLoader::ReloadDirectory(AssetAliasRegistry::Path(
                "wao.action_directory", "assets/gameplay/actions"));
        }
        WT_CORE_INFO("WAO: hot-reloaded action recipes ({} recipes)",
            count ? count : WAO::ActionDatabase::All().size());
    }

} // namespace Wheatear
