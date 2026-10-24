#include "wepch.h"
#include "VisualNovelScriptEditorPanel.h"

#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "VisualNovelScriptEditorPanelInternal.h"
#include "Editor/GameplayEditorShell.h"
#include "Panels/DataFileEditorPanel.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelScript.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <sstream>

namespace Wheatear {

    using namespace VisualNovelScriptEditorPanelInternal;

    namespace {

        const TextEditor::LanguageDefinition& VisualNovelLanguage()
        {
            static const TextEditor::LanguageDefinition definition = []()
            {
                TextEditor::LanguageDefinition language;
                language.mName = "Wheatear Visual Novel";
                language.mCaseSensitive = true;
                language.mAutoIndentation = false;
                language.mSingleLineComment = "#";
                language.mKeywords = {
                    "@background", "@music", "@bgm", "@speed", "@label",
                    "@goto", "@end", "@choice", "@expression", "@show",
                    "@hide", "@character", "@sheet", "@char", "@set", "@if"
                };
                language.mTokenRegexStrings = {
                    { "[@]?[a-zA-Z_][a-zA-Z0-9_:\\.]*", TextEditor::PaletteIndex::Identifier },
                    { "[0-9]+(\\.[0-9]+)?", TextEditor::PaletteIndex::Number },
                    { "\"(?:[^\"\\\\]|\\\\.)*\"", TextEditor::PaletteIndex::String },
                    { "'(?:[^'\\\\]|\\\\.)*'", TextEditor::PaletteIndex::CharLiteral }
                };
                return language;
            }();
            return definition;
        }

        void UpdateVisualNovelMarkers(TextEditor& editor, const std::string& text)
        {
            std::map<int, std::string> markers;
            for (const VisualNovelIssue& issue
                : VisualNovelScript::FromString(text).GetIssues())
            {
                if (issue.Error)
                    markers[issue.SourceLine] = issue.Message;
            }
            editor.SetErrorMarkers(markers);
        }

    } // namespace

    namespace VisualNovelEditorRequests {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;
        static int s_PendingOpenLine = 0;

        void RequestOpenScript(const std::string& sourcePath, int sourceLine)
        {
            s_PendingOpenPath = sourcePath;
            s_PendingOpenLine = sourceLine;
            s_HasPendingOpen = true;
        }

        bool ConsumeOpenScriptRequest(std::string& sourcePath, int& sourceLine)
        {
            if (!s_HasPendingOpen)
                return false;

            sourcePath = s_PendingOpenPath;
            sourceLine = s_PendingOpenLine;
            s_PendingOpenPath.clear();
            s_PendingOpenLine = 0;
            s_HasPendingOpen = false;
            return true;
        }

    } // namespace VisualNovelEditorRequests

    void VisualNovelScriptEditorPanel::Open(const std::string& sourcePath, int sourceLine)
    {
        m_Open = true;
        if (m_PendingAction != PendingAction::None)
            return;
        m_PendingRevealLine = sourceLine;
        if (!sourcePath.empty() && sourcePath != m_SourcePath)
        {
            if (m_Dirty && m_Loaded)
            {
                m_PendingAction = PendingAction::OpenPath;
                m_PendingPath = sourcePath;
                return;
            }
            m_SourcePath = sourcePath;
            m_Loaded = false;
        }
    }

    void VisualNovelScriptEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        int requestedLine = 0;
        if (VisualNovelEditorRequests::ConsumeOpenScriptRequest(requestedPath, requestedLine))
            Open(requestedPath, requestedLine);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        if (m_PendingRevealLine > 0 && !m_Rows.empty())
        {
            int best = 0;
            int bestDistance = std::numeric_limits<int>::max();
            for (size_t i = 0; i < m_Rows.size(); ++i)
            {
                const int distance = std::abs(m_Rows[i].SourceLine - m_PendingRevealLine);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = static_cast<int>(i);
                }
            }
            m_SelectedRow = best;
            m_PendingRevealLine = 0;
            m_ScrollToSelected = true;
        }

        const bool wasOpen = m_Open;
        const std::vector<Row> previousRows = m_Rows;
        const int previousSelectedRow = m_SelectedRow;

        if (!ImGui::GetIO().WantTextInput)
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
                Redo();
            else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
                Undo();
            else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
                Redo();
        }

        EditorFloatingWindow::Begin("VN Script Editor", &m_Open, 0, { 1180.0f, 760.0f });
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::GetIO().KeyCtrl
            && ImGui::IsKeyPressed(ImGuiKey_S, false))
            Save();
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

        if (!m_SuppressUndoCapture)
        {
            const std::string currentText = SerializeRows();
            if (currentText != m_LastObservedText)
            {
                PushUndo(previousRows, previousSelectedRow);
                m_LastObservedText = currentText;
                m_Dirty = currentText != m_SavedText;
            }
        }

        if (wasOpen && !m_Open && m_Dirty)
        {
            m_Open = true;
            m_PendingAction = PendingAction::Close;
        }

        DrawUnsavedPopup();
    }

    void VisualNovelScriptEditorPanel::PushUndo(const std::vector<Row>& rows,
        int selectedRow)
    {
        m_UndoStack.push_back({ rows, selectedRow });
        if (m_UndoStack.size() > 64)
            m_UndoStack.erase(m_UndoStack.begin());
        m_RedoStack.clear();
    }

    void VisualNovelScriptEditorPanel::Undo()
    {
        if (m_UndoStack.empty())
            return;

        RowSnapshot snapshot = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        m_RedoStack.push_back({ m_Rows, m_SelectedRow });
        m_Rows = std::move(snapshot.Rows);
        m_SelectedRow = snapshot.SelectedRow;
        m_SuppressUndoCapture = true;
        m_LastObservedText = SerializeRows();
        m_SuppressUndoCapture = false;
        m_Dirty = m_LastObservedText != m_SavedText;
        m_Status = "Undo.";
    }

    void VisualNovelScriptEditorPanel::Redo()
    {
        if (m_RedoStack.empty())
            return;

        RowSnapshot snapshot = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        m_UndoStack.push_back({ m_Rows, m_SelectedRow });
        m_Rows = std::move(snapshot.Rows);
        m_SelectedRow = snapshot.SelectedRow;
        m_SuppressUndoCapture = true;
        m_LastObservedText = SerializeRows();
        m_SuppressUndoCapture = false;
        m_Dirty = m_LastObservedText != m_SavedText;
        m_Status = "Redo.";
    }

    void VisualNovelScriptEditorPanel::ExecutePendingAction()
    {
        switch (m_PendingAction)
        {
        case PendingAction::OpenPath:
            m_SourcePath = m_PendingPath;
            m_Loaded = false;
            m_Dirty = false;
            break;
        case PendingAction::Reload:
            m_Dirty = false;
            m_Loaded = false;
            break;
        case PendingAction::Close:
            m_Dirty = false;
            m_Open = false;
            break;
        case PendingAction::None:
            break;
        }
        m_PendingAction = PendingAction::None;
        m_PendingPath.clear();
    }

    void VisualNovelScriptEditorPanel::DrawUnsavedPopup()
    {
        if (m_PendingAction == PendingAction::None)
            return;

        const char* popupId = "Unsaved VN Script##VNUnsaved";
        if (!ImGui::IsPopupOpen(popupId))
            ImGui::OpenPopup(popupId);

        if (ImGui::BeginPopupModal(popupId, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(EditorLocale::Text(
                "This script has unsaved changes.",
                "当前脚本有未保存的修改。"));
            ImGui::Spacing();

            if (ImGui::Button(EditorLocale::Text("Save", "保存")))
            {
                if (Save())
                {
                    ExecutePendingAction();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Discard", "放弃")))
            {
                ExecutePendingAction();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Cancel", "取消")))
            {
                m_PendingAction = PendingAction::None;
                m_PendingPath.clear();
                ImGui::CloseCurrentPopup();
            }
            if (!m_Status.empty())
            {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", m_Status.c_str());
            }
            ImGui::EndPopup();
        }
    }

    void VisualNovelScriptEditorPanel::Load()
    {
        m_RawEditor.reset();
        m_RawEditorText.clear();
        m_RawEditorDirty = false;
        m_ResolvedPath = AssetPath::Resolve(AssetAliasRegistry::Resolve(m_SourcePath));
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
            m_LastObservedText = SerializeRows();
            m_SavedText = m_LastObservedText;
            m_UndoStack.clear();
            m_RedoStack.clear();
            return;
        }

        std::ifstream input(m_ResolvedPath, std::ios::binary);
        if (!input)
        {
            m_Loaded = true;
            m_Dirty = false;
            m_Status = "Failed to open file.";
            m_LastObservedText = SerializeRows();
            m_SavedText = m_LastObservedText;
            m_UndoStack.clear();
            m_RedoStack.clear();
            return;
        }

        std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        ParseText(text);
        m_Loaded = true;
        m_Dirty = false;
        m_Status = "Loaded.";
        m_LastObservedText = SerializeRows();
        m_SavedText = m_LastObservedText;
        m_UndoStack.clear();
        m_RedoStack.clear();
    }

    bool VisualNovelScriptEditorPanel::Save()
    {
        if (m_RawEditorDirty && m_RawEditor)
        {
            ParseText(m_RawEditor->GetText());
            m_RawEditorDirty = false;
        }

        const std::string text = SerializeRows();
        const VisualNovelScript candidate = VisualNovelScript::FromString(text);
        for (const VisualNovelIssue& issue : candidate.GetIssues())
        {
            if (!issue.Error)
                continue;
            m_Status = "Save blocked at line " + std::to_string(issue.SourceLine)
                + ": " + issue.Message;
            return false;
        }

        m_ResolvedPath = AssetPath::Resolve(AssetAliasRegistry::Resolve(m_SourcePath));
        const std::filesystem::path parent = m_ResolvedPath.parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);

        std::ofstream output(m_ResolvedPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            m_Status = "Failed to save file.";
            return false;
        }

        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        m_Dirty = false;
        m_Status = "Saved.";
        m_LastObservedText = text;
        m_SavedText = text;
        m_RawEditorDirty = false;
        m_RawEditor.reset();
        m_RawEditorText.clear();
        return true;
    }

    void VisualNovelScriptEditorPanel::ParseText(const std::string& text)
    {
        std::istringstream stream(text);
        std::string rawLine;
        int sourceLine = 0;
        while (std::getline(stream, rawLine))
        {
            ++sourceLine;
            Row row;
            row.SourceLine = sourceLine;
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
                    const size_t separator = FindChoiceSeparator(choicesPayload);
                    if (separator != std::string::npos)
                    {
                        row.Value = UnescapeInlineText(
                            StripQuotes(Trim(choicesPayload.substr(7, separator - 7))));
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
                    row.Text = UnescapeInlineText(Trim(line.substr(colon + 1)));
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
                    out << row.Name << ": " << EscapeInlineText(row.Text);
                    break;
                case RowKind::Choice:
                    out << "@choice ";
                    if (!row.Value.empty())
                        out << "prompt:" << QuoteIfNeeded(row.Value) << " | ";
                    for (size_t i = 0; i < row.Choices.size(); ++i)
                    {
                        if (i > 0)
                            out << " | ";
                        out << QuoteIfNeeded(row.Choices[i].Text);
                        if (!row.Choices[i].Target.empty())
                        {
                            out << " -> " << QuoteIfNeeded(row.Choices[i].Target);
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
        std::string nextPath = m_SourcePath;
        if (EditorContentPickers::DrawAssetField("Script", nextPath, EditorWidgets::AssetReferenceKind::Script, 512)
            && nextPath != m_SourcePath)
        {
            if (m_Dirty)
            {
                m_PendingAction = PendingAction::OpenPath;
                m_PendingPath = nextPath;
            }
            else
            {
                m_SourcePath = nextPath;
                m_Loaded = false;
            }
        }
        ImGui::PopItemWidth();

        ImGui::BeginDisabled(m_UndoStack.empty());
        if (ImGui::Button(EditorLocale::Text("Undo", "撤销")))
            Undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_RedoStack.empty());
        if (ImGui::Button(EditorLocale::Text("Redo", "重做")))
            Redo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Load", "加载")))
        {
            if (m_Dirty)
                m_PendingAction = PendingAction::Reload;
            else
                Load();
        }

        ImGui::SameLine();
        bool reloadClicked = false;
        if (EditorWidgets::DirtySaveBar(m_Dirty, m_Status, "Save", "Reload", &reloadClicked))
            Save();
        if (reloadClicked)
        {
            if (m_Dirty)
                m_PendingAction = PendingAction::Reload;
            else
                Load();
        }

        if (!m_ResolvedPath.empty())
            ImGui::TextDisabled("%s", m_ResolvedPath.generic_string().c_str());
    }




    void VisualNovelScriptEditorPanel::DrawRawPreview()
    {
        EditorWidgets::SectionHeader("Raw Script",
            "Syntax-highlighted inline editing. Apply writes the text back into the Timeline rows; Save still writes the normalized script.");
        const std::string preview = SerializeRows();

        if (!m_RawEditor)
        {
            m_RawEditor = std::make_unique<TextEditor>();
            m_RawEditor->SetLanguageDefinition(VisualNovelLanguage());
            m_RawEditor->SetPalette(TextEditor::GetDarkPalette());
            m_RawEditor->SetText(preview);
            UpdateVisualNovelMarkers(*m_RawEditor, preview);
            m_RawEditorText = preview;
        }
        else if (!m_RawEditorDirty
            && m_RawEditor->GetText() == m_RawEditorText
            && m_RawEditorText != preview)
        {
            // Timeline rows changed outside raw editing; refresh the text view.
            m_RawEditor->SetText(preview);
            UpdateVisualNovelMarkers(*m_RawEditor, preview);
            m_RawEditorText = preview;
        }

        m_RawEditor->Render("##VNCodeEditor", ImVec2(-1.0f, -46.0f), true);
        if (m_RawEditor->IsTextChanged())
        {
            m_RawEditorText = m_RawEditor->GetText();
            UpdateVisualNovelMarkers(*m_RawEditor, m_RawEditorText);
            m_RawEditorDirty = true;
            m_Dirty = true;
        }

        if (ImGui::Button(EditorLocale::Text("Apply Raw to Timeline", "应用到时间线")))
        {
            ParseText(m_RawEditorText);
            m_RawEditorDirty = false;
            m_Dirty = true;
            m_Status = "Applied raw text to the timeline.";
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(m_Dirty);
        if (ImGui::Button(EditorLocale::Text("Edit Source in Data File Editor", "在数据文件编辑器中编辑源文件")))
            DataFileEditorRequests::RequestOpen(m_SourcePath);
        ImGui::EndDisabled();
        if (m_Dirty)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s",
                EditorLocale::Text("Apply or save before editing the file elsewhere.",
                    "请先应用或保存，再到其它编辑器修改源文件。"));
        }
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
