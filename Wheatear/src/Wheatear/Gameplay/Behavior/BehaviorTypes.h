#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace Wheatear {

    class Scene;

    namespace WAO {

        enum class BehaviorValueType
        {
            None = 0,
            Bool,
            Number,
            String,
            UUID
        };

        struct BehaviorValue
        {
            using Data = std::variant<std::monostate, bool, double, std::string, UUID>;

            Data Value;

            BehaviorValue() = default;
            BehaviorValue(bool value) : Value(value) {}
            BehaviorValue(int value) : Value(static_cast<double>(value)) {}
            BehaviorValue(float value) : Value(static_cast<double>(value)) {}
            BehaviorValue(double value) : Value(value) {}
            BehaviorValue(const char* value) : Value(std::string(value ? value : "")) {}
            BehaviorValue(std::string value) : Value(std::move(value)) {}
            BehaviorValue(UUID value) : Value(value) {}

            BehaviorValueType Type() const;
            bool IsTruthy(bool fallback = false) const;
            double AsNumber(double fallback = 0.0) const;
            bool AsBool(bool fallback = false) const;
            std::string AsString(const std::string& fallback = {}) const;
            UUID AsUUID(UUID fallback = 0) const;
        };

        struct BehaviorArgument
        {
            bool IsLocal = false;
            std::string LocalName;
            BehaviorValue Literal;
        };

        enum class BehaviorOpcode
        {
            CallPrimitive = 0,
            BranchIfFalse,
            Jump,
            WaitSeconds,
            WaitEvent,
            SetLocal,
            End
        };

        struct BehaviorInstruction
        {
            BehaviorOpcode Opcode = BehaviorOpcode::End;
            std::string PrimitiveId;
            uint32_t PrimitiveVersion = 0;
            std::unordered_map<std::string, BehaviorArgument> Arguments;
            std::unordered_map<std::string, std::string> OutputLocals;
            std::string LocalName;
            BehaviorArgument LocalValue;
            std::string EventId;
            std::string DebugNodeId;
            float WaitSeconds = 0.0f;
            int TargetInstruction = -1;
            int AlternateInstruction = -1;
        };

        struct BehaviorEntry
        {
            std::string Id;
            uint32_t Version = 1;
            // First instruction of the behavior's node span, used by static
            // analysis and JumpToNode range checks.
            size_t InstructionBegin = 0;
            // Instruction of the configured entry node, used by the VM.
            size_t FirstInstruction = 0;
        };

        struct BehaviorProgram
        {
            std::string ModuleId;
            std::string SourcePath;
            uint32_t SchemaVersion = 1;
            uint64_t Revision = 0;
            std::vector<BehaviorEntry> Entries;
            std::vector<BehaviorInstruction> Instructions;

            const BehaviorEntry* FindEntry(const std::string& id) const;
        };

        enum class BehaviorInstanceStatus
        {
            Running = 0,
            Waiting,
            Completed,
            Cancelled,
            Failed
        };

        struct BehaviorEvent
        {
            std::string Id;
            UUID Source = 0;
            UUID Target = 0;
            std::unordered_map<std::string, BehaviorValue> Payload;
        };

        struct BehaviorInstance
        {
            uint64_t Handle = 0;
            std::string BehaviorId;
            std::string EntryId;
            uint64_t BehaviorRevision = 0;
            std::shared_ptr<const BehaviorProgram> Program;
            Scene* SceneContext = nullptr;

            ActionIntent Intent;
            ActionRecipe Recipe;
            ActionRuntime Runtime;
            std::shared_ptr<ActionRuntime> RuntimeOwner;
            ActionRuntime* RuntimeBinding = nullptr;
            std::shared_ptr<ActionRuntime> TargetRuntimeOwner;
            ActionRuntime* TargetRuntimeBinding = nullptr;
            bool ActionActivated = false;
            EffectBundle AppliedEffects;
            EffectLedger Ledger;

            UUID OwnerEntity = 0;
            UUID TargetEntity = 0;
            std::unordered_map<std::string, BehaviorValue> Locals;
            size_t ProgramCounter = 0;

            BehaviorInstanceStatus Status = BehaviorInstanceStatus::Running;
            float WaitRemaining = 0.0f;
            std::string WaitingEvent;
            uint64_t WaitingChildHandle = 0;
            int WaitingNextInstruction = -1;
            int WaitingTimeoutInstruction = -1;
            std::string LastPrimitiveId;
            std::string FailureReason;
            std::string Detail;

            bool Paused = false;
            bool StepRequested = false;
            bool SkipBreakpointOnce = false;
            std::string PausedNodeId;
            std::unordered_set<std::string> Breakpoints;
        };

    } // namespace WAO

} // namespace Wheatear
