#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace Wheatear::SideCombatTuningService {

    struct SideAttackTuning
    {
        glm::vec2 Size = { 1.0f, 0.7f };
        glm::vec2 Offset = { 0.8f, 0.0f };
        glm::vec2 Velocity = { 0.0f, 0.0f };
        glm::vec2 LaunchVelocity = { 1.2f, 2.0f };
        float AirHeight = 0.55f;
        float AirRange = 1.15f;
        float DamageScale = 0.75f;
        float DamageFlat = 8.0f;
        float Lifetime = 0.14f;
        float Startup = 0.0f;
        float Recovery = 0.0f;
        float CancelWindowStart = 0.0f;
        float CancelWindowEnd = 0.0f;
        float MovementScale = 1.0f;
        float HitStun = 0.30f;
        float AttackerAirImpulse = 0.0f;
        float AttackerAirFallStep = 0.0f;
        float TargetAirFallStep = 0.0f;
        float ProtectionGain = 0.0f;
        bool DestroyOnHit = true;
        std::string TextureFramePattern;
        int TextureFrameCount = 1;
        float TextureFrameRate = 16.0f;
        std::string SwingSound;
        std::string HitSound;
        float SoundVolume = 1.0f;
        float HitPause = 0.0f;
        float CameraShake = 0.0f;
        float CameraShakeDuration = 0.0f;
    };

    struct SidePlayerTuning
    {
        float MoveSpeed = 5.4f;
        int MaxJumps = 1;
        float JumpImpulse = 8.8f;
        float Gravity = 23.0f;
        float AirControl = 15.0f;
        float JumpBufferTime = 0.12f;
        float CoyoteTime = 0.08f;
        float LaneSpeedScale = 0.72f;
        float LaneAcceleration = 30.0f;
        float GroundAcceleration = 40.0f;
        float GroundFriction = 18.0f;
        float BasicCooldown = 0.19f;
        float BasicFinisherExtraCooldown = 0.08f;
        float LauncherCooldown = 0.42f;
        float MagicBoltCooldown = 0.64f;
        float AllySupportCooldown = 4.2f;
        float BasicChainWindow = 0.72f;
        float LauncherChainWindow = 0.82f;
        float MagicChainWindow = 0.78f;
        float SupportChainWindow = 1.0f;
    };

    struct SideCombatRuleTuning
    {
        float ComboDropDelay = 1.15f;
        float HitInvulnerableTime = 0.06f;
        float DefenseBase = 100.0f;
        float MinDamage = 1.0f;
    };

    struct SideAirComboTuning
    {
        int AirActionLimit = 3;
        int AirActionLimitAfterBreak = 3;
        bool BreakLimitEnabled = false;
        bool BreakLimitDebugKeyEnabled = false;
        bool ShowBreakLimitHint = false;
        float AirBasicCooldown = 0.16f;
        float AirChaseCooldown = 0.36f;
        float BreakLimitCooldown = 3.0f;
        int BreakLimitMinCombo = 4;
        float BreakLimitMaxHeight = 2.4f;
        float BreakLimitFallingVelocity = 1.8f;
        float BreakLimitHangImpulse = 2.2f;
        float BreakLimitHeightBoost = 0.22f;
        float BreakLimitGaugeCost = 1.0f;
        float MagicSwordGaugeMax = 3.0f;
        float GaugeGainGroundHit = 0.10f;
        float GaugeGainAirHit = 0.16f;
        float GroundThreatHeight = 1.25f;
        float HighAirSafetyHeight = 2.2f;
    };

    struct SideProtectionTuning
    {
        float BossProtectionMax = 100.0f;
        float BossProtectionDecayPerSecond = 16.0f;
        float BossProtectionLimitTime = 1.15f;
        float BossProtectionForceFallVelocity = -6.0f;
        float BossProtectionBreakLimitThreshold = 35.0f;
        float BreakLimitProtectionReduce = 42.0f;
        float GroundResetDelay = 0.20f;
        bool ShowBossProtectionHud = false;
        bool ShowCombatStateHud = false;
    };

    struct SideSkillDefinition
    {
        std::string DisplayName;
        std::string InputLabel;
        std::string ComboRole;
        std::vector<std::string> AttackIds;
        int UnlockChapter = 2;
        bool CoreMove = false;
    };

    struct SideUnlockProfile
    {
        std::string Id;
        std::string DisplayName;
        int Chapter = 2;
        std::vector<std::string> UnlockedSkills;
        std::vector<std::string> DebugSkills;
        bool ShowBossProtectionHud = false;
        bool ShowCombatStateHud = false;
        bool ShowBreakLimitHint = false;
    };

    struct SideAnimationClipTuning
    {
        std::string Pattern;
        int FrameCount = 1;
        float FrameRate = 8.0f;
        bool Loop = true;
        glm::vec2 RenderOffset = { 0.0f, 0.0f };
        glm::vec2 RenderScale = { 1.0f, 1.0f };
    };

    struct SideAnimationSetTuning
    {
        std::unordered_map<std::string, SideAnimationClipTuning> Clips;
    };

    struct SideFeedbackTuning
    {
        float HitPauseTimeScale = 0.12f;
        std::string JumpSound = "side.audio.jump";
        std::string LandSound = "side.audio.land";
        float JumpSoundVolume = 0.55f;
        float LandSoundVolume = 0.58f;
    };

    struct SideProgressionTuning
    {
        std::string DefaultProfileId = "CH02_MAIN_BearAwakening";
        std::unordered_map<std::string, SideUnlockProfile> Profiles;
    };

    struct SideEnemyTuning
    {
        float InitialAttackDelay = 0.35f;
        float AttackRangePadding = 0.45f;
        float LaneAttackPadding = 0.28f;
        float BossPreferredRangeBonus = 0.45f;
        float GruntMoveSpeedScale = 0.88f;
        float BossMoveSpeedScale = 0.62f;
        float GruntLaneSpeedScale = 0.68f;
        float BossLaneSpeedScale = 0.46f;
        float XApproachAcceleration = 24.0f;
        float XBrakeAcceleration = 20.0f;
        float LaneApproachAcceleration = 18.0f;
        float LaneBrakeAcceleration = 16.0f;

        float BearBossMoveSpeed = 3.4f;
        float BearBossAggroRange = 12.0f;
        float BearBossAttackRange = 1.55f;
        float BearBossPreferredRange = 1.2f;
        float BearBossAttackInterval = 1.18f;
        float BearBossLaneTolerance = 0.58f;
        float BearBossMidHealthThreshold = 0.68f;
        float BearBossLowHealthThreshold = 0.36f;
        float BearBossMidAttackInterval = 0.96f;
        float BearBossLowAttackInterval = 0.78f;
        float BearBossChargeDistance = 1.8f;
        float BearBossShockwaveDistance = 2.0f;
        float BearBossChargeSpeed = 5.8f;
    };

    struct SidePickupTuning
    {
        float PickupRadius = 0.45f;
        float AttractRadius = 2.25f;
        float AttractSpeed = 6.0f;
    };

    struct SideCombatTuning
    {
        bool Loaded = false;
        SidePlayerTuning Player;
        SideCombatRuleTuning Combat;
        SideAirComboTuning AirCombo;
        SideProtectionTuning Protection;
        SideEnemyTuning Enemy;
        SidePickupTuning Pickup;
        float LaneMinY = -3.55f;
        float LaneMaxY = -1.30f;
        float LaneSpeedScale = 0.72f;
        float LaneAcceleration = 28.0f;
        float SortScale = 0.015f;
        float ShadowMinAlpha = 0.22f;
        float ShadowMaxAlpha = 0.46f;
        float ShadowAirFadeHeight = 3.8f;
        glm::vec2 ShadowOffset = { 0.0f, -0.10f };
        float BossLaunchBonus = 1.35f;
        SideFeedbackTuning Feedback;
        SideAnimationSetTuning PlayerAnimations;
        SideAnimationSetTuning GruntAnimations;
        SideAnimationSetTuning BossAnimations;
        std::unordered_map<std::string, SideAttackTuning> Attacks;
        std::unordered_map<std::string, SideSkillDefinition> Skills;
        SideProgressionTuning Progression;
    };

    WHEATEAR_API float CalculateSortZ(float groundY, const SideCombatTuning& tuning);
    WHEATEAR_API const SideCombatTuning& GetTuning(const SideCombatLevelComponent& level);
    WHEATEAR_API const SideAttackTuning& GetAttack(const SideCombatTuning& tuning, const std::string& id);
    WHEATEAR_API bool IsSkillUnlocked(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning,
        const std::string& skillId);
    WHEATEAR_API bool IsDebugSkillEnabled(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning,
        const std::string& skillId);
    WHEATEAR_API bool IsSkillUsable(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning,
        const std::string& skillId);
    WHEATEAR_API bool IsBreakLimitOfficiallyAvailable(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning);
    WHEATEAR_API bool IsBreakLimitDebugAvailable(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning);
    WHEATEAR_API bool ShouldShowBreakLimitUi(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning);
    WHEATEAR_API bool ShouldShowBossProtectionHud(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning);
    WHEATEAR_API bool ShouldShowCombatStateHud(
        const SideCombatLevelComponent& level,
        const SideCombatTuning& tuning);
    WHEATEAR_API void ApplyPlayerTuning(const SideCombatTuning& tuning,
        SideCombatantComponent& combatant,
        SidePlayerControllerComponent& controller);
    WHEATEAR_API void ApplyBearBossTuning(const SideCombatTuning& tuning,
        SideCombatantComponent& combatant,
        SideEnemyAIComponent& ai);

} // namespace Wheatear::SideCombatTuningService
