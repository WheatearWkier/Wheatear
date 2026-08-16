#pragma once

#include "Wheatear/Core/Log.h"

#include <string>
#include <vector>

namespace Wheatear {

    // Editor console: drains the engine's ring buffer (Log::DrainEditorMessages)
    // and shows it in a dockable floating window with level filtering, search,
    // autoscroll and clear.
    class ConsolePanel
    {
    public:
        void OnImGuiRender();
        void SetOpen(bool open) { m_Open = open; }

    private:
        void DrainPendingMessages();
        void DrawFilterBar();
        void DrawMessageList();

        bool m_Open = false;
        std::vector<LogMessage> m_Messages;

        bool m_ShowTrace = true;
        bool m_ShowInfo = true;
        bool m_ShowWarn = true;
        bool m_ShowError = true;
        bool m_AutoScroll = true;
        std::string m_Filter;
    };

} // namespace Wheatear
