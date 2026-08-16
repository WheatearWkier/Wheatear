#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <glm/glm.hpp>

#include <string>

namespace Wheatear::TacticalCombatTuningService {

    // Global (data-driven) tactical-combat tuning: level flow timings, board
    // layout and tile colors, plus formula coefficients. Loaded from the YAML
    // file named by TacticalCombatLevelComponent::TuningPath (hot-reloaded on
    // mtime change); component fields act as per-scene fallbacks.
    struct TacticalLevelTuning
    {
        float StartFadeDuration = 0.45f;
        float IntroDuration = 0.65f;
        float ActionDuration = 0.62f;
        float EnemyStepDuration = 0.42f;
        float VictoryReturnDelay = 1.75f;
        float DefeatReturnDelay = 1.35f;

        int GridWidth = 8;
        int GridHeight = 6;
        glm::vec2 BoardOrigin = { 0.275f, 0.115f };
        glm::vec2 CellSize = { 0.0625f, 0.0875f };

        glm::vec4 TileNormalColor = { 1.0f, 1.0f, 1.0f, 0.92f };
        glm::vec4 TileMoveColor = { 0.32f, 0.78f, 1.0f, 0.88f };
        glm::vec4 TileAttackColor = { 1.0f, 0.36f, 0.28f, 0.92f };
        glm::vec4 TileSelectedColor = { 1.0f, 0.88f, 0.32f, 1.0f };
    };

    struct TacticalFormulaTuning
    {
        float MagicDefenseMultiplier = 0.45f;
        float MinDamage = 6.0f;
    };

    struct TacticalCombatTuning
    {
        bool Loaded = false;
        TacticalLevelTuning Level;
        TacticalFormulaTuning Formula;
    };

    WHEATEAR_API const TacticalCombatTuning& GetTuning(const TacticalCombatLevelComponent& level);

    // Overwrites the level's flow / board / color fields with the tuning
    // values (when the tuning file exists and is parseable); used at runtime
    // start so every consumer reads the data-driven numbers through the
    // component fields.
    WHEATEAR_API void ApplyLevelTuning(const TacticalCombatTuning& tuning,
        TacticalCombatLevelComponent& level);

} // namespace Wheatear::TacticalCombatTuningService
