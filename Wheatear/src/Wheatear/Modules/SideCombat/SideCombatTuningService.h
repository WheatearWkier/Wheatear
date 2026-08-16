#pragma once

#include "SideCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"

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
        GameplayVisualService::TextureAtlasFrameSpec TextureAtlas;
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
        float DashCooldown = 1.0f;
        float HealItemCooldown = 5.0f;
        float ManaItemCooldown = 5.0f;
        float AttackBuffItemCooldown = 10.0f;
        float DashManaCost = 16.0f;
        float DashSpeed = 10.5f;
        float DashInvulnerableTime = 0.18f;
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
        float BreakLimitGaugeCost = 1.5f;
        float MagicSwordGaugeMax = 3.0f;
        float GaugeGainGroundHit = 0.10f;
        float GaugeGainAirHit = 0.16f;
        float GroundThreatHeight = 1.25f;
        float HighAirSafetyHeight = 2.2f;
    };

    struct SideProtectionTuning
    {
        float BossProtectionMax = 100.0f;
        float BossProtectionDecayPerSecond = 36.0f;
        float BossProtectionLimitTime = 1.15f;
        float BossProtectionForceFallVelocity = -6.0f;
        float BossProtectionBreakLimitThreshold = 0.0f;
        float BreakLimitProtectionReduce = 100.0f;
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

    // One consumable-item slot. Data-driven so designers can add extra item
    // slots (new keys + effects) from the editor without touching C++: add a
    // row here, create the matching input action (Input Bindings panel) and
    // bind a key, and the runtime polls it like the built-in three slots.
    enum class SideItemSlotKind
    {
        Heal = 0,       // restores Health by controller.HealItemAmount
        Mana,           // restores Mana by controller.ManaItemAmount
        AttackBuff      // sets AttackBuffMultiplier/Duration from controller
    };

    struct SideItemSlotTuning
    {
        int Slot = 0;
        std::string ActionId = "side.item1";
        SideItemSlotKind Kind = SideItemSlotKind::Heal;
        float Cooldown = 5.0f;
        // Optional WAO recipe id. When set, the item executes the recipe's
        // effects (Heal / ConsumeResource / ModifyAttribute / AddState / ...)
        // instead of the built-in kind path, so new effect types are authored
        // as data in the WAO Action Editor instead of C++.
        std::string RecipeId;
    };

    // One skill (combat move) slot. The skill's behaviour *kind* (basic /
    // launcher / magic bolt / dash / ally support / break limit) is a runtime
    // behaviour and stays in C++; the slot table decides which input action
    // triggers which kind, so extra hotkeys and duplicated moves (a second
    // magic bolt with a different WAO attack) need no code. `Custom` kinds
    // dispatch through SideCombatSkillRegistry (see SideCombatSkillRegistry.h)
    // so new skill behaviours are one registration instead of enum + switches.
    enum class SideSkillSlotKind
    {
        Basic = 0,
        Launcher,
        MagicBolt,
        Dash,
        AllySupport,
        BreakLimit,
        Custom
    };

    struct SideSkillSlotTuning
    {
        std::string SlotId;                 // stable id, e.g. "basic"
        std::string ActionId = "side.basic";
        SideSkillSlotKind Kind = SideSkillSlotKind::Basic;
        bool Enabled = true;
        // Registered behaviour id used when Kind == Custom.
        std::string CustomBehavior;
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
        GameplayVisualService::TextureAtlasFrameSpec Atlas;
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
        float BreakLimitCinematicDuration = 3.0f;
        float BreakLimitCinematicTimeScale = 0.12f;
        float BreakLimitCameraZoom = 1.30f;
        glm::vec2 BreakLimitCameraOffset = { 0.0f, 0.25f };
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
        float InitialAttackDelay = 0.24f;
        float AttackRangePadding = 0.45f;
        float LaneAttackPadding = 0.28f;
        float BossPreferredRangeBonus = 0.18f;
        float GruntMoveSpeedScale = 0.88f;
        float BossMoveSpeedScale = 0.72f;
        float GruntLaneSpeedScale = 0.68f;
        float BossLaneSpeedScale = 0.46f;
        float XApproachAcceleration = 24.0f;
        float XBrakeAcceleration = 20.0f;
        float LaneApproachAcceleration = 18.0f;
        float LaneBrakeAcceleration = 16.0f;

        float BearBossMoveSpeed = 3.75f;
        float BearBossAggroRange = 14.0f;
        float BearBossAttackRange = 1.72f;
        float BearBossPreferredRange = 1.08f;
        float BearBossAttackInterval = 0.82f;
        float BearBossLaneTolerance = 0.66f;
        float BearBossMidHealthThreshold = 0.74f;
        float BearBossLowHealthThreshold = 0.44f;
        float BearBossMidAttackInterval = 0.66f;
        float BearBossLowAttackInterval = 0.52f;
        float BearBossChargeDistance = 2.15f;
        float BearBossShockwaveDistance = 2.35f;
        float BearBossChargeSpeed = 7.20f;
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
        float LaneMaxY = -0.65f;
        float LaneSpeedScale = 0.72f;
        float LaneAcceleration = 28.0f;
        float SortScale = 0.015f;
        float ShadowMinAlpha = 0.34f;
        float ShadowMaxAlpha = 0.68f;
        float ShadowAirFadeHeight = 3.8f;
        glm::vec2 ShadowOffset = { 0.0f, -0.04f };
        glm::vec4 ShadowColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        float BossLaunchBonus = 1.35f;
        SideFeedbackTuning Feedback;
        SideAnimationSetTuning PlayerAnimations;
        SideAnimationSetTuning GruntAnimations;
        SideAnimationSetTuning BossAnimations;
        std::unordered_map<std::string, SideAttackTuning> Attacks;
        std::unordered_map<std::string, SideSkillDefinition> Skills;
        SideProgressionTuning Progression;
        // Consumable item slots (data-driven; see SideItemSlotTuning).
        std::vector<SideItemSlotTuning> ItemSlots;
        // Skill slots (data-driven trigger table; see SideSkillSlotTuning).
        std::vector<SideSkillSlotTuning> SkillSlots;
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
