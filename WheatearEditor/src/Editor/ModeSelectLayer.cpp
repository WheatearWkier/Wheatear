#include "wepch.h"
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
            ImVec4(0.050f, 0.055f, 0.068f, 1.0f));

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
            ImVec4(0.125f, 0.140f, 0.168f, 1.0f));

        ImGui::Begin(
            "##ModeSelect",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);

        // =====================================================================
        // =====================================================================

        // Engine monogram: rounded teal tile with the initial, drawn in code so
        // the launcher needs no external asset and matches the editor theme.
        const float logoSize = 76.0f;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 logoOrigin = ImGui::GetCursorScreenPos();
        const ImVec2 logoMin(logoOrigin.x + (availWidth - logoSize) * 0.5f, logoOrigin.y);
        const ImVec2 logoMax(logoMin.x + logoSize, logoMin.y + logoSize);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            logoMin, logoMax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.230f, 0.720f, 0.800f, 1.0f)),
            16.0f);
        ImGui::SetWindowFontScale(2.8f);
        const ImVec2 markSize = ImGui::CalcTextSize("W");
        drawList->AddText(
            ImVec2(logoMin.x + (logoSize - markSize.x) * 0.5f,
                logoMin.y + (logoSize - markSize.y) * 0.5f),
            IM_COL32(9, 11, 15, 255),
            "W");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, logoSize + 10.0f));

        ImGui::SetWindowFontScale(1.8f);

        const char* title = EngineInfo::EditorName;

        float titleWidth = ImGui::CalcTextSize(title).x;

        ImGui::SetCursorPosX(
            (ImGui::GetContentRegionAvail().x - titleWidth) * 0.5f);

        ImGui::PushStyleColor(
            ImGuiCol_Text,
            ImVec4(0.850f, 0.650f, 0.360f, 1.0f));

        ImGui::Text("%s", title);

        ImGui::PopStyleColor();

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Separator();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        ImGui::SetWindowFontScale(1.3f);

        ImGui::TextWrapped(
            "选择编辑模式启动：");

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

        // 2D is the primary path: teal accent button.
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.150f, 0.420f, 0.480f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.190f, 0.520f, 0.600f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            ImVec4(0.120f, 0.340f, 0.400f, 1.0f));

        if (ImGui::Button(
            "2D 模式\n精灵 | 物理 | 动画",
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

        // 3D is the secondary path: neutral surface that brightens on hover.
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.170f, 0.190f, 0.230f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.220f, 0.250f, 0.310f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            ImVec4(0.130f, 0.150f, 0.190f, 1.0f));

        if (ImGui::Button(
            "3D 模式\n网格 | PBR / IBL | 光照",
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

        Application::Get().PushLayer(std::make_unique<EditorLayer2D>());
        Application::Get().PopLayer(this);
    }

    void ModeSelectLayer::LaunchEditor3D()
    {
        if (m_Decided)
            return;

        m_Decided = true;

        Application::Get().PushLayer(std::make_unique<EditorLayer3D>());
        Application::Get().PopLayer(this);
    }

} // namespace Wheatear
