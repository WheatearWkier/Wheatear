#include "wtpch.h"
#include "InputBindingService.h"

#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/KeyCodes.h"
#include "Wheatear/Core/UserSettings.h"

#include <sstream>

namespace Wheatear {

    namespace {

        const std::vector<int>& EmptyKeys()
        {
            static const std::vector<int> keys;
            return keys;
        }

    } // namespace

    bool InputBindingService::IsActionDown(const std::string& actionId)
    {
        for (int key : GetKeys(actionId))
        {
            if (Input::IsKeyPressed(key))
                return true;
        }
        return false;
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
        if (actionId.empty() || keys.empty())
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
        if (keyCode >= WT_KEY_A && keyCode <= WT_KEY_Z)
            return std::string(1, static_cast<char>('A' + keyCode - WT_KEY_A));
        if (keyCode >= WT_KEY_0 && keyCode <= WT_KEY_9)
            return std::string(1, static_cast<char>('0' + keyCode - WT_KEY_0));

        switch (keyCode)
        {
        case WT_KEY_SPACE: return "Space";
        case WT_KEY_ENTER: return "Enter";
        case WT_KEY_ESCAPE: return "Esc";
        case WT_KEY_LEFT: return "Left";
        case WT_KEY_RIGHT: return "Right";
        case WT_KEY_UP: return "Up";
        case WT_KEY_DOWN: return "Down";
        case WT_KEY_F5: return "F5";
        case WT_KEY_F9: return "F9";
        default: return std::to_string(keyCode);
        }
    }

} // namespace Wheatear
