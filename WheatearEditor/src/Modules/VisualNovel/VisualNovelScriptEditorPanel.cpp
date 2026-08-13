#include "wepch.h"
#include "VisualNovelScriptEditorPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "VisualNovelScriptEditorPanelInternal.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <sstream>

namespace Wheatear {

    using namespace VisualNovelScriptEditorPanelInternal;

    namespace {
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

        if (ImGui::Button(EditorLocale::Text("Load", "加载")))
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
