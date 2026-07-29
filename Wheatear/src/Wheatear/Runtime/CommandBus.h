#pragma once

#include "Wheatear/Core/Core.h"

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
        std::string TargetTag;
        std::string EventName;
    };

    class WHEATEAR_API CommandBus
    {
    public:
        static bool IsNativeCommand(const std::string& command);
        static CommandResult Execute(Scene* scene, const std::string& command);

        static void QueueRuntimeCommand(const std::string& command);
        static std::vector<std::string> DrainRuntimeCommands();

        static void QueueEventCommand(const EventCommandRequest& request);
        static std::vector<EventCommandRequest> DrainEventCommands(Scene* scene);
    };

} // namespace Wheatear
