#pragma once

#include "RTSComponents.h"

#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Renderer/Mesh.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Wheatear::RTS {

    struct CameraView
    {
        entt::entity Entity = entt::null;
        TransformComponent* Transform = nullptr;
        CameraComponent* Camera = nullptr;
        RTSCameraComponent* RTS = nullptr;
        glm::mat4 ViewProjection = glm::mat4(1.0f);
        glm::vec3 Position = glm::vec3(0.0f);
    };

    inline bool FindCamera(Scene* scene, CameraView& out)
    {
        if (!scene)
            return false;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TransformComponent, CameraComponent, RTSCameraComponent>())
        {
            auto& camera = registry.get<CameraComponent>(e);
            if (!camera.Primary)
                continue;

            auto& transform = registry.get<TransformComponent>(e);
            out.Entity = e;
            out.Transform = &transform;
            out.Camera = &camera;
            out.RTS = &registry.get<RTSCameraComponent>(e);
            out.Position = transform.Translation;
            out.ViewProjection =
                camera.Camera.GetProjection() * glm::inverse(transform.GetTransform());
            return true;
        }
        return false;
    }

    inline bool ScreenRay(const glm::mat4& viewProjection,
        float mouseX, float mouseY,
        uint32_t width, uint32_t height,
        glm::vec3& origin, glm::vec3& direction)
    {
        if (width == 0 || height == 0)
            return false;

        const float ndcX = 2.0f * mouseX / static_cast<float>(width) - 1.0f;
        const float ndcY = 1.0f - 2.0f * mouseY / static_cast<float>(height);
        const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

        glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPoint.w) < 1e-6f || std::abs(farPoint.w) < 1e-6f)
            return false;

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        origin = glm::vec3(nearPoint);
        direction = glm::normalize(glm::vec3(farPoint - nearPoint));
        return true;
    }

    inline bool RayGround(const glm::vec3& origin, const glm::vec3& direction,
        float groundY, glm::vec3& hit)
    {
        if (std::abs(direction.y) < 1e-6f)
            return false;

        const float t = (groundY - origin.y) / direction.y;
        if (t < 0.0f)
            return false;

        hit = origin + direction * t;
        return true;
    }

    inline bool RaySphere(const glm::vec3& origin, const glm::vec3& direction,
        const glm::vec3& center, float radius, float& outT)
    {
        const glm::vec3 oc = center - origin;
        const float tca = glm::dot(oc, direction);
        if (tca < 0.0f)
            return false;

        const float d2 = glm::dot(oc, oc) - tca * tca;
        const float r2 = radius * radius;
        if (d2 > r2)
            return false;

        const float thc = std::sqrt(r2 - d2);
        float t = tca - thc;
        if (t < 0.0f)
            t = tca + thc;
        if (t < 0.0f)
            return false;

        outT = t;
        return true;
    }

    inline bool WorldToScreen(const glm::mat4& viewProjection, const glm::vec3& world,
        uint32_t width, uint32_t height, glm::vec2& screen)
    {
        const glm::vec4 clip = viewProjection * glm::vec4(world, 1.0f);
        if (clip.w <= 0.0f)
            return false;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        screen.x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
        screen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);
        return true;
    }

    inline NavGridComponent* FindNavGrid(Scene* scene)
    {
        if (!scene)
            return nullptr;
        auto& registry = scene->GetRegistry();
        auto view = registry.view<NavGridComponent>();
        return view.empty() ? nullptr : &view.get<NavGridComponent>(*view.begin());
    }

    inline TerrainComponent* FindTerrain(Scene* scene)
    {
        if (!scene)
            return nullptr;
        auto& registry = scene->GetRegistry();
        auto view = registry.view<TerrainComponent>();
        return view.empty() ? nullptr : &view.get<TerrainComponent>(*view.begin());
    }

    inline float GroundHeight(Scene* scene, float x, float z)
    {
        if (const NavGridComponent* nav = FindNavGrid(scene); nav && nav->Built)
            return nav->Grid.SampleHeight(x, z);
        if (const TerrainComponent* terrain = FindTerrain(scene))
            return terrain->SampleHeight(x, z);
        return 0.0f;
    }

    inline bool RayTerrain(Scene* scene, const glm::vec3& origin,
        const glm::vec3& direction, glm::vec3& hit)
    {
        if (const NavGridComponent* nav = FindNavGrid(scene); nav && nav->Built)
            return nav->Grid.Raycast(origin, direction, 400.0f, hit);
        return RayGround(origin, direction, 0.0f, hit);
    }

    inline RTSMatchComponent* FindMatch(Scene* scene)
    {
        if (!scene)
            return nullptr;
        auto& registry = scene->GetRegistry();
        auto view = registry.view<RTSMatchComponent>();
        return view.empty() ? nullptr : &view.get<RTSMatchComponent>(*view.begin());
    }

    inline RTSCommandStateComponent* FindCommandState(Scene* scene)
    {
        if (!scene)
            return nullptr;
        auto& registry = scene->GetRegistry();
        auto view = registry.view<RTSCommandStateComponent>();
        return view.empty()
            ? nullptr
            : &view.get<RTSCommandStateComponent>(*view.begin());
    }

    inline float UnitCost(RTSUnitType type)
    {
        switch (type)
        {
        case RTSUnitType::Harvester: return 75.0f;
        case RTSUnitType::Soldier:   return 50.0f;
        default:                     return 0.0f;
        }
    }

    inline float UnitBuildTime(RTSUnitType type)
    {
        return type == RTSUnitType::Harvester ? 4.0f : 2.5f;
    }

    inline int UnitSupplyCost(RTSUnitType type)
    {
        return type == RTSUnitType::Building ? 0 : 1;
    }

    inline int CountSupply(Scene* scene, RTSTeam team)
    {
        int used = 0;
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<RTSUnitComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(e);
            if (unit.Team == team && unit.HP > 0.0f
                && unit.Type != RTSUnitType::Building)
            {
                used += 1;
            }
        }
        return used;
    }

    inline RTSProductionComponent* FindProductionBuilding(Scene* scene,
        RTSTeam team)
    {
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<RTSProductionComponent, RTSUnitComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(e);
            auto& production = registry.get<RTSProductionComponent>(e);
            if (unit.Team == team && unit.HP > 0.0f
                && unit.BuildingKind == RTSBuildingKind::Barracks
                && production.Constructed)
            {
                return &production;
            }
        }
        return nullptr;
    }

    inline bool EnqueueUnit(Scene* scene, RTSTeam team, RTSUnitType type)
    {
        RTSMatchComponent* match = FindMatch(scene);
        RTSProductionComponent* production = FindProductionBuilding(scene, team);
        if (!match || !production)
            return false;

        const float cost = UnitCost(type);
        const int supply = UnitSupplyCost(type);
        const int teamIndex = static_cast<int>(team);
        if (match->Credits[teamIndex] < cost)
            return false;
        if (match->Supply[teamIndex] + supply > match->SupplyCap[teamIndex])
            return false;

        match->Credits[teamIndex] -= cost;
        production->Queue.push_back({ type, UnitBuildTime(type) });
        return true;
    }

    inline Entity SpawnBuilding(Scene* scene, RTSTeam team,
        RTSBuildingKind kind, const glm::vec3& position, const glm::vec3& scale,
        float hp, bool constructed, bool providesProduction,
        const glm::vec3& rallyPoint = glm::vec3(0.0f))
    {
        static Ref<Mesh> s_BuildingMesh = Mesh::CreateCube();

        Entity entity = scene->CreateEntity(
            kind == RTSBuildingKind::Base ? "Base" : "Barracks");
        auto& transform = entity.GetComponent<TransformComponent>();
        transform.Translation = position;
        transform.Scale = scale;

        auto& renderer = entity.AddComponent<MeshRendererComponent>();
        renderer.Mesh = s_BuildingMesh;
        renderer.Material = Material::Create();
        renderer.Material->Albedo = team == RTSTeam::Player
            ? glm::vec4(0.22f, 0.42f, 0.82f, 1.0f)
            : glm::vec4(0.86f, 0.24f, 0.20f, 1.0f);
        renderer.Material->Metallic = 0.1f;
        renderer.Material->Roughness = 0.72f;

        auto& unit = entity.AddComponent<RTSUnitComponent>();
        unit.Team = team;
        unit.Type = RTSUnitType::Building;
        unit.BuildingKind = kind;
        unit.HP = unit.MaxHP = hp;
        unit.MoveSpeed = 0.0f;
        unit.AggroRange = 0.0f;
        unit.AttackRange = 0.0f;
        unit.AttackDamage = 0.0f;
        unit.VisualScale = scale;
        unit.TargetRadius = std::max(scale.x, scale.z) * 0.5f;
        unit.State = RTSUnitState::Idle;

        entity.AddComponent<NavObstacleComponent>();

        if (providesProduction)
        {
            auto& production = entity.AddComponent<RTSProductionComponent>();
            production.Team = team;
            production.IsBuilding = true;
            production.Constructed = constructed;
            production.ConstructionTime = constructed ? 0.0f : 3.0f;
            production.ConstructionProgress = constructed ? 1.0f : 0.0f;
            production.RallyPoint = rallyPoint;
        }

        return entity;
    }

    inline void SetDestination(Scene* scene, entt::entity entity,
        const glm::vec3& destination)
    {
        auto& registry = scene->GetRegistry();
        if (!registry.valid(entity))
            return;

        auto& unit = registry.get<RTSUnitComponent>(entity);
        unit.MoveTarget = destination;
        unit.HasMoveTarget = true;
        unit.Target = entt::null;

        auto* agent = registry.try_get<NavAgentComponent>(entity);
        if (!agent)
            agent = &registry.emplace<NavAgentComponent>(entity);
        agent->Path.clear();
        agent->PathIndex = 0;
        agent->HasPath = false;

        if (NavGridComponent* nav = FindNavGrid(scene); nav && nav->Built)
        {
            glm::vec3 start = registry.get<TransformComponent>(entity).Translation;
            start.y = nav->Grid.SampleHeight(start.x, start.z);
            if (nav->Grid.FindPath(start, destination, agent->Path))
            {
                agent->PathIndex = 0;
                agent->HasPath = true;
            }
        }
    }

} // namespace Wheatear::RTS
