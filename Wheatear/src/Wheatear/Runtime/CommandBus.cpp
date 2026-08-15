#include "wtpch.h"
#include "CommandBus.h"

#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/SceneTransitionService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/EntityReference.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIWidgetLayout.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <vector>

namespace Wheatear {

    using Wheatear::StringUtils::PayloadAfter;
    using Wheatear::StringUtils::StartsWith;

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

        static std::vector<std::string>& GameplayCommandQueue()
        {
            static std::vector<std::string> commands;
            return commands;
        }

        static std::vector<std::string>& NativeCommandPrefixes()
        {
            static std::vector<std::string> prefixes;
            return prefixes;
        }

        static std::vector<std::string>& GameplayCommandPrefixes()
        {
            static std::vector<std::string> prefixes;
            return prefixes;
        }

        static bool StartsWithAnyRegisteredPrefix(
            const std::string& command,
            const std::vector<std::string>& prefixes)
        {
            for (const std::string& prefix : prefixes)
            {
                if (!prefix.empty() && StartsWith(command, prefix))
                    return true;
            }
            return false;
        }

        static bool StartsWithBuiltInGameplayPrefix(const std::string& command)
        {
            return StartsWith(command, "vn:")
                || StartsWith(command, "turn:")
                || StartsWith(command, "tactic:");
        }

        static void RegisterCommandPrefix(std::vector<std::string>& prefixes, const std::string& prefix)
        {
            if (prefix.empty())
                return;

            if (std::find(prefixes.begin(), prefixes.end(), prefix) != prefixes.end())
                return;

            prefixes.push_back(prefix);
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

            const std::string& pagerSelector = parts[2];
            const std::string& action = parts[3];
            if (pagerSelector.empty() || action.empty())
                return result;

            UIWidgetLayout::Context layout(scene);
            const UUID pagerID = EntityReferences::ParseUUIDSelector(pagerSelector);
            if (static_cast<uint64_t>(pagerID) == 0)
            {
                result.Handled = true;
                result.Message = "UI pager command requires @UUID target: " + pagerSelector;
                return result;
            }

            const entt::entity pagerEntity = layout.FindByUUID(pagerID);
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

        static CommandResult ExecuteAnimationCommand(Scene* scene, const std::string& command)
        {
            CommandResult result;
            if (!scene || !StartsWith(command, "anim:"))
                return result;

            const std::vector<std::string> parts = SplitCommand(command);
            if (parts.size() < 3 || parts[0] != "anim")
                return result;

            result.Handled = true;
            const std::string& action = parts[1];
            const std::string& targetSelector = parts[2];
            if (!EntityReferences::IsUUIDSelector(targetSelector))
            {
                result.Message = "Animation command requires @UUID target: " + targetSelector;
                return result;
            }

            Entity target = EntityReferences::ResolveSelector(scene, targetSelector);
            if (!target || !target.HasComponent<SpriteAnimatorComponent>())
            {
                result.Message = "Animation target not found: " + targetSelector;
                return result;
            }

            auto& animator = target.GetComponent<SpriteAnimatorComponent>();
            if (action == "play")
            {
                if (parts.size() < 4)
                {
                    result.Message = "Missing animation clip name.";
                    return result;
                }

                const std::string& clipName = parts[3];
                if (!animator.Clips.count(clipName))
                {
                    result.Message = "Animation clip not found: " + clipName;
                    return result;
                }

                animator.Play(clipName);
                result.Success = true;
                result.Changed = true;
                return result;
            }

            if (action == "restart")
            {
                if (!animator.CurrentClipName.empty())
                {
                    animator.ElapsedTime = 0.0f;
                    animator.CurrentFrameIndex = 0;
                    animator.IsPlaying = true;
                    animator.IsFinished = false;
                    result.Success = true;
                    result.Changed = true;
                }
                return result;
            }

            if (action == "pause")
            {
                animator.IsPlaying = false;
                result.Success = true;
                result.Changed = true;
                return result;
            }

            if (action == "resume")
            {
                if (!animator.CurrentClipName.empty())
                {
                    animator.IsPlaying = true;
                    result.Success = true;
                    result.Changed = true;
                }
                return result;
            }

            if (action == "stop")
            {
                animator.ElapsedTime = 0.0f;
                animator.CurrentFrameIndex = 0;
                animator.IsPlaying = false;
                animator.IsFinished = false;
                result.Success = true;
                result.Changed = true;
                return result;
            }

            if (action == "seek" && parts.size() >= 4)
            {
                try
                {
                    animator.ElapsedTime = std::max(0.0f, std::stof(parts[3]));
                    animator.IsFinished = false;
                    result.Success = true;
                    result.Changed = true;
                }
                catch (...)
                {
                    result.Message = "Invalid animation seek time.";
                }
                return result;
            }

            result.Message = "Unknown animation command: " + action;
            return result;
        }

        static CommandResult ExecuteRuntimeQueuedCommand(Scene* scene, const std::string& command)
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
                if (scene && !scene->GetEffectiveSavePolicy().CanLoad)
                {
                    result.Handled = true;
                    result.Message = "当前场景禁止读取。";
                    GameProgress::GetState().LastResultMessage = result.Message;
                    return result;
                }

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
                CommandBus::QueueEventCommand({ UUID(0), parts[1] });
                result.Handled = true;
                result.Success = true;
            }
            else if (parts.size() >= 3)
            {
                const UUID targetID = EntityReferences::ParseUUIDSelector(parts[1]);
                result.Handled = true;
                if (static_cast<uint64_t>(targetID) == 0)
                {
                    result.Message = "Targeted event command requires @UUID target: " + parts[1];
                    return result;
                }

                CommandBus::QueueEventCommand({
                    targetID,
                    parts[2]
                });
                result.Success = true;
            }
            return result;
        }

        static CommandResult ExecuteGameplayCommand(const std::string& command)
        {
            CommandResult result;
            if (!StartsWithAnyRegisteredPrefix(command, GameplayCommandPrefixes())
                && !StartsWithBuiltInGameplayPrefix(command))
                return result;

            CommandBus::QueueGameplayCommand(command);
            result.Handled = true;
            result.Success = true;
            result.Changed = true;
            return result;
        }

    } // namespace


    void CommandBus::RegisterGameplayCommandPrefix(const std::string& prefix)
    {
        RegisterCommandPrefix(GameplayCommandPrefixes(), prefix);
    }


    bool CommandBus::IsNativeCommand(const std::string& command)
    {
        return command == "quit"
            || StartsWith(command, "scene:")
            || StartsWith(command, "newgame:")
            || StartsWith(command, "loadgame:")
            || StartsWith(command, "progression:")
            || StartsWith(command, "anim:")
            || StartsWith(command, "ui:")
            || StartsWith(command, "event:")
            || StartsWithAnyRegisteredPrefix(command, NativeCommandPrefixes())
            || StartsWithAnyRegisteredPrefix(command, GameplayCommandPrefixes())
            || StartsWithBuiltInGameplayPrefix(command);
    }

    CommandResult CommandBus::Execute(Scene* scene, const std::string& command)
    {
        if (command.empty())
            return {};

        if (CommandResult result = ExecuteUIPagerCommand(scene, command); result.Handled)
            return result;

        if (CommandResult result = ExecuteProgressionCommand(command); result.Handled)
            return result;

        if (CommandResult result = ExecuteAnimationCommand(scene, command); result.Handled)
            return result;

        if (CommandResult result = ExecuteEventCommand(scene, command); result.Handled)
            return result;

        if (CommandResult result = ExecuteGameplayCommand(command); result.Handled)
            return result;

        if (CommandResult result = ExecuteRuntimeQueuedCommand(scene, command); result.Handled)
            return result;

        return {};
    }

    void CommandBus::QueueRuntimeCommand(const std::string& command)
    {
        if (!command.empty())
            RuntimeCommandQueue().push_back(command);
    }

    void CommandBus::ClearQueuedCommands()
    {
        RuntimeCommandQueue().clear();
        GameplayCommandQueue().clear();
        EventCommandQueue().clear();
    }

    std::vector<std::string> CommandBus::DrainRuntimeCommands()
    {
        std::vector<std::string> commands;
        commands.swap(RuntimeCommandQueue());
        return commands;
    }

    void CommandBus::QueueGameplayCommand(const std::string& command)
    {
        if (!command.empty())
            GameplayCommandQueue().push_back(command);
    }

    std::vector<std::string> CommandBus::DrainGameplayCommands(const std::string& prefix)
    {
        // Take the whole queue once (O(M) swap) and filter locally instead of
        // copying the unmatched tail back on every prefix drain.
        std::vector<std::string> all = DrainAllGameplayCommands();
        if (prefix.empty())
            return all;

        std::vector<std::string> matched;
        matched.reserve(all.size());
        for (const std::string& command : all)
        {
            if (StartsWith(command, prefix))
                matched.push_back(std::move(command));
        }
        return matched;
    }

    std::vector<std::string> CommandBus::DrainAllGameplayCommands()
    {
        std::vector<std::string> all;
        GameplayCommandQueue().swap(all);
        return all;
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
