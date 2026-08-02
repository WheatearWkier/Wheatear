#include "wtpch.h"
#include "ModuleBootstrap.h"

#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatActionCatalog.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatActionResolver.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSignalHandlers.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSystem.h"
#include "Wheatear/Modules/Progression/ProgressionSystem.h"
#include "Wheatear/Modules/SideCombat/SideCombatActionCatalog.h"
#include "Wheatear/Modules/SideCombat/SideCombatSystem.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatActionCatalog.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatSystem.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatActionCatalog.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatSystem.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelSystem.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/SceneSystemRegistry.h"
#include "Wheatear/Scripting/EventScriptSystem.h"

namespace Wheatear {

    void RegisterDefaultGameplayModules()
    {
        CommandBus::RegisterNativeCommandPrefix("vn:");
        CommandBus::RegisterGameplayCommandPrefix("turn:");
        CommandBus::RegisterGameplayCommandPrefix("tactic:");

        ArcadeCombatActionCatalog::RegisterActionRecipes();
        ArcadeCombatSignalHandlers::RegisterHandlers();
        ArcadeCombatActionResolver::RegisterResolver();
        SideCombatActionCatalog::RegisterActionRecipes();
        TurnCombatActionCatalog::RegisterActionRecipes();
        TacticalCombatActionCatalog::RegisterActionRecipes();
        WAO::RegisterRecipePreviewResolver("side.", "Side action recipe resolved");
        WAO::RegisterRecipePreviewResolver("turn.", "Turn action recipe resolved");
        WAO::RegisterRecipePreviewResolver("tactical.", "Tactical action recipe resolved");
        WAO::ActionAssetLoader::LoadDirectory("assets/gameplay/actions");

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
