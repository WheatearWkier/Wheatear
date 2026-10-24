#pragma once

#include "entt.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Wheatear {

    enum class RTSTeam : uint8_t
    {
        Player = 0,
        Enemy = 1
    };

    enum class RTSUnitState : uint8_t
    {
        Idle = 0,
        Move,
        Attack,
        Dying
    };

    enum class RTSUnitType : uint8_t
    {
        Soldier = 0,
        Harvester,
        Building
    };

    enum class RTSBuildingKind : uint8_t
    {
        None = 0,
        Base,
        Barracks
    };

    struct RTSProductionOrder
    {
        RTSUnitType Type = RTSUnitType::Soldier;
        float Remaining = 0.0f;
    };

    enum class RTSMatchResult : uint8_t
    {
        InProgress = 0,
        Victory,
        Defeat
    };

    // One entity per unit for the first vertical slice. This is intentionally
    // the "slow baseline": RenderSystem draws each unit as its own mesh, and
    // targeting below is the straightforward O(n^2) pass.
    struct RTSUnitComponent
    {
        RTSTeam Team = RTSTeam::Player;
        RTSUnitType Type = RTSUnitType::Soldier;
        RTSBuildingKind BuildingKind = RTSBuildingKind::None;
        float HP = 100.0f;
        float MaxHP = 100.0f;
        float MoveSpeed = 4.0f;
        float AggroRange = 22.0f;
        float AttackRange = 2.0f;
        float AttackDamage = 10.0f;
        float AttackInterval = 1.0f;
        float AttackTimer = 0.0f;
        float TargetRadius = 0.5f;

        entt::entity Target = entt::null;
        glm::vec3 MoveTarget = glm::vec3(0.0f);
        bool HasMoveTarget = false;
        bool Selected = false;

        glm::vec4 TeamColor = glm::vec4(1.0f);

        // Presentation state. The unit system owns transitions; the RTS
        // animation system maps these to skeletal clips or a greybox scale
        // fallback.
        RTSUnitState State = RTSUnitState::Idle;
        float StateTime = 0.0f;
        float DeathTimer = 0.0f;
        float DeathDuration = 0.8f;
        glm::vec3 VisualScale = glm::vec3(0.8f);

        std::string IdleClip = "Idle";
        std::string MoveClip = "Walk";
        std::string AttackClip = "Attack";
        std::string DeathClip = "Death";

        // Harvester loop.
        entt::entity GatherTarget = entt::null;
        float GatherTimer = 0.0f;
        float GatherInterval = 1.5f;
        float GatherAmount = 15.0f;

        RTSUnitComponent() = default;
        RTSUnitComponent(const RTSUnitComponent&) = default;
    };

    // Production / construction state for bases and barracks.
    struct RTSProductionComponent
    {
        RTSTeam Team = RTSTeam::Player;
        bool IsBuilding = true;
        bool Constructed = true;
        float ConstructionTime = 0.0f;
        float ConstructionProgress = 1.0f;
        float ProductionTime = 2.5f;
        std::vector<RTSProductionOrder> Queue;
        glm::vec3 RallyPoint = glm::vec3(0.0f);

        RTSProductionComponent() = default;
        RTSProductionComponent(const RTSProductionComponent&) = default;
    };

    // Neutral harvestable resource node. Harvesters mine in place; a future
    // pass can add depot trips and worker carry capacity.
    struct RTSResourceNodeComponent
    {
        float Amount = 1500.0f;
        float MaxAmount = 1500.0f;
        float Radius = 2.5f;

        RTSResourceNodeComponent() = default;
        RTSResourceNodeComponent(const RTSResourceNodeComponent&) = default;
    };

    // UI -> player-system command channel (currently only build placement).
    struct RTSCommandStateComponent
    {
        bool BuildBarracks = false;

        RTSCommandStateComponent() = default;
        RTSCommandStateComponent(const RTSCommandStateComponent&) = default;
    };

    struct RTSAIControllerComponent
    {
        float TrainTimer = 3.0f;
        float AttackTimer = 25.0f;
        float PassiveIncome = 10.0f;

        RTSAIControllerComponent() = default;
        RTSAIControllerComponent(const RTSAIControllerComponent&) = default;
    };

    struct RTSCameraComponent
    {
        glm::vec3 Target = glm::vec3(0.0f, 0.0f, 0.0f);
        float Distance = 40.0f;
        float MinDistance = 12.0f;
        float MaxDistance = 70.0f;
        float Yaw = 0.785398185f;   // 45 degrees
        float Pitch = 0.907571196f; // 52 degrees

        float PanSpeed = 22.0f;
        float RotateSpeed = 0.006f;
        float ZoomStep = 2.0f;
        float ZoomDelta = 0.0f;

        glm::vec3 BoundsMin = glm::vec3(-28.0f, 0.0f, -28.0f);
        glm::vec3 BoundsMax = glm::vec3(28.0f, 0.0f, 28.0f);

        RTSCameraComponent() = default;
        RTSCameraComponent(const RTSCameraComponent&) = default;
    };

    struct RTSMatchComponent
    {
        int Wave = 0;
        int MaxWaves = 3;
        float NextWaveTimer = 3.0f;
        bool WaveActive = false;
        RTSMatchResult Result = RTSMatchResult::InProgress;
        int PlayerCount = 0;
        int EnemyCount = 0;

        float Credits[2] = { 400.0f, 350.0f };
        int Supply[2] = { 0, 0 };
        int SupplyCap[2] = { 60, 60 };

        RTSMatchComponent() = default;
        RTSMatchComponent(const RTSMatchComponent&) = default;
    };

    // Singleton runtime state consumed by the HUD. Kept as a component so the
    // gameplay systems and RTSGameLayer both read it through the scene.
    struct RTSSelectionStateComponent
    {
        bool Dragging = false;
        glm::vec2 DragStart = glm::vec2(0.0f);
        glm::vec2 DragEnd = glm::vec2(0.0f);
        int SelectedCount = 0;

        RTSSelectionStateComponent() = default;
        RTSSelectionStateComponent(const RTSSelectionStateComponent&) = default;
    };

} // namespace Wheatear
