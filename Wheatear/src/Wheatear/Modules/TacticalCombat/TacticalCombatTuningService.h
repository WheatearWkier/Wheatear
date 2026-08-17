#pragma once

#include "TacticalCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

    // One frame-animation slot of a tactical unit (idle / attack / hit / down).
    struct TacticalFrameTuning
    {
        std::string Sheet;
        int CellWidth = 96;
        int CellHeight = 112;
        int Columns = 4;
        int StartFrame = 0;
        int Count = 1;
    };

    // Data-table entry for a tactical unit. Matched at runtime start against
    // scene entities whose TagComponent.Tag uses the tactical unit prefix plus
    // <Tag>; the tuning
    // values overwrite TacticalUnitComponent fields (scene component acts as
    // per-scene fallback when the tag is not present in the tuning).
    struct TacticalUnitTuning
    {
        std::string Tag;
        int Team = 0;
        int Slot = 0;
        int GridX = 0;
        int GridY = 0;
        std::string DisplayName = "Unit";
        std::string ClassName;

        float MaxHealth = 100.0f;
        float Attack = 24.0f;
        float Magic = 18.0f;
        float Defense = 8.0f;
        int MoveRange = 3;
        int AttackRange = 1;
        bool Controllable = false;

        std::string BasicSkillId = "sword_slash";
        std::string Skill1Id;
        std::string Skill2Id;

        TacticalFrameTuning IdleFrames;
        TacticalFrameTuning AttackFrames;
        TacticalFrameTuning HitFrames;
        TacticalFrameTuning DownFrames;
        float AnimationFrameRate = 8.0f;
    };

    struct TacticalCombatTuning
    {
        bool Loaded = false;
        TacticalLevelTuning Level;
        TacticalFormulaTuning Formula;
        std::vector<TacticalUnitTuning> Units;
    };

    WHEATEAR_API std::filesystem::path TuningSourcePath(const TacticalCombatLevelComponent& level);
    WHEATEAR_API bool IsFieldManagedByTuning(std::string_view fieldId);
    WHEATEAR_API const TacticalCombatTuning& GetTuning(const TacticalCombatLevelComponent& level);

    // Overwrites the level's flow / board / color fields with the tuning
    // values (when the tuning file exists and is parseable); used at runtime
    // start so every consumer reads the data-driven numbers through the
    // component fields.
    WHEATEAR_API void ApplyLevelTuning(const TacticalCombatTuning& tuning,
        TacticalCombatLevelComponent& level);

    // Applies the tuning's unit table to scene entities whose tag is
    // the tactical unit prefix plus <tag> (matching units are overwritten, non-matching scene
    // units keep their component values). Called at runtime start; the
    // editor exposes the same operation for edit-time preview.
    WHEATEAR_API size_t ApplyUnitTuningToScene(Scene* scene,
        const TacticalCombatTuning& tuning);

} // namespace Wheatear::TacticalCombatTuningService
