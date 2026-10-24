#include "wtpch.h"
#include "RTSPlayerSystem.h"

#include "RTSComponents.h"
#include "RTSUtilities.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Systems/NavigationSystem.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Wheatear {

    namespace {

        constexpr float kPickRadius = 0.65f;
        constexpr float kFormationSpacing = 1.45f;
        constexpr float kSelectionDragThreshold = 6.0f;
        constexpr float kMoveBounds = 28.0f;
        const glm::vec4 kSelectedColor(1.0f, 0.85f, 0.15f, 1.0f);

        RTSSelectionStateComponent* FindSelectionState(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            auto view = registry.view<RTSSelectionStateComponent>();
            if (view.empty())
                return nullptr;
            return &view.get<RTSSelectionStateComponent>(*view.begin());
        }

        void SetSelected(Scene* scene, entt::entity entity, bool selected)
        {
            auto& registry = scene->GetRegistry();
            if (!registry.valid(entity))
                return;
            if (!registry.all_of<RTSUnitComponent, MeshRendererComponent>(entity))
                return;

            auto& unit = registry.get<RTSUnitComponent>(entity);
            if (unit.Team != RTSTeam::Player || unit.Type == RTSUnitType::Building)
                return;

            unit.Selected = selected;
            auto& renderer = registry.get<MeshRendererComponent>(entity);
            if (renderer.Material)
                renderer.Material->Albedo = selected ? kSelectedColor : unit.TeamColor;
        }

        void ClearSelection(Scene* scene)
        {
            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<RTSUnitComponent>())
                SetSelected(scene, entity, false);
        }

        int CountSelected(Scene* scene)
        {
            int count = 0;
            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<RTSUnitComponent>())
            {
                const auto& unit = registry.get<RTSUnitComponent>(entity);
                if (unit.Team == RTSTeam::Player && unit.Selected && unit.HP > 0.0f
                    && unit.Type != RTSUnitType::Building)
                    ++count;
            }
            return count;
        }

        entt::entity PickPlayerUnit(Scene* scene,
            const glm::vec3& origin, const glm::vec3& direction)
        {
            entt::entity best = entt::null;
            float bestT = std::numeric_limits<float>::max();

            auto& registry = scene->GetRegistry();
            auto units = registry.view<RTSUnitComponent, TransformComponent>();
            for (auto entity : units)
            {
                const auto& unit = units.get<RTSUnitComponent>(entity);
                if (unit.Team != RTSTeam::Player || unit.HP <= 0.0f
                    || unit.Type == RTSUnitType::Building)
                    continue;

                const auto& transform = units.get<TransformComponent>(entity);
                float t = 0.0f;
                if (RTS::RaySphere(origin, direction, transform.Translation, kPickRadius, t)
                    && t < bestT)
                {
                    bestT = t;
                    best = entity;
                }
            }
            return best;
        }

        void SelectBox(Scene* scene, const RTS::CameraView& camera,
            const glm::vec2& min, const glm::vec2& max,
            uint32_t width, uint32_t height, bool additive)
        {
            if (!additive)
                ClearSelection(scene);

            auto& registry = scene->GetRegistry();
            auto units = registry.view<RTSUnitComponent, TransformComponent>();
            for (auto entity : units)
            {
                const auto& unit = units.get<RTSUnitComponent>(entity);
                if (unit.Team != RTSTeam::Player || unit.HP <= 0.0f
                    || unit.Type == RTSUnitType::Building)
                    continue;

                glm::vec2 screen;
                const auto& transform = units.get<TransformComponent>(entity);
                if (!RTS::WorldToScreen(camera.ViewProjection, transform.Translation,
                        width, height, screen))
                    continue;

                if (screen.x >= min.x && screen.x <= max.x
                    && screen.y >= min.y && screen.y <= max.y)
                {
                    SetSelected(scene, entity, true);
                }
            }
        }

        void IssueMove(Scene* scene, const glm::vec3& hit)
        {
            std::vector<entt::entity> selected;
            auto& registry = scene->GetRegistry();
            auto units = registry.view<RTSUnitComponent>();
            for (auto entity : units)
            {
                const auto& unit = units.get<RTSUnitComponent>(entity);
                if (unit.Team == RTSTeam::Player && unit.Selected && unit.HP > 0.0f
                    && unit.Type != RTSUnitType::Building)
                    selected.push_back(entity);
            }

            if (selected.empty())
                return;

            const int count = static_cast<int>(selected.size());
            const int columns = static_cast<int>(
                std::ceil(std::sqrt(static_cast<float>(count))));
            const int rows = (count + columns - 1) / columns;

            // Orient the formation with the group's movement direction instead
            // of always using world axes.
            glm::vec3 centroid(0.0f);
            for (entt::entity entity : selected)
                centroid += registry.get<TransformComponent>(entity).Translation;
            centroid /= static_cast<float>(count);

            glm::vec3 forward = hit - centroid;
            forward.y = 0.0f;
            if (glm::length(forward) < 0.1f)
                forward = glm::vec3(0.0f, 0.0f, -1.0f);
            forward = glm::normalize(forward);
            const glm::vec3 right(forward.z, 0.0f, -forward.x);

            const NavGridComponent* nav = RTS::FindNavGrid(scene);
            std::vector<glm::vec3> slots;
            slots.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                const int column = i % columns;
                const int row = i / columns;
                const float lateral =
                    (static_cast<float>(column) - (columns - 1) * 0.5f)
                    * kFormationSpacing;
                const float depth =
                    (static_cast<float>(row) - (rows - 1) * 0.5f)
                    * kFormationSpacing;

                glm::vec3 target = hit + right * lateral - forward * depth;
                target.x = glm::clamp(target.x, -kMoveBounds, kMoveBounds);
                target.z = glm::clamp(target.z, -kMoveBounds, kMoveBounds);

                if (nav && nav->Built && !nav->Grid.IsWalkableWorld(target))
                    target = nav->Grid.ClampToWalkable(target);
                target.y = 0.0f;
                slots.push_back(target);
            }

            // Greedy nearest-slot assignment avoids sending units across the
            // formation to reach an arbitrary index-based slot.
            std::vector<uint8_t> used(slots.size(), 0);
            for (entt::entity entity : selected)
            {
                const glm::vec3 position =
                    registry.get<TransformComponent>(entity).Translation;

                size_t bestSlot = slots.size();
                float bestDistance = std::numeric_limits<float>::max();
                for (size_t i = 0; i < slots.size(); ++i)
                {
                    if (used[i])
                        continue;
                    const glm::vec3 delta = slots[i] - position;
                    const float distanceSq = delta.x * delta.x + delta.z * delta.z;
                    if (distanceSq < bestDistance)
                    {
                        bestDistance = distanceSq;
                        bestSlot = i;
                    }
                }

                if (bestSlot == slots.size())
                    break;
                used[bestSlot] = 1;
                RTS::SetDestination(scene, entity, slots[bestSlot]);
            }
        }

    } // namespace

    void RTSPlayerSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        (void)ts;

        RTS::CameraView camera;
        if (!RTS::FindCamera(scene, camera))
            return;

        RTSSelectionStateComponent* selectionState = FindSelectionState(scene);

        auto& registry = scene->GetRegistry();
        auto matchView = registry.view<RTSMatchComponent>();
        if (!matchView.empty())
        {
            const auto& match = matchView.get<RTSMatchComponent>(*matchView.begin());
            if (match.Result != RTSMatchResult::InProgress)
            {
                m_Dragging = false;
                m_LeftDownLast = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
                m_RightDownLast = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_RIGHT);
                if (selectionState)
                {
                    selectionState->Dragging = false;
                    selectionState->SelectedCount = CountSelected(scene);
                }
                return;
            }
        }

        const bool trainSoldier = Input::IsKeyPressed(WT_KEY_1);
        if (trainSoldier && !m_TrainSoldierLast)
            RTS::EnqueueUnit(scene, RTSTeam::Player, RTSUnitType::Soldier);
        m_TrainSoldierLast = trainSoldier;

        const bool trainHarvester = Input::IsKeyPressed(WT_KEY_2);
        if (trainHarvester && !m_TrainHarvesterLast)
            RTS::EnqueueUnit(scene, RTSTeam::Player, RTSUnitType::Harvester);
        m_TrainHarvesterLast = trainHarvester;

        const bool buildKey = Input::IsKeyPressed(WT_KEY_B);
        if (buildKey && !m_BuildLast)
        {
            if (RTSCommandStateComponent* command = RTS::FindCommandState(scene))
                command->BuildBarracks = !command->BuildBarracks;
        }
        m_BuildLast = buildKey;

        Window& window = Application::Get().GetWindow();
        const uint32_t width = window.GetWidth();
        const uint32_t height = window.GetHeight();
        if (width == 0 || height == 0)
            return;

        // Greybox navigation smoke test: select the whole army and path it
        // across the map, around the rocks.
        const bool f4Down = Input::IsKeyPressed(WT_KEY_F4);
        if (f4Down && !m_F4DownLast)
        {
            for (auto entity : registry.view<RTSUnitComponent>())
            {
                const auto& unit = registry.get<RTSUnitComponent>(entity);
                if (unit.Team == RTSTeam::Player && unit.HP > 0.0f)
                    SetSelected(scene, entity, true);
            }
            IssueMove(scene, glm::vec3(18.0f, 0.0f, 18.0f));
        }
        m_F4DownLast = f4Down;

        const auto mousePosition = Input::GetMousePosition();
        const glm::vec2 mouse(mousePosition.first, mousePosition.second);
        const bool leftDown = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
        const bool rightDown = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_RIGHT);
        const bool additive = Input::IsKeyPressed(WT_KEY_LEFT_SHIFT)
            || Input::IsKeyPressed(WT_KEY_RIGHT_SHIFT);

        RTSCommandStateComponent* command = RTS::FindCommandState(scene);
        if (command && command->BuildBarracks)
        {
            if (rightDown && !m_RightDownLast
                || Input::IsKeyPressed(WT_KEY_ESCAPE))
            {
                command->BuildBarracks = false;
                m_LeftDownLast = leftDown;
                m_RightDownLast = rightDown;
                return;
            }

            if (leftDown && !m_LeftDownLast)
            {
                glm::vec3 origin(0.0f);
                glm::vec3 direction(0.0f, 0.0f, -1.0f);
                glm::vec3 hit(0.0f);
                RTSMatchComponent* match = RTS::FindMatch(scene);
                NavGridComponent* nav = RTS::FindNavGrid(scene);
                const int playerIndex = static_cast<int>(RTSTeam::Player);
                constexpr float kBarracksCost = 150.0f;

                if (match && match->Credits[playerIndex] >= kBarracksCost
                    && RTS::ScreenRay(camera.ViewProjection, mouse.x, mouse.y,
                        width, height, origin, direction)
                    && RTS::RayTerrain(scene, origin, direction, hit)
                    && nav && nav->Built
                    && nav->Grid.IsWalkableWorld(hit))
                {
                    match->Credits[playerIndex] -= kBarracksCost;
                    RTS::SpawnBuilding(scene, RTSTeam::Player,
                        RTSBuildingKind::Barracks,
                        hit + glm::vec3(0.0f, 1.5f, 0.0f),
                        glm::vec3(4.0f, 3.0f, 4.0f), 600.0f,
                        false, true, hit + glm::vec3(0.0f, 0.0f, 4.0f));
                    NavigationSystem::Rebuild(scene);
                    command->BuildBarracks = false;
                }
            }

            m_LeftDownLast = leftDown;
            m_RightDownLast = rightDown;
            return;
        }

        if (leftDown && !m_LeftDownLast)
        {
            m_Dragging = true;
            m_DragStart = mouse;
            m_DragEnd = mouse;
        }

        if (leftDown && m_Dragging)
            m_DragEnd = mouse;

        if (!leftDown && m_LeftDownLast && m_Dragging)
        {
            m_Dragging = false;

            if (glm::length(m_DragEnd - m_DragStart) < kSelectionDragThreshold)
            {
                glm::vec3 origin(0.0f);
                glm::vec3 direction(0.0f, 0.0f, -1.0f);
                if (RTS::ScreenRay(camera.ViewProjection, mouse.x, mouse.y,
                        width, height, origin, direction))
                {
                    const entt::entity picked = PickPlayerUnit(scene, origin, direction);
                    if (!additive)
                        ClearSelection(scene);
                    if (picked != entt::null)
                        SetSelected(scene, picked, true);
                }
            }
            else
            {
                const glm::vec2 min = glm::min(m_DragStart, m_DragEnd);
                const glm::vec2 max = glm::max(m_DragStart, m_DragEnd);
                SelectBox(scene, camera, min, max, width, height, additive);
            }
        }

        if (rightDown && !m_RightDownLast)
        {
            glm::vec3 origin(0.0f);
            glm::vec3 direction(0.0f, 0.0f, -1.0f);
            glm::vec3 hit(0.0f);
            if (RTS::ScreenRay(camera.ViewProjection, mouse.x, mouse.y,
                    width, height, origin, direction)
                && RTS::RayTerrain(scene, origin, direction, hit))
            {
                IssueMove(scene, hit);
            }
        }

        m_LeftDownLast = leftDown;
        m_RightDownLast = rightDown;

        if (selectionState)
        {
            selectionState->Dragging = m_Dragging;
            selectionState->DragStart = m_DragStart;
            selectionState->DragEnd = m_DragEnd;
            selectionState->SelectedCount = CountSelected(scene);
        }
    }

} // namespace Wheatear
