#pragma once

// Native event-script component (.wts). The former C#/Mono ScriptComponent
// was removed from the engine; EventScriptComponent is the active runtime
// scripting path.

#include <string>
#include <vector>

namespace Wheatear {

    struct EventScriptFrame
    {
        std::string EventName;
        size_t InstructionIndex = 0;
        float WaitRemaining = 0.0f;
        double Result = 0.0;
    };

    // Wheatear event script (.wts). This is a lightweight native event sequencer.
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
        // Legacy single-frame mirrors kept in sync with the top of the stack
        // so the inspector and existing code keep working.
        std::string RuntimeEventName;
        size_t RuntimeInstructionIndex = 0;
        float RuntimeWaitRemaining = 0.0f;
        // Value produced by the most recently completed/cancelled child event.
        double RuntimeEventResult = 0.0;
        // Nested event calls run as a stack: a child pauses its parent until it
        // completes, instead of overwriting the parent's position.
        std::vector<EventScriptFrame> RuntimeFrames;

        EventScriptComponent() = default;
        EventScriptComponent(const EventScriptComponent&) = default;
    };

} // namespace Wheatear
