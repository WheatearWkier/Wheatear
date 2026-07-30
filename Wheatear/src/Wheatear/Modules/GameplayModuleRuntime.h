#pragma once

#include "Wheatear/Modules/GameplayModuleComponents.h"
#include "Wheatear/Scene/Scene.h"

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

    inline void ApplyVisualNovelAutoLoadSlot(Scene* scene, int slot)
    {
        if (!scene || slot <= 0)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<VisualNovelComponent>())
            registry.get<VisualNovelComponent>(entity).AutoLoadSlot = slot;
    }

} // namespace Wheatear
