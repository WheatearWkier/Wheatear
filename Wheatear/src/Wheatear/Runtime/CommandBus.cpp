#include "wtpch.h"
#include "CommandBus.h"

#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/SceneTransitionService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIWidgetLayout.h"

#include <algorithm>
#include <vector>

namespace Wheatear {

    namespace {

        static std::vector<std::string>& RuntimeCommandQueue()
        {
            static std::vector<std::string> commands;
            return commands;
        }

        static std::vector<EventCommandRequest>& EventCommandQueue()
        {
            static std::vector<EventCommandRequest> commands;
            return commands;
        }

        static bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        static std::string PayloadAfter(const std::string& value, const std::string& prefix)
        {
            return StartsWith(value, prefix) ? value.substr(prefix.size()) : std::string{};
        }

        static bool TryParsePositiveInt(const std::string& value, int& result)
        {
            if (value.empty())
                return false;

            try
            {
                size_t parsed = 0;
                const int parsedValue = std::stoi(value, &parsed);
                if (parsed != value.size() || parsedValue < 1)
                    return false;

                result = parsedValue;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        static std::vector<std::string> SplitCommand(const std::string& command)
        {
            std::vector<std::string> parts;
            size_t start = 0;
            while (start <= command.size())
            {
                const size_t separator = command.find(':', start);
                if (separator == std::string::npos)
                {
                    parts.push_back(command.substr(start));
                    break;
                }

                parts.push_back(command.substr(start, separator - start));
                start = separator + 1;
            }
            return parts;
        }

        static CommandResult ExecuteUIPagerCommand(Scene* scene, const std::string& command)
        {
            CommandResult result;
            if (!scene || !StartsWith(command, "ui:pager:"))
                return result;

            const std::vector<std::string> parts = SplitCommand(command);
            if (parts.size() < 4 || parts[0] != "ui" || parts[1] != "pager")
                return result;

            const std::string& pagerTag = parts[2];
            const std::string& action = parts[3];
            if (pagerTag.empty() || action.empty())
                return result;

            UIWidgetLayout::Context layout(scene);
            const entt::entity pagerEntity = layout.FindByTag(pagerTag);
            auto& registry = scene->GetRegistry();
            if (pagerEntity == entt::null
                || !registry.valid(pagerEntity)
                || !registry.all_of<UIPagerComponent>(pagerEntity))
            {
                return result;
            }

            auto& pager = registry.get<UIPagerComponent>(pagerEntity);
            pager.PageCount = std::max(pager.PageCount, 1);
            pager.CurrentPage = std::clamp(pager.CurrentPage, 1, pager.PageCount);

            int nextPage = pager.CurrentPage;
            if (action == "next")
            {
                nextPage = pager.CurrentPage + 1;
                if (nextPage > pager.PageCount)
                    nextPage = pager.Wrap ? 1 : pager.PageCount;
            }
            else if (action == "prev" || action == "previous")
            {
                nextPage = pager.CurrentPage - 1;
                if (nextPage < 1)
                    nextPage = pager.Wrap ? pager.PageCount : 1;
            }
            else if (action == "first")
            {
                nextPage = 1;
            }
            else if (action == "last")
            {
                nextPage = pager.PageCount;
            }
            else if ((action == "page" || action == "set") && parts.size() >= 5)
            {
                try
                {
                    nextPage = std::stoi(parts[4]);
                }
                catch (...)
                {
                    return result;
                }
            }
            else
            {
                return result;
            }

            pager.CurrentPage = std::clamp(nextPage, 1, pager.PageCount);
            result.Handled = true;
            result.Success = true;
            result.Changed = true;
            return result;
        }

        static CommandResult ExecuteProgressionCommand(const std::string& command)
        {
            CommandResult result;
            if (!StartsWith(command, "progression:"))
                return result;

            const auto progression = GameProgress::ExecuteCommand(command);
            result.Handled = progression.Handled;
            result.Success = progression.Success;
            result.Changed = progression.Changed;
            result.Message = progression.Message;
            return result;
        }

        static CommandResult ExecuteRuntimeQueuedCommand(const std::string& command)
        {
            CommandResult result;
            if (command == "quit")
            {
                CommandBus::QueueRuntimeCommand(command);
                result.Handled = true;
                result.Success = true;
            }
            else if (StartsWith(command, "scene:"))
            {
                SceneTransitionService::RequestLoadScene(PayloadAfter(command, "scene:"), command);
                result.Handled = true;
                result.Success = true;
                result.Changed = true;
            }
            else if (StartsWith(command, "newgame:"))
            {
                SceneTransitionService::RequestNewGame(PayloadAfter(command, "newgame:"), command);
                result.Handled = true;
                result.Success = true;
                result.Changed = true;
            }
            else if (StartsWith(command, "loadgame:"))
            {
                std::string payload = PayloadAfter(command, "loadgame:");
                int slot = 1;

                const size_t separator = payload.rfind(':');
                if (separator != std::string::npos)
                {
                    int parsedSlot = 1;
                    if (TryParsePositiveInt(payload.substr(separator + 1), parsedSlot))
                    {
                        slot = parsedSlot;
                        payload = payload.substr(0, separator);
                    }
                }

                SceneTransitionService::RequestLoadGame(payload, slot, command);
                result.Handled = true;
                result.Success = true;
                result.Changed = true;
            }
            return result;
        }

        static CommandResult ExecuteEventCommand(Scene*, const std::string& command)
        {
            CommandResult result;
            if (!StartsWith(command, "event:"))
                return result;

            const std::vector<std::string> parts = SplitCommand(command);
            if (parts.size() == 2)
            {
                CommandBus::QueueEventCommand({ "", parts[1] });
                result.Handled = true;
                result.Success = true;
            }
            else if (parts.size() >= 3)
            {
                CommandBus::QueueEventCommand({ parts[1], parts[2] });
                result.Handled = true;
                result.Success = true;
            }
            return result;
        }

    } // namespace

    bool CommandBus::IsNativeCommand(const std::string& command)
    {
        return command == "quit"
            || StartsWith(command, "scene:")
            || StartsWith(command, "newgame:")
            || StartsWith(command, "loadgame:")
            || StartsWith(command, "progression:")
            || StartsWith(command, "ui:")
            || StartsWith(command, "event:")
            || StartsWith(command, "vn:");
    }

    CommandResult CommandBus::Execute(Scene* scene, const std::string& command)
    {
        if (command.empty())
            return {};

        if (CommandResult result = ExecuteUIPagerCommand(scene, command); result.Handled)
            return result;

        if (CommandResult result = ExecuteProgressionCommand(command); result.Handled)
            return result;

        if (CommandResult result = ExecuteEventCommand(scene, command); result.Handled)
            return result;

        if (CommandResult result = ExecuteRuntimeQueuedCommand(command); result.Handled)
            return result;

        return {};
    }

    void CommandBus::QueueRuntimeCommand(const std::string& command)
    {
        if (!command.empty())
            RuntimeCommandQueue().push_back(command);
    }

    std::vector<std::string> CommandBus::DrainRuntimeCommands()
    {
        std::vector<std::string> commands;
        commands.swap(RuntimeCommandQueue());
        return commands;
    }

    void CommandBus::QueueEventCommand(const EventCommandRequest& request)
    {
        if (!request.EventName.empty())
            EventCommandQueue().push_back(request);
    }

    std::vector<EventCommandRequest> CommandBus::DrainEventCommands(Scene*)
    {
        std::vector<EventCommandRequest> commands;
        commands.swap(EventCommandQueue());
        return commands;
    }

} // namespace Wheatear
