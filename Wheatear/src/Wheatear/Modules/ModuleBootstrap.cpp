#include "wtpch.h"
#include "ModuleBootstrap.h"

#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSystem.h"
#include "Wheatear/Modules/Progression/ProgressionSystem.h"
#include "Wheatear/Modules/SideCombat/SideCombatSystem.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatSystem.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatSystem.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelSystem.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/SceneSystemRegistry.h"
#include "Wheatear/Scripting/EventScriptSystem.h"

namespace Wheatear {

    void RegisterDefaultGameplayModules()
    {
        CommandBus::RegisterNativeCommandPrefix("vn:");
        CommandBus::RegisterGameplayCommandPrefix("turn:");
        CommandBus::RegisterGameplayCommandPrefix("tactic:");

        SceneSystemRegistry::RegisterRuntimeSystem(
            "VisualNovel",
            []() -> Scope<ISystem> { return CreateScope<VisualNovelSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "ArcadeCombat",
            []() -> Scope<ISystem> { return CreateScope<ArcadeCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "SideCombat",
            []() -> Scope<ISystem> { return CreateScope<SideCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "TurnCombat",
            []() -> Scope<ISystem> { return CreateScope<TurnCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "TacticalCombat",
            []() -> Scope<ISystem> { return CreateScope<TacticalCombatSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "Progression",
            []() -> Scope<ISystem> { return CreateScope<ProgressionSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "EventScript",
            []() -> Scope<ISystem> { return CreateScope<EventScriptSystem>(); });
    }

} // namespace Wheatear
