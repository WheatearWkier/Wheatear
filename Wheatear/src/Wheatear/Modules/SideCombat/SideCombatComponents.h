#pragma once

#include "Wheatear/Core/UUID.h"

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace Wheatear {

    enum class SideCombatTeam
    {
        Neutral = 0,
        Player = 1,
        Enemy = 2
    };

    enum class SideEnemyKind
    {
        Grunt = 0,
        Thrower = 1,
        Pouncer = 2,
        BearBoss = 3
    };

    enum class SideAttackKind
    {
        Basic = 0,
        Launcher = 1,
        MagicBolt = 2,
        AllySupport = 3,
        EnemyMelee = 4,
        EnemyProjectile = 5,
        EnemyShockwave = 6,
        BreakLimit = 7
    };

    enum class SideCombatState
    {
        Normal = 0,
        HitStun = 1,
        Launched = 2,
        Knockdown = 3,
        Recovery = 4,
        SuperArmor = 5,
        Broken = 6,
        Dead = 7
    };

    struct SideCombatLevelComponent
    {
        bool        PlayOnStart = true;
        std::string LevelId = "CH02_MAIN_BearAwakening";
        std::string TuningPath = "side.tuning";
        glm::vec2   ArenaMin = { -8.8f, -4.0f };
        glm::vec2   ArenaMax = { 8.8f, 4.6f };
        float       GroundY = -3.25f;
        float       LaneMinY = -3.55f;
        float       LaneMaxY = -0.65f;

        std::string PlayerEntityName = "SC_Player";
        std::string BossEntityName = "SC_Boss_BearHusband";
        std::string FadeEntityName = "SC_Fade";
        std::string MessageTextEntityName = "SC_Message";
        std::string ComboTextEntityName = "SC_ComboText";
        std::string SkillTextEntityName = "SC_SkillText";
        std::string RewardTextEntityName = "SC_RewardText";
        std::string PlayerHealthBarEntityName = "SC_PlayerHealth";
        std::string PlayerHealthTextEntityName = "SC_PlayerHealthText";
        std::string BossHealthBarEntityName = "SC_BossHealth";
        std::string BossHealthTextEntityName = "SC_BossHealthText";

        float       StartFadeDuration = 0.65f;
        float       VictoryReturnDelay = 4.0f;
        float       DefeatReturnDelay = 2.5f;
        float       ResultSceneFadeDuration = 0.65f;
        std::string VictorySceneCommand = "scene:assets/scenes/VerticalSliceHub.wt";
        std::string DefeatSceneCommand = "scene:assets/scenes/SideCombatVerticalSlice.wt";

        float       ComboDropDelay = 1.15f;
        std::string FirstClearRewardText = "获得: 魔核碎片 x1 / 兽筋 x2 / 熊爪 x1";

        bool        WaveModeEnabled = false;
        int         WaveCount = 3;
        float       Wave1RightWall = -2.2f;
        float       Wave2RightWall = 3.2f;
        float       Wave3RightWall = 8.8f;

        float       RuntimeElapsed = 0.0f;
        float       RuntimeFadeAlpha = 1.0f;
        bool        RuntimePaused = false;
        bool        RuntimeVictory = false;
        bool        RuntimeDefeat = false;
        float       RuntimeResultTimer = 0.0f;
        bool        RuntimeResultCommandIssued = false;
        std::string RuntimeRequestedCommand = "";

        int         RuntimeComboCount = 0;
        int         RuntimeBestCombo = 0;
        float       RuntimeComboTimer = 0.0f;
        int         RuntimeCollectedPickups = 0;
        bool        RuntimeRewardsSpawned = false;
        int         RuntimePlayerHitsTaken = 0;
        int         RuntimeResultExperience = 0;
        int         RuntimeResultRepeatExperience = 0;
        bool        RuntimeResultFirstClear = false;
        UUID        RuntimePlayerEntity = 0;
        UUID        RuntimeBossEntity = 0;
        std::string RuntimeResultGrade = "";
        std::string RuntimeResultSummary = "";
        float       RuntimeHitPauseTimer = 0.0f;
        float       RuntimeCameraShakeTimer = 0.0f;
        float       RuntimeCameraShakeDuration = 0.0f;
        float       RuntimeCameraShakeStrength = 0.0f;
        glm::vec3   RuntimeCameraBaseTranslation = { 0.0f, 0.0f, 0.0f };
        bool        RuntimeCameraBaseCaptured = false;
        int         RuntimeWaveIndex = 0;
        float       RuntimeWaveRightWall = 8.8f;
        bool        RuntimeWaveSpawnsCreated = false;

        SideCombatLevelComponent() = default;
        SideCombatLevelComponent(const SideCombatLevelComponent&) = default;
    };

    struct SideCombatantComponent
    {
        int       Team = (int)SideCombatTeam::Neutral;
        float     MaxHealth = 100.0f;
        float     Health = 100.0f;
        float     Attack = 20.0f;
        float     Defense = 10.0f;
        float     MoveSpeed = 4.8f;
        glm::vec2 CollisionSize = { 0.7f, 1.1f };
        float     CollisionHeight = 1.25f;
        float     GravityScale = 1.0f;
        float     KnockbackResistance = 0.0f;
        bool      Invulnerable = false;

        bool      Alive = true;
        bool      ControlsLocked = false;
        bool      RuntimeOnGround = false;
        bool      RuntimeDeathProcessed = false;
        bool      RuntimeRemoveAfterDeath = false;
        SideCombatState RuntimeState = SideCombatState::Normal;
        float     RuntimeStateTimer = 0.0f;
        float     RuntimeDeathTimer = 0.0f;
        float     RuntimeFacing = 1.0f;
        float     RuntimeHitStun = 0.0f;
        float     RuntimeInvulnerableTimer = 0.0f;
        float     RuntimeProtection = 0.0f;
        float     RuntimeProtectionMax = 100.0f;
        glm::vec2 RuntimeVelocity = { 0.0f, 0.0f };
        glm::vec2 RuntimeGroundPosition = { 0.0f, 0.0f };
        float     RuntimeAirHeight = 0.0f;
        float     RuntimeAirVelocity = 0.0f;
        std::string RuntimeVisualClipKey = "";
        float     RuntimeVisualTimer = 0.0f;
        uint32_t  RuntimeVisualActionSequence = 0;

        SideCombatantComponent() = default;
        SideCombatantComponent(const SideCombatantComponent&) = default;
    };

    struct SidePlayerControllerComponent
    {
        int   MaxJumps = 1;
        float JumpImpulse = 8.1f;
        float Gravity = 22.0f;
        float AirControl = 16.0f;
        float JumpBufferTime = 0.12f;
        float CoyoteTime = 0.08f;
        float LaneSpeedScale = 0.72f;
        float LaneAcceleration = 28.0f;
        float GroundFriction = 18.0f;

        float BasicCooldown = 0.20f;
        float LauncherCooldown = 0.45f;
        float MagicBoltCooldown = 0.70f;
        float AllySupportCooldown = 4.80f;

        float RuntimeBasicCooldown = 0.0f;
        float RuntimeLauncherCooldown = 0.0f;
        float RuntimeMagicBoltCooldown = 0.0f;
        float RuntimeAllySupportCooldown = 0.0f;
        float RuntimeBreakLimitCooldown = 0.0f;
        float RuntimeMagicSwordGauge = 3.0f;
        float RuntimeMagicSwordGaugeMax = 3.0f;
        int   RuntimeJumpsRemaining = 1;
        float RuntimeJumpBufferTimer = 0.0f;
        float RuntimeCoyoteTimer = 0.0f;
        int   RuntimeAttackChain = 0;
        float RuntimeAttackChainTimer = 0.0f;
        int   RuntimeAirActionsRemaining = 0;
        std::string RuntimeActionAttackId = "";
        std::string RuntimeActionRecipeId = "";
        std::string RuntimeActionEntityName = "";
        SideAttackKind RuntimeActionKind = SideAttackKind::Basic;
        float RuntimeActionTimer = 0.0f;
        float RuntimeActionDuration = 0.0f;
        float RuntimeActionHitboxTime = 0.0f;
        float RuntimeActionCancelStart = 0.0f;
        float RuntimeActionCancelEnd = 0.0f;
        float RuntimeActionMovementScale = 1.0f;
        bool  RuntimeActionHitboxSpawned = false;
        uint32_t RuntimeActionSequence = 0;

        SidePlayerControllerComponent() = default;
        SidePlayerControllerComponent(const SidePlayerControllerComponent&) = default;
    };

    struct SideEnemyAIComponent
    {
        SideEnemyKind Kind = SideEnemyKind::Grunt;
        float AggroRange = 8.0f;
        float AttackRange = 1.35f;
        float PreferredRange = 1.05f;
        float AttackInterval = 1.20f;
        float PatrolMinX = -7.5f;
        float PatrolMaxX = 7.5f;
        float LaneTolerance = 0.42f;

        float RuntimeAttackTimer = 0.0f;
        float RuntimeDecisionTimer = 0.0f;
        bool  RuntimeAwake = true;
        std::string RuntimeActionAttackId = "";
        std::string RuntimeActionRecipeId = "";
        std::string RuntimeActionEntityName = "";
        SideAttackKind RuntimeActionKind = SideAttackKind::EnemyMelee;
        float RuntimeActionTimer = 0.0f;
        float RuntimeActionDuration = 0.0f;
        float RuntimeActionHitboxTime = 0.0f;
        float RuntimeActionMovementScale = 1.0f;
        float RuntimeActionFacing = 1.0f;
        bool  RuntimeActionHitboxSpawned = false;
        uint32_t RuntimeActionSequence = 0;

        SideEnemyAIComponent() = default;
        SideEnemyAIComponent(const SideEnemyAIComponent&) = default;
    };

    struct SideHitboxComponent
    {
        int            Team = (int)SideCombatTeam::Player;
        SideAttackKind AttackKind = SideAttackKind::Basic;
        std::string    ActionRecipeId;
        glm::vec2      Size = { 0.8f, 0.4f };
        glm::vec2      Velocity = { 0.0f, 0.0f };
        glm::vec2      LaunchVelocity = { 0.0f, 0.0f };
        float          AirHeight = 0.55f;
        float          AirRange = 1.1f;
        float          Damage = 10.0f;
        float          Lifetime = 0.12f;
        float          HitStun = 0.22f;
        float          AttackerAirImpulse = 0.0f;
        float          AttackerAirFallStep = 0.0f;
        float          TargetAirFallStep = 0.0f;
        float          ProtectionGain = 0.0f;
        bool           DestroyOnHit = true;
        std::string    TextureFramePattern;
        int            TextureFrameCount = 1;
        float          TextureFrameRate = 16.0f;
        std::string    HitSound;
        float          HitSoundVolume = 1.0f;
        float          HitPause = 0.0f;
        float          CameraShake = 0.0f;
        float          CameraShakeDuration = 0.0f;
        float          RuntimeAge = 0.0f;
        bool           RuntimeHitSomething = false;
        glm::vec2      RuntimeGroundPosition = { 0.0f, 0.0f };
        uint32_t       RuntimeOwnerEntity = 0;

        SideHitboxComponent() = default;
        SideHitboxComponent(const SideHitboxComponent&) = default;
    };

    struct SidePickupComponent
    {
        std::string ItemId = "MAT-BEAST-SINEW";
        std::string DisplayName = "兽筋";
        int         Amount = 1;
        float       PickupRadius = 0.45f;
        float       AttractRadius = 2.25f;
        float       AttractSpeed = 6.0f;

        SidePickupComponent() = default;
        SidePickupComponent(const SidePickupComponent&) = default;
    };

} // namespace Wheatear
