#include "wepch.h"
#include "ConsolePanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>

#include <algorithm>

namespace Wheatear {

    namespace {

        static ImU32 LevelColor(spdlog::level::level_enum level)
        {
            switch (level)
            {
            case spdlog::level::trace:  return IM_COL32(140, 140, 140, 255);
            case spdlog::level::info:   return IM_COL32(200, 220, 235, 255);
            case spdlog::level::warn:   return IM_COL32(235, 200, 110, 255);
            case spdlog::level::err:
            case spdlog::level::critical:
            case spdlog::level::off:    return IM_COL32(235, 110, 110, 255);
            default:                    return IM_COL32(200, 200, 200, 255);
            }
        }

        static const char* LevelTag(spdlog::level::level_enum level)
        {
            switch (level)
            {
            case spdlog::level::trace:    return "[TRACE]";
            case spdlog::level::info:     return "[INFO] ";
            case spdlog::level::warn:     return "[WARN] ";
            case spdlog::level::err:      return "[ERROR]";
            case spdlog::level::critical: return "[FATAL]";
            default:                      return "[?]    ";
            }
        }

    } // namespace

    void ConsolePanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!EditorFloatingWindow::Begin("Console", &m_Open,
            0, { 720.0f, 400.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }

        DrainPendingMessages();

        DrawFilterBar();
        ImGui::Separator();
        DrawMessageList();

        EditorFloatingWindow::End();
    }

    void ConsolePanel::DrainPendingMessages()
    {
        Log::DrainEditorMessages(m_Messages);
    }

    void ConsolePanel::DrawFilterBar()
    {
        EditorFloatingWindow::DrawToggleButton("Console");
        ImGui::SameLine();

        ImGui::Checkbox(EditorLocale::Text("Trace", "追踪"), &m_ShowTrace);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Info", "信息"), &m_ShowInfo);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Warn", "警告"), &m_ShowWarn);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Error", "错误"), &m_ShowError);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Auto Scroll", "自动滚动"), &m_AutoScroll);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        EditorWidgets::InputString("##ConsoleFilter", m_Filter, 256);

        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Clear", "清空")))
        {
            m_Messages.clear();
            Log::ClearEditorMessages();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%d %s", static_cast<int>(m_Messages.size()),
            EditorLocale::Text("messages", "条消息"));
    }

    void ConsolePanel::DrawMessageList()
    {
        const bool filterActive = !m_Filter.empty();

        ImGui::BeginChild("##ConsoleMessages", ImVec2(0, 0),
            ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        int drawn = 0;
        for (const LogMessage& message : m_Messages)
        {
            const bool levelVisible =
                (message.Level == spdlog::level::trace && m_ShowTrace) ||
                (message.Level == spdlog::level::info && m_ShowInfo) ||
                (message.Level == spdlog::level::warn && m_ShowWarn) ||
                (message.Level == spdlog::level::err && m_ShowError) ||
                (message.Level == spdlog::level::critical && m_ShowError);
            if (!levelVisible)
                continue;

            if (filterActive && message.Text.find(m_Filter) == std::string::npos)
                continue;

            ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(message.Level));
            ImGui::TextUnformatted(LevelTag(message.Level));
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", message.Text.c_str());
            ++drawn;
        }

        if (drawn == 0)
            ImGui::TextDisabled(EditorLocale::Text("(no messages)", "（暂无消息）"));

        if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - lineHeight * 2.0f)
        {
            // Only follow when the user is already near the bottom; explicit
            // pinning would fight manual scrolling.
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
    }

} // namespace Wheatear
