#include "wtpch.h"
#include "BehaviorVM.h"

#include "BehaviorAssetLoader.h"
#include "BehaviorDatabase.h"
#include "BehaviorPrimitiveRegistry.h"
#include "Wheatear/Gameplay/Action/ActionDebugHistory.h"
#include "Wheatear/Gameplay/Action/ActionRuntimeStore.h"
#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <unordered_map>
#include <utility>

namespace Wheatear::WAO {

    namespace {

        constexpr size_t kMaxInstructionsPerTick = 256;

        struct SceneRuntime
        {
            uint64_t NextHandle = 1;
            // Ordered by handle so multi-instance updates and debug snapshots
            // are deterministic frame to frame.
            std::map<uint64_t, BehaviorInstance> Instances;
            std::map<uint64_t, bool> CompletedResults;
            std::deque<BehaviorEvent> Events;
        };

        static std::unordered_map<Scene*, SceneRuntime>& Runtimes()
        {
            static std::unordered_map<Scene*, SceneRuntime> runtimes;
            return runtimes;
        }

        static BehaviorValue ResolveArgument(const BehaviorInstance& instance,
            const BehaviorArgument& argument)
        {
            if (!argument.IsLocal)
                return argument.Literal;

            const auto it = instance.Locals.find(argument.LocalName);
            return it == instance.Locals.end() ? BehaviorValue{} : it->second;
        }

        static void RecordFailure(BehaviorInstance& instance,
            const std::string& reason)
        {
            instance.Status = BehaviorInstanceStatus::Failed;
            instance.FailureReason = reason;
            instance.Detail = reason;
            instance.Ledger.Record({
                instance.Intent.ActionId,
                EffectType::None,
                instance.Intent.Actor,
                instance.Intent.ExplicitTarget,
                reason,
                0.0f,
                false
            });
            // Failed instances are erased at the end of the tick, so record
            // them immediately or the editor's ledger never sees the failure.
            ActionDebugHistory::Record(instance.Ledger, false, reason);
        }

        static void FinishInstance(BehaviorInstance& instance,
            BehaviorInstanceStatus status,
            const std::string& detail)
        {
            instance.Status = status;
            instance.Detail = detail;
            if (status == BehaviorInstanceStatus::Completed)
                ActionDebugHistory::Record(instance.Ledger, true, detail);
            else
                ActionDebugHistory::Record(instance.Ledger, false, detail);
        }

        static void SetNextOrFinish(BehaviorInstance& instance,
            int nextInstruction)
        {
            if (nextInstruction < 0)
            {
                FinishInstance(instance,
                    BehaviorInstanceStatus::Completed,
                    "Behavior completed");
                return;
            }

            instance.ProgramCounter = static_cast<size_t>(nextInstruction);
            if (!instance.Program ||
                instance.ProgramCounter >= instance.Program->Instructions.size())
            {
                FinishInstance(instance,
                    BehaviorInstanceStatus::Completed,
                    "Behavior completed");
            }
        }

        static bool RunInstance(BehaviorInstance& instance,
            float deltaSeconds,
            size_t instructionLimit = kMaxInstructionsPerTick)
        {
            if (!instance.Program ||
                instance.Status != BehaviorInstanceStatus::Running)
            {
                return false;
            }

            if (deltaSeconds < 0.0f)
                deltaSeconds = 0.0f;

            size_t executed = 0;
            while (instance.Status == BehaviorInstanceStatus::Running &&
                executed++ < instructionLimit)
            {
                if (instance.ProgramCounter >= instance.Program->Instructions.size())
                {
                    FinishInstance(instance,
                        BehaviorInstanceStatus::Completed,
                        "Behavior completed");
                    break;
                }

                const BehaviorInstruction& instruction =
                    instance.Program->Instructions[instance.ProgramCounter];

                if (!instance.SkipBreakpointOnce
                    && !instruction.DebugNodeId.empty()
                    && instance.Breakpoints.count(instruction.DebugNodeId) > 0)
                {
                    instance.Paused = true;
                    instance.PausedNodeId = instruction.DebugNodeId;
                    return true;
                }
                instance.SkipBreakpointOnce = false;

                switch (instruction.Opcode)
                {
                case BehaviorOpcode::CallPrimitive:
                {
                    std::unordered_map<std::string, BehaviorValue> inputs;
                    for (const auto& [name, argument] : instruction.Arguments)
                        inputs[name] = ResolveArgument(instance, argument);

                    std::unordered_map<std::string, BehaviorValue> outputs;
                    BehaviorPrimitiveContext context;
                    context.SceneContext = instance.SceneContext;
                    context.Instance = &instance;
                    context.Recipe = &instance.Recipe;
                    context.Intent = instance.Intent;
                    context.Runtime = instance.RuntimeBinding
                        ? instance.RuntimeBinding
                        : &instance.Runtime;
                    context.Inputs = &inputs;
                    context.Outputs = &outputs;
                    context.AppliedEffects = &instance.AppliedEffects;
                    context.Ledger = &instance.Ledger;
                    context.EmitEvent = [scene = instance.SceneContext](const BehaviorEvent& event)
                    {
                        BehaviorVM::EmitEvent(scene, event);
                    };

                    instance.LastPrimitiveId = instruction.PrimitiveId;
                    if (!BehaviorPrimitiveRegistry::Execute(instruction.PrimitiveId, context))
                    {
                        RecordFailure(instance,
                            context.FailureReason.empty()
                                ? "Behavior primitive failed: " + instruction.PrimitiveId
                                : context.FailureReason);
                        break;
                    }

                    for (const auto& [outputName, localName] : instruction.OutputLocals)
                    {
                        const auto output = outputs.find(outputName);
                        if (output != outputs.end() && !localName.empty())
                            instance.Locals[localName] = output->second;
                    }

                    if (instance.WaitingChildHandle != 0)
                    {
                        instance.Status = BehaviorInstanceStatus::Waiting;
                        instance.WaitRemaining = 0.0f;
                        instance.WaitingEvent.clear();
                        instance.WaitingNextInstruction = instruction.TargetInstruction;
                        instance.WaitingTimeoutInstruction = -1;
                        return true;
                    }

                    SetNextOrFinish(instance, instruction.TargetInstruction);
                    break;
                }
                case BehaviorOpcode::BranchIfFalse:
                {
                    const auto local = instance.Locals.find(instruction.LocalName);
                    const bool value = local != instance.Locals.end()
                        && local->second.IsTruthy();
                    SetNextOrFinish(instance,
                        value ? instruction.TargetInstruction : instruction.AlternateInstruction);
                    break;
                }
                case BehaviorOpcode::Jump:
                    SetNextOrFinish(instance, instruction.TargetInstruction);
                    break;
                case BehaviorOpcode::WaitSeconds:
                    if (instruction.WaitSeconds <= 0.0f)
                    {
                        SetNextOrFinish(instance, instruction.TargetInstruction);
                        break;
                    }

                    instance.Status = BehaviorInstanceStatus::Waiting;
                    instance.WaitRemaining = instruction.WaitSeconds;
                    instance.WaitingEvent.clear();
                    instance.WaitingChildHandle = 0;
                    instance.WaitingNextInstruction = instruction.TargetInstruction;
                    instance.WaitingTimeoutInstruction = -1;
                    return true;
                case BehaviorOpcode::WaitEvent:
                    instance.Status = BehaviorInstanceStatus::Waiting;
                    instance.WaitRemaining = instruction.WaitSeconds;
                    instance.WaitingEvent = instruction.EventId;
                    instance.WaitingChildHandle = 0;
                    instance.WaitingNextInstruction = instruction.TargetInstruction;
                    instance.WaitingTimeoutInstruction = instruction.AlternateInstruction;
                    return true;
                case BehaviorOpcode::SetLocal:
                    instance.Locals[instruction.LocalName] =
                        ResolveArgument(instance, instruction.LocalValue);
                    SetNextOrFinish(instance, instruction.TargetInstruction);
                    break;
                case BehaviorOpcode::End:
                    FinishInstance(instance,
                        BehaviorInstanceStatus::Completed,
                        "Behavior completed");
                    break;
                }
            }

            if (executed >= kMaxInstructionsPerTick &&
                instance.Status == BehaviorInstanceStatus::Running)
            {
                RecordFailure(instance,
                    "Behavior exceeded the per-tick instruction limit.");
            }
            return instance.Status == BehaviorInstanceStatus::Running
                || instance.Status == BehaviorInstanceStatus::Waiting;
        }

        static bool EventMatches(const BehaviorInstance& instance,
            const BehaviorEvent& event)
        {
            if (instance.WaitingEvent.empty() || event.Id != instance.WaitingEvent)
                return false;
            if (event.Target == 0)
                return true;
            return event.Target == instance.OwnerEntity ||
                event.Target == instance.TargetEntity;
        }

        static void DeliverEvent(BehaviorInstance& instance,
            const BehaviorEvent& event)
        {
            for (const auto& [name, value] : event.Payload)
                instance.Locals["event." + name] = value;
            instance.Locals["event.source"] = BehaviorValue(event.Source);
            instance.Locals["event.target"] = BehaviorValue(event.Target);
            instance.WaitingEvent.clear();
            instance.WaitRemaining = 0.0f;
            instance.Status = BehaviorInstanceStatus::Running;
            SetNextOrFinish(instance, instance.WaitingNextInstruction);
        }

        static void UpdateWaitingTimer(BehaviorInstance& instance,
            float deltaSeconds)
        {
            if (instance.Status != BehaviorInstanceStatus::Waiting ||
                !instance.WaitingEvent.empty())
            {
                return;
            }

            instance.WaitRemaining = std::max(
                0.0f,
                instance.WaitRemaining - deltaSeconds);
            if (instance.WaitRemaining <= 0.0f)
            {
                instance.Status = BehaviorInstanceStatus::Running;
                SetNextOrFinish(instance, instance.WaitingNextInstruction);
            }
        }

        static BehaviorStartResult BuildResult(const BehaviorInstance& instance,
            bool completed)
        {
            BehaviorStartResult result;
            result.Started = true;
            result.Completed = completed;
            result.Success = instance.Status == BehaviorInstanceStatus::Completed
                || instance.Status == BehaviorInstanceStatus::Running
                || instance.Status == BehaviorInstanceStatus::Waiting;
            result.Handle = instance.Handle;
            result.BehaviorRevision = instance.BehaviorRevision;
            result.Detail = instance.Detail.empty()
                ? (completed ? "Behavior completed" : "Behavior started")
                : instance.Detail;
            result.AppliedEffects = instance.AppliedEffects;
            result.Ledger = instance.Ledger;
            return result;
        }

        static std::string ResolveBehaviorId(const ActionRecipe& recipe)
        {
            if (!recipe.BehaviorId.empty())
                return recipe.BehaviorId;
            if (!recipe.BehaviorEntry.empty())
                return recipe.BehaviorEntry;
            return {};
        }

        static thread_local int s_StartDepth = 0;
        static thread_local const std::shared_ptr<const BehaviorProgram>* s_ProgramOverride = nullptr;
        static thread_local std::string s_EntryOverride;

        struct StartDepthGuard
        {
            StartDepthGuard() { ++s_StartDepth; }
            ~StartDepthGuard() { --s_StartDepth; }
        };

    } // namespace

    BehaviorStartResult BehaviorVM::StartWithProgram(Scene* scene,
        const ActionIntent& intent,
        const ActionRecipe& recipe,
        const std::shared_ptr<const BehaviorProgram>& program,
        const std::string& entryId,
        ActionRuntime* runtime)
    {
        if (!program)
        {
            BehaviorStartResult result;
            result.Detail = "Behavior program is null.";
            return result;
        }

        struct OverrideGuard
        {
            const std::shared_ptr<const BehaviorProgram>* PreviousProgram = s_ProgramOverride;
            std::string PreviousEntry = s_EntryOverride;
            ~OverrideGuard()
            {
                s_ProgramOverride = PreviousProgram;
                s_EntryOverride = std::move(PreviousEntry);
            }
        } guard;

        s_ProgramOverride = &program;
        s_EntryOverride = entryId;
        return Start(scene, intent, recipe, runtime);
    }

    BehaviorStartResult BehaviorVM::Start(Scene* scene,
        const ActionIntent& intent,
        const ActionRecipe& recipe,
        ActionRuntime* runtime)
    {
        BehaviorStartResult result;
        if (s_StartDepth >= 32)
        {
            result.Detail = "Behavior start recursion limit reached.";
            return result;
        }
        StartDepthGuard depthGuard;
        const std::string behaviorId = ResolveBehaviorId(recipe);
        if (behaviorId.empty())
        {
            result.Detail = "Action has no behavior id.";
            return result;
        }

        if (!s_ProgramOverride
            && !BehaviorDatabase::Has(behaviorId)
            && !recipe.BehaviorFile.empty())
            BehaviorAssetLoader::LoadFile(recipe.BehaviorFile);

        const std::shared_ptr<const BehaviorProgram> program = s_ProgramOverride
            ? *s_ProgramOverride
            : BehaviorDatabase::Find(behaviorId);
        if (!program)
        {
            result.Detail = "Behavior program not found: " + behaviorId;
            return result;
        }

        const std::string entryId = !s_EntryOverride.empty()
            ? s_EntryOverride
            : (recipe.BehaviorEntry.empty() ? behaviorId : recipe.BehaviorEntry);
        // The override applies only to this start; nested wao.start_action
        // calls must resolve through the normal database.
        s_ProgramOverride = nullptr;
        s_EntryOverride.clear();
        const BehaviorEntry* entry = program->FindEntry(entryId);
        if (!entry)
        {
            result.Detail = "Behavior entry not found: " + entryId;
            return result;
        }
        if (recipe.BehaviorVersion > 0 &&
            recipe.BehaviorVersion != entry->Version)
        {
            result.Detail = "Behavior version mismatch for " + entryId +
                ": recipe requires " + std::to_string(recipe.BehaviorVersion) +
                ", loaded " + std::to_string(entry->Version);
            return result;
        }

        BehaviorInstance instance;
        instance.BehaviorId = behaviorId;
        instance.EntryId = entryId;
        instance.BehaviorRevision = program->Revision;
        instance.Program = program;
        instance.SceneContext = scene;
        instance.Intent = intent;
        if (instance.Intent.ActionId.empty())
            instance.Intent.ActionId = recipe.Id;
        instance.Recipe = recipe;
        ActionRuntime* effectiveRuntime = runtime;
        if (!effectiveRuntime && scene && instance.Intent.Actor != 0)
        {
            instance.RuntimeOwner = ActionRuntimeStore::Get(
                scene,
                instance.Intent.Actor);
            effectiveRuntime = instance.RuntimeOwner.get();
        }
        else if (scene && instance.Intent.Actor != 0)
        {
            instance.RuntimeOwner = ActionRuntimeStore::Find(
                scene,
                instance.Intent.Actor);
        }

        instance.Runtime = effectiveRuntime ? *effectiveRuntime : ActionRuntime{};
        instance.RuntimeBinding = effectiveRuntime;
        instance.OwnerEntity = instance.Intent.Actor;
        instance.TargetEntity = instance.Intent.ExplicitTarget;

        if (scene && instance.TargetEntity != 0)
        {
            instance.TargetRuntimeOwner = ActionRuntimeStore::Get(
                scene,
                instance.TargetEntity);
            instance.TargetRuntimeBinding = instance.TargetRuntimeOwner.get();
        }

        instance.ProgramCounter = entry->FirstInstruction;
        instance.Ledger.BeginAction(instance.Intent);

        ActionRuntime* activationRuntime = instance.RuntimeBinding
            ? instance.RuntimeBinding
            : &instance.Runtime;
        ActionActivationResult activation = BeginAction(
            instance.Intent,
            instance.Recipe,
            *activationRuntime,
            instance.TargetRuntimeBinding,
            activationRuntime);
        if (!activation.Success)
        {
            result.Started = false;
            result.Completed = true;
            result.Success = false;
            result.BehaviorRevision = instance.BehaviorRevision;
            result.Detail = activation.Detail.empty()
                ? "Action activation failed"
                : activation.Detail;
            result.AppliedEffects = std::move(activation.AppliedEffects);
            result.Ledger = std::move(activation.Ledger);
            ActionDebugHistory::Record(result.Ledger, false, result.Detail);
            return result;
        }

        for (const EffectSpec& effect : activation.AppliedEffects.Effects)
            instance.AppliedEffects.Add(effect);
        for (const EffectLedgerEntry& entry : activation.Ledger.Entries())
            instance.Ledger.Record(entry);
        instance.ActionActivated = true;

        if (scene)
        {
            SceneRuntime& runtimeState = Runtimes()[scene];
            instance.Handle = runtimeState.NextHandle++;
            if (instance.Handle == 0)
                instance.Handle = runtimeState.NextHandle++;
        }

        RunInstance(instance, 0.0f);

        const bool completed =
            instance.Status == BehaviorInstanceStatus::Completed ||
            instance.Status == BehaviorInstanceStatus::Failed ||
            instance.Status == BehaviorInstanceStatus::Cancelled;

        if (completed)
        {
            if (runtime &&
                instance.RuntimeBinding == nullptr &&
                instance.Status == BehaviorInstanceStatus::Completed)
                *runtime = instance.Runtime;
            return BuildResult(instance, true);
        }

        if (!scene)
        {
            result.Detail = "Suspended behavior requires a Scene runtime.";
            return result;
        }

        const uint64_t handle = instance.Handle;
        Runtimes()[scene].Instances.emplace(handle, std::move(instance));
        const BehaviorInstance& stored = Runtimes()[scene].Instances.at(handle);
        return BuildResult(stored, false);
    }

    void BehaviorVM::Update(Scene* scene, float deltaSeconds)
    {
        if (!scene)
            return;

        auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end())
            return;

        SceneRuntime& runtime = runtimeIt->second;
        const std::vector<BehaviorEvent> events(
            runtime.Events.begin(),
            runtime.Events.end());
        runtime.Events.clear();

        // Instances created synchronously by a primitive (woa.start_action) get
        // their first Update on the next tick, not later in this same loop.
        const uint64_t lastHandleAtStart = runtime.NextHandle > 0
            ? runtime.NextHandle - 1
            : 0;

        for (auto& [handle, instance] : runtime.Instances)
        {
            if (handle > lastHandleAtStart)
                continue;
            const bool stepping = instance.StepRequested;
            if (instance.Paused && !stepping)
                continue;

            if (stepping)
            {
                instance.StepRequested = false;
                instance.Paused = false;
            }

            if (instance.Status == BehaviorInstanceStatus::Waiting &&
                instance.WaitingChildHandle != 0)
            {
                continue;
            }

            if (instance.Status == BehaviorInstanceStatus::Waiting &&
                !instance.WaitingEvent.empty())
            {
                const auto event = std::find_if(events.begin(), events.end(),
                    [&](const BehaviorEvent& candidate)
                    {
                        return EventMatches(instance, candidate);
                    });

                if (event != events.end())
                {
                    DeliverEvent(instance, *event);
                }
                else if (instance.WaitRemaining > 0.0f)
                {
                    instance.WaitRemaining = std::max(
                        0.0f,
                        instance.WaitRemaining - std::max(0.0f, deltaSeconds));
                    if (instance.WaitRemaining <= 0.0f)
                    {
                        instance.WaitingEvent.clear();
                        instance.Status = BehaviorInstanceStatus::Running;
                        if (instance.WaitingTimeoutInstruction >= 0)
                            SetNextOrFinish(instance, instance.WaitingTimeoutInstruction);
                        else
                            FinishInstance(instance,
                                BehaviorInstanceStatus::Completed,
                                "Behavior event timed out");
                    }
                }
            }

            UpdateWaitingTimer(instance, deltaSeconds);
            if (instance.Status == BehaviorInstanceStatus::Running)
                RunInstance(instance, deltaSeconds,
                    stepping ? 1 : kMaxInstructionsPerTick);

            if (stepping
                && instance.Status != BehaviorInstanceStatus::Completed
                && instance.Status != BehaviorInstanceStatus::Cancelled
                && instance.Status != BehaviorInstanceStatus::Failed)
            {
                instance.Paused = true;
                if (instance.PausedNodeId.empty()
                    && instance.Program
                    && instance.ProgramCounter < instance.Program->Instructions.size())
                {
                    instance.PausedNodeId =
                        instance.Program->Instructions[instance.ProgramCounter].DebugNodeId;
                }
            }
        }

        for (auto& [handle, instance] : runtime.Instances)
        {
            if (instance.Status != BehaviorInstanceStatus::Waiting ||
                instance.WaitingChildHandle == 0)
            {
                continue;
            }

            bool childFinished = false;
            bool childSucceeded = false;
            const auto childIt = runtime.Instances.find(instance.WaitingChildHandle);
            if (childIt != runtime.Instances.end())
            {
                const BehaviorInstanceStatus childStatus = childIt->second.Status;
                if (childStatus == BehaviorInstanceStatus::Completed ||
                    childStatus == BehaviorInstanceStatus::Cancelled ||
                    childStatus == BehaviorInstanceStatus::Failed)
                {
                    childFinished = true;
                    childSucceeded = childStatus == BehaviorInstanceStatus::Completed;
                }
            }
            else
            {
                const auto completedIt = runtime.CompletedResults.find(
                    instance.WaitingChildHandle);
                if (completedIt != runtime.CompletedResults.end())
                {
                    childFinished = true;
                    childSucceeded = completedIt->second;
                }
            }

            if (!childFinished)
                continue;

            instance.WaitingChildHandle = 0;
            if (!childSucceeded)
            {
                RecordFailure(instance, "Nested behavior failed.");
                continue;
            }

            instance.Status = BehaviorInstanceStatus::Running;
            SetNextOrFinish(instance, instance.WaitingNextInstruction);
        }

        for (auto it = runtime.Instances.begin(); it != runtime.Instances.end();)
        {
            const BehaviorInstanceStatus status = it->second.Status;
            if (status == BehaviorInstanceStatus::Completed ||
                status == BehaviorInstanceStatus::Cancelled ||
                status == BehaviorInstanceStatus::Failed)
            {
                runtime.CompletedResults[it->first] =
                    status == BehaviorInstanceStatus::Completed;
                it = runtime.Instances.erase(it);
            }
            else
            {
                ++it;
            }
        }
        runtime.CompletedResults.clear();
    }

    void BehaviorVM::EmitEvent(Scene* scene, const BehaviorEvent& event)
    {
        if (!scene || event.Id.empty())
            return;
        Runtimes()[scene].Events.push_back(event);
    }

    void BehaviorVM::Cancel(Scene* scene,
        uint64_t handle,
        const std::string& reason)
    {
        auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end())
            return;

        auto instanceIt = runtimeIt->second.Instances.find(handle);
        if (instanceIt == runtimeIt->second.Instances.end())
            return;

        BehaviorInstance& instance = instanceIt->second;
        FinishInstance(instance,
            BehaviorInstanceStatus::Cancelled,
            reason.empty() ? "Behavior cancelled" : reason);
    }

    void BehaviorVM::SetPaused(Scene* scene, uint64_t handle, bool paused)
    {
        auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end())
            return;

        auto instanceIt = runtimeIt->second.Instances.find(handle);
        if (instanceIt == runtimeIt->second.Instances.end())
            return;

        BehaviorInstance& instance = instanceIt->second;
        instance.Paused = paused;
        instance.StepRequested = false;

        if (paused)
        {
            if (instance.Program
                && instance.ProgramCounter < instance.Program->Instructions.size())
            {
                instance.PausedNodeId =
                    instance.Program->Instructions[instance.ProgramCounter].DebugNodeId;
            }
        }
        else
        {
            instance.SkipBreakpointOnce = true;
            instance.PausedNodeId.clear();
        }
    }

    void BehaviorVM::Step(Scene* scene, uint64_t handle)
    {
        auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end())
            return;

        auto instanceIt = runtimeIt->second.Instances.find(handle);
        if (instanceIt == runtimeIt->second.Instances.end())
            return;

        BehaviorInstance& instance = instanceIt->second;
        instance.Paused = false;
        instance.SkipBreakpointOnce = true;
        instance.StepRequested = true;
        instance.PausedNodeId.clear();
    }

    bool BehaviorVM::JumpToNode(Scene* scene, uint64_t handle,
        const std::string& nodeId)
    {
        auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end() || nodeId.empty())
            return false;

        auto instanceIt = runtimeIt->second.Instances.find(handle);
        if (instanceIt == runtimeIt->second.Instances.end())
            return false;

        BehaviorInstance& instance = instanceIt->second;
        if (!instance.Program)
            return false;

        const BehaviorEntry* entry = instance.Program->FindEntry(instance.EntryId);
        if (!entry)
            return false;

        size_t begin = entry->InstructionBegin;
        size_t end = instance.Program->Instructions.size();
        for (const BehaviorEntry& candidate : instance.Program->Entries)
        {
            if (candidate.InstructionBegin > begin
                && candidate.InstructionBegin < end)
                end = candidate.InstructionBegin;
        }

        for (size_t i = begin; i < end; ++i)
        {
            if (instance.Program->Instructions[i].DebugNodeId != nodeId)
                continue;

            instance.ProgramCounter = i;
            instance.Status = BehaviorInstanceStatus::Running;
            instance.Paused = false;
            instance.StepRequested = false;
            instance.SkipBreakpointOnce = true;
            instance.PausedNodeId.clear();
            instance.WaitRemaining = 0.0f;
            instance.WaitingEvent.clear();
            instance.WaitingChildHandle = 0;
            instance.WaitingNextInstruction = -1;
            instance.WaitingTimeoutInstruction = -1;
            return true;
        }
        return false;
    }

    void BehaviorVM::SetBreakpoint(Scene* scene, uint64_t handle,
        const std::string& nodeId, bool enabled)
    {
        if (nodeId.empty())
            return;

        auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end())
            return;

        auto instanceIt = runtimeIt->second.Instances.find(handle);
        if (instanceIt == runtimeIt->second.Instances.end())
            return;

        BehaviorInstance& instance = instanceIt->second;
        if (enabled)
            instance.Breakpoints.insert(nodeId);
        else
            instance.Breakpoints.erase(nodeId);
    }

    void BehaviorVM::StopScene(Scene* scene)
    {
        auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end())
            return;

        for (auto& [handle, instance] : runtimeIt->second.Instances)
        {
            if (instance.Status != BehaviorInstanceStatus::Completed &&
                instance.Status != BehaviorInstanceStatus::Failed)
            {
                FinishInstance(instance,
                    BehaviorInstanceStatus::Cancelled,
                    "Scene stopped");
            }
        }
        Runtimes().erase(runtimeIt);
    }

    void BehaviorVM::Clear()
    {
        for (auto& [scene, runtime] : Runtimes())
        {
            for (auto& [handle, instance] : runtime.Instances)
            {
                if (instance.Status != BehaviorInstanceStatus::Completed &&
                    instance.Status != BehaviorInstanceStatus::Failed)
                {
                    FinishInstance(instance,
                        BehaviorInstanceStatus::Cancelled,
                        "Behavior runtime cleared");
                }
            }
        }
        Runtimes().clear();
    }

    std::vector<BehaviorDebugSnapshot> BehaviorVM::Active(Scene* scene)
    {
        std::vector<BehaviorDebugSnapshot> snapshots;
        const auto runtimeIt = Runtimes().find(scene);
        if (runtimeIt == Runtimes().end())
            return snapshots;

        snapshots.reserve(runtimeIt->second.Instances.size());
        for (const auto& [handle, instance] : runtimeIt->second.Instances)
        {
            std::string currentNodeId;
            if (instance.Program &&
                instance.ProgramCounter < instance.Program->Instructions.size())
            {
                currentNodeId =
                    instance.Program->Instructions[instance.ProgramCounter].DebugNodeId;
            }
            BehaviorDebugSnapshot snapshot{
                instance.Handle,
                instance.BehaviorId,
                instance.EntryId,
                instance.BehaviorRevision,
                instance.OwnerEntity,
                instance.TargetEntity,
                instance.ProgramCounter,
                currentNodeId,
                instance.Status,
                instance.WaitRemaining,
                instance.WaitingEvent,
                instance.LastPrimitiveId,
                instance.FailureReason,
                instance.Paused,
                instance.PausedNodeId,
                !currentNodeId.empty() && instance.Breakpoints.count(currentNodeId) > 0
            };
            snapshot.Locals = instance.Locals;
            snapshot.AppliedEffects = instance.AppliedEffects.Effects;
            snapshot.Ledger = instance.Ledger.Entries();
            snapshots.push_back(std::move(snapshot));
        }
        std::sort(snapshots.begin(), snapshots.end(),
            [](const BehaviorDebugSnapshot& left, const BehaviorDebugSnapshot& right)
            {
                return left.Handle < right.Handle;
            });
        return snapshots;
    }

} // namespace Wheatear::WAO
