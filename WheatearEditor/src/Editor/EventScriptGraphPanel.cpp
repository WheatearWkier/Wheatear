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
#include <sstream>
#include <vector>

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

        m_Status = "Parsed " + std::to_string(m_EditableEvents.size()) + " event(s).";
    }

    void EventScriptGraphPanel::SyncEditableEventsFromScript()
    {
        m_EditableEvents = m_Script.GetEvents();
    }

    void EventScriptGraphPanel::SyncSourceFromEditableEvents(bool dirty)
    {
        SetTextAssetBuffer(m_SourceEditorState,
            m_SourcePath,
            SerializeEventScript(m_EditableEvents),
            dirty,
            SourceEditorCapacity);

        m_Script = EventScript::FromString(m_SourceEditorState.Document.GetText());
        SyncEditableEventsFromScript();
    }

    bool EventScriptGraphPanel::SaveGraph()
    {
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
            instruction.Text = defaultText.empty() ? "scene:assets/scenes/VerticalSliceHub.wt" : defaultText;
            break;
        case EventScriptInstructionType::Wait:
            instruction.Seconds = 0.10f;
            break;
        case EventScriptInstructionType::If:
            instruction.Text = defaultText.empty() ? "flag FLAG_HUB_UNLOCKED" : defaultText;
            break;
        case EventScriptInstructionType::EndIf:
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
        if (ImGui::SmallButton("Center"))
            m_GraphPan = { 18.0f, 18.0f };
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("Zoom", &m_Zoom, 0.65f, 1.45f, "%.2f");

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

        EditorWidgets::InlineStatus("Read-only generated source. Use Graph to edit and Save Asset to write the .wts file.",
            EditorWidgets::StatusKind::Info);

        ImGui::Spacing();
        std::string preview = SerializeEventScript(m_EditableEvents);
        EditorGameplayShell::DrawRawPreview(preview, "##EventScriptSourcePreview");
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

                if (EditorGameplayShell::BeginRawPreviewTab(EditorLocale::Text("Advanced Source Preview", "高级源码预览")))
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
