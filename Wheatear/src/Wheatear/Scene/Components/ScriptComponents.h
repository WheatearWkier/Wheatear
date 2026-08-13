#pragma once

// Native, C#/Mono, and event-script components.

#include <string>

namespace Wheatear {

    class ScriptableEntity;

    struct NativeScriptComponent
    {
        ScriptableEntity* Instance = nullptr;

        ScriptableEntity* (*InstantiateScript)() = nullptr;
        void              (*DestroyScript)(NativeScriptComponent*) = nullptr;

        template<typename T>
        void Bind()
        {
            InstantiateScript = []() -> ScriptableEntity*
                {
                    return static_cast<ScriptableEntity*>(new T());
                };
            DestroyScript = [](NativeScriptComponent* nsc)
                {
                    delete nsc->Instance;
                    nsc->Instance = nullptr;
                };
        }
    };

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
