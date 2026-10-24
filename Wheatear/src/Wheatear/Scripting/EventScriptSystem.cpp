#include "wtpch.h"
#include "EventScriptSystem.h"

#include "EventScript.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Logic/LogicDiagnostics.h"
#include "Wheatear/Logic/LogicExpression.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace Wheatear {

    namespace {

        struct CachedEventScript
        {
            EventScript Script;
            std::filesystem::file_time_type LastWriteTime{};
            bool Loaded = false;
        };

        static std::unordered_map<std::string, CachedEventScript>& ScriptCache()
        {
            static std::unordered_map<std::string, CachedEventScript> cache;
            return cache;
        }

        static void SyncRuntimeMirror(EventScriptComponent& component)
        {
            if (component.RuntimeFrames.empty())
            {
                component.RuntimeActive = false;
                component.RuntimeEventName.clear();
                component.RuntimeInstructionIndex = 0;
                component.RuntimeWaitRemaining = 0.0f;
                return;
            }

            const EventScriptFrame& top = component.RuntimeFrames.back();
            component.RuntimeActive = true;
            component.RuntimeEventName = top.EventName;
            component.RuntimeInstructionIndex = top.InstructionIndex;
            component.RuntimeWaitRemaining = top.WaitRemaining;
        }

        static void ClearRuntime(EventScriptComponent& component)
        {
            component.RuntimeFrames.clear();
            component.RuntimeActive = false;
            component.RuntimeCompleted = false;
            component.RuntimeStarted = false;
            component.RuntimeEventName.clear();
            component.RuntimeInstructionIndex = 0;
            component.RuntimeWaitRemaining = 0.0f;
            component.RuntimeEventResult = 0.0;
        }

        static bool EvaluateEventNumber(const std::string& expression,
            const EventScriptComponent& component, double* value)
        {
            Logic::VariableScope scope;
            scope.SetLocal("event_result", component.RuntimeEventResult);
            scope.SetLocal("event_depth",
                static_cast<double>(component.RuntimeFrames.size()));
            std::string error;
            return Logic::Evaluate(expression, scope.MakeContext(), value, &error);
        }

        static bool EvaluateCondition(const std::string& expression,
            const EventScriptComponent& component)
        {
            const std::string normalized = Logic::NormalizeCondition(expression);
            Logic::VariableScope scope;
            scope.SetLocal("event_result", component.RuntimeEventResult);
            scope.SetLocal("event_depth",
                static_cast<double>(component.RuntimeFrames.size()));
            const Logic::EvalContext context = scope.MakeContext();

            bool value = false;
            std::string error;
            if (!Logic::EvaluateBool(normalized, context, &value, &error))
            {
                WT_CORE_WARN("EventScript: invalid condition '{}' ({})",
                    expression, error);
                Logic::Diagnostics::Warn("event-script",
                    "Invalid condition '" + expression + "': " + error);
                return false;
            }
            return value;
        }

        static void ValidateScript(const EventScript& script,
            const std::string& scriptPath)
        {
            for (const EventScriptIssue& issue : script.Validate())
            {
                const std::string location =
                    scriptPath + ":" + std::to_string(issue.SourceLine);
                if (issue.Error)
                {
                    WT_CORE_ERROR("EventScript {}: {}", location, issue.Message);
                    Logic::Diagnostics::Error("event-script",
                        location + ": " + issue.Message);
                }
                else
                {
                    WT_CORE_WARN("EventScript {}: {}", location, issue.Message);
                    Logic::Diagnostics::Warn("event-script",
                        location + ": " + issue.Message);
                }
            }
        }

        static const EventScript* LoadScript(const std::string& scriptPath)
        {
            if (scriptPath.empty())
                return nullptr;

            const std::filesystem::path resolved =
                AssetPath::ResolveRuntimeData(scriptPath);
            std::error_code error;
            const auto writeTime = std::filesystem::exists(resolved, error)
                ? std::filesystem::last_write_time(resolved, error)
                : std::filesystem::file_time_type{};

            auto& cache = ScriptCache()[scriptPath];
            if (!cache.Loaded || cache.LastWriteTime != writeTime)
            {
                cache.Script = EventScript::FromFile(resolved);
                cache.LastWriteTime = writeTime;
                cache.Loaded = true;
                ValidateScript(cache.Script, scriptPath);
                WT_CORE_INFO("EventScriptSystem: reloaded '{}' ({} events)",
                    scriptPath, cache.Script.GetEvents().size());
            }

            return &cache.Script;
        }

        static size_t FindMatchingEndIf(const std::vector<EventScriptInstruction>& instructions, size_t ifIndex)
        {
            int depth = 0;
            for (size_t i = ifIndex + 1; i < instructions.size(); ++i)
            {
                if (instructions[i].Type == EventScriptInstructionType::If)
                {
                    ++depth;
                }
                else if (instructions[i].Type == EventScriptInstructionType::EndIf)
                {
                    if (depth == 0)
                        return i + 1;
                    --depth;
                }
            }
            return instructions.size();
        }

        static bool MatchesEventTarget(entt::registry& registry,
            entt::entity entity,
            const EventCommandRequest& request)
        {
            if (static_cast<uint64_t>(request.TargetEntity) != 0)
            {
                return registry.all_of<IDComponent>(entity)
                    && registry.get<IDComponent>(entity).ID == request.TargetEntity;
            }

            return true;
        }

    } // namespace

    void EventScriptSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<EventScriptComponent>())
        {
            auto& script = registry.get<EventScriptComponent>(entity);
            ClearRuntime(script);

            if (script.Enabled && script.RunOnStart)
                StartEvent(scene, entity, script.StartEvent);
        }
    }

    void EventScriptSystem::OnRuntimeStop(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<EventScriptComponent>())
        {
            auto& script = registry.get<EventScriptComponent>(entity);
            ClearRuntime(script);
        }
    }

    void EventScriptSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        std::vector<entt::entity> startedThisFrame;
        for (const EventCommandRequest& request : CommandBus::DrainEventCommands(scene))
        {
            for (auto entity : registry.view<EventScriptComponent>())
            {
                if (!MatchesEventTarget(registry, entity, request))
                    continue;

                if (std::find(startedThisFrame.begin(), startedThisFrame.end(), entity)
                    != startedThisFrame.end())
                {
                    // Keep sibling requests in author order: run at most one
                    // external event per component per frame and queue the rest.
                    CommandBus::QueueEventCommand({ request.TargetEntity, request.EventName });
                    continue;
                }

                if (StartEvent(scene, entity, request.EventName,
                    request.TargetEntity != 0))
                {
                    startedThisFrame.push_back(entity);
                }
            }
        }

        const float deltaSeconds = ts.GetSeconds();
        for (auto entity : registry.view<EventScriptComponent>())
            UpdateScript(scene, entity, deltaSeconds);
    }

    bool EventScriptSystem::StartEvent(Scene* scene, entt::entity entity,
        const std::string& eventName, bool reportMissing)
    {
        if (!scene || eventName.empty())
            return false;

        auto& registry = scene->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<EventScriptComponent>(entity))
            return false;

        auto& component = registry.get<EventScriptComponent>(entity);
        if (!component.Enabled)
            return false;

        // RunOnce only guards the automatic start event. Explicit UI/gameplay
        // dispatches must still be able to trigger later events on the entity.
        if (component.RunOnce && component.RuntimeCompleted
            && eventName == component.StartEvent)
            return false;

        const EventScript* script = LoadScript(component.ScriptPath);
        const EventScriptBlock* block = script ? script->FindEvent(eventName) : nullptr;
        if (!block)
        {
            if (reportMissing)
            {
                WT_CORE_WARN("EventScriptSystem: event '{}' not found in '{}'",
                    eventName, component.ScriptPath);
                Logic::Diagnostics::Warn("event-script",
                    component.ScriptPath + ": event '" + eventName + "' not found");
            }
            return false;
        }

        for (const EventScriptFrame& frame : component.RuntimeFrames)
        {
            if (frame.EventName == eventName)
            {
                WT_CORE_WARN("EventScriptSystem: recursive event '{}' in '{}' ignored.",
                    eventName, component.ScriptPath);
                Logic::Diagnostics::Warn("event-script",
                    component.ScriptPath + ": recursive event '" + eventName + "' ignored");
                return false;
            }
        }

        constexpr size_t kMaxEventDepth = 8;
        if (component.RuntimeFrames.size() >= kMaxEventDepth)
        {
            WT_CORE_WARN("EventScriptSystem: event stack limit ({}) reached in '{}'.",
                kMaxEventDepth, component.ScriptPath);
            return false;
        }

        component.RuntimeStarted = true;
        component.RuntimeCompleted = false;
        component.RuntimeFrames.push_back({ eventName, 0, 0.0f });
        SyncRuntimeMirror(component);
        return true;
    }

    void EventScriptSystem::UpdateScript(Scene* scene, entt::entity entity, float deltaSeconds)
    {
        auto& registry = scene->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<EventScriptComponent>(entity))
            return;

        auto& component = registry.get<EventScriptComponent>(entity);
        if (!component.Enabled || component.RuntimeFrames.empty())
            return;

        SyncRuntimeMirror(component);
        EventScriptFrame& frame = component.RuntimeFrames.back();
        if (frame.WaitRemaining > 0.0f)
        {
            frame.WaitRemaining = std::max(0.0f, frame.WaitRemaining - deltaSeconds);
            SyncRuntimeMirror(component);
            if (frame.WaitRemaining > 0.0f)
                return;
        }

        const EventScript* script = LoadScript(component.ScriptPath);
        const EventScriptBlock* block = script ? script->FindEvent(frame.EventName) : nullptr;
        if (!block)
        {
            WT_CORE_WARN("EventScriptSystem: running event '{}' no longer exists in '{}' after reload; stopped.", frame.EventName, component.ScriptPath);
            component.RuntimeFrames.pop_back();
            SyncRuntimeMirror(component);
            if (component.RuntimeFrames.empty())
                component.RuntimeCompleted = true;
            return;
        }

        const auto& instructions = block->Instructions;
        while (frame.InstructionIndex < instructions.size())
        {
            const EventScriptInstruction& instruction = instructions[frame.InstructionIndex];
            switch (instruction.Type)
            {
            case EventScriptInstructionType::Command:
            {
                const std::string& commandText = instruction.Text;
                if (commandText.rfind("event:return:", 0) == 0)
                {
                    double value = 0.0;
                    if (!EvaluateEventNumber(commandText.substr(13), component, &value))
                        value = 0.0;
                    frame.Result = value;
                    component.RuntimeEventResult = value;
                    component.RuntimeFrames.pop_back();
                    SyncRuntimeMirror(component);
                    if (component.RuntimeFrames.empty())
                        component.RuntimeCompleted = true;
                    return;
                }

                if (commandText == "event:stop" || commandText.rfind("event:stop:", 0) == 0)
                {
                    const std::string target = commandText == "event:stop"
                        ? std::string()
                        : commandText.substr(11);
                    size_t stopIndex = component.RuntimeFrames.size() - 1;
                    if (!target.empty())
                    {
                        const auto found = std::find_if(
                            component.RuntimeFrames.rbegin(),
                            component.RuntimeFrames.rend(),
                            [&target](const EventScriptFrame& candidate)
                            {
                                return candidate.EventName == target;
                            });
                        if (found == component.RuntimeFrames.rend())
                        {
                            ++frame.InstructionIndex;
                            SyncRuntimeMirror(component);
                            break;
                        }
                        stopIndex = static_cast<size_t>(
                            std::distance(component.RuntimeFrames.begin(),
                                found.base()) - 1);
                    }

                    component.RuntimeEventResult =
                        component.RuntimeFrames[stopIndex].Result;
                    component.RuntimeFrames.erase(
                        component.RuntimeFrames.begin()
                            + static_cast<std::ptrdiff_t>(stopIndex),
                        component.RuntimeFrames.end());
                    SyncRuntimeMirror(component);
                    if (component.RuntimeFrames.empty())
                        component.RuntimeCompleted = true;
                    return;
                }

                if (commandText.rfind("event:call:", 0) == 0)
                {
                    const std::string child = Wheatear::StringUtils::Trim(
                        commandText.substr(11));
                    if (!child.empty() && registry.all_of<IDComponent>(entity))
                    {
                        CommandBus::QueueEventCommand(
                            { registry.get<IDComponent>(entity).ID, child });
                    }
                    ++frame.InstructionIndex;
                    SyncRuntimeMirror(component);
                    return;
                }

                const CommandResult result = CommandBus::Execute(scene, instruction.Text);
                if (!result.Handled || !result.Success)
                {
                    const std::string reason = !result.Message.empty()
                        ? result.Message
                        : (result.Handled ? "command failed" : "unhandled command");
                    WT_CORE_WARN("EventScriptSystem '{}:{}' (event '{}'): '{}' - {}",
                        component.ScriptPath, instruction.SourceLine,
                        frame.EventName, instruction.Text, reason);
                    Logic::Diagnostics::Warn("event-script",
                        component.ScriptPath + ":" + std::to_string(instruction.SourceLine)
                            + " (event '" + frame.EventName + "'): '"
                            + instruction.Text + "' - " + reason);
                }

                ++frame.InstructionIndex;
                SyncRuntimeMirror(component);
                // A scene transition owns the rest of the frame; running more
                // instructions from the old scene would target stale entities.
                if (result.Success
                    && (instruction.Text.rfind("scene:", 0) == 0
                        || instruction.Text.rfind("newgame:", 0) == 0
                        || instruction.Text.rfind("loadgame:", 0) == 0))
                {
                    component.RuntimeFrames.clear();
                    SyncRuntimeMirror(component);
                    component.RuntimeCompleted = true;
                    return;
                }
                // Event calls are queued and pushed onto this component's
                // stack next frame; suspend the parent until the child ends.
                if (result.Success && instruction.Text.rfind("event:", 0) == 0)
                {
                    SyncRuntimeMirror(component);
                    return;
                }
                break;
            }
            case EventScriptInstructionType::Wait:
                ++frame.InstructionIndex;
                frame.WaitRemaining = std::clamp(instruction.Seconds, 0.0f, 3600.0f);
                SyncRuntimeMirror(component);
                if (frame.WaitRemaining > 0.0f)
                    return;
                break;
            case EventScriptInstructionType::If:
                if (EvaluateCondition(instruction.Text, component))
                    ++frame.InstructionIndex;
                else
                    frame.InstructionIndex = FindMatchingEndIf(instructions, frame.InstructionIndex);
                SyncRuntimeMirror(component);
                break;
            case EventScriptInstructionType::EndIf:
                ++frame.InstructionIndex;
                SyncRuntimeMirror(component);
                break;
            case EventScriptInstructionType::RawLine:
                // Preserved uninterpretable line: skip without executing.
                ++frame.InstructionIndex;
                SyncRuntimeMirror(component);
                break;
            }
        }

        // The top frame finished. Resume the parent (if any) on the next tick.
        component.RuntimeFrames.pop_back();
        SyncRuntimeMirror(component);
        if (component.RuntimeFrames.empty())
            component.RuntimeCompleted = true;
    }

} // namespace Wheatear
