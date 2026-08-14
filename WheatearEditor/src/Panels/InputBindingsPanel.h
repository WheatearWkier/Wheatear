#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear {

    // Editor panel for browsing and remapping input actions. Lists every
    // action from UserSettings defaults grouped by prefix; clicking a binding
    // enters "wait for key" mode and the next key/mouse press replaces it
    // (persisted through InputBindingService::SetKeys -> UserSettings::Save).
    class InputBindingsPanel
    {
    public:
        void OnImGuiRender();
        void SetOpen(bool open) { m_Open = open; }

    private:
        void DrawActionRow(const std::string& actionId);
        int  PollPressedInput() const;   // WT_KEY_* or negative mouse binding, 0 = none
        void CommitBinding(const std::string& actionId, int binding);

        bool m_Open = false;
        std::string m_WaitingAction;     // non-empty while waiting for a key
    };

} // namespace Wheatear
