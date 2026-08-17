#include "wtpch.h"
#include "TacticalCombatSystem.h"

#include "TacticalCombatCommandService.h"
#include "TacticalCombatComponents.h"
#include "TacticalCombatFlowService.h"
#include "TacticalCombatTuningService.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Scene.h"

#include <string>
#include <vector>

namespace Wheatear {

    void TacticalCombatSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<TacticalCombatLevelComponent>())
        {
            auto& level = registry.get<TacticalCombatLevelComponent>(entity);

            // The tuning unit table is the authoritative data source; scene
            // component values act as per-scene fallbacks for unmatched tags.
            const auto& tuning = TacticalCombatTuningService::GetTuning(level);
            TacticalCombatTuningService::ApplyUnitTuningToScene(scene, tuning);
            TacticalCombatTuningService::ApplyLevelTuning(tuning, level);

            if (level.PlayOnStart)
                TacticalCombatFlowService::ResetLevel(scene, level);
        }
    }

    void TacticalCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        auto view = registry.view<TacticalCombatLevelComponent>();

        const std::vector<std::string> commands = CommandBus::DrainGameplayCommands("tactic:");
        for (const std::string& command : commands)
        {
            for (auto entity : view)
                TacticalCombatCommandService::ProcessCommand(scene, registry.get<TacticalCombatLevelComponent>(entity), command);
        }

        for (auto entity : view)
            TacticalCombatFlowService::UpdateLevel(scene, registry.get<TacticalCombatLevelComponent>(entity), ts.GetSeconds());
    }

} // namespace Wheatear
