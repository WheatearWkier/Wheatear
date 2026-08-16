#pragma once

#include "ArcadeCombatComponents.h"
#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <string>
#include <unordered_map>

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

        // Jump target geometry (sin/cos Lissajous path inside the arena).
        float JumpXFrequency = 1.73f;
        float JumpYFrequency = 1.11f;
        float JumpXAmplitude = 5.4f;
        float JumpYAmplitude = 0.65f;
        float JumpYBase = 2.0f;
        float JumpArcHeight = 0.85f;
        float JumpMarginX = 1.2f;
        float JumpMarginTop = 1.2f;
        float JumpMarginBottom = 0.9f;

        // Boss bullet payload.
        std::string BulletEntityName = "Arcade_BossBullet";
        float BulletSpeed = 4.2f;
        float BulletLifetime = 2.4f;
        float BulletRadius = 0.19f;
        glm::vec4 BulletColor = { 0.95f, 0.22f, 0.34f, 1.0f };
        glm::vec2 BulletSpawnOffset = { 0.65f, -0.1f };
    };

    // Per-weapon projectile payload (geometry only; cooldown/damage come
    // from the WAO recipe of the weapon).
    struct ArcadeWeaponTuning
    {
        std::string EntityName;
        float Speed = 0.0f;
        float Lifetime = 0.0f;
        float Radius = 0.0f;
        glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec2 MuzzleOffset = { 0.0f, 0.0f };
        bool Heavy = false;
        bool Melee = false;
        float SlashOffset = 0.0f; // melee spawn offset along the aim direction
    };

    struct ArcadePlayerTuning
    {
        float MoveSpeed = 4.2f;
        bool  AutoAim = true;
        // gun / cannon / katana; pre-filled with the classic payloads so a
        // missing entry always falls back to the historical behaviour.
        std::unordered_map<std::string, ArcadeWeaponTuning> Weapons;
    };

    struct ArcadeCombatTuning
    {
        bool Loaded = false;
        ArcadeLevelTuning Level;
        ArcadeBossTuning Boss;
        ArcadePlayerTuning Player;
    };

    WHEATEAR_API const ArcadeCombatTuning& GetTuning(const ArcadeCombatLevelComponent& level);

    // Weapon payload for the given weapon type; falls back to the baked
    // default when the tuning table has no entry for it.
    WHEATEAR_API const ArcadeWeaponTuning& GetWeaponTuning(const ArcadeCombatTuning& tuning,
        ArcadeWeaponType weapon);

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
