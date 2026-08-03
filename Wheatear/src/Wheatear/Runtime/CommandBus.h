#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"

#include <string>
#include <vector>

namespace Wheatear {

    class Scene;

    struct CommandResult
    {
        bool Handled = false;
        bool Success = false;
        bool Changed = false;
        std::string Message;
    };

    struct EventCommandRequest
    {
        UUID TargetEntity = 0;
        std::string EventName;
    };

    class WHEATEAR_API CommandBus
    {
    public:
        static void RegisterNativeCommandPrefix(const std::string& prefix);
        static void RegisterGameplayCommandPrefix(const std::string& prefix);
        static void ClearRegisteredCommandPrefixes();

        static bool IsNativeCommand(const std::string& command);
        static CommandResult Execute(Scene* scene, const std::string& command);
        static void ClearQueuedCommands();

        static void QueueRuntimeCommand(const std::string& command);
        static std::vector<std::string> DrainRuntimeCommands();

        static void QueueGameplayCommand(const std::string& command);
        static std::vector<std::string> DrainGameplayCommands(const std::string& prefix = {});

        static void QueueEventCommand(const EventCommandRequest& request);
        static std::vector<EventCommandRequest> DrainEventCommands(Scene* scene);
    };

} // namespace Wheatear
