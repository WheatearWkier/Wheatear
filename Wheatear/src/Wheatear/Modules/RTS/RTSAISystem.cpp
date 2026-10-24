#include "wtpch.h"
#include "RTSAISystem.h"

#include "RTSComponents.h"
#include "RTSUtilities.h"

#include "Wheatear/Scene/Scene.h"

#include <glm/glm.hpp>

namespace Wheatear {

    void RTSAISystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<RTSAIControllerComponent>())
        {
            auto& ai = registry.get<RTSAIControllerComponent>(entity);
            ai.TrainTimer = 2.5f;
            ai.AttackTimer = 22.0f;
        }
    }

    void RTSAISystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        RTSMatchComponent* match = RTS::FindMatch(scene);
        if (!match || match->Result != RTSMatchResult::InProgress)
            return;

        auto& registry = scene->GetRegistry();
        auto aiView = registry.view<RTSAIControllerComponent>();
        if (aiView.empty())
            return;

        auto& ai = aiView.get<RTSAIControllerComponent>(*aiView.begin());
        const float dt = ts.GetSeconds();
        const int enemyIndex = static_cast<int>(RTSTeam::Enemy);

        match->Credits[enemyIndex] += ai.PassiveIncome * dt;

        ai.TrainTimer -= dt;
        if (ai.TrainTimer <= 0.0f)
        {
            ai.TrainTimer = RTS::EnqueueUnit(scene, RTSTeam::Enemy,
                RTSUnitType::Soldier) ? 2.8f : 0.8f;
        }

        ai.AttackTimer -= dt;
        if (ai.AttackTimer > 0.0f)
            return;

        ai.AttackTimer = 24.0f;

        glm::vec3 playerBasePosition(0.0f);
        bool foundBase = false;
        for (auto entity : registry.view<RTSUnitComponent, TransformComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(entity);
            if (unit.Team == RTSTeam::Player
                && unit.BuildingKind == RTSBuildingKind::Base
                && unit.HP > 0.0f)
            {
                playerBasePosition =
                    registry.get<TransformComponent>(entity).Translation;
                foundBase = true;
                break;
            }
        }

        if (!foundBase)
            return;

        int index = 0;
        for (auto entity : registry.view<RTSUnitComponent, TransformComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(entity);
            if (unit.Team != RTSTeam::Enemy || unit.HP <= 0.0f
                || unit.Type == RTSUnitType::Building)
            {
                continue;
            }

            const int column = index % 6;
            const int row = index / 6;
            ++index;
            const glm::vec3 offset(
                (static_cast<float>(column) - 2.5f) * 1.6f,
                0.0f,
                static_cast<float>(row) * 1.6f);
            RTS::SetDestination(scene, entity, playerBasePosition + offset);
        }
    }

} // namespace Wheatear
