#include "wepch.h"
#include "LogicDiagnosticsPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorSourceNavigation.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Logic/LogicDiagnostics.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace Wheatear {

    namespace {

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        const char* SeverityLabel(Logic::DiagnosticSeverity severity)
        {
            switch (severity)
            {
            case Logic::DiagnosticSeverity::Info: return "Info";
            case Logic::DiagnosticSeverity::Warning: return "Warning";
            case Logic::DiagnosticSeverity::Error: return "Error";
            default: return "Unknown";
            }
        }

        ImVec4 SeverityColor(Logic::DiagnosticSeverity severity)
        {
            switch (severity)
            {
            case Logic::DiagnosticSeverity::Error:
                return ImVec4(0.92f, 0.35f, 0.35f, 1.0f);
            case Logic::DiagnosticSeverity::Warning:
                return ImVec4(0.95f, 0.72f, 0.30f, 1.0f);
            default:
                return ImVec4(0.55f, 0.72f, 0.92f, 1.0f);
            }
        }

        bool MatchesFilter(const Logic::LogicDiagnostic& diagnostic,
            const std::string& filter)
        {
            if (filter.empty())
                return true;
            return ToLowerAscii(diagnostic.Source).find(filter) != std::string::npos
                || ToLowerAscii(diagnostic.Message).find(filter) != std::string::npos;
        }

        void ParseSourceLocation(const std::string& source,
            std::string& path, int& line)
        {
            path = source;
            line = 0;
            const size_t suffix = path.find(" (event ");
            if (suffix != std::string::npos)
                path = path.substr(0, suffix);

            const size_t colon = path.rfind(':');
            if (colon == std::string::npos || colon + 1 >= path.size())
                return;

            size_t end = colon + 1;
            while (end < path.size()
                && std::isdigit(static_cast<unsigned char>(path[end])))
                ++end;
            if (end == colon + 1)
                return;

            try
            {
                line = std::stoi(path.substr(colon + 1, end - colon - 1));
                path = path.substr(0, colon);
            }
            catch (...)
            {
                line = 0;
            }
        }

    } // namespace

    void LogicDiagnosticsPanel::Open(const EditorToolContext&)
    {
        m_Open = true;
    }

    void LogicDiagnosticsPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!EditorFloatingWindow::Begin("Logic Diagnostics", &m_Open, 0,
            { 860.0f, 480.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }

        const size_t infoCount =
            Logic::Diagnostics::Count(Logic::DiagnosticSeverity::Info);
        const size_t warningCount =
            Logic::Diagnostics::Count(Logic::DiagnosticSeverity::Warning);
        const size_t errorCount =
            Logic::Diagnostics::Count(Logic::DiagnosticSeverity::Error);

        EditorWidgets::PanelHeader(
            EditorLocale::Text("Logic Diagnostics", "逻辑诊断"),
            EditorLocale::Text(
                "Expression, condition and command issues reported by the shared logic runtime (.wts / .vn / WAO).",
                "共享逻辑运行时（.wts / .vn / WAO）报告的表达式、条件和命令问题。"));
        EditorWidgets::StatusBadge((std::to_string(errorCount) + " error(s)").c_str(),
            errorCount == 0 ? EditorWidgets::StatusKind::Neutral : EditorWidgets::StatusKind::Error);
        ImGui::SameLine();
        EditorWidgets::StatusBadge((std::to_string(warningCount) + " warning(s)").c_str(),
            warningCount == 0 ? EditorWidgets::StatusKind::Neutral : EditorWidgets::StatusKind::Warning);
        ImGui::SameLine();
        EditorWidgets::StatusBadge((std::to_string(infoCount) + " info").c_str(),
            EditorWidgets::StatusKind::Info);
        ImGui::SameLine();
        EditorFloatingWindow::DrawToggleButton(
            EditorLocale::Text("Logic Diagnostics", "逻辑诊断"));

        if (ImGui::Button(EditorLocale::Text("Clear", "清空")))
            Logic::Diagnostics::Clear();
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Info", "信息"), &m_ShowInfo);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Warnings", "警告"), &m_ShowWarnings);
        ImGui::SameLine();
        ImGui::Checkbox(EditorLocale::Text("Errors", "错误"), &m_ShowErrors);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260.0f);
        EditorWidgets::InputString(EditorLocale::Text("Filter", "过滤"),
            m_Filter, 260);

        const std::vector<Logic::LogicDiagnostic> diagnostics =
            Logic::Diagnostics::Snapshot();
        const std::string filter = ToLowerAscii(m_Filter);

        EditorWidgets::SectionHeader(
            EditorLocale::Text("Reported Issues", "已报告的问题"),
            EditorLocale::Text(
                "Warnings are deduplicated until the message changes; fix the asset and rerun to clear repeats.",
                "相同警告会去重，直到内容变化；修复资源后重新运行即可清除重复项。"));

        size_t visible = 0;
        for (const Logic::LogicDiagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.Severity == Logic::DiagnosticSeverity::Info && !m_ShowInfo)
                continue;
            if (diagnostic.Severity == Logic::DiagnosticSeverity::Warning && !m_ShowWarnings)
                continue;
            if (diagnostic.Severity == Logic::DiagnosticSeverity::Error && !m_ShowErrors)
                continue;
            if (!MatchesFilter(diagnostic, filter))
                continue;
            ++visible;
        }

        if (visible == 0)
        {
            ImGui::TextDisabled("%s",
                EditorLocale::Text("No logic diagnostics.", "没有逻辑诊断信息。"));
            EditorFloatingWindow::End();
            return;
        }

        if (ImGui::BeginChild("##LogicDiagnosticList", ImVec2(0, 0), true))
        {
            if (ImGui::BeginTable("##LogicDiagnostics", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
                    | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn(EditorLocale::Text("Severity", "级别"),
                    ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn(EditorLocale::Text("Source", "来源"),
                    ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn(EditorLocale::Text("Message", "信息"),
                    ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (const Logic::LogicDiagnostic& diagnostic : diagnostics)
                {
                    if (diagnostic.Severity == Logic::DiagnosticSeverity::Info && !m_ShowInfo)
                        continue;
                    if (diagnostic.Severity == Logic::DiagnosticSeverity::Warning && !m_ShowWarnings)
                        continue;
                    if (diagnostic.Severity == Logic::DiagnosticSeverity::Error && !m_ShowErrors)
                        continue;
                    if (!MatchesFilter(diagnostic, filter))
                        continue;

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(SeverityColor(diagnostic.Severity), "%s",
                        SeverityLabel(diagnostic.Severity));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(diagnostic.Source.c_str());
                    if (ImGui::IsItemHovered()
                        && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        std::string path;
                        int line = 0;
                        ParseSourceLocation(diagnostic.Source, path, line);
                        if (!path.empty())
                            EditorSourceNavigation::OpenAsset(path, line);
                    }
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextWrapped("%s", diagnostic.Message.c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        EditorFloatingWindow::End();
    }

} // namespace Wheatear
