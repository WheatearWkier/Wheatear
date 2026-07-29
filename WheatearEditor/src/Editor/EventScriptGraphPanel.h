#pragma once

#include "Wheatear/Scripting/EventScript.h"

#include <string>

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
        void DrawToolbar();
        void DrawGraph();
        void DrawDetails(const EventScriptBlock* block);

        const EventScriptBlock* GetSelectedBlock() const;

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        std::string m_SourcePath = "assets/events/vertical_slice_flow.wts";
        std::string m_SelectedEvent;
        std::string m_Status;
        EventScript m_Script;
        int m_SelectedInstruction = -1;
        ImVec2 m_GraphPan = { 18.0f, 18.0f };
        float m_Zoom = 1.0f;
    };

} // namespace Wheatear
