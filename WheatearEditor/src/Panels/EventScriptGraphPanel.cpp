#include "wepch.h"
#include "EventScriptGraphPanel.h"

#include "Editor/CommandBuilder.h"
#include "EventScriptGraphPanelInternal.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/GameplayEditorShell.h"

#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>

namespace Wheatear {

    using namespace EventScriptGraphPanelInternal;

    namespace {
    } // namespace

    namespace EventScriptGraphRequests {

        void RequestOpenScript(const std::string& sourcePath, const std::string& eventName)
        {
            s_HasPendingOpen = true;
            s_PendingOpenPath = sourcePath;
            s_PendingOpenEvent = eventName;
        }

        bool ConsumeOpenScriptRequest(std::string& sourcePath, std::string& eventName)
        {
            if (!s_HasPendingOpen)
                return false;

            s_HasPendingOpen = false;
            sourcePath = s_PendingOpenPath;
            eventName = s_PendingOpenEvent;
            s_PendingOpenPath.clear();
            s_PendingOpenEvent.clear();
            return true;
        }

    } // namespace EventScriptGraphRequests

    void EventScriptGraphPanel::Open(const std::string& sourcePath, const std::string& eventName)
    {
        const bool sourceChanged = !sourcePath.empty() && sourcePath != m_SourcePath;
        const bool eventChanged = !eventName.empty() && eventName != m_SelectedEvent;
        if (!sourcePath.empty())
            m_SourcePath = sourcePath;
        if (!eventName.empty())
            m_SelectedEvent = eventName;

        m_Open = true;
        if (sourceChanged || eventChanged)
            m_SelectedInstruction = -1;
        SyncSourcePathInput();

        if (sourceChanged || !m_Loaded)
            Load();
        else if (!eventName.empty())
            RefreshScriptFromSource();
    }

    void EventScriptGraphPanel::Load()
    {
        EditorUI::LoadTextAsset(m_SourceEditorState, m_SourcePath, SourceEditorCapacity);
        SyncSourcePathInput();
        RefreshScriptFromSource();
        m_Loaded = true;
    }

    void EventScriptGraphPanel::SyncSourcePathInput()
    {
        CopyToPathInput(m_SourcePathInput, m_SourcePath);
    }

    void EventScriptGraphPanel::RefreshScriptFromSource()
    {
        const size_t textLength = EditorUI::GetTextLength(m_SourceEditorState.Buffer);
        const std::string sourceText(m_SourceEditorState.Buffer.data(), m_SourceEditorState.Buffer.data() + textLength);
        const std::string previousEvent = m_SelectedEvent;

        m_Script = EventScript::FromString(sourceText);
        SyncEditableEventsFromScript();

        if (m_EditableEvents.empty())
        {
            m_SelectedEvent.clear();
            m_SelectedInstruction = -1;
            m_Status = "No events found.";
            return;
        }

        const auto selected = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
            [&](const EventScriptBlock& block) { return block.Name == m_SelectedEvent; });
        if (m_SelectedEvent.empty() || selected == m_EditableEvents.end())
            m_SelectedEvent = m_EditableEvents.front().Name;

        if (m_SelectedEvent != previousEvent)
            m_SelectedInstruction = -1;
        else if (const EventScriptBlock* block = GetSelectedBlock();
                 block && m_SelectedInstruction >= static_cast<int>(block->Instructions.size()))
            m_SelectedInstruction = -1;

        m_Status = "Parsed " + std::to_string(m_EditableEvents.size()) + " event(s)";
        const size_t unrecognized = m_Script.GetUnrecognizedLineCount();
        if (unrecognized > 0)
            m_Status += ", " + std::to_string(unrecognized) + " unrecognized line(s) preserved";
        m_Status += ".";
    }

    void EventScriptGraphPanel::SyncEditableEventsFromScript()
    {
        m_EditableEvents = m_Script.GetEvents();
        m_EditableOrphans = m_Script.GetOrphanLines();
    }

    void EventScriptGraphPanel::SyncSourceFromEditableEvents(bool dirty)
    {
        SetTextAssetBuffer(m_SourceEditorState,
            m_SourcePath,
            SerializeEventScript(m_EditableEvents, m_Script.GetHeaderComments(), m_EditableOrphans),
            dirty,
            SourceEditorCapacity);

        m_Script = EventScript::FromString(m_SourceEditorState.Document.GetText());
        SyncEditableEventsFromScript();
        // Graph edits win over raw edits: switch back to graph-authoring view.
        m_RawSourceEdited = false;
        m_RawSourceMode = false;
    }

    void EventScriptGraphPanel::CommitRawTextToGraph()
    {
        if (!m_RawSourceEdited)
            return;

        // Re-parse the raw text so the graph reflects the hand-written edits;
        // unrecognized lines are preserved rather than dropped.
        m_Script = EventScript::FromString(
            std::string(m_SourceEditorState.Buffer.data(),
                m_SourceEditorState.Buffer.data() + EditorUI::GetTextLength(m_SourceEditorState.Buffer)));
        SyncEditableEventsFromScript();
        m_SourceEditorState.Document.SetText(
            std::string(m_SourceEditorState.Buffer.data(),
                m_SourceEditorState.Buffer.data() + EditorUI::GetTextLength(m_SourceEditorState.Buffer)),
            true);
        m_RawSourceEdited = false;
        m_Status = "Raw text applied to graph (unrecognized lines preserved).";
    }

    bool EventScriptGraphPanel::SaveGraph()
    {
        if (m_RawSourceMode && m_RawSourceEdited)
        {
            // Raw text is authoritative: re-parse it so the graph reflects
            // the hand-written edits, then write the buffer verbatim so the
            // author's formatting (blank lines, indentation) survives.
            CommitRawTextToGraph();
            if (!EditorUI::SaveTextAsset(m_SourceEditorState, m_SourcePath))
            {
                m_Status = m_SourceEditorState.Status;
                return false;
            }

            RefreshScriptFromSource();
            m_Status = "Saved " + std::to_string(m_EditableEvents.size())
                + " event script event(s) (raw text).";
            EditorCommandBuilder::RefreshAssetChoices();
            return true;
        }

        SyncSourceFromEditableEvents(true);
        if (!EditorUI::SaveTextAsset(m_SourceEditorState, m_SourcePath))
        {
            m_Status = m_SourceEditorState.Status;
            return false;
        }

        RefreshScriptFromSource();
        m_Status = "Saved " + std::to_string(m_EditableEvents.size()) + " event script event(s).";
        EditorCommandBuilder::RefreshAssetChoices();
        return true;
    }

    void EventScriptGraphPanel::AddEvent()
    {
        EventScriptBlock event;
        event.Name = UniqueEventName(m_EditableEvents, "new_event");
        m_EditableEvents.push_back(std::move(event));
        m_SelectedEvent = m_EditableEvents.back().Name;
        m_SelectedInstruction = -1;
        SyncSourceFromEditableEvents(true);
        m_Status = "Added event.";
    }

    void EventScriptGraphPanel::AddInstruction(EventScriptInstructionType type, const std::string& defaultText)
    {
        EventScriptBlock* block = GetSelectedBlockMutable();
        if (!block)
            return;

        EventScriptInstruction instruction;
        instruction.Type = type;
        switch (type)
        {
        case EventScriptInstructionType::Command:
            // A neutral placeholder instead of a demo-scene default so new
            // scripts cannot accidentally reference an unrelated scene.
            instruction.Text = defaultText.empty() ? "scene:assets/scenes/" : defaultText;
            break;
        case EventScriptInstructionType::Wait:
            instruction.Seconds = 0.10f;
            break;
        case EventScriptInstructionType::If:
            instruction.Text = defaultText.empty() ? "flag " : defaultText;
            break;
        case EventScriptInstructionType::EndIf:
            break;
        case EventScriptInstructionType::RawLine:
            instruction.Text = defaultText.empty() ? "raw line" : defaultText;
            break;
        }

        const int insertAt = m_SelectedInstruction >= 0
            ? std::min(m_SelectedInstruction + 1, static_cast<int>(block->Instructions.size()))
            : static_cast<int>(block->Instructions.size());
        block->Instructions.insert(block->Instructions.begin() + insertAt, std::move(instruction));
        m_SelectedInstruction = insertAt;
        SyncSourceFromEditableEvents(true);
        m_Status = "Added instruction.";
    }

    const EventScriptBlock* EventScriptGraphPanel::GetSelectedBlock() const
    {
        if (m_SelectedEvent.empty())
            return nullptr;

        const auto it = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
            [&](const EventScriptBlock& block) { return block.Name == m_SelectedEvent; });
        return it == m_EditableEvents.end() ? nullptr : &(*it);
    }

    EventScriptBlock* EventScriptGraphPanel::GetSelectedBlockMutable()
    {
        if (m_SelectedEvent.empty())
            return nullptr;

        const auto it = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
            [&](const EventScriptBlock& block) { return block.Name == m_SelectedEvent; });
        return it == m_EditableEvents.end() ? nullptr : &(*it);
    }

    void EventScriptGraphPanel::DrawToolbar()
    {
        ImGui::SetNextItemWidth(340.0f);
        ImGui::InputText("Script Path", m_SourcePathInput.data(), m_SourcePathInput.size());
        const bool pathCommitted = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Load", "加载")) || pathCommitted)
        {
            m_SourcePath = m_SourcePathInput.data();
            Load();
        }

        const auto& events = m_EditableEvents;
        if (ImGui::BeginCombo("Event", m_SelectedEvent.empty() ? "(none)" : m_SelectedEvent.c_str()))
        {
            for (size_t i = 0; i < events.size(); ++i)
            {
                const auto& event = events[i];
                const bool selected = event.Name == m_SelectedEvent;
                const std::string label = EditorWidgets::LabelWithId(
                    event.Name,
                    "event_script_event:" + std::to_string(i));
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    m_SelectedEvent = event.Name;
                    m_SelectedInstruction = -1;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Add Event"))
            AddEvent();
        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedEvent.empty());
        if (ImGui::SmallButton("Duplicate Event"))
        {
            const auto it = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
                [&](const EventScriptBlock& event) { return event.Name == m_SelectedEvent; });
            if (it != m_EditableEvents.end())
            {
                const std::string copyName =
                    UniqueEventName(m_EditableEvents, it->Name + "_copy");
                EventScriptBlock copy = *it;
                copy.Name = copyName;
                copy.LeadingComments.clear();
                m_EditableEvents.insert(it + 1, std::move(copy));
                m_SelectedEvent = copyName;
                m_SelectedInstruction = -1;
                SyncSourceFromEditableEvents(true);
                m_Status = "Duplicated event.";
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete Event"))
        {
            const auto it = std::find_if(m_EditableEvents.begin(), m_EditableEvents.end(),
                [&](const EventScriptBlock& event) { return event.Name == m_SelectedEvent; });
            if (it != m_EditableEvents.end())
            {
                m_EditableEvents.erase(it);
                m_SelectedEvent = m_EditableEvents.empty() ? std::string{} : m_EditableEvents.front().Name;
                m_SelectedInstruction = -1;
                SyncSourceFromEditableEvents(true);
                m_Status = "Deleted event.";
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::SmallButton("Save Graph"))
            SaveGraph();
        ImGui::SameLine();
        if (ImGui::SmallButton(EditorLocale::Text("Center", "居中")))
            m_GraphPan = { 18.0f, 18.0f };
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("Zoom", &m_Zoom, 0.65f, 1.45f, "%.2f");
        ImGui::SameLine();
        if (ImGui::SmallButton(m_RawSourceMode
                ? EditorLocale::Text("Graph Mode", "图形模式")
                : EditorLocale::Text("Edit Raw", "编辑原文")))
        {
            if (m_RawSourceMode)
            {
                CommitRawTextToGraph();
                m_RawSourceMode = false;
            }
            else
            {
                SyncSourceFromEditableEvents(false);
                m_RawSourceMode = true;
                m_RawSourceEdited = false;
            }
        }

        if (!m_Status.empty())
            ImGui::TextDisabled("%s", m_Status.c_str());
    }


    void EventScriptGraphPanel::DrawSourceEditor()
    {
        if (m_SourceEditorState.Buffer.empty())
            EditorUI::ResetBuffer(m_SourceEditorState, SourceEditorCapacity);

        if (ImGui::Button(EditorLocale::Text("Reload", "重载")))
            Load();

        if (m_SourceEditorState.Dirty)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.24f, 1.0f), "Graph has unsaved changes");
        }

        if (!m_SourceEditorState.Status.empty())
            ImGui::TextDisabled("%s", m_SourceEditorState.Status.c_str());
        if (!m_SourceEditorState.ResolvedPath.empty())
            ImGui::TextDisabled("Resolved: %s", m_SourceEditorState.ResolvedPath.generic_string().c_str());

        if (!m_RawSourceMode)
        {
            EditorWidgets::InlineStatus(
                "Generated source (read-only in Graph mode). Turn on Edit Raw to edit the text; saving "
                "writes it verbatim and unrecognized lines are preserved instead of being dropped.",
                EditorWidgets::StatusKind::Info);

            ImGui::Spacing();
            const std::string preview = SerializeEventScript(
                m_EditableEvents, m_Script.GetHeaderComments(), m_EditableOrphans);
            EditorGameplayShell::DrawRawPreview(preview, "##EventScriptSourcePreview");
            return;
        }

        EditorWidgets::InlineStatus(
            "Raw text editing: your text is authoritative. Save writes it verbatim. "
            "Unrecognized lines are preserved in the graph instead of being dropped.",
            EditorWidgets::StatusKind::Info);
        if (m_RawSourceEdited)
            EditorWidgets::InlineStatus(
                "Raw text modified; Save will write it verbatim.",
                EditorWidgets::StatusKind::Warning);

        ImGui::Spacing();

        const size_t textLength = EditorUI::GetTextLength(m_SourceEditorState.Buffer);
        std::string rawText(m_SourceEditorState.Buffer.data(), m_SourceEditorState.Buffer.data() + textLength);
        if (EditorWidgets::InputMultilineString(
                "##EventScriptRawEdit",
                rawText,
                { -1.0f, -1.0f },
                SourceEditorCapacity,
                ImGuiInputTextFlags_AllowTabInput))
        {
            const size_t copyLength = std::min(rawText.size(), SourceEditorCapacity - 1);
            m_SourceEditorState.Buffer.assign(SourceEditorCapacity, '\0');
            if (copyLength > 0)
                std::memcpy(m_SourceEditorState.Buffer.data(), rawText.data(), copyLength);
            m_RawSourceEdited = true;
            m_SourceEditorState.Dirty = true;
        }
    }

    void EventScriptGraphPanel::DrawOrphansPanel()
    {
        if (m_EditableOrphans.empty())
        {
            EditorWidgets::EmptyState(
                "No orphan lines.",
                "Lines outside any event block are preserved here instead of being silently dropped.");
            if (ImGui::Button(EditorLocale::Text("Add Orphan Line", "添加游离行")))
            {
                EventScriptInstruction orphan;
                orphan.Type = EventScriptInstructionType::RawLine;
                orphan.Text = "raw line";
                m_EditableOrphans.push_back(std::move(orphan));
                SyncSourceFromEditableEvents(true);
                m_Status = "Added orphan line.";
            }
            return;
        }

        ImGui::TextDisabled(EditorLocale::Text(
            "Orphan lines keep their original text on save; convert them to commands, edit, or delete.",
            "游离行保存时保留原文；可转换为命令、编辑或删除。"));

        size_t indexToDelete = m_EditableOrphans.size();
        for (size_t i = 0; i < m_EditableOrphans.size(); ++i)
        {
            EventScriptInstruction& orphan = m_EditableOrphans[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextDisabled("L%d", orphan.SourceLine);
            ImGui::SameLine();
            std::string text = orphan.Text;
            if (EditorWidgets::InputString(EditorLocale::Text("Line", "行"), text, 512))
                orphan.Text = text;

            ImGui::SameLine();
            if (ImGui::SmallButton(EditorLocale::Text("As Command", "转为命令")))
            {
                orphan.Type = EventScriptInstructionType::Command;
                SyncSourceFromEditableEvents(true);
                m_Status = "Converted orphan to command.";
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(EditorLocale::Text("Delete", "删除")))
            {
                indexToDelete = i;
                SyncSourceFromEditableEvents(true);
                m_Status = "Deleted orphan line.";
            }
            ImGui::PopID();
        }

        if (indexToDelete < m_EditableOrphans.size())
            m_EditableOrphans.erase(m_EditableOrphans.begin() + static_cast<std::ptrdiff_t>(indexToDelete));
    }


    void EventScriptGraphPanel::OnImGuiRender()
    {
        std::string pendingPath;
        std::string pendingEvent;
        if (EventScriptGraphRequests::ConsumeOpenScriptRequest(pendingPath, pendingEvent))
            Open(pendingPath, pendingEvent);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        if (EditorFloatingWindow::Begin("Event Script Editor", &m_Open, 0, { 1220.0f, 760.0f }))
        {
            EditorFloatingWindow::DrawToggleButton("Event Script Editor");
            ImGui::Separator();
            EditorGameplayShell::DrawDocumentStatus({
                EditorGameplayShell::DocumentKind::Asset,
                m_SourceEditorState.Dirty,
                true,
                m_SourcePath,
                m_Status
            });
            ImGui::Separator();
            DrawToolbar();
            ImGui::Separator();
            if (ImGui::BeginTabBar("##EventScriptEditorTabs"))
            {
                if (ImGui::BeginTabItem(EditorLocale::Text("Graph", "图形")))
                {
                    DrawGraph();
                    ImGui::EndTabItem();
                }

                const std::string orphansTabLabel =
                    std::string(EditorLocale::Text("Orphans", "游离行"))
                    + (m_EditableOrphans.empty()
                        ? std::string{}
                        : std::string(" (") + std::to_string(m_EditableOrphans.size()) + ")");
                if (ImGui::BeginTabItem(orphansTabLabel.c_str()))
                {
                    DrawOrphansPanel();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(EditorLocale::Text("Source", "源码")))
                {
                    DrawSourceEditor();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        EditorFloatingWindow::End();
    }

} // namespace Wheatear
