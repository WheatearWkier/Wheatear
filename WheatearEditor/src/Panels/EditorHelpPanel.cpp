#include "wepch.h"
#include "EditorHelpPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Wheatear {

    void EditorHelpPanel::LoadTopics()
    {
        m_Topics.clear();

        std::filesystem::path helpDir = std::filesystem::current_path() / "Resources" / "Help";
        std::error_code error;
        if (!std::filesystem::is_directory(helpDir, error))
            return;

        for (const auto& entry : std::filesystem::directory_iterator(helpDir, error))
        {
            if (entry.path().extension() != ".md")
                continue;

            std::string filename = entry.path().stem().string();
            // "00_快速上手" -> "快速上手"
            const size_t underscore = filename.find('_');
            const std::string title = (underscore != std::string::npos && underscore + 1 < filename.size())
                ? filename.substr(underscore + 1)
                : filename;

            m_Topics.push_back({ title, entry.path().string() });
        }

        std::sort(m_Topics.begin(), m_Topics.end(),
            [](const HelpTopic& a, const HelpTopic& b) { return a.Path < b.Path; });

        m_TopicsLoaded = true;
    }

    void EditorHelpPanel::LoadContent(int index)
    {
        m_Content.clear();
        if (index < 0 || index >= static_cast<int>(m_Topics.size()))
        {
            m_ContentLoaded = true;
            return;
        }

        std::ifstream input(m_Topics[index].Path);
        if (input.is_open())
        {
            m_Content.assign(
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>());
        }

        m_ContentLoaded = true;
    }

    void EditorHelpPanel::OnImGuiRender()
    {
        if (!m_Open)
            return;

        if (!m_TopicsLoaded)
            LoadTopics();
        if (!m_ContentLoaded)
            LoadContent(m_SelectedTopic);

        ImGui::SetNextWindowSize(ImVec2(1000.0f, 660.0f), ImGuiCond_FirstUseEver);
        if (!EditorFloatingWindow::Begin("Editor Help", &m_Open, 0, { 1000.0f, 660.0f }))
        {
            EditorFloatingWindow::End();
            return;
        }
        EditorFloatingWindow::DrawToggleButton("Editor Help");

        ImGui::BeginChild("##helptopics", ImVec2(210.0f, 0.0f), true);
        for (int i = 0; i < static_cast<int>(m_Topics.size()); ++i)
        {
            if (ImGui::Selectable(m_Topics[i].Title.c_str(), i == m_SelectedTopic))
            {
                m_SelectedTopic = i;
                LoadContent(i);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##helpcontent");
        if (m_Content.empty())
            ImGui::TextDisabled("%s", EditorLocale::Text("No help content found in Resources/Help/.", "Resources/Help/ 下没有帮助内容。"));
        else
            RenderMarkdown(m_Content);
        ImGui::EndChild();

        EditorFloatingWindow::End();
    }

    void EditorHelpPanel::RenderTable(std::istringstream& stream)
    {
        std::vector<std::vector<std::string>> rows;
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line.front() != '|')
                break;

            std::vector<std::string> cells;
            std::string cell;
            std::istringstream cellsStream(line.substr(1));
            while (std::getline(cellsStream, cell, '|'))
            {
                // strip surrounding spaces
                const size_t first = cell.find_first_not_of(' ');
                const size_t last = cell.find_last_not_of(' ');
                if (first == std::string::npos)
                    cells.push_back("");
                else
                    cells.push_back(cell.substr(first, last - first + 1));
            }

            // Skip the markdown separator row (|---|).
            bool separatorRow = !cells.empty();
            for (const std::string& cellText : cells)
            {
                for (char c : cellText)
                {
                    if (c != '-' && c != ':' && c != ' ')
                    {
                        separatorRow = false;
                        break;
                    }
                }
                if (!separatorRow)
                    break;
            }
            if (separatorRow)
                continue;

            rows.push_back(std::move(cells));
        }

        if (rows.empty())
            return;

        const int columnCount = static_cast<int>(rows[0].size());
        ImGui::Separator();
        ImGui::Columns(columnCount, nullptr, false);
        for (size_t r = 0; r < rows.size(); ++r)
        {
            const bool header = (r == 0);
            for (int c = 0; c < columnCount; ++c)
            {
                const std::string text = (c < static_cast<int>(rows[r].size())) ? rows[r][c] : "";
                if (header)
                    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", text.c_str());
                else
                    ImGui::TextUnformatted(text.c_str());
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();
    }

    void EditorHelpPanel::RenderMarkdown(const std::string& text)
    {
        std::istringstream stream(text);
        std::string line;
        bool inCode = false;

        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.rfind("```", 0) == 0)
            {
                inCode = !inCode;
                ImGui::Spacing();
                continue;
            }

            if (inCode)
            {
                ImGui::Text("    %s", line.c_str());
                continue;
            }

            if (line.empty())
            {
                ImGui::Spacing();
                continue;
            }

            if (line.rfind("# ", 0) == 0)
            {
                ImGui::Spacing();
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", line.c_str() + 2);
                ImGui::Spacing();
            }
            else if (line.rfind("## ", 0) == 0)
            {
                ImGui::Spacing();
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", line.c_str() + 3);
            }
            else if (line.rfind("### ", 0) == 0)
            {
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s", line.c_str() + 4);
            }
            else if (line.rfind("> ", 0) == 0)
            {
                ImGui::TextWrapped("%s", line.c_str() + 2);
            }
            else if (line.rfind("- ", 0) == 0)
            {
                ImGui::BulletText("%s", line.c_str() + 2);
            }
            else if (line.size() >= 3 && std::isdigit(static_cast<unsigned char>(line[0]))
                && line[1] == '.' && line[2] == ' ')
            {
                ImGui::BulletText("%s", line.c_str() + 3);
            }
            else if (line.front() == '|')
            {
                RenderTable(stream);
            }
            else
            {
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
    }

} // namespace Wheatear
