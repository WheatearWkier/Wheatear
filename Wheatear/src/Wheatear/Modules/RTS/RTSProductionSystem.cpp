#include "wtpch.h"
#include "RTSProductionSystem.h"

#include "RTSComponents.h"
#include "RTSUnitSystem.h"
#include "RTSUtilities.h"

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <cmath>

namespace Wheatear {

    void RTSProductionSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        RTSUnitSystem* unitSystem = scene->GetSystem<RTSUnitSystem>();
        if (!unitSystem)
            return;

        auto& registry = scene->GetRegistry();
        const float dt = ts.GetSeconds();

        for (auto entity : registry.view<RTSProductionComponent, TransformComponent>())
        {
            auto& production = registry.get<RTSProductionComponent>(entity);
            auto& transform = registry.get<TransformComponent>(entity);
            auto* unit = registry.try_get<RTSUnitComponent>(entity);

            if (!production.Constructed)
            {
                production.ConstructionProgress = std::min(1.0f,
                    production.ConstructionProgress
                    + dt / std::max(production.ConstructionTime, 0.01f));

                if (unit)
                {
                    const float t = production.ConstructionProgress;
                    transform.Scale = unit->VisualScale
                        * glm::mix(0.2f, 1.0f, t);
                }

                if (production.ConstructionProgress >= 1.0f)
                {
                    production.Constructed = true;
                    if (unit)
                        transform.Scale = unit->VisualScale;
                    WT_CORE_INFO("RTS construction complete: team {}",
                        static_cast<int>(production.Team));
                }
                continue;
            }

            if (production.Queue.empty())
                continue;

            RTSProductionOrder& order = production.Queue.front();
            order.Remaining -= dt;
            if (order.Remaining > 0.0f)
                continue;

            const RTSUnitType type = order.Type;
            production.Queue.erase(production.Queue.begin());

            glm::vec3 spawnPosition = transform.Translation;
            glm::vec3 direction = production.RallyPoint - transform.Translation;
            direction.y = 0.0f;
            if (glm::length(direction) < 0.1f)
                direction = glm::vec3(0.0f, 0.0f, 1.0f);
            direction = glm::normalize(direction);
            spawnPosition += direction * (transform.Scale.x * 0.5f + 2.0f);

            WT_CORE_INFO("RTS production complete: team {} type {}",
                static_cast<int>(production.Team), static_cast<int>(type));
            const entt::entity spawned =
                unitSystem->SpawnUnit(scene, production.Team, spawnPosition, type);
            if (spawned != entt::null)
                RTS::SetDestination(scene, spawned, production.RallyPoint);
        }
    }

} // namespace Wheatear
