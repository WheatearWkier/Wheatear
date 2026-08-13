#pragma once

#include "Editor/TextAssetEditor.h"
#include "Wheatear/Scripting/EventScript.h"

#include <array>
#include <string>
#include <vector>

#include <imgui/imgui.h>

namespace Wheatear {

    namespace EventScriptGraphRequests {
        void RequestOpenScript(const std::string& sourcePath, const std::string& eventName = {});
        bool ConsumeOpenScriptRequest(std::string& sourcePath, std::string& eventName);
    }

    class EventScriptGraphPanel
    {
    public:
        void Open(const std::string& sourcePath, const std::string& eventName = {});
        void OnImGuiRender();

    private:
        void Load();
        void SyncSourcePathInput();
        void DrawToolbar();
        void DrawGraph();
        void DrawSourceEditor();
        void DrawDetails(const EventScriptBlock* block);
        void RefreshScriptFromSource();
        void SyncEditableEventsFromScript();
        void SyncSourceFromEditableEvents(bool dirty);
        bool SaveGraph();
        void AddEvent();
        void AddInstruction(EventScriptInstructionType type, const std::string& defaultText = {});

        const EventScriptBlock* GetSelectedBlock() const;
        EventScriptBlock* GetSelectedBlockMutable();

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        std::string m_SourcePath = "assets/events/vertical_slice_flow.wts";
        std::string m_SelectedEvent;
        std::string m_Status;
        std::array<char, 512> m_SourcePathInput{};
        EditorUI::TextAssetEditorState m_SourceEditorState;
        EventScript m_Script;
        std::vector<EventScriptBlock> m_EditableEvents;
        int m_SelectedInstruction = -1;
        ImVec2 m_GraphPan = { 18.0f, 18.0f };
        float m_Zoom = 1.0f;
    };

} // namespace Wheatear
