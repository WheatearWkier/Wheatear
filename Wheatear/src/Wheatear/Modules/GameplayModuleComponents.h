#pragma once

#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatComponents.h"
#include "Wheatear/Modules/SideCombat/SideCombatComponents.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatComponents.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatComponents.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelComponents.h"
#include "Wheatear/Scene/ComponentGroup.h"

namespace Wheatear {

    using GameplayModuleSceneComponents = ComponentGroup
    <
        VisualNovelComponent,
        ArcadeCombatLevelComponent,
        ArcadeCombatantComponent,
        ArcadePlayerControllerComponent,
        ArcadeBossComponent,
        ArcadeProjectileComponent,
        ArcadeCoverComponent,
        ArcadeTriggerComponent,
        SideCombatLevelComponent,
        SideCombatantComponent,
        SidePlayerControllerComponent,
        SideEnemyAIComponent,
        SideHitboxComponent,
        SidePickupComponent,
        TacticalCombatLevelComponent,
        TacticalUnitComponent,
        TurnCombatLevelComponent,
        TurnCombatantComponent
    >;

    using GameplayRuntimeCommandComponents = ComponentGroup
    <
        VisualNovelComponent,
        ArcadeCombatLevelComponent,
        SideCombatLevelComponent,
        TacticalCombatLevelComponent,
        TurnCombatLevelComponent
    >;

} // namespace Wheatear
