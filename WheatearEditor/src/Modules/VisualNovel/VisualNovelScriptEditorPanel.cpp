#include "VisualNovelScriptEditorPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
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

        using EditorWidgets::InputString;

        static bool InputMultiline(const char* label,
            std::string& value,
            const ImVec2& size,
            size_t capacity = 4096)
        {
            return EditorWidgets::InputMultilineString(label,
                value,
                size,
                capacity,
                ImGuiInputTextFlags_AllowTabInput);
        }

        // Mirrors the runtime grammar: an optional trailing " if flag <id>" /
// "if flag:<id>" clause (the last three whitespace tokens after "->")
// gates this option behind a progression story flag.
static void ExtractChoiceRequiredCondition(std::string& target,
            std::string& outFlag,
            std::string& outCondition)
        {
            target = Trim(target);
            if (target.empty())
                return;

            std::vector<std::string> words;
            {
                std::istringstream w(target);
                std::string w0;
                while (w >> w0)
                    words.push_back(w0);
            }
            // Trailing clause after a standalone " if ":  "if flag <id>" or
            // "if <expr>". Anything else is left as a plain label.
            if (words.size() < 3 || words[words.size() - 3] != "if")
                return;

            std::string label;
            for (size_t i = 0; i < words.size() - 3; ++i)
            {
                if (!label.empty())
                    label += " ";
                label += words[i];
            }

            std::string clause;
            for (size_t i = words.size() - 2; i < words.size(); ++i)
            {
                if (!clause.empty())
                    clause += " ";
                clause += words[i];
            }

            // "if flag <id>" is a 4-token trailing clause ("Label if flag X");
            // match on the second-last token being "flag" (mirrors runtime
            // ExtractRequiredCondition in VisualNovelScript.cpp).
            if (words.size() >= 3 && words[words.size() - 3] == "if" && words[words.size() - 2] == "flag")
            {
                std::string flagId = words.back();
                if (flagId.rfind("flag:", 0) == 0 && flagId.size() > 5)
                    flagId = flagId.substr(5);
                outFlag = Trim(flagId);
            }
            else
            {
                outCondition = Trim(clause);
            }

            target = Trim(label);
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
                    ExtractChoiceRequiredCondition(choice.Target, choice.RequiredFlag, choice.RequiredCondition);
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
                case VisualNovelScriptEditorPanel::RowKind::Set: return "Set";
                case VisualNovelScriptEditorPanel::RowKind::If: return "If";
                case VisualNovelScriptEditorPanel::RowKind::Sheet: return "Sheet";
                case VisualNovelScriptEditorPanel::RowKind::Char: return "Char";
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
                case VisualNovelScriptEditorPanel::RowKind::Set: return row.Name + " = " + row.Value;
                case VisualNovelScriptEditorPanel::RowKind::If: return row.Value + " -> " + row.Text;
                case VisualNovelScriptEditorPanel::RowKind::Sheet: return row.Value;
                case VisualNovelScriptEditorPanel::RowKind::Char: return row.Name + " (" + row.Value + "," + row.Text + ")";
            }
            return {};
        }

        static void SortUnique(std::vector<std::string>& values)
        {
            values.erase(std::remove_if(values.begin(), values.end(), [](const std::string& value)
            {
                return Trim(value).empty();
            }), values.end());
            std::sort(values.begin(), values.end());
            values.erase(std::unique(values.begin(), values.end()), values.end());
        }

        static std::vector<std::string> CollectLabels(const std::vector<VisualNovelScriptEditorPanel::Row>& rows)
        {
            std::vector<std::string> labels;
            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Label)
                    labels.push_back(row.Name);
            }
            SortUnique(labels);
            return labels;
        }

        // Returns human-readable descriptions of jump targets that do not match
        // any @label row (goto / if targets / choice targets). Playback warns at
        // runtime; surfacing it here lets designers catch typos before playing.
        static std::vector<std::string> ValidateLabelTargets(
            const std::vector<VisualNovelScriptEditorPanel::Row>& rows)
        {
            const std::vector<std::string> labels = CollectLabels(rows);
            std::vector<std::string> issues;

            auto isDangling = [&labels](const std::string& target)
            {
                if (target.empty())
                    return false;
                // External commands (scene:/event:/newgame:/loadgame:) are not
                // label jumps and must not be flagged.
                if (StartsWith(target, "scene:")
                    || StartsWith(target, "event:")
                    || StartsWith(target, "newgame:")
                    || StartsWith(target, "loadgame:"))
                    return false;
                return std::find(labels.begin(), labels.end(), target) == labels.end();
            };

            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                switch (row.Kind)
                {
                case VisualNovelScriptEditorPanel::RowKind::Goto:
                    if (isDangling(row.Value))
                        issues.push_back("Row: @goto target '" + row.Value + "' has no matching @label.");
                    break;
                case VisualNovelScriptEditorPanel::RowKind::If:
                    if (isDangling(row.Text))
                        issues.push_back("Row: @if jump target '" + row.Text + "' has no matching @label.");
                    break;
                case VisualNovelScriptEditorPanel::RowKind::Choice:
                    for (const auto& choice : row.Choices)
                    {
                        if (isDangling(choice.Target))
                            issues.push_back("Row: choice '" + choice.Text + "' target '" + choice.Target + "' has no matching @label.");
                    }
                    break;
                default:
                    break;
                }
            }
            return issues;
        }

        static std::vector<std::string> CollectCharacterIds(const std::vector<VisualNovelScriptEditorPanel::Row>& rows)
        {
            std::vector<std::string> ids;
            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Character)
                    ids.push_back(row.Name);
                else if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Dialogue)
                    ids.push_back(row.Name);
                else if (row.Kind == VisualNovelScriptEditorPanel::RowKind::Expression)
                    ids.push_back(row.Name);
            }
            SortUnique(ids);
            return ids;
        }

        static std::vector<std::string> CollectExpressionIds(const std::vector<VisualNovelScriptEditorPanel::Row>& rows,
            const std::string& character)
        {
            std::vector<std::string> ids;
            for (const VisualNovelScriptEditorPanel::Row& row : rows)
            {
                if (row.Kind != VisualNovelScriptEditorPanel::RowKind::Expression)
                    continue;
                if (!character.empty() && row.Name != character)
                    continue;
                ids.push_back(row.Value);
            }
            SortUnique(ids);
            return ids;
        }

        std::vector<std::string>& PortraitStyleChoicesCache()
        {
            static std::vector<std::string> cache;
            return cache;
        }

        void RefreshPortraitStyleChoices()
        {
            auto& choices = PortraitStyleChoicesCache();
            choices = { AssetAliasRegistry::Path("vn.default.portrait_pattern") };
            const std::filesystem::path portraitRoot = AssetPath::Resolve("assets/vertical_slice/vn/portraits");
            if (std::filesystem::is_directory(portraitRoot))
            {
                std::error_code error;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(portraitRoot, error))
                {
                    if (error || !entry.is_regular_file())
                        continue;

                    const std::string extension = entry.path().extension().generic_string();
                    if (extension != ".png" && extension != ".jpg" && extension != ".jpeg")
                        continue;

                    std::filesystem::path relative = std::filesystem::relative(entry.path(), AssetPath::GetProjectRoot(), error);
                    if (!error)
                        choices.push_back(relative.generic_string());
                }
            }
            SortUnique(choices);
        }

        const std::vector<std::string>& PortraitStyleChoices()
        {
            auto& cache = PortraitStyleChoicesCache();
            if (cache.empty())
                RefreshPortraitStyleChoices();
            return cache;
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

        EditorFloatingWindow::Begin("VN Script Editor", &m_Open, 0, { 1180.0f, 760.0f });
        EditorWidgets::PanelHeader(
            EditorLocale::Text("VN Script Editor", "视觉小说脚本编辑器"),
            EditorLocale::Text("Timeline-first authoring for dialogue, choices, characters, BGM cues, and raw script output.", "以时间线方式编辑对白、选项、角色、BGM 和脚本预览。"));
        EditorWidgets::StatusBadge((std::to_string(m_Rows.size()) + " row(s)").c_str(), EditorWidgets::StatusKind::Info);
        ImGui::SameLine();
        EditorFloatingWindow::DrawToggleButton("VN Script Editor");
        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_Dirty,
            true,
            m_SourcePath,
            m_Status
        });

        const std::vector<std::string> danglingTargets = ValidateLabelTargets(m_Rows);
        if (!danglingTargets.empty())
        {
            ImGui::SameLine();
            EditorWidgets::StatusBadge((std::to_string(danglingTargets.size()) + " dangling jump target(s)").c_str(),
                EditorWidgets::StatusKind::Warning);
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                for (const std::string& issue : danglingTargets)
                    ImGui::BulletText("%s", issue.c_str());
                ImGui::EndTooltip();
            }
        }

        DrawToolbar();

        if (ImGui::BeginTabBar("##VNEditorTabs"))
        {
            if (ImGui::BeginTabItem(EditorLocale::Text("Timeline", "时间线")))
            {
                DrawTimeline();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(EditorLocale::Text("Characters / BGM", "角色 / BGM")))
            {
                DrawLibraries();
                ImGui::EndTabItem();
            }

            if (EditorGameplayShell::BeginRawPreviewTab("Advanced Raw"))
            {
                DrawRawPreview();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        EditorFloatingWindow::End();
    }

    void VisualNovelScriptEditorPanel::Load()
    {
        m_ResolvedPath = AssetPath::Resolve(m_SourcePath);
        m_Rows.clear();
        m_SelectedRow = -1;
        m_BGMAssets.clear();
        RefreshPortraitStyleChoices();

        const std::filesystem::path bgmDirectory = AssetPath::Resolve(AssetAliasRegistry::Path("vn.path.bgm"));
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
                std::string choicesPayload = payload;
                if (choicesPayload.rfind("prompt:", 0) == 0)
                {
                    const size_t separator = choicesPayload.find('|');
                    if (separator != std::string::npos)
                    {
                        row.Value = StripQuotes(Trim(choicesPayload.substr(7, separator - 7)));
                        choicesPayload = choicesPayload.substr(separator + 1);
                    }
                }
                row.Choices = ParseChoices(choicesPayload);
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
            else if (ReadCommandPayload(line, { "set" }, payload))
            {
                // @set name value | @set name = value
                row.Kind = RowKind::Set;
                std::string remaining = payload;
                row.Name = ConsumeToken(remaining);
                remaining = Trim(remaining);
                if (StartsWith(remaining, "="))
                    remaining = Trim(remaining.substr(1));
                row.Value = StripQuotes(remaining);
            }
            else if (ReadCommandPayload(line, { "if" }, payload))
            {
                // @if <condition> -> <label> | @if <condition> goto <label>
                row.Kind = RowKind::If;
                std::string remaining = payload;
                const size_t arrow = remaining.find("->");
                if (arrow != std::string::npos)
                {
                    row.Value = Trim(remaining.substr(0, arrow));
                    row.Text = StripQuotes(Trim(remaining.substr(arrow + 2)));
                }
                else
                {
                    // "goto <label>" form: split at the first standalone " goto "
                    size_t gotoPos = std::string::npos;
                    for (size_t i = 0; i + 4 < remaining.size(); ++i)
                    {
                        if (remaining.compare(i, 5, "goto ") == 0
                            && (i == 0 || std::isspace(static_cast<unsigned char>(remaining[i - 1])))
                            && (i + 5 >= remaining.size() || std::isspace(static_cast<unsigned char>(remaining[i + 5]))))
                        {
                            gotoPos = i;
                            break;
                        }
                    }
                    if (gotoPos != std::string::npos)
                    {
                        row.Value = Trim(remaining.substr(0, gotoPos));
                        row.Text = StripQuotes(Trim(remaining.substr(gotoPos + 5)));
                    }
                    else
                    {
                        row.Value = remaining;
                    }
                }
            }
            else if (ReadCommandPayload(line, { "sheet" }, payload))
            {
                // @sheet <tex> <w> <h>
                row.Kind = RowKind::Sheet;
                std::istringstream command(payload);
                std::string texture;
                std::string w, h;
                command >> texture >> w >> h;
                row.Value = StripQuotes(texture);
                row.Name = w;
                row.Text = h;
            }
            else if (ReadCommandPayload(line, { "char" }, payload))
            {
                // @char <name> <x> <y>
                row.Kind = RowKind::Char;
                std::istringstream command(payload);
                std::string name, x, y;
                command >> name >> x >> y;
                row.Name = name;
                row.Value = x;
                row.Text = y;
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
                    if (!row.Value.empty())
                        out << "prompt:" << row.Value << " | ";
                    for (size_t i = 0; i < row.Choices.size(); ++i)
                    {
                        if (i > 0)
                            out << " | ";
                        out << row.Choices[i].Text;
                        if (!row.Choices[i].Target.empty())
                        {
                            out << " -> " << row.Choices[i].Target;
                            if (!row.Choices[i].RequiredFlag.empty())
                                out << " if flag " << row.Choices[i].RequiredFlag;
                            else if (!row.Choices[i].RequiredCondition.empty())
                                out << " if " << row.Choices[i].RequiredCondition;
                        }
                    }
                    break;
                case RowKind::Goto:
                    out << "@goto " << row.Value;
                    break;
                case RowKind::End:
                    out << "@end";
                    break;
                case RowKind::Set:
                    out << "@set " << row.Name << " = " << row.Value;
                    break;
                case RowKind::If:
                    out << "@if " << row.Value << " -> " << row.Text;
                    break;
                case RowKind::Sheet:
                    out << "@sheet " << row.Value << " " << row.Name << " " << row.Text;
                    break;
                case RowKind::Char:
                    out << "@char " << row.Name << " " << row.Value << " " << row.Text;
                    break;
            }
            out << "\n";
        }
        return out.str();
    }

    void VisualNovelScriptEditorPanel::DrawToolbar()
    {
        EditorWidgets::SectionHeader("Source", "The script file remains the data source used by runtime VN playback.");

        ImGui::PushItemWidth(-1.0f);
        if (EditorContentPickers::DrawAssetField("Script", m_SourcePath, EditorWidgets::AssetReferenceKind::Script, 512))
            m_Loaded = false;
        ImGui::PopItemWidth();

        if (ImGui::Button("Load"))
            Load();

        ImGui::SameLine();
        bool reloadClicked = false;
        if (EditorWidgets::DirtySaveBar(m_Dirty, m_Status, "Save", "Reload", &reloadClicked))
            Save();
        if (reloadClicked)
            Load();

        if (!m_ResolvedPath.empty())
            ImGui::TextDisabled("%s", m_ResolvedPath.generic_string().c_str());
    }

    void VisualNovelScriptEditorPanel::DrawTimeline()
    {
        const float leftWidth = 390.0f;
        ImGui::BeginChild("##VNRows", ImVec2(leftWidth, 0.0f), true);
        EditorWidgets::SectionHeader("Timeline", "Add, select, and reorder script rows.");

        if (ImGui::Button("+ Dialogue")) AddRow(RowKind::Dialogue);
        ImGui::SameLine();
        if (ImGui::Button("+ BGM")) AddRow(RowKind::Music);
        ImGui::SameLine();
        if (ImGui::Button("+ Choice")) AddRow(RowKind::Choice);
        if (ImGui::Button("+ Character")) AddRow(RowKind::Character);
        ImGui::SameLine();
        if (ImGui::Button("+ Show")) AddRow(RowKind::Show);
        ImGui::SameLine();
        if (ImGui::Button("+ Hide")) AddRow(RowKind::Hide);
        ImGui::SameLine();
        if (ImGui::Button("+ Expression")) AddRow(RowKind::Expression);
        if (ImGui::Button("+ Goto")) AddRow(RowKind::Goto);
        ImGui::SameLine();
        if (ImGui::Button("+ Background")) AddRow(RowKind::Background);
        ImGui::SameLine();
        if (ImGui::Button("+ Label")) AddRow(RowKind::Label);
        ImGui::SameLine();
        if (ImGui::Button("+ End")) AddRow(RowKind::End);
        ImGui::SameLine();
        if (ImGui::Button("+ Set")) AddRow(RowKind::Set);
        ImGui::SameLine();
        if (ImGui::Button("+ If")) AddRow(RowKind::If);
        ImGui::SameLine();
        if (ImGui::Button("+ Speed")) AddRow(RowKind::Speed);
        ImGui::SameLine();
        if (ImGui::Button("+ Sheet")) AddRow(RowKind::Sheet);
        ImGui::SameLine();
        if (ImGui::Button("+ Char")) AddRow(RowKind::Char);
        ImGui::SameLine();
        if (ImGui::Button("+ Raw")) AddRow(RowKind::Raw);

        ImGui::Separator();
        if (m_Rows.empty())
        {
            EditorWidgets::EmptyState("No rows yet.", "Add dialogue, choice, character, background, or BGM rows to start this script.");
        }
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            const Row& row = m_Rows[i];
            std::string label = std::to_string(i + 1) + "  [" + KindName(row.Kind) + "]  " + RowSummary(row);
            if (label.size() > 96)
                label = label.substr(0, 93) + "...";
            label = EditorWidgets::LabelWithId(label, "vn_row:" + std::to_string(i));
            if (ImGui::Selectable(label.c_str(), m_SelectedRow == i))
                m_SelectedRow = i;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##VNRowEditor", ImVec2(0.0f, 0.0f), true);
        if (m_SelectedRow >= 0 && m_SelectedRow < static_cast<int>(m_Rows.size()))
        {
            Row& row = m_Rows[m_SelectedRow];
            const std::string header = std::string("Row ") + std::to_string(m_SelectedRow + 1) + "  " + KindName(row.Kind);
            EditorWidgets::SectionHeader(header.c_str(), "Edit the selected script row.");
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
            EditorWidgets::EmptyState("No row selected.", "Select a row from the timeline or add a new one.");
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
                if (EditorContentPickers::DrawAssetField("Background", row.Value, EditorWidgets::AssetReferenceKind::Texture, 512))
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
                        for (size_t i = 0; i < m_BGMAssets.size(); ++i)
                        {
                            const std::string& asset = m_BGMAssets[i];
                            const std::string label = EditorWidgets::LabelWithId(
                                asset,
                                "vn_bgm:" + std::to_string(i));
                            if (ImGui::Selectable(label.c_str(), asset == row.Value))
                            {
                                row.Value = asset;
                                m_Dirty = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (EditorContentPickers::DrawAssetField("BGM Path", row.Value, EditorWidgets::AssetReferenceKind::Audio, 512))
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
                if (EditorContentPickers::DrawStringPicker("Portrait Style", row.Value, PortraitStyleChoices(), 512))
                    m_Dirty = true;
                break;
            case RowKind::Show:
                if (EditorContentPickers::DrawStringPicker("Characters", row.Value, CollectCharacterIds(m_Rows), 512))
                    m_Dirty = true;
                break;
            case RowKind::Hide:
                if (EditorContentPickers::DrawStringPicker("Characters / all", row.Value, CollectCharacterIds(m_Rows), 512))
                    m_Dirty = true;
                break;
            case RowKind::Expression:
                if (EditorContentPickers::DrawStringPicker("Character", row.Name, CollectCharacterIds(m_Rows), 128))
                    m_Dirty = true;
                if (EditorContentPickers::DrawStringPicker("Expression", row.Value, CollectExpressionIds(m_Rows, row.Name), 128))
                    m_Dirty = true;
                break;
            case RowKind::Dialogue:
                if (EditorContentPickers::DrawStringPicker("Speaker", row.Name, CollectCharacterIds(m_Rows), 128))
                    m_Dirty = true;
                if (InputMultiline("Text", row.Text, ImVec2(-1.0f, 130.0f)))
                    m_Dirty = true;
                break;
            case RowKind::Choice:
                if (InputString("Prompt", row.Value, 256))
                    m_Dirty = true;
                EditorWidgets::HelpTooltip("Text shown while waiting for a choice. Empty uses the default.");
                ImGui::Separator();
                for (int i = 0; i < static_cast<int>(row.Choices.size()); ++i)
                {
                    ImGui::PushID(i);
                    ImGui::Text("Choice %d", i + 1);
                    if (InputString("Text", row.Choices[i].Text, 512))
                        m_Dirty = true;
                    if (EditorContentPickers::DrawStringPicker("Target", row.Choices[i].Target, CollectLabels(m_Rows), 512))
                        m_Dirty = true;
                    if (EditorContentPickers::DrawStoryFlagField("Required Flag", row.Choices[i].RequiredFlag, 256))
                        m_Dirty = true;
                    EditorWidgets::HelpTooltip("Optional. The choice only renders when this story flag is set.");
                    if (EditorWidgets::InputString("Required Condition", row.Choices[i].RequiredCondition, 256))
                        m_Dirty = true;
                    EditorWidgets::HelpTooltip("Optional expression, e.g. \"gold >= 5\" or \"not flag FLAG_X\". Evaluated against script variables when Required Flag is empty.");
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
                    row.Choices.push_back({ EditorLocale::Text("New option", "新选项"), "target_label", {}, {} });
                    m_Dirty = true;
                }
                break;
            case RowKind::Goto:
                if (EditorContentPickers::DrawStringPicker("Target Label", row.Value, CollectLabels(m_Rows), 256))
                    m_Dirty = true;
                break;
            case RowKind::End:
                ImGui::TextDisabled("Ends this VN script.");
                break;
            case RowKind::Set:
                if (InputString("Variable", row.Name, 128))
                    m_Dirty = true;
                if (InputString("Value", row.Value, 128))
                    m_Dirty = true;
                EditorWidgets::HelpTooltip("Assigns a literal number or copies another variable (e.g. \"5\" or \"maxhp\").");
                break;
            case RowKind::If:
                if (EditorWidgets::InputString("Condition", row.Value, 256))
                    m_Dirty = true;
                EditorWidgets::HelpTooltip("Expression: always | never | flag <id> | <var> OP <number> | <number> OP <number>, optional leading \"not \".");
                if (EditorContentPickers::DrawStringPicker("Jump To", row.Text, CollectLabels(m_Rows), 256))
                    m_Dirty = true;
                break;
            case RowKind::Sheet:
                if (EditorContentPickers::DrawAssetField("Sheet Texture", row.Value, EditorWidgets::AssetReferenceKind::Texture, 512))
                    m_Dirty = true;
                if (InputString("Cell Width", row.Name, 32))
                    m_Dirty = true;
                if (InputString("Cell Height", row.Text, 32))
                    m_Dirty = true;
                break;
            case RowKind::Char:
                if (InputString("Name", row.Name, 128))
                    m_Dirty = true;
                if (InputString("Sheet X", row.Value, 32))
                    m_Dirty = true;
                if (InputString("Sheet Y", row.Text, 32))
                    m_Dirty = true;
                break;
        }
    }

    void VisualNovelScriptEditorPanel::DrawLibraries()
    {
        EditorWidgets::SectionHeader("Characters", "Rows declared with @character and used by dialogue playback.");
        bool hasCharacters = false;
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            Row& row = m_Rows[i];
            if (row.Kind != RowKind::Character)
                continue;
            hasCharacters = true;
            ImGui::PushID(i);
            ImGui::Separator();
            ImGui::Text("Row %d", i + 1);
            if (InputString("Id", row.Name, 128)) m_Dirty = true;
            if (InputString("Display", row.Text, 128)) m_Dirty = true;
            if (InputString("Style", row.Value, 512)) m_Dirty = true;
            ImGui::PopID();
        }
        if (!hasCharacters)
            EditorWidgets::EmptyState("No characters declared.", "Add a Character row in Timeline to register speaker metadata.");

        ImGui::Separator();
        EditorWidgets::SectionHeader("BGM Cues", "Rows declared with @music / @bgm.");
        bool hasBgm = false;
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            Row& row = m_Rows[i];
            if (row.Kind != RowKind::Music)
                continue;
            hasBgm = true;
            ImGui::PushID(10000 + i);
            ImGui::Separator();
            ImGui::Text("Row %d", i + 1);
            if (EditorContentPickers::DrawAssetField("Path", row.Value, EditorWidgets::AssetReferenceKind::Audio, 512)) m_Dirty = true;
            if (InputString("Display Name", row.Text, 256)) m_Dirty = true;
            ImGui::PopID();
        }
        if (!hasBgm)
            EditorWidgets::EmptyState("No BGM cues declared.", "Add a BGM row in Timeline to bind music cues.");
    }

    void VisualNovelScriptEditorPanel::DrawRawPreview()
    {
        EditorWidgets::SectionHeader("Raw Preview", "Generated script text. Save writes the Timeline data to this format.");
        std::string preview = SerializeRows();
        EditorWidgets::InputMultilineString("##RawPreview",
            preview,
            ImVec2(-1.0f, -1.0f),
            std::max<size_t>(preview.size() + 1, 4096),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);
    }

    void VisualNovelScriptEditorPanel::AddRow(RowKind kind)
    {
        Row row;
        row.Kind = kind;
        const std::vector<std::string> characterIds = CollectCharacterIds(m_Rows);
        const std::vector<std::string> labels = CollectLabels(m_Rows);
        switch (kind)
        {
            case RowKind::Label:
                row.Name = "new_label";
                break;
            case RowKind::Background:
                row.Value = AssetAliasRegistry::Path("vn.default.background");
                break;
            case RowKind::Music:
                row.Value = m_BGMAssets.empty() ? AssetAliasRegistry::Path("vn.default.bgm") : m_BGMAssets.front();
                row.Text = EditorLocale::Text("New BGM", "新BGM");
                break;
            case RowKind::Character:
                row.Name = "Character";
                row.Text = EditorLocale::Text("Character", "角色");
                row.Value = AssetAliasRegistry::Path("vn.default.portrait_pattern");
                break;
            case RowKind::Show:
                row.Value = characterIds.empty() ? "Character" : characterIds.front();
                break;
            case RowKind::Hide:
                row.Value = characterIds.empty() ? "Character" : characterIds.front();
                break;
            case RowKind::Expression:
                row.Name = characterIds.empty() ? "Character" : characterIds.front();
                row.Value = "neutral";
                break;
            case RowKind::Choice:
                row.Choices.push_back({ EditorLocale::Text("Option text", "选项文本"), "target_label", {}, {} });
                break;
            case RowKind::Dialogue:
                row.Name = "Leo";
                row.Text = EditorLocale::Text("New line.", "新台词。");
                break;
            case RowKind::Goto:
                row.Value = labels.empty() ? "target_label" : labels.front();
                break;
            case RowKind::End:
                break;
            case RowKind::Set:
                row.Name = "gold";
                row.Value = "0";
                break;
            case RowKind::If:
                row.Value = "always";
                row.Text = labels.empty() ? "target_label" : labels.front();
                break;
            case RowKind::Sheet:
                row.Value = "assets/";
                row.Name = "512";
                row.Text = "512";
                break;
            case RowKind::Char:
                row.Name = "Character";
                row.Value = "0";
                row.Text = "0";
                break;
            case RowKind::Speed:
                row.Value = "42";
                break;
            case RowKind::Raw:
                row.Raw = "";
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
