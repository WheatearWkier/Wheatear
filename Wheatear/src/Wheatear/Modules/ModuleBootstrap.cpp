#include "wtpch.h"
#include "ModuleBootstrap.h"

#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatActionResolver.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSignalHandlers.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatSystem.h"
#include "Wheatear/Modules/Progression/ProgressionSystem.h"
#include "Wheatear/Modules/SideCombat/SideCombatSystem.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatSystem.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatSystem.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelSystem.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Gameplay/Action/ActionAssetLoader.h"
#include "Wheatear/Gameplay/Action/ActionResolver.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/SceneSystemRegistry.h"
#include "Wheatear/Scripting/EventScriptSystem.h"

namespace Wheatear {

    void RegisterDefaultGameplayModules()
    {
        CommandBus::RegisterGameplayCommandPrefix("vn:");
        CommandBus::RegisterGameplayCommandPrefix("turn:");
        CommandBus::RegisterGameplayCommandPrefix("tactic:");

        ArcadeCombatSignalHandlers::RegisterHandlers();
        ArcadeCombatActionResolver::RegisterResolver();
        WAO::RegisterRecipePreviewResolver("side.", "Side action recipe resolved");
        WAO::RegisterRecipePreviewResolver("turn.", "Turn action recipe resolved");
        WAO::RegisterRecipePreviewResolver("tactical.", "Tactical action recipe resolved");
        AssetAliasRegistry::Load();
        const size_t actionRecipeCount = WAO::ActionAssetLoader::LoadManifest(
            AssetAliasRegistry::Path("wao.action_sets", "assets/gameplay/actions/action_sets.yaml"));
        if (actionRecipeCount == 0)
        {
            WAO::ActionAssetLoader::LoadDirectory(
                AssetAliasRegistry::Path("wao.action_directory", "assets/gameplay/actions"));
        }

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
