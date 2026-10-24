#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Navigation/NavGrid.h"
#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Renderer/Mesh.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace Wheatear {

    // Heightfield terrain for the 3D greybox. Heights are stored per grid
    // vertex in world units. TerrainSystem turns them into a renderable mesh
    // and NavigationSystem uses the same data for the nav grid.
    struct TerrainComponent
    {
        int CellsX = 60;
        int CellsZ = 60;
        float CellSize = 1.0f;
        float HeightScale = 1.0f;
        uint32_t Seed = 1337;

        std::vector<float> Heights;
        Ref<Mesh> Mesh;
        Ref<Material> Material;
        bool Dirty = true;

        int GetVertexCountX() const { return CellsX + 1; }
        int GetVertexCountZ() const { return CellsZ + 1; }

        void GenerateProcedural()
        {
            Heights.assign(static_cast<size_t>(CellsX + 1)
                * static_cast<size_t>(CellsZ + 1), 0.0f);

            for (int z = 0; z <= CellsZ; ++z)
            {
                for (int x = 0; x <= CellsX; ++x)
                {
                    const float worldX = (static_cast<float>(x) - CellsX * 0.5f)
                        * CellSize;
                    const float worldZ = (static_cast<float>(z) - CellsZ * 0.5f)
                        * CellSize;

                    const float hillA = 0.85f * std::exp(
                        -((worldX - 10.0f) * (worldX - 10.0f)
                          + (worldZ + 6.0f) * (worldZ + 6.0f)) / 80.0f);
                    const float hillB = 0.60f * std::exp(
                        -((worldX + 12.0f) * (worldX + 12.0f)
                          + (worldZ - 10.0f) * (worldZ - 10.0f)) / 65.0f);
                    const float ripple = 0.16f
                        * std::sin(worldX * 0.33f)
                        * std::cos(worldZ * 0.27f);

                    const float distance = std::sqrt(worldX * worldX + worldZ * worldZ);
                    const float flatten = glm::clamp((distance - 4.0f) / 9.0f,
                        0.0f, 1.0f);
                    Heights[static_cast<size_t>(z) * (CellsX + 1) + x] =
                        (hillA + hillB + ripple) * HeightScale * flatten;
                }
            }
            Dirty = true;
        }

        float SampleHeight(float worldX, float worldZ) const
        {
            if (Heights.empty() || CellsX <= 0 || CellsZ <= 0)
                return 0.0f;

            const float gridX = worldX / CellSize + CellsX * 0.5f;
            const float gridZ = worldZ / CellSize + CellsZ * 0.5f;
            const int x0 = glm::clamp(static_cast<int>(std::floor(gridX)), 0, CellsX);
            const int z0 = glm::clamp(static_cast<int>(std::floor(gridZ)), 0, CellsZ);
            const int x1 = std::min(x0 + 1, CellsX);
            const int z1 = std::min(z0 + 1, CellsZ);
            const float fx = glm::clamp(gridX - static_cast<float>(x0), 0.0f, 1.0f);
            const float fz = glm::clamp(gridZ - static_cast<float>(z0), 0.0f, 1.0f);

            const auto at = [&](int x, int z) -> float
            {
                return Heights[static_cast<size_t>(z) * (CellsX + 1) + x];
            };
            const float top = glm::mix(at(x0, z0), at(x1, z0), fx);
            const float bottom = glm::mix(at(x0, z1), at(x1, z1), fx);
            return glm::mix(top, bottom, fz);
        }

        glm::vec3 GetWorldMinimum() const
        {
            return glm::vec3(-CellsX * CellSize * 0.5f, 0.0f,
                -CellsZ * CellSize * 0.5f);
        }

        TerrainComponent() = default;
        TerrainComponent(const TerrainComponent&) = default;
    };

    // Marks a mesh as a navigation blocker. Physical collision can be added
    // later; the nav grid only needs the entity's world AABB.
    struct NavObstacleComponent
    {
        bool Enabled = true;

        NavObstacleComponent() = default;
        NavObstacleComponent(const NavObstacleComponent&) = default;
    };

    // One per scene (attached to the RTS controller) and rebuilt from terrain
    // plus NavObstacleComponent entities on runtime start.
    struct NavGridComponent
    {
        Navigation::NavGrid Grid;
        bool Built = false;
        bool DebugVisible = false;

        NavGridComponent() = default;
        NavGridComponent(const NavGridComponent&) = default;
    };

    // Runtime path following state. Path points are world-space and already
    // follow the terrain height.
    struct NavAgentComponent
    {
        std::vector<glm::vec3> Path;
        size_t PathIndex = 0;
        float Radius = 0.45f;
        bool HasPath = false;
        float RepathTimer = 0.0f;
        float RepathInterval = 0.75f;

        NavAgentComponent() = default;
        NavAgentComponent(const NavAgentComponent&) = default;
    };

} // namespace Wheatear
