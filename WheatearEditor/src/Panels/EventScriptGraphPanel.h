#pragma once

#include "Editor/TextAssetEditor.h"
#include "Wheatear/Scripting/EventScript.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <imgui/imgui.h>
#include "TextEditor.h"

namespace Wheatear {

    namespace EventScriptGraphRequests {
        void RequestOpenScript(const std::string& sourcePath,
            const std::string& eventName = {}, int sourceLine = 0);
        bool ConsumeOpenScriptRequest(std::string& sourcePath,
            std::string& eventName, int& sourceLine);
    }

    class EventScriptGraphPanel
    {
    public:
        ~EventScriptGraphPanel();
        void Open(const std::string& sourcePath,
            const std::string& eventName = {}, int sourceLine = 0);
        void OnImGuiRender();

    private:
        void Load();
        void SyncSourcePathInput();
        void DrawToolbar();
        void DrawGraph();
        void DrawSourceEditor();
        void DrawOrphansPanel();
        void DrawDetails(const EventScriptBlock* block);
        void RefreshScriptFromSource();
        void SyncEditableEventsFromScript();
        void SyncSourceFromEditableEvents(bool dirty);
        bool SaveGraph();
        void AddEvent();
        void AddInstruction(EventScriptInstructionType type, const std::string& defaultText = {});
        void CommitRawTextToGraph();
        void CaptureUndoSnapshot(const std::string& nextSource);
        void Undo();
        void Redo();
        void ApplySnapshot(size_t index, bool redo);

        const EventScriptBlock* GetSelectedBlock() const;
        EventScriptBlock* GetSelectedBlockMutable();

    private:
        struct EditSnapshot
        {
            std::vector<EventScriptBlock> Events;
            std::vector<EventScriptInstruction> Orphans;
            std::string SelectedEvent;
            int SelectedInstruction = -1;
            std::string Source;
        };

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        std::string m_SourcePath = "assets/events/sandbox_flow.wts";
        std::string m_SelectedEvent;
        std::string m_Status;
        std::array<char, 512> m_SourcePathInput{};
        EditorUI::TextAssetEditorState m_SourceEditorState;
        EventScript m_Script;
        std::vector<EventScriptBlock> m_EditableEvents;
        // Lines that appeared outside any event block; kept editable so the
        // graph round-trip is lossless (nothing is silently dropped).
        std::vector<EventScriptInstruction> m_EditableOrphans;
        // When true the source preview owns the document: raw text edits are
        // authoritative and Save writes them verbatim (after re-parsing).
        bool m_RawSourceMode = false;
        bool m_RawSourceEdited = false;
        int m_SelectedInstruction = -1;
        ImVec2 m_GraphPan = { 18.0f, 18.0f };
        float m_Zoom = 1.0f;
        float m_DetailsWidth = 300.0f;
        std::vector<EditSnapshot> m_UndoStack;
        std::vector<EditSnapshot> m_RedoStack;
        std::string m_LastCommittedSource;
        std::string m_SavedSource;
        bool m_SuppressUndoCapture = false;
        int m_PendingRevealLine = 0;
        std::unique_ptr<TextEditor> m_RawEditor;
        std::string m_RawEditorText;
    };

} // namespace Wheatear
