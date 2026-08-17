#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear {

    // Editor panel for browsing and remapping input actions. Lists every
    // action from the data-driven catalog (assets/input/action_bindings.yaml)
    // plus built-in C++ defaults, grouped by prefix; clicking a binding
    // enters "wait for key" mode and the next key/mouse press replaces it
    // (persisted through InputBindingService::SetKeys -> UserSettings::Save).
    // New actions are added to / removed from the project's catalog file.
    class InputBindingsPanel
    {
    public:
        void OnImGuiRender();
        void SetOpen(bool open) { m_Open = open; }

    private:
        void DrawActionRow(const std::string& actionId);
        void DrawAddActionBar();
        int  PollPressedInput() const;   // WT_KEY_* or negative mouse binding, 0 = none
        void CommitBinding(const std::string& actionId, int binding);

        bool m_Open = false;
        std::string m_WaitingAction;     // non-empty while waiting for keys
        std::string m_NewActionId;

        // Label editing modal.
        std::string m_LabelEditAction;
        char m_LabelEditBuffer[256] = {};
        bool m_ShowLabelEdit = false;
    };

} // namespace Wheatear
