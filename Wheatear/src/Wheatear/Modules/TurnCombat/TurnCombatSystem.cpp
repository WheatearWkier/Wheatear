#include "wtpch.h"
#include "TurnCombatSystem.h"

#include "TurnCombatCommandService.h"
#include "TurnCombatComponents.h"
#include "TurnCombatFlowService.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Scene.h"

#include <string>
#include <vector>

namespace Wheatear {

    void TurnCombatSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<TurnCombatLevelComponent>())
        {
            auto& level = registry.get<TurnCombatLevelComponent>(entity);
            if (level.PlayOnStart)
                TurnCombatFlowService::ResetLevel(scene, level);
        }
    }

    void TurnCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        auto view = registry.view<TurnCombatLevelComponent>();

        const std::vector<std::string> commands = CommandBus::DrainGameplayCommands("turn:");
        for (const std::string& command : commands)
        {
            for (auto entity : view)
                TurnCombatCommandService::ProcessCommand(scene, registry.get<TurnCombatLevelComponent>(entity), command);
        }

        for (auto entity : view)
            TurnCombatFlowService::UpdateLevel(scene, registry.get<TurnCombatLevelComponent>(entity), ts.GetSeconds());
    }

} // namespace Wheatear
