#include "wtpch.h"
#include "ModeSelectLayer.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/EngineInfo.h"

#include "EditorLayer2D.h"
#include "EditorLayer3D.h"

#include <imgui/imgui.h>

namespace Wheatear {

    ModeSelectLayer::ModeSelectLayer()
        : Layer("ModeSelectLayer")
    {
    }

    void ModeSelectLayer::OnAttach()
    {
    }

    void ModeSelectLayer::OnImGuiRender()
    {
        if (m_Decided)
            return;

        // =====================================================================
        // =====================================================================

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            ImVec4(0.08f, 0.08f, 0.08f, 0.95f));

        ImGui::Begin(
            "##ModeSelectBg",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // =====================================================================
        // =====================================================================

        ImVec2 center = ImVec2(
            viewport->Pos.x + viewport->Size.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f
        );

        ImGui::SetNextWindowPos(
            center,
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f));

        ImGui::SetNextWindowSize(
            ImVec2(960, 600),
            ImGuiCond_Always);

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            12.0f);

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(32, 32));

        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing,
            ImVec2(16, 20));

        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            ImVec4(0.13f, 0.14f, 0.16f, 1.0f));

        ImGui::Begin(
            "##ModeSelect",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);

        // =====================================================================
        // =====================================================================

        ImGui::SetWindowFontScale(1.8f);

        const char* title = EngineInfo::EditorName;

        float titleWidth = ImGui::CalcTextSize(title).x;

        ImGui::SetCursorPosX(
            (ImGui::GetContentRegionAvail().x - titleWidth) * 0.5f);

        ImGui::PushStyleColor(
            ImGuiCol_Text,
            ImVec4(0.95f, 0.78f, 0.25f, 1.0f));

        ImGui::Text("%s", title);

        ImGui::PopStyleColor();

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Separator();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        ImGui::SetWindowFontScale(1.3f);

        ImGui::TextWrapped(
            "Select the editing mode to launch:");

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        // =====================================================================
        // =====================================================================

        const ImVec2 buttonSize =
        {
            ImGui::GetContentRegionAvail().x,
            84.0f
        };

        // =====================================================================
        // =====================================================================

        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            ImVec2(12, 12));

        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.18f, 0.38f, 0.62f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.25f, 0.50f, 0.80f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            ImVec4(0.15f, 0.30f, 0.55f, 1.0f));

        if (ImGui::Button(
            "2D Mode\nSprites | Physics | Animation",
            buttonSize))
        {
            LaunchEditor2D();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::Dummy(ImVec2(0.0f, 18.0f));

        // =====================================================================
        // =====================================================================

        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            ImVec2(12, 12));

        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.22f, 0.48f, 0.30f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.30f, 0.62f, 0.40f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            ImVec4(0.18f, 0.40f, 0.25f, 1.0f));

        if (ImGui::Button(
            "3D Mode\nMesh | PBR / IBL | Lighting",
            buttonSize))
        {
            LaunchEditor3D();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        // =====================================================================

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    // =========================================================================
    // =========================================================================

    void ModeSelectLayer::LaunchEditor2D()
    {
        if (m_Decided)
            return;

        m_Decided = true;

        Application::Get().PushLayer(new EditorLayer2D());
        Application::Get().PopLayer(this);
    }

    void ModeSelectLayer::LaunchEditor3D()
    {
        if (m_Decided)
            return;

        m_Decided = true;

        Application::Get().PushLayer(new EditorLayer3D());
        Application::Get().PopLayer(this);
    }

} // namespace Wheatear
