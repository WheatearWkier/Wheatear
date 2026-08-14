#pragma once

#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear {

    // Action-based input layer.
    //
    // Gameplay code asks about semantic actions ("side.jump", "vn.advance")
    // instead of raw key codes; which physical key (or mouse button) triggers
    // an action lives in UserSettings.KeyBindings and can be remapped at
    // runtime (SetKeys / ResetToDefaults, persisted to user_settings).
    //
    // Binding encoding: positive = keyboard key code (WT_KEY_*), negative =
    // mouse button (-1 = left, -2 = right, -3 = middle, ...).
    //
    // Edge queries (IsActionPressed / IsActionReleased) are frame-based:
    // call EndFrame() once per frame from the application loop, then any
    // action queried within the frame reports a consistent "pressed this
    // frame" answer. This removes the hand-rolled previous-frame flags that
    // used to live in each gameplay system.
    class WHEATEAR_API InputBindingService
    {
    public:
        // Level: true while any bound input is held.
        static bool IsActionDown(const std::string& actionId);

        // Edge: true only on the frame the action went from up to down.
        static bool IsActionPressed(const std::string& actionId);

        // Edge: true only on the frame the action went from down to up.
        static bool IsActionReleased(const std::string& actionId);

        // Programmatically fire an action once (used to route gameplay
        // commands such as "side:item:1" through the same edge machinery as
        // physical input, so commands and keys share one input path).
        static void InjectActionPress(const std::string& actionId);

        static const std::vector<int>& GetKeys(const std::string& actionId);
        static void SetKeys(const std::string& actionId, const std::vector<int>& keys);
        static void ResetToDefaults();

        // Advances the edge state machine. Call once per frame.
        static void EndFrame();

        static std::string GetBindingLabel(const std::string& actionId);
        static std::string KeyName(int keyCode);

        // Mouse-button bindings are stored as -(button + 1); these helpers
        // convert between the storage encoding and the button constants.
        static bool IsMouseBinding(int binding) { return binding < 0; }
        static int  MouseBindingToButton(int binding) { return -binding - 1; }
        static int  ButtonToMouseBinding(int mouseButton) { return -(mouseButton + 1); }
    };

} // namespace Wheatear
