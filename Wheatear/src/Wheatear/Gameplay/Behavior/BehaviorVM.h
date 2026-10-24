#pragma once

#include "BehaviorTypes.h"
#include "Wheatear/Core/Core.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::WAO {

    struct BehaviorStartResult
    {
        bool Started = false;
        bool Completed = false;
        bool Success = false;
        uint64_t Handle = 0;
        uint64_t BehaviorRevision = 0;
        std::string Detail;
        EffectBundle AppliedEffects;
        EffectLedger Ledger;
    };

    struct BehaviorDebugSnapshot
    {
        uint64_t Handle = 0;
        std::string BehaviorId;
        std::string EntryId;
        uint64_t BehaviorRevision = 0;
        UUID OwnerEntity = 0;
        UUID TargetEntity = 0;
        size_t ProgramCounter = 0;
        std::string CurrentNodeId;
        BehaviorInstanceStatus Status = BehaviorInstanceStatus::Running;
        float WaitRemaining = 0.0f;
        std::string WaitingEvent;
        std::string LastPrimitiveId;
        std::string FailureReason;
        bool Paused = false;
        std::string PausedNodeId;
        bool CurrentNodeBreakpoint = false;
        std::unordered_map<std::string, BehaviorValue> Locals;
        std::vector<EffectSpec> AppliedEffects;
        std::vector<EffectLedgerEntry> Ledger;
    };

    class WHEATEAR_API BehaviorVM
    {
    public:
        static BehaviorStartResult Start(Scene* scene,
            const ActionIntent& intent,
            const ActionRecipe& recipe,
            ActionRuntime* runtime = nullptr);

        // Runs a compiled program without touching BehaviorDatabase. Used by
        // the editor sandbox and tests for isolated execution.
        static BehaviorStartResult StartWithProgram(Scene* scene,
            const ActionIntent& intent,
            const ActionRecipe& recipe,
            const std::shared_ptr<const BehaviorProgram>& program,
            const std::string& entryId,
            ActionRuntime* runtime = nullptr);

        static void Update(Scene* scene, float deltaSeconds);
        static void EmitEvent(Scene* scene, const BehaviorEvent& event);
        static void Cancel(Scene* scene, uint64_t handle, const std::string& reason = {});
        static void StopScene(Scene* scene);
        static void Clear();

        static void SetPaused(Scene* scene, uint64_t handle, bool paused);
        static void Step(Scene* scene, uint64_t handle);
        // Debug-only: move an existing instance to a node and resume it.
        static bool JumpToNode(Scene* scene, uint64_t handle,
            const std::string& nodeId);
        static void SetBreakpoint(Scene* scene, uint64_t handle,
            const std::string& nodeId, bool enabled);

        static std::vector<BehaviorDebugSnapshot> Active(Scene* scene);
    };

} // namespace Wheatear::WAO
