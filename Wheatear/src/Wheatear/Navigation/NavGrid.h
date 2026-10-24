#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Wheatear::Navigation {

    // Grid-based navigation for the greybox RTS phase. Cells are laid out on
    // the XZ plane; heights come from the terrain so path points follow the
    // ground. A* + line-of-sight smoothing is enough for a few hundred units
    // and is deliberately simple to profile before Recast/Detour or a flow
    // field replaces it.
    class NavGrid
    {
    public:
        bool Build(int cellsX, int cellsZ, float cellSize,
            const glm::vec3& origin,
            const std::vector<float>& heights,
            float maxStepHeight = 0.65f);

        void ClearObstacles();
        void BlockWorldAABB(const glm::vec3& minimum, const glm::vec3& maximum,
            float agentRadius = 0.4f);

        bool IsValidCell(int x, int z) const;
        bool IsWalkable(int x, int z) const;
        bool WorldToCell(const glm::vec3& world, int& x, int& z) const;
        glm::vec3 CellCenter(int x, int z) const;
        glm::vec3 ClampToWalkable(const glm::vec3& world) const;
        bool IsWalkableWorld(const glm::vec3& world) const;

        // Pushes a circle out of blocked cells (rocks, walls, buildings) after
        // movement/separation so agents cannot tunnel through nav obstacles.
        void ResolveCircle(glm::vec3& position, float radius,
            int iterations = 4) const;

        float SampleHeight(float worldX, float worldZ) const;
        bool Raycast(const glm::vec3& origin, const glm::vec3& direction,
            float maxDistance, glm::vec3& hit) const;

        bool FindPath(const glm::vec3& start, const glm::vec3& end,
            std::vector<glm::vec3>& path) const;

        int GetCellsX() const { return m_CellsX; }
        int GetCellsZ() const { return m_CellsZ; }
        float GetCellSize() const { return m_CellSize; }
        const glm::vec3& GetOrigin() const { return m_Origin; }
        const std::vector<uint8_t>& GetWalkable() const { return m_Walkable; }
        const std::vector<float>& GetHeights() const { return m_Heights; }

    private:
        bool FindNearestWalkable(int& x, int& z, int maxRadius) const;
        bool HasLineOfSight(const glm::vec3& from, const glm::vec3& to) const;

    private:
        int m_CellsX = 0;
        int m_CellsZ = 0;
        float m_CellSize = 1.0f;
        glm::vec3 m_Origin = glm::vec3(0.0f);
        std::vector<float> m_Heights;
        std::vector<uint8_t> m_Walkable;
        std::vector<uint8_t> m_BaseWalkable;
    };

} // namespace Wheatear::Navigation
