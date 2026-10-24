#include "wtpch.h"
#include "RTSUnitSystem.h"
#include "RTSUtilities.h"

#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Renderer/Mesh.h"
#include "Wheatear/Systems/NavigationSystem.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    namespace {

        constexpr float kUnitY = 0.4f;
        constexpr float kUnitScale = 0.8f;
        constexpr float kGroundBounds = 28.0f;
        constexpr int kPlayerUnitCount = 18;

        const glm::vec4 kPlayerColor(0.20f, 0.45f, 1.00f, 1.0f);
        const glm::vec4 kEnemyColor(1.00f, 0.25f, 0.20f, 1.0f);

        void SetUnitState(RTSUnitComponent& unit, RTSUnitState state, float dt)
        {
            if (unit.State == state)
                unit.StateTime += dt;
            else
            {
                unit.State = state;
                unit.StateTime = 0.0f;
            }
        }

        entt::entity FindNearestResource(Scene* scene, const glm::vec3& position)
        {
            entt::entity best = entt::null;
            float bestDistanceSq = std::numeric_limits<float>::max();
            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<RTSResourceNodeComponent, TransformComponent>())
            {
                const auto& node = registry.get<RTSResourceNodeComponent>(e);
                if (node.Amount <= 0.0f)
                    continue;

                const auto& transform = registry.get<TransformComponent>(e);
                const glm::vec3 delta = transform.Translation - position;
                const float distanceSq = delta.x * delta.x + delta.z * delta.z;
                if (distanceSq < bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    best = e;
                }
            }
            return best;
        }

        template<typename TView>
        entt::entity FindNearestEnemy(TView& units, entt::entity self,
            RTSTeam team, const glm::vec3& position, float aggroRange)
        {
            entt::entity best = entt::null;
            float bestDistanceSq = aggroRange * aggroRange;

            for (auto entity : units)
            {
                if (entity == self)
                    continue;

                const auto& other = units.get<RTSUnitComponent>(entity);
                if (other.Team == team || other.HP <= 0.0f)
                    continue;

                const auto& otherTransform = units.get<TransformComponent>(entity);
                const glm::vec3 delta = otherTransform.Translation - position;
                const float distanceSq = delta.x * delta.x + delta.z * delta.z;
                if (distanceSq < bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    best = entity;
                }
            }
            return best;
        }

    } // namespace

    void RTSUnitSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        m_PlayerMesh = Mesh::CreateCube();
        m_EnemyMesh = Mesh::CreateSphere(16, 10);
        m_HarvesterMesh = Mesh::CreateSphere(12, 8);

        auto& registry = scene->GetRegistry();
        RTSMatchComponent* match = nullptr;
        RTSCommandStateComponent* command = nullptr;
        if (registry.view<RTSMatchComponent>().empty())
        {
            Entity controller = scene->CreateEntity("RTS Controller");
            match = &controller.AddComponent<RTSMatchComponent>();
            controller.AddComponent<RTSSelectionStateComponent>();
            command = &controller.AddComponent<RTSCommandStateComponent>();
            controller.AddComponent<RTSAIControllerComponent>();
        }
        else
        {
            match = &registry.view<RTSMatchComponent>().get<RTSMatchComponent>(
                *registry.view<RTSMatchComponent>().begin());
            auto commandView = registry.view<RTSCommandStateComponent>();
            if (!commandView.empty())
                command = &commandView.get<RTSCommandStateComponent>(*commandView.begin());
        }
        (void)command;

        // Resource nodes.
        const glm::vec3 resourcePositions[] = {
            { 12.0f, 0.0f, 6.0f },
            { -12.0f, 0.0f, 6.0f },
            { 14.0f, 0.0f, -8.0f },
            { -14.0f, 0.0f, -8.0f },
        };
        for (const glm::vec3& position : resourcePositions)
        {
            Entity node = scene->CreateEntity("Resource Node");
            auto& transform = node.GetComponent<TransformComponent>();
            transform.Translation = position;
            transform.Translation.y = RTS::GroundHeight(scene, position.x, position.z)
                + 0.9f;
            transform.Scale = glm::vec3(1.2f);

            auto& renderer = node.AddComponent<MeshRendererComponent>();
            renderer.Mesh = m_HarvesterMesh;
            renderer.Material = Material::Create();
            renderer.Material->Albedo = glm::vec4(0.95f, 0.78f, 0.22f, 1.0f);
            renderer.Material->Metallic = 0.35f;
            renderer.Material->Roughness = 0.4f;

            node.AddComponent<RTSResourceNodeComponent>();
        }

        const auto spawnBuilding = [&](RTSTeam team, RTSBuildingKind kind,
            float x, float z, float sx, float sy, float sz, float hp,
            bool constructed, bool production, const glm::vec3& rally)
        {
            const float y = RTS::GroundHeight(scene, x, z) + sy * 0.5f;
            return RTS::SpawnBuilding(scene, team, kind, { x, y, z },
                { sx, sy, sz }, hp, constructed, production, rally);
        };

        spawnBuilding(RTSTeam::Player, RTSBuildingKind::Base,
            0.0f, 18.0f, 5.0f, 3.0f, 5.0f, 3000.0f, true, false, {});
        spawnBuilding(RTSTeam::Player, RTSBuildingKind::Barracks,
            8.0f, 12.0f, 4.0f, 3.0f, 4.0f, 800.0f, true, true,
            { 4.0f, 0.0f, 10.0f });
        spawnBuilding(RTSTeam::Enemy, RTSBuildingKind::Base,
            0.0f, -20.0f, 5.0f, 3.0f, 5.0f, 8000.0f, true, false, {});
        spawnBuilding(RTSTeam::Enemy, RTSBuildingKind::Barracks,
            -8.0f, -14.0f, 4.0f, 3.0f, 4.0f, 1000.0f, true, true,
            { -4.0f, 0.0f, -10.0f });

        // Buildings must be in the nav grid before units path around them.
        NavigationSystem::Rebuild(scene);

        // Player army + economy.
        for (int i = 0; i < kPlayerUnitCount; ++i)
        {
            const int column = i % 6;
            const int row = i / 6;
            const glm::vec3 position(
                (static_cast<float>(column) - 2.5f) * 1.7f,
                kUnitY,
                6.0f + static_cast<float>(row) * 1.7f);
            SpawnUnit(scene, RTSTeam::Player, position);
        }
        for (int i = 0; i < 3; ++i)
        {
            SpawnUnit(scene, RTSTeam::Player,
                { -3.0f + static_cast<float>(i) * 3.0f, kUnitY, 14.0f },
                RTSUnitType::Harvester);
        }

        // Small enemy garrison; AI production reinforces it.
        for (int i = 0; i < 20; ++i)
        {
            const int column = i % 6;
            const int row = i / 6;
            SpawnUnit(scene, RTSTeam::Enemy,
                { (static_cast<float>(column) - 2.5f) * 1.7f, kUnitY,
                  -12.0f - static_cast<float>(row) * 1.7f });
        }

        if (match)
        {
            match->Credits[static_cast<int>(RTSTeam::Player)] = 400.0f;
            match->Credits[static_cast<int>(RTSTeam::Enemy)] = 350.0f;
            match->SupplyCap[static_cast<int>(RTSTeam::Player)] = 60;
            match->SupplyCap[static_cast<int>(RTSTeam::Enemy)] = 60;
        }
    }

    void RTSUnitSystem::OnRuntimeStop(Scene* scene)
    {
        (void)scene;
        m_PlayerMesh.reset();
        m_EnemyMesh.reset();
        m_HarvesterMesh.reset();
    }

    void RTSUnitSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        auto& registry = scene->GetRegistry();
        auto matchView = registry.view<RTSMatchComponent>();
        if (matchView.empty())
            return;

        auto& match = matchView.get<RTSMatchComponent>(*matchView.begin());
        const float dt = ts.GetSeconds();

        if (match.Result == RTSMatchResult::InProgress)
            UpdateUnits(scene, dt);

        UpdateCounts(scene, match);
    }

    entt::entity RTSUnitSystem::SpawnUnit(Scene* scene, RTSTeam team,
        const glm::vec3& position, RTSUnitType type)
    {
        const bool player = team == RTSTeam::Player;
        const bool harvester = type == RTSUnitType::Harvester;
        Entity entity = scene->CreateEntity(
            harvester ? "Harvester" : (player ? "PlayerUnit" : "EnemyUnit"));

        auto& transform = entity.GetComponent<TransformComponent>();
        transform.Translation = position;
        transform.Translation.y = RTS::GroundHeight(scene, position.x, position.z)
            + kUnitY;
        transform.Rotation = glm::vec3(0.0f);
        transform.Scale = glm::vec3(kUnitScale);

        auto& renderer = entity.AddComponent<MeshRendererComponent>();
        renderer.Mesh = harvester
            ? m_HarvesterMesh
            : (player ? m_PlayerMesh : m_EnemyMesh);
        renderer.Material = Material::Create();
        renderer.Material->Albedo = harvester
            ? glm::vec4(0.95f, 0.75f, 0.20f, 1.0f)
            : (player ? kPlayerColor : kEnemyColor);
        renderer.Material->Metallic = harvester ? 0.3f : 0.05f;
        renderer.Material->Roughness = 0.7f;

        auto& unit = entity.AddComponent<RTSUnitComponent>();
        entity.AddComponent<NavAgentComponent>();
        unit.Team = team;
        unit.Type = type;
        unit.TeamColor = renderer.Material->Albedo;
        unit.VisualScale = transform.Scale;
        unit.State = RTSUnitState::Idle;

        if (harvester)
        {
            unit.HP = unit.MaxHP = 90.0f;
            unit.MoveSpeed = 5.2f;
            unit.AggroRange = 0.0f;
            unit.AttackRange = 0.1f;
            unit.AttackDamage = 0.0f;
            unit.AttackInterval = 1.0f;
        }
        else if (player)
        {
            unit.HP = unit.MaxHP = 120.0f;
            unit.MoveSpeed = 4.2f;
            unit.AggroRange = 22.0f;
            unit.AttackRange = 2.2f;
            unit.AttackDamage = 11.0f;
            unit.AttackInterval = 0.8f;
        }
        else
        {
            unit.HP = unit.MaxHP = 85.0f;
            unit.MoveSpeed = 3.8f;
            unit.AggroRange = 30.0f;
            unit.AttackRange = 2.0f;
            unit.AttackDamage = 7.0f;
            unit.AttackInterval = 1.0f;

            // Enemies advance toward the player's side until they acquire a target.
            RTS::SetDestination(scene, static_cast<entt::entity>(entity),
                glm::vec3(0.0f, 0.0f, 4.0f));
        }

        return static_cast<entt::entity>(entity);
    }

    void RTSUnitSystem::SpawnWave(Scene* scene, int count)
    {
        const int columns = static_cast<int>(
            std::ceil(std::sqrt(static_cast<float>(count))));

        for (int i = 0; i < count; ++i)
        {
            const int column = i % columns;
            const int row = i / columns;
            const glm::vec3 position(
                (static_cast<float>(column) - (columns - 1) * 0.5f) * 1.5f,
                kUnitY,
                -12.0f - static_cast<float>(row) * 1.5f);
            SpawnUnit(scene, RTSTeam::Enemy, position);
        }
    }

    void RTSUnitSystem::UpdateWaves(Scene* scene, RTSMatchComponent& match, float dt)
    {
        const int enemies = static_cast<int>(CountUnits(scene, RTSTeam::Enemy));

        if (match.WaveActive)
        {
            if (enemies == 0)
            {
                match.WaveActive = false;
                if (match.Wave >= match.MaxWaves)
                {
                    match.Result = RTSMatchResult::Victory;
                    return;
                }
                match.NextWaveTimer = 5.0f;
            }
            return;
        }

        if (match.Wave >= match.MaxWaves)
            return;

        match.NextWaveTimer -= dt;
        if (match.NextWaveTimer <= 0.0f)
        {
            ++match.Wave;
            SpawnWave(scene, 40 + match.Wave * 30);
            match.WaveActive = true;
        }
    }

    void RTSUnitSystem::UpdateUnits(Scene* scene, float dt)
    {
        auto& registry = scene->GetRegistry();
        auto units = registry.view<RTSUnitComponent, TransformComponent>();
        RTSMatchComponent* match = RTS::FindMatch(scene);
        std::vector<entt::entity> dead;

        for (auto entity : units)
        {
            auto& unit = units.get<RTSUnitComponent>(entity);
            auto& transform = units.get<TransformComponent>(entity);

            if (unit.State == RTSUnitState::Dying)
            {
                unit.StateTime += dt;
                unit.DeathTimer -= dt;
                if (unit.DeathTimer <= 0.0f)
                    dead.push_back(entity);
                continue;
            }

            if (unit.HP <= 0.0f)
            {
                unit.State = RTSUnitState::Dying;
                unit.StateTime = 0.0f;
                unit.DeathTimer = unit.DeathDuration;
                unit.Selected = false;
                unit.HasMoveTarget = false;
                unit.Target = entt::null;
                if (auto* agent = registry.try_get<NavAgentComponent>(entity))
                {
                    agent->Path.clear();
                    agent->PathIndex = 0;
                    agent->HasPath = false;
                }
                continue;
            }

            if (unit.Type == RTSUnitType::Building)
            {
                SetUnitState(unit, RTSUnitState::Idle, dt);
                continue;
            }

            auto* agent = registry.try_get<NavAgentComponent>(entity);

            if (unit.Type == RTSUnitType::Harvester)
            {
                bool hasNode = false;
                if (unit.GatherTarget != entt::null
                    && registry.valid(unit.GatherTarget)
                    && registry.all_of<RTSResourceNodeComponent, TransformComponent>(
                        unit.GatherTarget))
                {
                    const auto& node =
                        registry.get<RTSResourceNodeComponent>(unit.GatherTarget);
                    hasNode = node.Amount > 0.0f;
                }

                if (!hasNode)
                {
                    unit.GatherTarget = FindNearestResource(scene, transform.Translation);
                    unit.GatherTimer = 0.0f;
                }

                if (unit.GatherTarget == entt::null)
                {
                    unit.HasMoveTarget = false;
                    if (agent)
                    {
                        agent->Path.clear();
                        agent->PathIndex = 0;
                        agent->HasPath = false;
                    }
                    SetUnitState(unit, RTSUnitState::Idle, dt);
                    continue;
                }

                const auto& nodeTransform =
                    registry.get<TransformComponent>(unit.GatherTarget);
                const glm::vec3 delta = nodeTransform.Translation - transform.Translation;
                const float distance = glm::length(glm::vec2(delta.x, delta.z));
                const float gatherRange =
                    registry.get<RTSResourceNodeComponent>(unit.GatherTarget).Radius + 0.6f;
                if (distance > gatherRange
                    && !unit.HasMoveTarget
                    && !(agent && agent->HasPath))
                {
                    RTS::SetDestination(scene, entity, nodeTransform.Translation);
                }
            }

            unit.AttackTimer = std::max(0.0f, unit.AttackTimer - dt);
            if (agent)
                agent->RepathTimer = std::max(0.0f, agent->RepathTimer - dt);

            if (unit.Target != entt::null)
            {
                if (!registry.valid(unit.Target)
                    || !registry.all_of<RTSUnitComponent, TransformComponent>(unit.Target))
                {
                    unit.Target = entt::null;
                }
                else if (registry.get<RTSUnitComponent>(unit.Target).HP <= 0.0f)
                {
                    unit.Target = entt::null;
                }
            }

            if (unit.Target == entt::null)
            {
                const entt::entity previousTarget = unit.Target;
                unit.Target = FindNearestEnemy(
                    units, entity, unit.Team, transform.Translation, unit.AggroRange);

                if (unit.Target != previousTarget && agent && !unit.HasMoveTarget)
                {
                    agent->Path.clear();
                    agent->PathIndex = 0;
                    agent->HasPath = false;
                    agent->RepathTimer = 0.0f;
                }

                if (unit.Target != entt::null && unit.Team == RTSTeam::Enemy)
                    unit.HasMoveTarget = false;
            }

            bool inAttackRange = false;
            if (unit.Target != entt::null)
            {
                const auto& targetTransform = registry.get<TransformComponent>(unit.Target);
                auto& targetUnit = registry.get<RTSUnitComponent>(unit.Target);
                glm::vec3 delta = targetTransform.Translation - transform.Translation;
                delta.y = 0.0f;

                if (glm::length(delta) <= unit.AttackRange + targetUnit.TargetRadius)
                {
                    inAttackRange = true;
                    if (unit.AttackTimer <= 0.0f)
                    {
                        targetUnit.HP -= unit.AttackDamage;
                        unit.AttackTimer = unit.AttackInterval;
                    }
                }
            }

            if (inAttackRange)
            {
                SetUnitState(unit, RTSUnitState::Attack, dt);
                if (agent && !unit.HasMoveTarget && agent->HasPath)
                {
                    agent->Path.clear();
                    agent->PathIndex = 0;
                    agent->HasPath = false;
                }
                continue;
            }

            // Chase paths are refreshed periodically so a moving target does
            // not leave the unit following a stale route forever.
            if (agent && !unit.HasMoveTarget && unit.Target != entt::null
                && agent->RepathTimer <= 0.0f)
            {
                if (NavGridComponent* nav = RTS::FindNavGrid(scene);
                    nav && nav->Built)
                {
                    glm::vec3 start = transform.Translation;
                    start.y = nav->Grid.SampleHeight(start.x, start.z);
                    const glm::vec3 targetPosition =
                        registry.get<TransformComponent>(unit.Target).Translation;

                    agent->Path.clear();
                    agent->PathIndex = 0;
                    agent->HasPath = nav->Grid.FindPath(start, targetPosition,
                        agent->Path);
                    agent->RepathTimer = agent->RepathInterval;
                }
            }

            bool hasDestination = false;
            float stopDistance = 0.25f;

            if (agent && agent->HasPath && agent->PathIndex < agent->Path.size())
            {
                glm::vec3 waypoint = agent->Path[agent->PathIndex];
                glm::vec3 delta = waypoint - transform.Translation;
                delta.y = 0.0f;
                const float distance = glm::length(delta);
                const bool finalWaypoint =
                    agent->PathIndex + 1 >= agent->Path.size();
                const float arriveDistance = finalWaypoint ? 0.25f : 0.35f;

                if (distance <= arriveDistance)
                {
                    ++agent->PathIndex;
                    if (agent->PathIndex >= agent->Path.size())
                    {
                        agent->HasPath = false;
                        agent->Path.clear();
                        agent->PathIndex = 0;
                        unit.HasMoveTarget = false;
                    }
                }
                else
                {
                    const glm::vec3 direction = delta / distance;
                    const float step = std::min(unit.MoveSpeed * dt,
                        distance - arriveDistance);
                    transform.Translation += direction * step;
                }
                hasDestination = true;
            }
            else if (unit.HasMoveTarget)
            {
                hasDestination = true;
                glm::vec3 delta = unit.MoveTarget - transform.Translation;
                delta.y = 0.0f;
                const float distance = glm::length(delta);
                if (distance > stopDistance)
                {
                    const glm::vec3 direction = delta / distance;
                    const float step = std::min(unit.MoveSpeed * dt,
                        distance - stopDistance);
                    transform.Translation += direction * step;
                }
                if (distance <= 0.3f)
                    unit.HasMoveTarget = false;
            }
            else if (unit.Target != entt::null)
            {
                hasDestination = true;
                const glm::vec3 destination =
                    registry.get<TransformComponent>(unit.Target).Translation;
                const auto& targetUnit =
                    registry.get<RTSUnitComponent>(unit.Target);
                glm::vec3 delta = destination - transform.Translation;
                delta.y = 0.0f;
                const float distance = glm::length(delta);
                stopDistance = unit.AttackRange * 0.85f + targetUnit.TargetRadius;
                if (distance > stopDistance)
                {
                    const glm::vec3 direction = delta / distance;
                    const float step = std::min(unit.MoveSpeed * dt,
                        distance - stopDistance);
                    transform.Translation += direction * step;
                }
            }

            if (unit.Type == RTSUnitType::Harvester
                && unit.GatherTarget != entt::null
                && registry.valid(unit.GatherTarget)
                && registry.all_of<RTSResourceNodeComponent, TransformComponent>(
                    unit.GatherTarget))
            {
                const auto& nodeTransform =
                    registry.get<TransformComponent>(unit.GatherTarget);
                const glm::vec3 delta = nodeTransform.Translation - transform.Translation;
                const float distance = glm::length(glm::vec2(delta.x, delta.z));
                auto& node = registry.get<RTSResourceNodeComponent>(unit.GatherTarget);
                if (distance <= node.Radius + 0.6f)
                {
                    if (unit.HasMoveTarget || (agent && agent->HasPath))
                    {
                        unit.HasMoveTarget = false;
                        if (agent)
                        {
                            agent->Path.clear();
                            agent->PathIndex = 0;
                            agent->HasPath = false;
                        }
                    }

                    unit.GatherTimer += dt;
                    if (unit.GatherTimer >= unit.GatherInterval && match)
                    {
                        const float mined = std::min(unit.GatherAmount, node.Amount);
                        node.Amount -= mined;
                        match->Credits[static_cast<int>(unit.Team)] += mined;
                        unit.GatherTimer = 0.0f;
                        if (node.Amount <= 0.0f)
                            unit.GatherTarget = entt::null;
                    }

                    SetUnitState(unit, RTSUnitState::Idle, dt);
                    continue;
                }
            }

            if (!hasDestination)
            {
                SetUnitState(unit, RTSUnitState::Idle, dt);
                continue;
            }

            transform.Translation.x = glm::clamp(
                transform.Translation.x, -kGroundBounds, kGroundBounds);
            transform.Translation.z = glm::clamp(
                transform.Translation.z, -kGroundBounds, kGroundBounds);
            transform.Translation.y = RTS::GroundHeight(
                scene, transform.Translation.x, transform.Translation.z) + kUnitY;

            SetUnitState(unit, RTSUnitState::Move, dt);
        }

        // Local separation via a uniform spatial hash. This replaces the
        // previous O(n^2) pass; target acquisition is still O(n^2) and is the
        // next candidate for the same treatment.
        constexpr float kSeparationDistance = 0.9f;
        constexpr float kHashCellSize = 2.0f;

        std::vector<entt::entity> live;
        live.reserve(256);
        for (auto entity : units)
        {
            const auto& unit = units.get<RTSUnitComponent>(entity);
            if (unit.HP > 0.0f)
                live.push_back(entity);
        }

        std::unordered_map<uint64_t, std::vector<size_t>> buckets;
        buckets.reserve(live.size() * 2);
        const auto bucketKey = [](int x, int z) -> uint64_t
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
                | static_cast<uint64_t>(static_cast<uint32_t>(z));
        };

        for (size_t i = 0; i < live.size(); ++i)
        {
            const auto& transform = registry.get<TransformComponent>(live[i]);
            const int x = static_cast<int>(std::floor(transform.Translation.x / kHashCellSize));
            const int z = static_cast<int>(std::floor(transform.Translation.z / kHashCellSize));
            buckets[bucketKey(x, z)].push_back(i);
        }

        for (size_t i = 0; i < live.size(); ++i)
        {
            auto& a = registry.get<TransformComponent>(live[i]);
            const int cellX = static_cast<int>(std::floor(a.Translation.x / kHashCellSize));
            const int cellZ = static_cast<int>(std::floor(a.Translation.z / kHashCellSize));

            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const auto bucket = buckets.find(bucketKey(cellX + dx, cellZ + dz));
                    if (bucket == buckets.end())
                        continue;

                    for (size_t j : bucket->second)
                    {
                        if (j <= i)
                            continue;

                        auto& b = registry.get<TransformComponent>(live[j]);
                        glm::vec3 delta = b.Translation - a.Translation;
                        delta.y = 0.0f;
                        const float distanceSq = delta.x * delta.x + delta.z * delta.z;
                        if (distanceSq >= kSeparationDistance * kSeparationDistance)
                            continue;

                        const float distance = std::sqrt(std::max(distanceSq, 1e-5f));
                        const glm::vec3 direction = delta / distance;
                        const float push = (kSeparationDistance - distance) * 0.5f;
                        a.Translation -= direction * push;
                        b.Translation += direction * push;
                    }
                }
            }
        }

        // Resolve against nav blockers after separation. This is a cheap
        // circle-vs-blocked-cell push, not full physics, but it prevents units
        // from being shoved through rocks, walls, and buildings.
        NavGridComponent* nav = RTS::FindNavGrid(scene);
        for (entt::entity entity : live)
        {
            auto& transform = registry.get<TransformComponent>(entity);
            const auto* agent = registry.try_get<NavAgentComponent>(entity);
            const float radius = agent ? agent->Radius : 0.45f;

            if (nav && nav->Built)
            {
                nav->Grid.ResolveCircle(transform.Translation, radius);
            }
            else
            {
                transform.Translation.x = glm::clamp(
                    transform.Translation.x, -kGroundBounds, kGroundBounds);
                transform.Translation.z = glm::clamp(
                    transform.Translation.z, -kGroundBounds, kGroundBounds);
            }
            transform.Translation.y = RTS::GroundHeight(
                scene, transform.Translation.x, transform.Translation.z) + kUnitY;
        }

        for (auto entity : dead)
        {
            if (registry.valid(entity))
                scene->DestroyEntity(Entity(entity, scene));
        }
    }

    void RTSUnitSystem::UpdateCounts(Scene* scene, RTSMatchComponent& match)
    {
        match.PlayerCount = static_cast<int>(CountUnits(scene, RTSTeam::Player));
        match.EnemyCount = static_cast<int>(CountUnits(scene, RTSTeam::Enemy));
        match.Supply[static_cast<int>(RTSTeam::Player)] =
            RTS::CountSupply(scene, RTSTeam::Player);
        match.Supply[static_cast<int>(RTSTeam::Enemy)] =
            RTS::CountSupply(scene, RTSTeam::Enemy);

        if (match.Result != RTSMatchResult::InProgress)
            return;

        bool playerBaseExists = false;
        bool enemyBaseExists = false;
        bool playerBaseAlive = false;
        bool enemyBaseAlive = false;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<RTSUnitComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(entity);
            if (unit.Type != RTSUnitType::Building
                || unit.BuildingKind != RTSBuildingKind::Base)
            {
                continue;
            }

            if (unit.Team == RTSTeam::Player)
            {
                playerBaseExists = true;
                playerBaseAlive = playerBaseAlive || unit.HP > 0.0f;
            }
            else
            {
                enemyBaseExists = true;
                enemyBaseAlive = enemyBaseAlive || unit.HP > 0.0f;
            }
        }

        if (enemyBaseExists && !enemyBaseAlive)
        {
            match.Result = RTSMatchResult::Victory;
            WT_CORE_INFO("RTS match result: Victory");
            return;
        }
        if (playerBaseExists && !playerBaseAlive)
        {
            match.Result = RTSMatchResult::Defeat;
            WT_CORE_INFO("RTS match result: Defeat");
            return;
        }

        // Fallback for scenes that have no bases.
        if (!playerBaseExists && match.PlayerCount == 0)
        {
            match.Result = RTSMatchResult::Defeat;
            return;
        }
        if (!enemyBaseExists && !playerBaseExists && match.EnemyCount == 0)
            match.Result = RTSMatchResult::Victory;
    }

    int RTSUnitSystem::CountUnits(Scene* scene, RTSTeam team) const
    {
        int count = 0;
        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<RTSUnitComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(entity);
            if (unit.Team == team && unit.HP > 0.0f
                && unit.Type != RTSUnitType::Building)
            {
                ++count;
            }
        }
        return count;
    }

} // namespace Wheatear
