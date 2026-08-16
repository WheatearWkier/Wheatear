#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <string>

namespace Wheatear::ArcadeCombatTuningService {

    // Global (data-driven) arcade-combat tuning: level flow timings, boss
    // behaviour and player feel. Loaded from the YAML file named by
    // ArcadeCombatLevelComponent::TuningPath (hot-reloaded on mtime change);
    // component fields act as per-scene fallbacks.
    struct ArcadeLevelTuning
    {
        float StartFadeDuration = 0.80f;
        float VictoryReturnDelay = 2.60f;
        float DefeatReturnDelay = 2.20f;
        float ResultSceneFadeDuration = 0.55f;
        float BossDefeatFadeDuration = 1.15f;
    };

    struct ArcadeBossTuning
    {
        float IntroDuration = 1.25f;
        float ShootInterval = 1.05f;
        float JumpInterval = 2.75f;
        float JumpDuration = 0.65f;
    };

    struct ArcadePlayerTuning
    {
        float MoveSpeed = 4.2f;
        bool  AutoAim = true;
    };

    struct ArcadeCombatTuning
    {
        bool Loaded = false;
        ArcadeLevelTuning Level;
        ArcadeBossTuning Boss;
        ArcadePlayerTuning Player;
    };

    WHEATEAR_API const ArcadeCombatTuning& GetTuning(const ArcadeCombatLevelComponent& level);

    // Applies the tuning values to the scene at runtime start: level flow
    // timings onto the level component, boss behaviour onto the boss entity
    // (level.BossEntityName) and player feel onto the player entity
    // (level.PlayerEntityName). No-op when the tuning file is absent.
    WHEATEAR_API void ApplyLevelTuning(const ArcadeCombatTuning& tuning,
        ArcadeCombatLevelComponent& level);
    WHEATEAR_API void ApplyBossTuning(const ArcadeCombatTuning& tuning,
        Scene* scene,
        const ArcadeCombatLevelComponent& level);
    WHEATEAR_API void ApplyPlayerTuning(const ArcadeCombatTuning& tuning,
        Scene* scene,
        const ArcadeCombatLevelComponent& level);

} // namespace Wheatear::ArcadeCombatTuningService
