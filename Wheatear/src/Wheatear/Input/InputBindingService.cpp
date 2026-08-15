#include "wtpch.h"
#include "InputBindingService.h"

#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Config/UserSettings.h"

#include <sstream>
#include <unordered_map>

namespace Wheatear {

    namespace {

        const std::vector<int>& EmptyKeys()
        {
            static const std::vector<int> keys;
            return keys;
        }

        // Per-action edge state for the current frame.
        struct ActionEdgeState
        {
            bool Down = false;    // level this frame (queried lazily)
            bool Prev = false;    // level at the previous EndFrame
            bool Queried = false; // queried at least once this frame
            bool InjectedPress = false;
        };

        std::unordered_map<std::string, ActionEdgeState>& EdgeStates()
        {
            static std::unordered_map<std::string, ActionEdgeState> states;
            return states;
        }

        bool IsActionPhysicallyDown(const std::string& actionId)
        {
            for (int binding : InputBindingService::GetKeys(actionId))
            {
                const bool down = binding >= 0
                    ? Input::IsKeyPressed(binding)
                    : Input::IsMouseButtonPressed(InputBindingService::MouseBindingToButton(binding));
                if (down)
                    return true;
            }
            return false;
        }

    } // namespace

    bool InputBindingService::IsActionDown(const std::string& actionId)
    {
        auto& states = EdgeStates();
        auto it = states.find(actionId);
        return (it != states.end() && it->second.InjectedPress)
            || IsActionPhysicallyDown(actionId);
    }

    bool InputBindingService::IsActionPressed(const std::string& actionId)
    {
        ActionEdgeState& state = EdgeStates()[actionId];
        if (!state.Queried)
        {
            state.Down = IsActionDown(actionId);
            state.Queried = true;
        }
        return state.Down && !state.Prev;
    }

    bool InputBindingService::IsActionReleased(const std::string& actionId)
    {
        ActionEdgeState& state = EdgeStates()[actionId];
        if (!state.Queried)
        {
            state.Down = IsActionDown(actionId);
            state.Queried = true;
        }
        return !state.Down && state.Prev;
    }

    void InputBindingService::InjectActionPress(const std::string& actionId)
    {
        ActionEdgeState& state = EdgeStates()[actionId];
        state.Down = true;
        state.Queried = true;
        state.InjectedPress = true;
    }

    void InputBindingService::EndFrame()
    {
        for (auto& [actionId, state] : EdgeStates())
        {
            state.Prev = IsActionPhysicallyDown(actionId);
            state.Down = state.Prev;
            state.Queried = false;
            state.InjectedPress = false;
        }
    }

    const std::vector<int>& InputBindingService::GetKeys(const std::string& actionId)
    {
        auto& bindings = UserSettings::Get().KeyBindings;
        if (auto it = bindings.find(actionId); it != bindings.end())
            return it->second;

        const UserSettingsData defaults = UserSettings::Defaults();
        if (auto it = defaults.KeyBindings.find(actionId); it != defaults.KeyBindings.end())
        {
            bindings[actionId] = it->second;
            return bindings[actionId];
        }

        return EmptyKeys();
    }

    void InputBindingService::SetKeys(const std::string& actionId, const std::vector<int>& keys)
    {
        if (actionId.empty())
            return;

        UserSettings::Get().KeyBindings[actionId] = keys;
        UserSettings::Save();
    }

    void InputBindingService::ResetToDefaults()
    {
        UserSettings::Get().KeyBindings = UserSettings::Defaults().KeyBindings;
        UserSettings::Save();
    }

    std::string InputBindingService::GetBindingLabel(const std::string& actionId)
    {
        const std::vector<int>& keys = GetKeys(actionId);
        if (keys.empty())
            return "-";

        std::ostringstream stream;
        for (size_t i = 0; i < keys.size(); ++i)
        {
            if (i > 0)
                stream << " / ";
            stream << KeyName(keys[i]);
        }
        return stream.str();
    }

    std::string InputBindingService::KeyName(int keyCode)
    {
        if (keyCode < 0)
        {
            switch (MouseBindingToButton(keyCode))
            {
            case WT_MOUSE_BUTTON_LEFT:   return "Mouse Left";
            case WT_MOUSE_BUTTON_RIGHT:  return "Mouse Right";
            case WT_MOUSE_BUTTON_MIDDLE: return "Mouse Middle";
            default: return "Mouse " + std::to_string(MouseBindingToButton(keyCode) + 1);
            }
        }

        if (keyCode >= WT_KEY_A && keyCode <= WT_KEY_Z)
            return std::string(1, static_cast<char>('A' + keyCode - WT_KEY_A));
        if (keyCode >= WT_KEY_0 && keyCode <= WT_KEY_9)
            return std::string(1, static_cast<char>('0' + keyCode - WT_KEY_0));
        if (keyCode >= WT_KEY_F1 && keyCode <= WT_KEY_F12)
            return "F" + std::to_string(keyCode - WT_KEY_F1 + 1);

        switch (keyCode)
        {
        case WT_KEY_SPACE: return "Space";
        case WT_KEY_ENTER: return "Enter";
        case WT_KEY_ESCAPE: return "Esc";
        case WT_KEY_TAB: return "Tab";
        case WT_KEY_BACKSPACE: return "Backspace";
        case WT_KEY_DELETE: return "Delete";
        case WT_KEY_LEFT: return "Left";
        case WT_KEY_RIGHT: return "Right";
        case WT_KEY_UP: return "Up";
        case WT_KEY_DOWN: return "Down";
        case WT_KEY_HOME: return "Home";
        case WT_KEY_END: return "End";
        case WT_KEY_LEFT_SHIFT: return "Shift";
        case WT_KEY_LEFT_CONTROL: return "Ctrl";
        case WT_KEY_LEFT_ALT: return "Alt";
        default: return std::to_string(keyCode);
        }
    }

} // namespace Wheatear
