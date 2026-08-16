#pragma once

#include <string>

#include <glm/glm.hpp>

namespace Wheatear {

    enum class ArcadeTeam
    {
        Neutral = 0,
        Player = 1,
        Enemy = 2
    };

    enum class ArcadeWeaponType
    {
        Gun = 0,
        Cannon = 1,
        Katana = 2
    };

    enum class ArcadeTriggerType
    {
        BossIntro = 0
    };

    struct ArcadeCombatLevelComponent
    {
        bool        PlayOnStart = true;
        glm::vec2   ArenaMin = { -8.0f, -4.2f };
        glm::vec2   ArenaMax = { 8.0f, 4.2f };

        std::string PlayerEntityName = "Battle_Player";
        std::string BossEntityName = "Battle_Boss";
        std::string FadeEntityName = "Battle_Fade";
        std::string PausePanelEntityName = "Battle_PausePanel";
        std::string MessageTextEntityName = "Battle_Message";
        std::string WeaponTextEntityName = "Battle_WeaponText";
        std::string PlayerHealthBarEntityName = "Battle_PlayerHealth";
        std::string PlayerHealthTextEntityName = "Battle_PlayerHealthText";
        std::string BossHealthBarEntityName = "Battle_BossHealth";
        std::string BossHealthTextEntityName = "Battle_BossHealthText";

        float       StartFadeDuration = 0.8f;
        float       VictoryReturnDelay = 2.6f;
        float       DefeatReturnDelay = 2.2f;
        float       ResultSceneFadeDuration = 0.55f;
        float       BossDefeatFadeDuration = 1.15f;
        std::string VictorySceneCommand = "";
        std::string DefeatSceneCommand = "";

        // Global tuning data file (data-driven flow / boss / player feel).
        // Empty = the default project path. Edited from the editor's
        // "Arcade Combat Tuning Editor".
        std::string TuningPath = "assets/vertical_slice/data/arcade_combat_tuning.yaml";

        float       RuntimeElapsed = 0.0f;
        float       RuntimeFadeAlpha = 1.0f;
        bool        RuntimePaused = false;
        bool        RuntimeBossIntroStarted = false;
        bool        RuntimeBossIntroFinished = false;
        bool        RuntimeVictory = false;
        bool        RuntimeDefeat = false;
        float       RuntimeResultTimer = 0.0f;
        bool        RuntimeResultCommandIssued = false;
        std::string RuntimeRequestedCommand = "";

        ArcadeCombatLevelComponent() = default;
        ArcadeCombatLevelComponent(const ArcadeCombatLevelComponent&) = default;
    };

    struct ArcadeCombatantComponent
    {
        int         Team = (int)ArcadeTeam::Neutral;
        float       MaxHealth = 100.0f;
        float       Health = 100.0f;
        float       MoveSpeed = 4.2f;
        float       CollisionRadius = 0.45f;
        bool        Invulnerable = false;

        bool        Alive = true;
        bool        ControlsLocked = false;

        ArcadeCombatantComponent() = default;
        ArcadeCombatantComponent(const ArcadeCombatantComponent&) = default;
    };

    struct ArcadePlayerControllerComponent
    {
        ArcadeWeaponType CurrentWeapon = ArcadeWeaponType::Gun;
        bool             AutoAim = true;
        float            WeaponCooldown = 0.0f;

        ArcadePlayerControllerComponent() = default;
        ArcadePlayerControllerComponent(const ArcadePlayerControllerComponent&) = default;
    };

    struct ArcadeBossComponent
    {
        bool      Active = false;
        glm::vec3 IntroStartPosition = { 0.0f, 6.0f, -0.05f };
        glm::vec3 FightPosition = { 0.0f, 2.2f, -0.05f };
        float     IntroDuration = 1.25f;
        float     ShootInterval = 1.05f;
        float     JumpInterval = 2.75f;
        float     JumpDuration = 0.65f;

        float     RuntimeIntroTimer = 0.0f;
        float     RuntimeShootTimer = 0.0f;
        float     RuntimeJumpTimer = 0.0f;
        float     RuntimeJumpProgress = 0.0f;
        bool      RuntimeJumping = false;
        glm::vec3 RuntimeJumpStart = { 0.0f, 0.0f, 0.0f };
        glm::vec3 RuntimeJumpTarget = { 0.0f, 0.0f, 0.0f };

        ArcadeBossComponent() = default;
        ArcadeBossComponent(const ArcadeBossComponent&) = default;
    };

    struct ArcadeProjectileComponent
    {
        glm::vec2 Velocity = { 0.0f, 0.0f };
        float     Damage = 1.0f;
        float     Lifetime = 1.0f;
        float     Radius = 0.15f;
        int       Team = (int)ArcadeTeam::Player;
        bool      Heavy = false;
        bool      Melee = false;

        ArcadeProjectileComponent() = default;
        ArcadeProjectileComponent(const ArcadeProjectileComponent&) = default;
    };

    struct ArcadeCoverComponent
    {
        float Radius = 0.55f;
        float MaxHealth = 60.0f;
        float Health = 60.0f;
        bool  BlocksProjectiles = true;

        ArcadeCoverComponent() = default;
        ArcadeCoverComponent(const ArcadeCoverComponent&) = default;
    };

    struct ArcadeTriggerComponent
    {
        ArcadeTriggerType Type = ArcadeTriggerType::BossIntro;
        std::string       TriggerName = "BossIntro";
        float             Radius = 0.8f;
        bool              Triggered = false;

        ArcadeTriggerComponent() = default;
        ArcadeTriggerComponent(const ArcadeTriggerComponent&) = default;
    };

} // namespace Wheatear
