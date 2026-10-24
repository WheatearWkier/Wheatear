#include "RTSGameLayer.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Events/ApplicationEvent.h"
#include "Wheatear/Events/MouseEvent.h"
#include "Wheatear/Modules/RTS/RTSComponents.h"
#include "Wheatear/Modules/RTS/RTSModule.h"
#include "Wheatear/Modules/RTS/RTSUtilities.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer3D.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneSerializer.h"

#include <imgui/imgui.h>

#include <glm/glm.hpp>

#include <chrono>
#include <cmath>

namespace Wheatear {

    namespace {

        const char* ResultToString(RTSMatchResult result)
        {
            switch (result)
            {
            case RTSMatchResult::Victory: return "Victory";
            case RTSMatchResult::Defeat: return "Defeat";
            default: return "In progress";
            }
        }

    } // namespace

    RTSGameLayer::RTSGameLayer()
        : Layer("RTSGameLayer")
    {
    }

    void RTSGameLayer::OnAttach()
    {
        RegisterRTSModule();
        LoadScene();
    }

    void RTSGameLayer::OnDetach()
    {
        if (m_Scene && m_RuntimeStarted)
        {
            m_Scene->OnRuntimeStop();
            m_RuntimeStarted = false;
        }
        m_Scene.reset();
    }

    void RTSGameLayer::LoadScene()
    {
        if (m_Scene && m_RuntimeStarted)
        {
            m_Scene->OnRuntimeStop();
            m_RuntimeStarted = false;
        }

        m_ScenePath = AssetPath::ResolveRuntimeData("assets/scenes/RTSVerticalSlice.wt");
        m_Scene = CreateRef<Scene>();

        SceneSerializer serializer(m_Scene);
        if (!serializer.DeserializeYaml(m_ScenePath))
        {
            WT_CORE_ERROR("RTSGameLayer: failed to load '{}'", m_ScenePath.string());
            m_Scene.reset();
            return;
        }

        m_ViewportWidth = 0;
        m_ViewportHeight = 0;
        UpdateViewport(true);

        Entity camera = m_Scene->GetPrimaryCameraEntity();
        if (camera)
        {
            auto& cameraComponent = camera.GetComponent<CameraComponent>();
            cameraComponent.Camera.SetPerspective(glm::radians(45.0f), 0.1f, 1000.0f);

            if (!camera.HasComponent<RTSCameraComponent>())
                camera.AddComponent<RTSCameraComponent>();

            auto& rtsCamera = camera.GetComponent<RTSCameraComponent>();
            rtsCamera.Target = glm::vec3(0.0f, 0.0f, 0.0f);
            rtsCamera.Distance = 40.0f;
            rtsCamera.Yaw = glm::radians(45.0f);
            rtsCamera.Pitch = glm::radians(52.0f);
        }

        // RTSUnitSystem spawns the player formation and the match controller
        // during OnRuntimeStart, after all systems are configured.
        m_Scene->OnRuntimeStart();
        m_RuntimeStarted = true;

        WT_CORE_INFO("RTSGameLayer: loaded '{}'", m_ScenePath.string());
    }

    void RTSGameLayer::Restart()
    {
        LoadScene();
    }

    void RTSGameLayer::UpdateViewport(bool force)
    {
        Window& window = Application::Get().GetWindow();
        const uint32_t width = window.GetWidth();
        const uint32_t height = window.GetHeight();
        if (width == 0 || height == 0)
            return;

        if (!force && width == m_ViewportWidth && height == m_ViewportHeight)
            return;

        m_ViewportWidth = width;
        m_ViewportHeight = height;

        RenderCommand::SetViewport(0, 0, width, height);
        if (m_Scene)
            m_Scene->OnViewportResize(width, height);
    }

    void RTSGameLayer::OnUpdate(Timestep ts)
    {
        if (m_RestartRequested)
        {
            m_RestartRequested = false;
            Restart();
        }

        if (!m_Scene)
            return;

        UpdateViewport();

        RenderCommand::SetClearColor({ 0.05f, 0.06f, 0.08f, 1.0f });
        RenderCommand::Clear();

        const auto start = std::chrono::steady_clock::now();
        m_Scene->OnUpdateRuntime(ts);
        const auto end = std::chrono::steady_clock::now();
        m_UpdateMilliseconds =
            std::chrono::duration<double, std::milli>(end - start).count();
    }

    void RTSGameLayer::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);

        dispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent&)
            {
                UpdateViewport();
                return false;
            });

        dispatcher.Dispatch<MouseScrolledEvent>(
            [this](MouseScrolledEvent& e)
            {
                if (!m_Scene)
                    return false;

                Entity camera = m_Scene->GetPrimaryCameraEntity();
                if (camera && camera.HasComponent<RTSCameraComponent>())
                    camera.GetComponent<RTSCameraComponent>().ZoomDelta += e.GetYOffset();
                return false;
            });
    }

    void RTSGameLayer::OnImGuiRender()
    {
        if (!m_Scene)
            return;

        auto& registry = m_Scene->GetRegistry();

        int playerCount = 0;
        int enemyCount = 0;
        int idleCount = 0;
        int moveCount = 0;
        int attackCount = 0;
        int dyingCount = 0;
        for (auto entity : registry.view<RTSUnitComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(entity);
            switch (unit.State)
            {
            case RTSUnitState::Idle: ++idleCount; break;
            case RTSUnitState::Move: ++moveCount; break;
            case RTSUnitState::Attack: ++attackCount; break;
            case RTSUnitState::Dying: ++dyingCount; break;
            }

            if (unit.HP <= 0.0f || unit.Type == RTSUnitType::Building)
                continue;
            if (unit.Team == RTSTeam::Player)
                ++playerCount;
            else if (unit.Team == RTSTeam::Enemy)
                ++enemyCount;
        }

        RTSSelectionStateComponent* selection = nullptr;
        auto selectionView = registry.view<RTSSelectionStateComponent>();
        if (!selectionView.empty())
            selection = &selectionView.get<RTSSelectionStateComponent>(*selectionView.begin());

        RTSMatchComponent* match = nullptr;
        auto matchView = registry.view<RTSMatchComponent>();
        if (!matchView.empty())
            match = &matchView.get<RTSMatchComponent>(*matchView.begin());

        const int selectedCount = selection ? selection->SelectedCount : 0;
        const int entityCount = static_cast<int>(registry.view<TagComponent>().size());
        const auto stats = Renderer3D::GetStats();
        const float fps = ImGui::GetIO().Framerate;

        ImGui::Begin("Wheatear RTS - Movement & States");
        ImGui::Text("FPS: %.1f   Frame: %.2f ms", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);
        ImGui::Text("Scene update+render CPU: %.2f ms", m_UpdateMilliseconds);
        ImGui::Text("3D draw calls (main pass): %u   Meshes: %u",
            stats.DrawCalls, stats.MeshCount);
        ImGui::Text("Entities: %d   Player: %d   Enemy: %d   Selected: %d",
            entityCount, playerCount, enemyCount, selectedCount);
        ImGui::Text("States: Idle %d   Move %d   Attack %d   Dying %d",
            idleCount, moveCount, attackCount, dyingCount);

        if (match)
        {
            const int playerIndex = static_cast<int>(RTSTeam::Player);
            ImGui::Text("Credits: %.0f   Supply: %d / %d",
                match->Credits[playerIndex],
                match->Supply[playerIndex], match->SupplyCap[playerIndex]);

            RTSProductionComponent* production =
                RTS::FindProductionBuilding(m_Scene.get(), RTSTeam::Player);
            if (production)
            {
                ImGui::Text("Barracks queue: %d", static_cast<int>(production->Queue.size()));
            }

            if (ImGui::Button("Train Soldier - 50"))
                RTS::EnqueueUnit(m_Scene.get(), RTSTeam::Player, RTSUnitType::Soldier);
            ImGui::SameLine();
            if (ImGui::Button("Train Harvester - 75"))
                RTS::EnqueueUnit(m_Scene.get(), RTSTeam::Player, RTSUnitType::Harvester);
            ImGui::SameLine();
            if (ImGui::Button("Build Barracks - 150"))
            {
                if (RTSCommandStateComponent* command =
                        RTS::FindCommandState(m_Scene.get()))
                {
                    command->BuildBarracks = true;
                }
            }

            RTSCommandStateComponent* command =
                RTS::FindCommandState(m_Scene.get());
            if (command && command->BuildBarracks)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                    "Click ground to place a Barracks (RMB/Esc to cancel)");
            }

            if (match->Result != RTSMatchResult::InProgress)
            {
                ImGui::Text("Result: %s", ResultToString(match->Result));
                if (ImGui::Button("Restart"))
                    m_RestartRequested = true;
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("LMB click / drag: select");
        ImGui::TextUnformatted("RMB: move selected");
        ImGui::TextUnformatted("WASD / arrows: pan   Wheel: zoom   MMB drag: rotate");
        ImGui::TextUnformatted("1: soldier   2: harvester   B: build barracks");
        ImGui::TextUnformatted("F3: nav debug   F4: select all + path across map");
        ImGui::TextUnformatted("Harvest gold nodes to fund training; destroy the enemy base");
        ImGui::End();

        if (selection && selection->Dragging)
        {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            const ImVec2 p0(selection->DragStart.x, selection->DragStart.y);
            const ImVec2 p1(selection->DragEnd.x, selection->DragEnd.y);
            drawList->AddRectFilled(p0, p1, IM_COL32(70, 140, 255, 35));
            drawList->AddRect(p0, p1, IM_COL32(120, 190, 255, 220), 0.0f, 0, 1.5f);
        }
    }

} // namespace Wheatear
