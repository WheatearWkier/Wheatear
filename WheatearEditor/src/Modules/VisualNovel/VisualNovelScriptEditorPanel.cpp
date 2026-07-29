#include "VisualNovelScriptEditorPanel.h"

#include "Wheatear/Core/AssetPath.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <sstream>

namespace Wheatear {

    namespace {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;

        static std::string Trim(const std::string& value)
        {
            const char* whitespace = " \t\r\n";
            const size_t begin = value.find_first_not_of(whitespace);
            if (begin == std::string::npos)
                return {};
            const size_t end = value.find_last_not_of(whitespace);
            return value.substr(begin, end - begin + 1);
        }

        static bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        static std::string StripQuotes(const std::string& value)
        {
            std::string result = Trim(value);
            if (result.size() >= 2)
            {
                const char first = result.front();
                const char last = result.back();
                if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
                    return result.substr(1, result.size() - 2);
            }
            return result;
        }

        static std::string QuoteIfNeeded(const std::string& value)
        {
            if (value.empty())
                return "\"\"";

            const bool needsQuote = value.find_first_of(" \t\r\n|") != std::string::npos;
            if (!needsQuote)
                return value;

            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');
            for (char c : value)
            {
                if (c == '"' || c == '\\')
                    escaped.push_back('\\');
                escaped.push_back(c);
            }
            escaped.push_back('"');
            return escaped;
        }

        static std::string ConsumeToken(std::string& text)
        {
            text = Trim(text);
            if (text.empty())
                return {};

            const char quote = text.front();
            if (quote == '"' || quote == '\'')
            {
                const size_t end = text.find(quote, 1);
                if (end == std::string::npos)
                {
                    std::string token = text.substr(1);
                    text.clear();
                    return token;
                }

                std::string token = text.substr(1, end - 1);
                text = Trim(text.substr(end + 1));
                return token;
            }

            const size_t end = text.find_first_of(" \t\r\n");
            if (end == std::string::npos)
            {
                std::string token = text;
                text.clear();
                return token;
            }

            std::string token = text.substr(0, end);
            text = Trim(text.substr(end + 1));
            return token;
        }

        static bool ReadCommandPayload(const std::string& line,
            std::initializer_list<const char*> commands,
            std::string& payload)
        {
            for (const char* command : commands)
            {
                const std::string tagged = std::string("@") + command;
                if (line == tagged)
                {
                    payload.clear();
                    return true;
                }

                if (StartsWith(line, tagged + " "))
                {
                    payload = Trim(line.substr(tagged.size() + 1));
                    return true;
                }
            }
            return false;
        }

        static bool InputString(const char* label,
            std::string& value,
            size_t capacity = 512)
        {
            std::vector<char> buffer(std::max<size_t>(capacity, value.size() + 32), 0);
            strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, buffer.data(), buffer.size()))
            {
                value = buffer.data();
                return true;
            }
            return false;
        }

        static bool InputMultiline(const char* label,
            std::string& value,
            const ImVec2& size,
            size_t capacity = 4096)
        {
            std::vector<char> buffer(std::max<size_t>(capacity, value.size() + 256), 0);
            strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
            if (ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), size, ImGuiInputTextFlags_AllowTabInput))
            {
                value = buffer.data();
                return true;
            }
            return false;
        }

        static std::vector<VisualNovelScriptEditorPanel::ChoiceEntry> ParseChoices(const std::string& payload)
        {
            std::vector<VisualNovelScriptEditorPanel::ChoiceEntry> choices;
            std::stringstream stream(payload);
            std::string segment;
            while (std::getline(stream, segment, '|'))
            {
                segment = Trim(segment);
                if (segment.empty())
                    continue;

                const size_t arrow = segment.find("->");
                VisualNovelScriptEditorPanel::ChoiceEntry choice;
                if (arrow == std::string::npos)
                {
                    choice.Text = StripQuotes(segment);
                }
                else
                {
                    choice.Text = StripQuotes(segment.substr(0, arrow));
                    choice.Target = StripQuotes(segment.substr(arrow + 2));
                }
                choices.push_back(std::move(choice));
            }
            return choices;
        }

        static const char* KindName(VisualNovelScriptEditorPanel::RowKind kind)
        {
            switch (kind)
            {
                case VisualNovelScriptEditorPanel::RowKind::Raw: return "Raw";
                case VisualNovelScriptEditorPanel::RowKind::Label: return "Label";
                case VisualNovelScriptEditorPanel::RowKind::Background: return "Background";
                case VisualNovelScriptEditorPanel::RowKind::Music: return "BGM";
                case VisualNovelScriptEditorPanel::RowKind::Speed: return "Speed";
                case VisualNovelScriptEditorPanel::RowKind::Character: return "Character";
                case VisualNovelScriptEditorPanel::RowKind::Show: return "Show";
                case VisualNovelScriptEditorPanel::RowKind::Hide: return "Hide";
                case VisualNovelScriptEditorPanel::RowKind::Expression: return "Expression";
                case VisualNovelScriptEditorPanel::RowKind::Dialogue: return "Dialogue";
                case VisualNovelScriptEditorPanel::RowKind::Choice: return "Choice";
                case VisualNovelScriptEditorPanel::RowKind::Goto: return "Goto";
                case VisualNovelScriptEditorPanel::RowKind::End: return "End";
            }
            return "Unknown";
        }

        static std::string RowSummary(const VisualNovelScriptEditorPanel::Row& row)
        {
            switch (row.Kind)
            {
                case VisualNovelScriptEditorPanel::RowKind::Raw: return row.Raw.empty() ? "(blank)" : row.Raw;
                case VisualNovelScriptEditorPanel::RowKind::Label: return "label " + row.Name;
                case VisualNovelScriptEditorPanel::RowKind::Background: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Music: return row.Text.empty() ? row.Value : row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Speed: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Character: return row.Name + " / " + row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Show: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Hide: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Expression: return row.Name + " -> " + row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Dialogue: return row.Name + ": " + row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Choice: return std::to_string(row.Choices.size()) + " choices";
                case VisualNovelScriptEditorPanel::RowKind::Goto: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::End: return "end";
            }
            return {};
        }

    } // namespace

    namespace VisualNovelEditorRequests {

        void RequestOpenScript(const std::string& sourcePath)
        {
            s_PendingOpenPath = sourcePath;
            s_HasPendingOpen = true;
        }

        bool ConsumeOpenScriptRequest(std::string& sourcePath)
        {
            if (!s_HasPendingOpen)
                return false;

            sourcePath = s_PendingOpenPath;
            s_PendingOpenPath.clear();
            s_HasPendingOpen = false;
            return true;
        }

    } // namespace VisualNovelEditorRequests

    void VisualNovelScriptEditorPanel::Open(const std::string& sourcePath)
    {
        m_Open = true;
        if (!sourcePath.empty() && sourcePath != m_SourcePath)
        {
            m_SourcePath = sourcePath;
            m_Loaded = false;
        }
    }

    void VisualNovelScriptEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        if (VisualNovelEditorRequests::ConsumeOpenScriptRequest(requestedPath))
            Open(requestedPath);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        ImGui::Begin("VN Script Editor", &m_Open);
        DrawToolbar();

        if (ImGui::BeginTabBar("##VNEditorTabs"))
        {
            if (ImGui::BeginTabItem("Timeline"))
            {
                DrawTimeline();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Characters / BGM"))
            {
                DrawLibraries();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Raw Preview"))
            {
                DrawRawPreview();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void VisualNovelScriptEditorPanel::Load()
    {
        m_ResolvedPath = AssetPath::Resolve(m_SourcePath);
        m_Rows.clear();
        m_SelectedRow = -1;
        m_BGMAssets.clear();

        const std::filesystem::path bgmDirectory = AssetPath::Resolve("assets/vertical_slice/audio/bgm");
        if (std::filesystem::is_directory(bgmDirectory))
        {
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(bgmDirectory, error))
            {
                if (error || !entry.is_regular_file())
                    continue;

                const std::string extension = entry.path().extension().generic_string();
                if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
                {
                    std::filesystem::path relative = std::filesystem::relative(entry.path(), AssetPath::GetProjectRoot(), error);
                    if (!error)
                        m_BGMAssets.push_back(relative.generic_string());
                }
            }
            std::sort(m_BGMAssets.begin(), m_BGMAssets.end());
        }

        if (!std::filesystem::exists(m_ResolvedPath))
        {
            m_Loaded = true;
            m_Dirty = false;
            m_Status = "File does not exist yet. Save will create it.";
            return;
        }

        std::ifstream input(m_ResolvedPath, std::ios::binary);
        if (!input)
        {
            m_Loaded = true;
            m_Dirty = false;
            m_Status = "Failed to open file.";
            return;
        }

        std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        ParseText(text);
        m_Loaded = true;
        m_Dirty = false;
        m_Status = "Loaded.";
    }

    void VisualNovelScriptEditorPanel::Save()
    {
        m_ResolvedPath = AssetPath::Resolve(m_SourcePath);
        const std::filesystem::path parent = m_ResolvedPath.parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);

        std::ofstream output(m_ResolvedPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            m_Status = "Failed to save file.";
            return;
        }

        const std::string text = SerializeRows();
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        m_Dirty = false;
        m_Status = "Saved.";
    }

    void VisualNovelScriptEditorPanel::ParseText(const std::string& text)
    {
        std::istringstream stream(text);
        std::string rawLine;
        while (std::getline(stream, rawLine))
        {
            Row row;
            row.Raw = rawLine;

            const std::string line = Trim(rawLine);
            std::string payload;
            if (line.empty() || StartsWith(line, "#") || StartsWith(line, "//") || StartsWith(line, ";"))
            {
                row.Kind = RowKind::Raw;
            }
            else if (ReadCommandPayload(line, { "label" }, payload))
            {
                row.Kind = RowKind::Label;
                row.Name = StripQuotes(payload);
            }
            else if (ReadCommandPayload(line, { "background" }, payload))
            {
                row.Kind = RowKind::Background;
                row.Value = StripQuotes(payload);
            }
            else if (ReadCommandPayload(line, { "music", "bgm" }, payload))
            {
                row.Kind = RowKind::Music;
                std::string remaining = payload;
                row.Value = StripQuotes(ConsumeToken(remaining));
                row.Text = StripQuotes(remaining);
            }
            else if (ReadCommandPayload(line, { "speed" }, payload))
            {
                row.Kind = RowKind::Speed;
                row.Value = StripQuotes(payload);
            }
            else if (ReadCommandPayload(line, { "character" }, payload))
            {
                row.Kind = RowKind::Character;
                std::string remaining = payload;
                row.Name = ConsumeToken(remaining);
                row.Text = ConsumeToken(remaining);
                row.Value = StripQuotes(remaining);
            }
            else if (ReadCommandPayload(line, { "show" }, payload))
            {
                row.Kind = RowKind::Show;
                row.Value = payload;
            }
            else if (ReadCommandPayload(line, { "hide" }, payload))
            {
                row.Kind = RowKind::Hide;
                row.Value = payload;
            }
            else if (ReadCommandPayload(line, { "expression" }, payload))
            {
                row.Kind = RowKind::Expression;
                std::string remaining = payload;
                row.Name = ConsumeToken(remaining);
                row.Value = ConsumeToken(remaining);
            }
            else if (ReadCommandPayload(line, { "choice" }, payload))
            {
                row.Kind = RowKind::Choice;
                row.Choices = ParseChoices(payload);
            }
            else if (ReadCommandPayload(line, { "goto" }, payload))
            {
                row.Kind = RowKind::Goto;
                row.Value = StripQuotes(payload);
            }
            else if (ReadCommandPayload(line, { "end" }, payload))
            {
                row.Kind = RowKind::End;
            }
            else
            {
                const size_t colon = line.find(':');
                if (colon != std::string::npos)
                {
                    row.Kind = RowKind::Dialogue;
                    row.Name = Trim(line.substr(0, colon));
                    row.Text = Trim(line.substr(colon + 1));
                }
                else
                {
                    row.Kind = RowKind::Raw;
                }
            }

            m_Rows.push_back(std::move(row));
        }
    }

    std::string VisualNovelScriptEditorPanel::SerializeRows() const
    {
        std::ostringstream out;
        for (const Row& row : m_Rows)
        {
            switch (row.Kind)
            {
                case RowKind::Raw:
                    out << row.Raw;
                    break;
                case RowKind::Label:
                    out << "@label " << row.Name;
                    break;
                case RowKind::Background:
                    out << "@background " << QuoteIfNeeded(row.Value);
                    break;
                case RowKind::Music:
                    out << "@music " << (row.Value.empty() ? "stop" : QuoteIfNeeded(row.Value));
                    if (!row.Text.empty())
                        out << " " << QuoteIfNeeded(row.Text);
                    break;
                case RowKind::Speed:
                    out << "@speed " << row.Value;
                    break;
                case RowKind::Character:
                    out << "@character " << row.Name << " " << QuoteIfNeeded(row.Text) << " " << QuoteIfNeeded(row.Value);
                    break;
                case RowKind::Show:
                    out << "@show " << row.Value;
                    break;
                case RowKind::Hide:
                    out << "@hide " << row.Value;
                    break;
                case RowKind::Expression:
                    out << "@expression " << row.Name << " " << row.Value;
                    break;
                case RowKind::Dialogue:
                    out << row.Name << ": " << row.Text;
                    break;
                case RowKind::Choice:
                    out << "@choice ";
                    for (size_t i = 0; i < row.Choices.size(); ++i)
                    {
                        if (i > 0)
                            out << " | ";
                        out << row.Choices[i].Text;
                        if (!row.Choices[i].Target.empty())
                            out << " -> " << row.Choices[i].Target;
                    }
                    break;
                case RowKind::Goto:
                    out << "@goto " << row.Value;
                    break;
                case RowKind::End:
                    out << "@end";
                    break;
            }
            out << "\n";
        }
        return out.str();
    }

    void VisualNovelScriptEditorPanel::DrawToolbar()
    {
        ImGui::PushItemWidth(-260.0f);
        if (InputString("Script", m_SourcePath, 512))
            m_Loaded = false;
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Load"))
            Load();
        ImGui::SameLine();
        if (ImGui::Button("Save"))
            Save();

        if (m_Dirty)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.25f, 1.0f), "Modified");
        }

        if (!m_Status.empty())
            ImGui::TextDisabled("%s  %s", m_Status.c_str(), m_ResolvedPath.generic_string().c_str());
    }

    void VisualNovelScriptEditorPanel::DrawTimeline()
    {
        const float leftWidth = 390.0f;
        ImGui::BeginChild("##VNRows", ImVec2(leftWidth, 0.0f), true);

        if (ImGui::Button("+ Dialogue")) AddRow(RowKind::Dialogue);
        ImGui::SameLine();
        if (ImGui::Button("+ BGM")) AddRow(RowKind::Music);
        ImGui::SameLine();
        if (ImGui::Button("+ Choice")) AddRow(RowKind::Choice);
        if (ImGui::Button("+ Character")) AddRow(RowKind::Character);
        ImGui::SameLine();
        if (ImGui::Button("+ Background")) AddRow(RowKind::Background);
        ImGui::SameLine();
        if (ImGui::Button("+ Label")) AddRow(RowKind::Label);

        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            const Row& row = m_Rows[i];
            std::string label = std::to_string(i + 1) + "  [" + KindName(row.Kind) + "]  " + RowSummary(row);
            if (label.size() > 96)
                label = label.substr(0, 93) + "...";
            if (ImGui::Selectable(label.c_str(), m_SelectedRow == i))
                m_SelectedRow = i;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##VNRowEditor", ImVec2(0.0f, 0.0f), true);
        if (m_SelectedRow >= 0 && m_SelectedRow < static_cast<int>(m_Rows.size()))
        {
            Row& row = m_Rows[m_SelectedRow];
            ImGui::Text("Row %d  %s", m_SelectedRow + 1, KindName(row.Kind));
            ImGui::Separator();
            DrawRowEditor(row);

            ImGui::Separator();
            if (ImGui::Button("Move Up") && m_SelectedRow > 0)
            {
                std::swap(m_Rows[m_SelectedRow], m_Rows[m_SelectedRow - 1]);
                --m_SelectedRow;
                m_Dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Move Down") && m_SelectedRow + 1 < static_cast<int>(m_Rows.size()))
            {
                std::swap(m_Rows[m_SelectedRow], m_Rows[m_SelectedRow + 1]);
                ++m_SelectedRow;
                m_Dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete"))
            {
                m_Rows.erase(m_Rows.begin() + m_SelectedRow);
                if (m_SelectedRow >= static_cast<int>(m_Rows.size()))
                    m_SelectedRow = static_cast<int>(m_Rows.size()) - 1;
                m_Dirty = true;
            }
        }
        else
        {
            ImGui::TextDisabled("Select a row or add a new one.");
        }
        ImGui::EndChild();
    }

    void VisualNovelScriptEditorPanel::DrawRowEditor(Row& row)
    {
        switch (row.Kind)
        {
            case RowKind::Raw:
                if (InputMultiline("Raw", row.Raw, ImVec2(-1.0f, 160.0f)))
                    m_Dirty = true;
                break;
            case RowKind::Label:
                if (InputString("Label", row.Name))
                    m_Dirty = true;
                break;
            case RowKind::Background:
                if (InputString("Background", row.Value, 512))
                    m_Dirty = true;
                break;
            case RowKind::Music:
                if (!m_BGMAssets.empty())
                {
                    const char* preview = row.Value.empty() ? "(stop)" : row.Value.c_str();
                    if (ImGui::BeginCombo("BGM Asset", preview))
                    {
                        if (ImGui::Selectable("(stop)", row.Value.empty()))
                        {
                            row.Value.clear();
                            m_Dirty = true;
                        }
                        for (const std::string& asset : m_BGMAssets)
                        {
                            if (ImGui::Selectable(asset.c_str(), asset == row.Value))
                            {
                                row.Value = asset;
                                m_Dirty = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (InputString("BGM Path", row.Value, 512))
                    m_Dirty = true;
                if (InputString("Display Name", row.Text, 256))
                    m_Dirty = true;
                break;
            case RowKind::Speed:
                if (InputString("Characters / Second", row.Value, 64))
                    m_Dirty = true;
                break;
            case RowKind::Character:
                if (InputString("Id", row.Name, 128))
                    m_Dirty = true;
                if (InputString("Display Name", row.Text, 128))
                    m_Dirty = true;
                if (InputString("Portrait Style", row.Value, 512))
                    m_Dirty = true;
                break;
            case RowKind::Show:
                if (InputString("Characters", row.Value, 512))
                    m_Dirty = true;
                break;
            case RowKind::Hide:
                if (InputString("Characters / all", row.Value, 512))
                    m_Dirty = true;
                break;
            case RowKind::Expression:
                if (InputString("Character", row.Name, 128))
                    m_Dirty = true;
                if (InputString("Expression", row.Value, 128))
                    m_Dirty = true;
                break;
            case RowKind::Dialogue:
                if (InputString("Speaker", row.Name, 128))
                    m_Dirty = true;
                if (InputMultiline("Text", row.Text, ImVec2(-1.0f, 130.0f)))
                    m_Dirty = true;
                break;
            case RowKind::Choice:
                for (int i = 0; i < static_cast<int>(row.Choices.size()); ++i)
                {
                    ImGui::PushID(i);
                    ImGui::Text("Choice %d", i + 1);
                    if (InputString("Text", row.Choices[i].Text, 512))
                        m_Dirty = true;
                    if (InputString("Target", row.Choices[i].Target, 512))
                        m_Dirty = true;
                    ImGui::SameLine();
                    if (ImGui::Button("Remove"))
                    {
                        row.Choices.erase(row.Choices.begin() + i);
                        m_Dirty = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (ImGui::Button("+ Choice Option"))
                {
                    row.Choices.push_back({ "新选项", "target_label" });
                    m_Dirty = true;
                }
                break;
            case RowKind::Goto:
                if (InputString("Target Label", row.Value, 256))
                    m_Dirty = true;
                break;
            case RowKind::End:
                ImGui::TextDisabled("Ends this VN script.");
                break;
        }
    }

    void VisualNovelScriptEditorPanel::DrawLibraries()
    {
        ImGui::TextDisabled("Characters defined by @character");
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            Row& row = m_Rows[i];
            if (row.Kind != RowKind::Character)
                continue;
            ImGui::PushID(i);
            ImGui::Separator();
            ImGui::Text("Row %d", i + 1);
            if (InputString("Id", row.Name, 128)) m_Dirty = true;
            if (InputString("Display", row.Text, 128)) m_Dirty = true;
            if (InputString("Style", row.Value, 512)) m_Dirty = true;
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::TextDisabled("BGM cues defined by @music / @bgm");
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            Row& row = m_Rows[i];
            if (row.Kind != RowKind::Music)
                continue;
            ImGui::PushID(10000 + i);
            ImGui::Separator();
            ImGui::Text("Row %d", i + 1);
            if (InputString("Path", row.Value, 512)) m_Dirty = true;
            if (InputString("Display Name", row.Text, 256)) m_Dirty = true;
            ImGui::PopID();
        }
    }

    void VisualNovelScriptEditorPanel::DrawRawPreview()
    {
        std::string preview = SerializeRows();
        InputMultiline("##RawPreview", preview, ImVec2(-1.0f, -1.0f), std::max<size_t>(preview.size() + 1, 4096));
        ImGui::TextDisabled("Raw preview is generated from structured rows. Edit rows in Timeline to save changes.");
    }

    void VisualNovelScriptEditorPanel::AddRow(RowKind kind)
    {
        Row row;
        row.Kind = kind;
        switch (kind)
        {
            case RowKind::Label:
                row.Name = "new_label";
                break;
            case RowKind::Background:
                row.Value = "assets/vertical_slice/vn/backgrounds/isekai_forest.png";
                break;
            case RowKind::Music:
                row.Value = m_BGMAssets.empty() ? "assets/vertical_slice/audio/bgm/vn_school_morning.wav" : m_BGMAssets.front();
                row.Text = "新BGM";
                break;
            case RowKind::Character:
                row.Name = "Character";
                row.Text = "角色";
                row.Value = "assets/vertical_slice/vn/portraits/character_{expression}.png";
                break;
            case RowKind::Choice:
                row.Choices.push_back({ "选项文本", "target_label" });
                break;
            case RowKind::Dialogue:
                row.Name = "Leo";
                row.Text = "新台词。";
                break;
            default:
                row.Raw = "";
                break;
        }

        const int insertIndex = (m_SelectedRow >= 0 && m_SelectedRow < static_cast<int>(m_Rows.size()))
            ? m_SelectedRow + 1
            : static_cast<int>(m_Rows.size());
        m_Rows.insert(m_Rows.begin() + insertIndex, std::move(row));
        m_SelectedRow = insertIndex;
        m_Dirty = true;
    }

} // namespace Wheatear
