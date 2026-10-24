#include "wtpch.h"
#include "NavigationSystem.h"

#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Navigation/DebugDraw3D.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <limits>

namespace Wheatear {

    namespace {

        const glm::vec4 kGridColor(0.55f, 0.62f, 0.68f, 0.65f);
        const glm::vec4 kBlockedColor(0.95f, 0.25f, 0.20f, 0.9f);
        const glm::vec4 kPathColor(1.0f, 0.82f, 0.15f, 1.0f);
        const glm::vec4 kWaypointColor(0.25f, 1.0f, 0.45f, 1.0f);

        NavGridComponent* FindNavGrid(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            auto view = registry.view<NavGridComponent>();
            if (view.empty())
                return nullptr;
            return &view.get<NavGridComponent>(*view.begin());
        }

        TerrainComponent* FindTerrain(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            auto view = registry.view<TerrainComponent>();
            if (view.empty())
                return nullptr;
            return &view.get<TerrainComponent>(*view.begin());
        }

    } // namespace

    bool NavigationSystem::Rebuild(Scene* scene)
    {
        if (!scene)
            return false;

        TerrainComponent* terrain = FindTerrain(scene);
        if (!terrain)
            return false;
        if (terrain->Heights.empty())
            terrain->GenerateProcedural();

        auto& registry = scene->GetRegistry();
        NavGridComponent* nav = FindNavGrid(scene);
        if (!nav)
        {
            // Keep the runtime nav state on the terrain entity so the editor
            // does not accumulate a new "Navigation" entity on every reload.
            entt::entity terrainEntity = entt::null;
            for (auto e : registry.view<TerrainComponent>())
            {
                terrainEntity = e;
                break;
            }

            if (terrainEntity != entt::null)
            {
                nav = &registry.emplace<NavGridComponent>(terrainEntity);
            }
            else
            {
                Entity entity = scene->CreateEntity("Navigation");
                nav = &entity.AddComponent<NavGridComponent>();
            }
        }

        const glm::vec3 origin = terrain->GetWorldMinimum();
        if (!nav->Grid.Build(terrain->CellsX, terrain->CellsZ, terrain->CellSize,
                origin, terrain->Heights))
        {
            nav->Built = false;
            return false;
        }

        for (auto e : registry.view<NavObstacleComponent, TransformComponent>())
        {
            const auto& obstacle = registry.get<NavObstacleComponent>(e);
            if (!obstacle.Enabled)
                continue;

            const glm::mat4 world = scene->GetWorldTransform(Entity(e, scene));
            glm::vec3 minimum(0.0f);
            glm::vec3 maximum(0.0f);
            if (const auto* renderer = registry.try_get<MeshRendererComponent>(e);
                renderer && renderer->Mesh)
            {
                const glm::vec3& localMin = renderer->Mesh->GetBoundsMin();
                const glm::vec3& localMax = renderer->Mesh->GetBoundsMax();
                minimum = glm::vec3(std::numeric_limits<float>::max());
                maximum = glm::vec3(-std::numeric_limits<float>::max());
                for (int corner = 0; corner < 8; ++corner)
                {
                    const glm::vec3 local(
                        (corner & 1) ? localMax.x : localMin.x,
                        (corner & 2) ? localMax.y : localMin.y,
                        (corner & 4) ? localMax.z : localMin.z);
                    const glm::vec3 point = glm::vec3(world * glm::vec4(local, 1.0f));
                    minimum = glm::min(minimum, point);
                    maximum = glm::max(maximum, point);
                }
            }
            else
            {
                const glm::vec3 center(world[3]);
                minimum = center - glm::vec3(0.75f);
                maximum = center + glm::vec3(0.75f);
            }

            nav->Grid.BlockWorldAABB(minimum, maximum, 0.45f);
        }

        nav->Built = true;
        return true;
    }

    void NavigationSystem::OnRuntimeStart(Scene* scene)
    {
        Rebuild(scene);
    }

    void NavigationSystem::OnEditorStart(Scene* scene)
    {
        Rebuild(scene);
    }

    void NavigationSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        (void)ts;
        if (!scene)
            return;

        NavGridComponent* nav = FindNavGrid(scene);
        if (!nav)
            return;

        const bool f3Down = Input::IsKeyPressed(WT_KEY_F3);
        if (f3Down && !m_F3DownLast)
            nav->DebugVisible = !nav->DebugVisible;
        m_F3DownLast = f3Down;

        DebugDraw3D::Begin();
        if (nav->DebugVisible)
            DrawDebug(scene);
    }

    void NavigationSystem::OnUpdateEditor(Scene* scene, Timestep ts)
    {
        (void)ts;
        if (!scene)
            return;

        NavGridComponent* nav = FindNavGrid(scene);
        if (!nav)
            return;

        const bool f3Down = Input::IsKeyPressed(WT_KEY_F3);
        if (f3Down && !m_F3DownLast)
            nav->DebugVisible = !nav->DebugVisible;
        m_F3DownLast = f3Down;

        DebugDraw3D::Begin();
        if (nav->DebugVisible)
            DrawDebug(scene);
    }

    void NavigationSystem::DrawDebug(Scene* scene)
    {
        NavGridComponent* nav = FindNavGrid(scene);
        if (!nav || !nav->Built)
            return;

        const auto& grid = nav->Grid;
        const int cellsX = grid.GetCellsX();
        const int cellsZ = grid.GetCellsZ();
        const float cellSize = grid.GetCellSize();
        const glm::vec3 origin = grid.GetOrigin();

        const int step = std::max(1, 8);
        for (int x = 0; x <= cellsX; x += step)
        {
            const float worldX = origin.x + static_cast<float>(x) * cellSize;
            for (int z = 0; z < cellsZ; z += step)
            {
                const float z0 = origin.z + static_cast<float>(z) * cellSize;
                const float z1 = origin.z + static_cast<float>(
                    std::min(z + step, cellsZ)) * cellSize;
                const glm::vec3 a(worldX, grid.SampleHeight(worldX, z0) + 0.06f, z0);
                const glm::vec3 b(worldX, grid.SampleHeight(worldX, z1) + 0.06f, z1);
                DebugDraw3D::Line(a, b, kGridColor);
            }
        }
        for (int z = 0; z <= cellsZ; z += step)
        {
            const float worldZ = origin.z + static_cast<float>(z) * cellSize;
            for (int x = 0; x < cellsX; x += step)
            {
                const float x0 = origin.x + static_cast<float>(x) * cellSize;
                const float x1 = origin.x + static_cast<float>(
                    std::min(x + step, cellsX)) * cellSize;
                const glm::vec3 a(x0, grid.SampleHeight(x0, worldZ) + 0.06f, worldZ);
                const glm::vec3 b(x1, grid.SampleHeight(x1, worldZ) + 0.06f, worldZ);
                DebugDraw3D::Line(a, b, kGridColor);
            }
        }

        for (int z = 0; z < cellsZ; ++z)
        {
            for (int x = 0; x < cellsX; ++x)
            {
                if (grid.IsWalkable(x, z))
                    continue;

                const glm::vec3 center = grid.CellCenter(x, z);
                const float half = cellSize * 0.48f;
                const float y = center.y + 0.07f;
                DebugDraw3D::Line({ center.x - half, y, center.z - half },
                    { center.x + half, y, center.z - half }, kBlockedColor);
                DebugDraw3D::Line({ center.x + half, y, center.z - half },
                    { center.x + half, y, center.z + half }, kBlockedColor);
                DebugDraw3D::Line({ center.x + half, y, center.z + half },
                    { center.x - half, y, center.z + half }, kBlockedColor);
                DebugDraw3D::Line({ center.x - half, y, center.z + half },
                    { center.x - half, y, center.z - half }, kBlockedColor);
            }
        }

        auto& registry = scene->GetRegistry();
        int drawnPaths = 0;
        for (auto e : registry.view<NavAgentComponent, TransformComponent>())
        {
            if (drawnPaths >= 12)
                break;

            const auto& agent = registry.get<NavAgentComponent>(e);
            if (!agent.HasPath || agent.Path.size() < 2)
                continue;
            ++drawnPaths;

            for (size_t i = agent.PathIndex; i + 1 < agent.Path.size(); ++i)
            {
                const glm::vec3 a = agent.Path[i] + glm::vec3(0.0f, 0.12f, 0.0f);
                const glm::vec3 b = agent.Path[i + 1] + glm::vec3(0.0f, 0.12f, 0.0f);
                DebugDraw3D::Line(a, b, kPathColor);
            }

            if (agent.PathIndex < agent.Path.size())
            {
                const glm::vec3 point = agent.Path[agent.PathIndex];
                DebugDraw3D::Box(
                    point - glm::vec3(0.16f, 0.0f, 0.16f),
                    point + glm::vec3(0.16f, 0.32f, 0.16f), kWaypointColor);
            }
        }
    }

} // namespace Wheatear
