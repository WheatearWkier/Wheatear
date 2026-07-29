#include "wtpch.h"
#include "ModuleBootstrap.h"

#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSystem.h"
#include "Wheatear/Modules/Progression/ProgressionSystem.h"
#include "Wheatear/Modules/SideCombat/SideCombatSystem.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelSystem.h"
#include "Wheatear/Scene/SceneSystemRegistry.h"
#include "Wheatear/Scripting/EventScriptSystem.h"

namespace Wheatear {

    void RegisterDefaultGameplayModules()
    {
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
            "Progression",
            []() -> Scope<ISystem> { return CreateScope<ProgressionSystem>(); });

        SceneSystemRegistry::RegisterRuntimeSystem(
            "EventScript",
            []() -> Scope<ISystem> { return CreateScope<EventScriptSystem>(); });
    }

} // namespace Wheatear
