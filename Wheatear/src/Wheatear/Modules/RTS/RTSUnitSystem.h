#pragma once

#include "RTSComponents.h"

#include "Wheatear/Core/Core.h"
#include "Wheatear/Systems/ISystem.h"

#include <glm/glm.hpp>

namespace Wheatear {

    class Mesh;
    class Scene;

    class RTSUnitSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;

        // Used by production queues and debug scripts.
        entt::entity SpawnUnit(Scene* scene, RTSTeam team, const glm::vec3& position,
            RTSUnitType type = RTSUnitType::Soldier);

    private:
        void SpawnWave(Scene* scene, int count);
        void UpdateWaves(Scene* scene, RTSMatchComponent& match, float dt);
        void UpdateUnits(Scene* scene, float dt);
        void UpdateCounts(Scene* scene, RTSMatchComponent& match);
        int CountUnits(Scene* scene, RTSTeam team) const;

        Ref<Mesh> m_PlayerMesh;
        Ref<Mesh> m_EnemyMesh;
        Ref<Mesh> m_HarvesterMesh;
    };

} // namespace Wheatear
