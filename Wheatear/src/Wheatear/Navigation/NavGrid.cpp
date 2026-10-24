#include "wtpch.h"
#include "NavGrid.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace Wheatear::Navigation {

    namespace {

        constexpr float kInfinity = std::numeric_limits<float>::max();
        constexpr float kDiagonalCost = 1.41421356f;

        float OctileHeuristic(int dx, int dz)
        {
            const int ax = std::abs(dx);
            const int az = std::abs(dz);
            const int diagonal = std::min(ax, az);
            const int straight = std::max(ax, az) - diagonal;
            return diagonal * kDiagonalCost + static_cast<float>(straight);
        }

    } // namespace

    bool NavGrid::Build(int cellsX, int cellsZ, float cellSize,
        const glm::vec3& origin,
        const std::vector<float>& heights,
        float maxStepHeight)
    {
        if (cellsX <= 0 || cellsZ <= 0 || cellSize <= 0.0001f)
            return false;

        const size_t heightCount =
            static_cast<size_t>(cellsX + 1) * static_cast<size_t>(cellsZ + 1);
        if (heights.size() < heightCount)
            return false;

        m_CellsX = cellsX;
        m_CellsZ = cellsZ;
        m_CellSize = cellSize;
        m_Origin = origin;
        m_Heights.assign(heights.begin(), heights.begin() + heightCount);
        m_Walkable.assign(static_cast<size_t>(cellsX) * cellsZ, 1);
        m_BaseWalkable = m_Walkable;

        const auto heightAt = [&](int x, int z) -> float
        {
            const int clampedX = std::clamp(x, 0, m_CellsX);
            const int clampedZ = std::clamp(z, 0, m_CellsZ);
            return m_Heights[static_cast<size_t>(clampedZ)
                * static_cast<size_t>(m_CellsX + 1)
                + static_cast<size_t>(clampedX)];
        };

        for (int z = 0; z < m_CellsZ; ++z)
        {
            for (int x = 0; x < m_CellsX; ++x)
            {
                const float h00 = heightAt(x, z);
                const float h10 = heightAt(x + 1, z);
                const float h01 = heightAt(x, z + 1);
                const float h11 = heightAt(x + 1, z + 1);
                const float minimum = std::min(std::min(h00, h10), std::min(h01, h11));
                const float maximum = std::max(std::max(h00, h10), std::max(h01, h11));
                if (maximum - minimum > maxStepHeight)
                    m_Walkable[static_cast<size_t>(z) * m_CellsX + x] = 0;
            }
        }

        m_BaseWalkable = m_Walkable;
        return true;
    }

    void NavGrid::ClearObstacles()
    {
        if (!m_BaseWalkable.empty())
            m_Walkable = m_BaseWalkable;
    }

    void NavGrid::BlockWorldAABB(const glm::vec3& minimum, const glm::vec3& maximum,
        float agentRadius)
    {
        if (m_CellsX <= 0 || m_CellsZ <= 0)
            return;

        const glm::vec3 expandedMin = minimum - glm::vec3(agentRadius, 0.0f, agentRadius);
        const glm::vec3 expandedMax = maximum + glm::vec3(agentRadius, 0.0f, agentRadius);

        const int minX = static_cast<int>(std::floor(
            (expandedMin.x - m_Origin.x) / m_CellSize));
        const int maxX = static_cast<int>(std::floor(
            (expandedMax.x - m_Origin.x) / m_CellSize));
        const int minZ = static_cast<int>(std::floor(
            (expandedMin.z - m_Origin.z) / m_CellSize));
        const int maxZ = static_cast<int>(std::floor(
            (expandedMax.z - m_Origin.z) / m_CellSize));

        for (int z = std::max(0, minZ); z <= std::min(m_CellsZ - 1, maxZ); ++z)
        {
            for (int x = std::max(0, minX); x <= std::min(m_CellsX - 1, maxX); ++x)
                m_Walkable[static_cast<size_t>(z) * m_CellsX + x] = 0;
        }
    }

    bool NavGrid::IsValidCell(int x, int z) const
    {
        return x >= 0 && z >= 0 && x < m_CellsX && z < m_CellsZ;
    }

    bool NavGrid::IsWalkable(int x, int z) const
    {
        return IsValidCell(x, z)
            && m_Walkable[static_cast<size_t>(z) * m_CellsX + x] != 0;
    }

    bool NavGrid::WorldToCell(const glm::vec3& world, int& x, int& z) const
    {
        if (m_CellsX <= 0 || m_CellsZ <= 0 || m_CellSize <= 0.0001f)
            return false;

        x = static_cast<int>(std::floor((world.x - m_Origin.x) / m_CellSize));
        z = static_cast<int>(std::floor((world.z - m_Origin.z) / m_CellSize));
        return IsValidCell(x, z);
    }

    bool NavGrid::IsWalkableWorld(const glm::vec3& world) const
    {
        int x = 0;
        int z = 0;
        return WorldToCell(world, x, z) && IsWalkable(x, z);
    }

    glm::vec3 NavGrid::ClampToWalkable(const glm::vec3& world) const
    {
        if (IsWalkableWorld(world))
            return glm::vec3(world.x, SampleHeight(world.x, world.z), world.z);

        int x = 0;
        int z = 0;
        if (WorldToCell(world, x, z) && FindNearestWalkable(x, z, 12))
            return CellCenter(x, z);
        return glm::vec3(world.x, SampleHeight(world.x, world.z), world.z);
    }

    void NavGrid::ResolveCircle(glm::vec3& position, float radius,
        int iterations) const
    {
        if (m_Walkable.empty() || radius <= 0.0f)
            return;

        // Deep penetration (e.g. after a separation push) can oscillate
        // between two adjacent blocked cells. Snap to the nearest walkable
        // cell first, then do the fine-grained edge pushes below.
        int centerX = 0;
        int centerZ = 0;
        if (WorldToCell(position, centerX, centerZ)
            && !IsWalkable(centerX, centerZ)
            && FindNearestWalkable(centerX, centerZ, 12))
        {
            const glm::vec3 safe = CellCenter(centerX, centerZ);
            position.x = safe.x;
            position.z = safe.z;
        }

        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            const int minX = static_cast<int>(std::floor(
                (position.x - radius - m_Origin.x) / m_CellSize));
            const int maxX = static_cast<int>(std::floor(
                (position.x + radius - m_Origin.x) / m_CellSize));
            const int minZ = static_cast<int>(std::floor(
                (position.z - radius - m_Origin.z) / m_CellSize));
            const int maxZ = static_cast<int>(std::floor(
                (position.z + radius - m_Origin.z) / m_CellSize));

            bool pushed = false;
            for (int z = minZ; z <= maxZ; ++z)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    if (IsWalkable(x, z))
                        continue;

                    const float cellMinX = m_Origin.x + static_cast<float>(x) * m_CellSize;
                    const float cellMinZ = m_Origin.z + static_cast<float>(z) * m_CellSize;
                    const float cellMaxX = cellMinX + m_CellSize;
                    const float cellMaxZ = cellMinZ + m_CellSize;
                    const float closestX = glm::clamp(position.x, cellMinX, cellMaxX);
                    const float closestZ = glm::clamp(position.z, cellMinZ, cellMaxZ);
                    float dx = position.x - closestX;
                    float dz = position.z - closestZ;
                    const float distanceSq = dx * dx + dz * dz;

                    if (distanceSq >= radius * radius)
                        continue;

                    glm::vec2 normal(0.0f);
                    float push = 0.0f;
                    if (distanceSq > 1e-8f)
                    {
                        const float distance = std::sqrt(distanceSq);
                        normal = glm::vec2(dx, dz) / distance;
                        push = radius - distance;
                    }
                    else
                    {
                        // Center is inside the cell: push toward the nearest edge.
                        const float left = position.x - cellMinX;
                        const float right = cellMaxX - position.x;
                        const float back = position.z - cellMinZ;
                        const float front = cellMaxZ - position.z;
                        const float nearest = std::min(std::min(left, right),
                            std::min(back, front));
                        if (nearest == left)
                            normal = glm::vec2(-1.0f, 0.0f);
                        else if (nearest == right)
                            normal = glm::vec2(1.0f, 0.0f);
                        else if (nearest == back)
                            normal = glm::vec2(0.0f, -1.0f);
                        else
                            normal = glm::vec2(0.0f, 1.0f);
                        push = radius + nearest;
                    }

                    position.x += normal.x * push;
                    position.z += normal.y * push;
                    pushed = true;
                }
            }

            if (!pushed)
                break;
        }

        const float minWorldX = m_Origin.x + radius;
        const float maxWorldX = m_Origin.x
            + static_cast<float>(m_CellsX) * m_CellSize - radius;
        const float minWorldZ = m_Origin.z + radius;
        const float maxWorldZ = m_Origin.z
            + static_cast<float>(m_CellsZ) * m_CellSize - radius;
        position.x = glm::clamp(position.x, minWorldX, maxWorldX);
        position.z = glm::clamp(position.z, minWorldZ, maxWorldZ);
        position.y = SampleHeight(position.x, position.z);
    }

    glm::vec3 NavGrid::CellCenter(int x, int z) const
    {
        const float worldX = m_Origin.x + (static_cast<float>(x) + 0.5f) * m_CellSize;
        const float worldZ = m_Origin.z + (static_cast<float>(z) + 0.5f) * m_CellSize;
        return glm::vec3(worldX, SampleHeight(worldX, worldZ), worldZ);
    }

    float NavGrid::SampleHeight(float worldX, float worldZ) const
    {
        if (m_CellsX <= 0 || m_CellsZ <= 0 || m_Heights.empty())
            return 0.0f;

        const float gridX = (worldX - m_Origin.x) / m_CellSize;
        const float gridZ = (worldZ - m_Origin.z) / m_CellSize;
        const int x0 = std::clamp(static_cast<int>(std::floor(gridX)), 0, m_CellsX);
        const int z0 = std::clamp(static_cast<int>(std::floor(gridZ)), 0, m_CellsZ);
        const int x1 = std::min(x0 + 1, m_CellsX);
        const int z1 = std::min(z0 + 1, m_CellsZ);
        const float fx = std::clamp(gridX - static_cast<float>(x0), 0.0f, 1.0f);
        const float fz = std::clamp(gridZ - static_cast<float>(z0), 0.0f, 1.0f);

        const auto at = [&](int x, int z) -> float
        {
            return m_Heights[static_cast<size_t>(z) * static_cast<size_t>(m_CellsX + 1)
                + static_cast<size_t>(x)];
        };

        const float top = glm::mix(at(x0, z0), at(x1, z0), fx);
        const float bottom = glm::mix(at(x0, z1), at(x1, z1), fx);
        return glm::mix(top, bottom, fz);
    }

    bool NavGrid::Raycast(const glm::vec3& origin, const glm::vec3& direction,
        float maxDistance, glm::vec3& hit) const
    {
        if (maxDistance <= 0.0f)
            return false;

        const glm::vec3 dir = glm::normalize(direction);
        const float step = std::max(0.05f, m_CellSize * 0.1f);
        float previousT = 0.0f;
        float previousDelta = origin.y - SampleHeight(origin.x, origin.z);

        for (float t = step; t <= maxDistance; t += step)
        {
            const glm::vec3 point = origin + dir * t;
            const float delta = point.y - SampleHeight(point.x, point.z);
            if (delta <= 0.0f)
            {
                const float span = std::max(1e-5f, previousDelta - delta);
                const float alpha = std::clamp(previousDelta / span, 0.0f, 1.0f);
                const float hitT = glm::mix(previousT, t, alpha);
                hit = origin + dir * hitT;
                hit.y = SampleHeight(hit.x, hit.z);
                return true;
            }
            previousT = t;
            previousDelta = delta;
        }
        return false;
    }

    bool NavGrid::FindNearestWalkable(int& x, int& z, int maxRadius) const
    {
        if (IsWalkable(x, z))
            return true;

        const int originX = x;
        const int originZ = z;
        int bestX = -1;
        int bestZ = -1;
        float bestDistance = kInfinity;

        for (int radius = 1; radius <= maxRadius; ++radius)
        {
            for (int dz = -radius; dz <= radius; ++dz)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (std::abs(dx) != radius && std::abs(dz) != radius)
                        continue;

                    const int candidateX = originX + dx;
                    const int candidateZ = originZ + dz;
                    if (!IsWalkable(candidateX, candidateZ))
                        continue;

                    const float distance = static_cast<float>(dx * dx + dz * dz);
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestX = candidateX;
                        bestZ = candidateZ;
                    }
                }
            }
            if (bestX >= 0)
                break;
        }

        if (bestX < 0)
            return false;

        x = bestX;
        z = bestZ;
        return true;
    }

    bool NavGrid::HasLineOfSight(const glm::vec3& from, const glm::vec3& to) const
    {
        const glm::vec3 delta = to - from;
        const float distance = glm::length(glm::vec2(delta.x, delta.z));
        if (distance < 1e-4f)
            return true;

        const float step = std::max(0.1f, m_CellSize * 0.35f);
        const int samples = std::max(1, static_cast<int>(std::ceil(distance / step)));
        for (int i = 1; i <= samples; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(samples);
            const glm::vec3 point = from + delta * t;
            int x = 0;
            int z = 0;
            if (!WorldToCell(point, x, z) || !IsWalkable(x, z))
                return false;

            const float height = SampleHeight(point.x, point.z);
            const float expected = glm::mix(from.y, to.y, t);
            if (std::abs(height - expected) > 0.7f)
                return false;
        }
        return true;
    }

    bool NavGrid::FindPath(const glm::vec3& start, const glm::vec3& end,
        std::vector<glm::vec3>& path) const
    {
        path.clear();
        if (m_Walkable.empty())
            return false;

        int startX = 0;
        int startZ = 0;
        int endX = 0;
        int endZ = 0;
        if (!WorldToCell(start, startX, startZ) || !WorldToCell(end, endX, endZ))
            return false;

        if (!FindNearestWalkable(startX, startZ, 8)
            || !FindNearestWalkable(endX, endZ, 12))
        {
            return false;
        }

        const int count = m_CellsX * m_CellsZ;
        const int startIndex = startZ * m_CellsX + startX;
        const int endIndex = endZ * m_CellsX + endX;

        std::vector<float> gScore(count, kInfinity);
        std::vector<int> cameFrom(count, -1);
        std::vector<uint8_t> closed(count, 0);

        struct Node
        {
            float Score;
            int Index;
            bool operator>(const Node& other) const { return Score > other.Score; }
        };
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

        gScore[startIndex] = 0.0f;
        open.push({ OctileHeuristic(endX - startX, endZ - startZ), startIndex });

        constexpr int kNeighborX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
        constexpr int kNeighborZ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
        constexpr float kCost[8] = {
            1.0f, 1.0f, 1.0f, 1.0f,
            kDiagonalCost, kDiagonalCost, kDiagonalCost, kDiagonalCost
        };

        bool found = false;
        while (!open.empty())
        {
            const Node node = open.top();
            open.pop();
            if (closed[node.Index])
                continue;
            closed[node.Index] = 1;

            if (node.Index == endIndex)
            {
                found = true;
                break;
            }

            const int currentX = node.Index % m_CellsX;
            const int currentZ = node.Index / m_CellsX;
            for (int neighbor = 0; neighbor < 8; ++neighbor)
            {
                const int nx = currentX + kNeighborX[neighbor];
                const int nz = currentZ + kNeighborZ[neighbor];
                if (!IsWalkable(nx, nz))
                    continue;

                if (neighbor >= 4)
                {
                    // Do not cut obstacle corners diagonally.
                    if (!IsWalkable(currentX + kNeighborX[neighbor], currentZ)
                        || !IsWalkable(currentX, currentZ + kNeighborZ[neighbor]))
                    {
                        continue;
                    }
                }

                const int neighborIndex = nz * m_CellsX + nx;
                if (closed[neighborIndex])
                    continue;

                const float tentative = gScore[node.Index] + kCost[neighbor];
                if (tentative >= gScore[neighborIndex])
                    continue;

                gScore[neighborIndex] = tentative;
                cameFrom[neighborIndex] = node.Index;
                open.push({
                    tentative + OctileHeuristic(endX - nx, endZ - nz),
                    neighborIndex
                });
            }
        }

        if (!found)
            return false;

        std::vector<int> cells;
        for (int index = endIndex; index != -1; index = cameFrom[index])
        {
            cells.push_back(index);
            if (index == startIndex)
                break;
        }
        std::reverse(cells.begin(), cells.end());

        std::vector<glm::vec3> raw;
        raw.reserve(cells.size() + 1);
        raw.push_back(glm::vec3(start.x,
            SampleHeight(start.x, start.z), start.z));
        for (int index : cells)
        {
            const int x = index % m_CellsX;
            const int z = index / m_CellsX;
            raw.push_back(CellCenter(x, z));
        }
        raw.push_back(glm::vec3(end.x, SampleHeight(end.x, end.z), end.z));

        // String pulling: keep the furthest visible point from the current
        // anchor, then continue from there.
        path.push_back(raw.front());
        size_t anchor = 0;
        while (anchor + 1 < raw.size())
        {
            size_t furthest = anchor + 1;
            for (size_t candidate = raw.size() - 1; candidate > anchor + 1; --candidate)
            {
                if (HasLineOfSight(raw[anchor], raw[candidate]))
                {
                    furthest = candidate;
                    break;
                }
            }
            path.push_back(raw[furthest]);
            anchor = furthest;
        }

        if (path.size() >= 2
            && glm::length(path.back() - path[path.size() - 2]) < 0.01f)
        {
            path.pop_back();
        }
        return path.size() >= 2;
    }

} // namespace Wheatear::Navigation
