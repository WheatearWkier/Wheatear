#pragma once

// Legacy C#/Mono and native event-script components. C#/Mono is hidden in the
// default build; EventScriptComponent remains the active runtime scripting path.

#include <string>

namespace Wheatear {

    struct ScriptComponent
    {
        std::string ClassName;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent&) = default;
    };

    // Wheatear event script (.wts). This is a lightweight native event sequencer,
    // separate from optional C#/Mono scripting.
    struct EventScriptComponent
    {
        std::string ScriptPath = "";
        std::string StartEvent = "on_start";
        bool RunOnStart = true;
        bool RunOnce = true;
        bool Enabled = true;

        bool RuntimeActive = false;
        bool RuntimeCompleted = false;
        bool RuntimeStarted = false;
        std::string RuntimeEventName;
        size_t RuntimeInstructionIndex = 0;
        float RuntimeWaitRemaining = 0.0f;

        EventScriptComponent() = default;
        EventScriptComponent(const EventScriptComponent&) = default;
    };

} // namespace Wheatear
