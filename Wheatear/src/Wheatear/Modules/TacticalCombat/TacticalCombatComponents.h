#pragma once

#include "Wheatear/Core/UUID.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace Wheatear {

    enum class TacticalCombatTeam
    {
        Neutral = 0,
        Player = 1,
        Enemy = 2
    };

    enum class TacticalCombatPhase
    {
        Intro = 0,
        PlayerTurn = 1,
        AwaitCommand = 2,
        Targeting = 3,
        Acting = 4,
        EnemyTurn = 5,
        Victory = 6,
        Defeat = 7
    };

    struct TacticalCombatLevelComponent
    {
        bool PlayOnStart = true;
        std::string LevelId = "CH01_TACTICAL_HeavenlyTribulationFake";

        int GridWidth = 8;
        int GridHeight = 6;
        glm::vec2 BoardOrigin = { 0.275f, 0.115f };
        glm::vec2 CellSize = { 0.0625f, 0.0875f };

        std::string CellEntityPrefix = "TK_Cell_";
        std::string UnitEntityPrefix = "TK_Unit_";
        std::string FadeEntityName = "TK_Fade";
        std::string MessageTextEntityName = "TK_MessageText";
        std::string PhaseTextEntityName = "TK_PhaseText";
        std::string DetailTextEntityName = "TK_DetailText";
        std::string CommandPanelEntityName = "TK_CommandPanel";
        std::string ActionEffectEntityName = "TK_ActionEffect";

        std::string VictorySceneCommand = "event:tactical_combat_victory";
        std::string DefeatSceneCommand = "event:tactical_combat_retry";

        // Global tuning data file (data-driven flow / board / colors).
        // Empty = the default project path. Edited from the editor's
        // "Tactical Combat Tuning Editor".
        std::string TuningPath = "assets/vertical_slice/data/tactical_combat_tuning.yaml";

        float StartFadeDuration = 0.45f;
        float IntroDuration = 0.65f;
        float ActionDuration = 0.62f;
        float EnemyStepDuration = 0.42f;
        float VictoryReturnDelay = 1.75f;
        float DefeatReturnDelay = 1.35f;

        glm::vec4 TileNormalColor = { 1.0f, 1.0f, 1.0f, 0.92f };
        glm::vec4 TileMoveColor = { 0.32f, 0.78f, 1.0f, 0.88f };
        glm::vec4 TileAttackColor = { 1.0f, 0.36f, 0.28f, 0.92f };
        glm::vec4 TileSelectedColor = { 1.0f, 0.88f, 0.32f, 1.0f };

        float RuntimeElapsed = 0.0f;
        float RuntimeFadeAlpha = 1.0f;
        TacticalCombatPhase RuntimePhase = TacticalCombatPhase::Intro;
        TacticalCombatPhase RuntimeActionReturnPhase = TacticalCombatPhase::PlayerTurn;
        int RuntimeRound = 1;
        int RuntimeEnemyCursor = 0;
        UUID RuntimeSelectedUnit = 0;
        std::string RuntimeCommandMenuPage = "root";
        std::string RuntimeSelectedSkillId;
        UUID RuntimeActionActor = 0;
        UUID RuntimeActionTarget = 0;
        std::string RuntimeActionSkillId;
        std::string RuntimeMessage;
        std::string RuntimeRequestedCommand;
        float RuntimeIntroTimer = 0.0f;
        float RuntimeActionTimer = 0.0f;
        float RuntimeEnemyStepTimer = 0.0f;
        float RuntimeResultTimer = 0.0f;
        bool RuntimeInitialized = false;
        bool RuntimeActionApplied = false;
        bool RuntimeResultCommandIssued = false;

        TacticalCombatLevelComponent() = default;
        TacticalCombatLevelComponent(const TacticalCombatLevelComponent&) = default;
    };

    struct TacticalUnitComponent
    {
        int Team = (int)TacticalCombatTeam::Neutral;
        int Slot = 0;
        int GridX = 0;
        int GridY = 0;
        std::string DisplayName = "Unit";
        std::string ClassName = "";

        float MaxHealth = 100.0f;
        float Health = 100.0f;
        float Attack = 24.0f;
        float Magic = 18.0f;
        float Defense = 8.0f;
        int MoveRange = 3;
        int AttackRange = 1;
        bool Controllable = false;
        bool Invulnerable = false;

        std::string BasicSkillId = "sword_slash";
        std::string Skill1Id = "";
        std::string Skill2Id = "";

        std::string HealthBarEntityName;
        std::string StatusTextEntityName;
        std::string MarkerEntityName;

        std::string IdleFramePattern;
        std::string AttackFramePattern;
        std::string HitFramePattern;
        std::string DownFramePattern;
        GameplayVisualService::TextureAtlasFrameSpec IdleFrameAtlas;
        GameplayVisualService::TextureAtlasFrameSpec AttackFrameAtlas;
        GameplayVisualService::TextureAtlasFrameSpec HitFrameAtlas;
        GameplayVisualService::TextureAtlasFrameSpec DownFrameAtlas;
        int IdleFrameCount = 1;
        int AttackFrameCount = 1;
        int HitFrameCount = 1;
        int DownFrameCount = 1;
        float AnimationFrameRate = 8.0f;

        bool RuntimeAlive = true;
        bool RuntimeHasActed = false;
        bool RuntimeMoved = false;
        bool RuntimeGuarding = false;
        float RuntimeHitFlashTimer = 0.0f;
        std::vector<WAO::RuntimeState> RuntimeStatusEffects;
        std::string RuntimeVisualClip;
        float RuntimeVisualTimer = 0.0f;

        TacticalUnitComponent() = default;
        TacticalUnitComponent(const TacticalUnitComponent&) = default;
    };

} // namespace Wheatear
