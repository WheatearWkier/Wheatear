#pragma once

#include "TurnCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace Wheatear::TurnCombatTuningService {

    // Global (data-driven) turn-combat tuning. Loaded from the YAML file named
    // by TurnCombatLevelComponent::TuningPath (hot-reloaded on mtime change);
    // the level's own component fields act as per-scene fallbacks.
    struct TurnLevelTuning
    {
        float StartFadeDuration = 0.55f;
        float IntroDuration = 0.80f;
        float ActionDuration = 0.72f;
        float VictoryReturnDelay = 1.80f;
        float DefeatReturnDelay = 1.40f;
    };

    struct TurnFormulaTuning
    {
        float DefenseMultiplier = 0.62f;
        float MinDamage = 1.0f;
    };

    struct TurnCombatTuning
    {
        bool Loaded = false;
        TurnLevelTuning Level;
        TurnFormulaTuning Formula;
    };

    WHEATEAR_API std::filesystem::path TuningSourcePath(const TurnCombatLevelComponent& level);
    WHEATEAR_API bool IsFieldManagedByTuning(std::string_view fieldId);
    WHEATEAR_API const TurnCombatTuning& GetTuning(const TurnCombatLevelComponent& level);

    // Overwrites the level's flow timings with the tuning values (when the
    // tuning file exists and is parseable); used at runtime start so every
    // consumer reads the data-driven numbers through the component fields.
    WHEATEAR_API void ApplyLevelTuning(const TurnCombatTuning& tuning,
        TurnCombatLevelComponent& level);

} // namespace Wheatear::TurnCombatTuningService
