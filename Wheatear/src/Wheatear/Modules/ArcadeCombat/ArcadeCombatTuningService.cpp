#include "wtpch.h"
#include "ArcadeCombatTuningService.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <chrono>
#include <filesystem>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace Wheatear::ArcadeCombatTuningService {

    namespace {

        static std::filesystem::path ResolveTuningPath(const std::string& path)
        {
            if (path.empty())
                return {};
            return AssetPath::ResolveRuntimeData(AssetAliasRegistry::Resolve(path));
        }

        static bool TryGetWriteTime(const std::filesystem::path& path,
            std::filesystem::file_time_type* writeTime)
        {
            if (!writeTime || path.empty())
                return false;

            std::error_code error;
            *writeTime = std::filesystem::last_write_time(path, error);
            return !error;
        }

        static glm::vec2 ReadVec2(const YAML::Node& node, const glm::vec2& fallback)
        {
            if (!node || !node.IsSequence() || node.size() < 2)
                return fallback;
            return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y) };
        }

        static glm::vec4 ReadVec4(const YAML::Node& node, const glm::vec4& fallback)
        {
            if (!node || !node.IsSequence() || node.size() < 4)
                return fallback;
            return {
                node[0].as<float>(fallback.r),
                node[1].as<float>(fallback.g),
                node[2].as<float>(fallback.b),
                node[3].as<float>(fallback.a)
            };
        }

        static ArcadeWeaponTuning ReadWeapon(const YAML::Node& node,
            const ArcadeWeaponTuning& fallback)
        {
            if (!node || !node.IsMap())
                return fallback;

            ArcadeWeaponTuning weapon;
            weapon.EntityName = node["entityName"].as<std::string>(fallback.EntityName);
            weapon.Speed = node["speed"].as<float>(fallback.Speed);
            weapon.Lifetime = node["lifetime"].as<float>(fallback.Lifetime);
            weapon.Radius = node["radius"].as<float>(fallback.Radius);
            weapon.Color = ReadVec4(node["color"], fallback.Color);
            weapon.MuzzleOffset = ReadVec2(node["muzzleOffset"], fallback.MuzzleOffset);
            weapon.Heavy = node["heavy"].as<bool>(fallback.Heavy);
            weapon.Melee = node["melee"].as<bool>(fallback.Melee);
            weapon.SlashOffset = node["slashOffset"].as<float>(fallback.SlashOffset);
            return weapon;
        }

        // Classic payloads; the tuning yaml overlays these per weapon so a
        // missing entry always preserves the historical behaviour.
        static void PrefillDefaultWeapons(std::unordered_map<std::string, ArcadeWeaponTuning>& weapons)
        {
            ArcadeWeaponTuning gun;
            gun.EntityName = "Arcade_PlayerBullet";
            gun.Speed = 9.0f;
            gun.Lifetime = 1.4f;
            gun.Radius = 0.13f;
            gun.Color = { 1.0f, 0.90f, 0.35f, 1.0f };
            gun.MuzzleOffset = { 0.55f, 0.05f };
            weapons["gun"] = gun;

            ArcadeWeaponTuning cannon;
            cannon.EntityName = "Arcade_PlayerCannon";
            cannon.Speed = 5.3f;
            cannon.Lifetime = 2.0f;
            cannon.Radius = 0.28f;
            cannon.Color = { 1.0f, 0.42f, 0.16f, 1.0f };
            cannon.MuzzleOffset = { 0.55f, 0.05f };
            cannon.Heavy = true;
            weapons["cannon"] = cannon;

            ArcadeWeaponTuning katana;
            katana.EntityName = "Arcade_KatanaSlash";
            katana.Speed = 1.0f;
            katana.Lifetime = 0.12f;
            katana.Radius = 0.75f;
            katana.Color = { 0.85f, 0.96f, 1.0f, 0.82f };
            katana.SlashOffset = 0.8f;
            katana.Melee = true;
            weapons["katana"] = katana;
        }

        static ArcadeCombatTuning LoadTuning(const std::string& path)
        {
            ArcadeCombatTuning tuning;
            PrefillDefaultWeapons(tuning.Player.Weapons);
            if (path.empty())
            {
                tuning.Loaded = true;
                return tuning;
            }

            try
            {
                const std::filesystem::path resolvedPath = ResolveTuningPath(path);
                const YAML::Node root = YAML::LoadFile(resolvedPath.string());

                if (const YAML::Node level = root["level"])
                {
                    tuning.Level.StartFadeDuration = level["startFadeDuration"].as<float>(tuning.Level.StartFadeDuration);
                    tuning.Level.VictoryReturnDelay = level["victoryReturnDelay"].as<float>(tuning.Level.VictoryReturnDelay);
                    tuning.Level.DefeatReturnDelay = level["defeatReturnDelay"].as<float>(tuning.Level.DefeatReturnDelay);
                    tuning.Level.ResultSceneFadeDuration = level["resultSceneFadeDuration"].as<float>(tuning.Level.ResultSceneFadeDuration);
                    tuning.Level.BossDefeatFadeDuration = level["bossDefeatFadeDuration"].as<float>(tuning.Level.BossDefeatFadeDuration);
                }

                if (const YAML::Node boss = root["boss"])
                {
                    tuning.Boss.IntroDuration = boss["introDuration"].as<float>(tuning.Boss.IntroDuration);
                    tuning.Boss.ShootInterval = boss["shootInterval"].as<float>(tuning.Boss.ShootInterval);
                    tuning.Boss.JumpInterval = boss["jumpInterval"].as<float>(tuning.Boss.JumpInterval);
                    tuning.Boss.JumpDuration = boss["jumpDuration"].as<float>(tuning.Boss.JumpDuration);

                    tuning.Boss.JumpXFrequency = boss["jumpXFrequency"].as<float>(tuning.Boss.JumpXFrequency);
                    tuning.Boss.JumpYFrequency = boss["jumpYFrequency"].as<float>(tuning.Boss.JumpYFrequency);
                    tuning.Boss.JumpXAmplitude = boss["jumpXAmplitude"].as<float>(tuning.Boss.JumpXAmplitude);
                    tuning.Boss.JumpYAmplitude = boss["jumpYAmplitude"].as<float>(tuning.Boss.JumpYAmplitude);
                    tuning.Boss.JumpYBase = boss["jumpYBase"].as<float>(tuning.Boss.JumpYBase);
                    tuning.Boss.JumpArcHeight = boss["jumpArcHeight"].as<float>(tuning.Boss.JumpArcHeight);
                    tuning.Boss.JumpMarginX = boss["jumpMarginX"].as<float>(tuning.Boss.JumpMarginX);
                    tuning.Boss.JumpMarginTop = boss["jumpMarginTop"].as<float>(tuning.Boss.JumpMarginTop);
                    tuning.Boss.JumpMarginBottom = boss["jumpMarginBottom"].as<float>(tuning.Boss.JumpMarginBottom);

                    tuning.Boss.BulletEntityName = boss["bulletEntityName"].as<std::string>(tuning.Boss.BulletEntityName);
                    tuning.Boss.BulletSpeed = boss["bulletSpeed"].as<float>(tuning.Boss.BulletSpeed);
                    tuning.Boss.BulletLifetime = boss["bulletLifetime"].as<float>(tuning.Boss.BulletLifetime);
                    tuning.Boss.BulletRadius = boss["bulletRadius"].as<float>(tuning.Boss.BulletRadius);
                    tuning.Boss.BulletColor = ReadVec4(boss["bulletColor"], tuning.Boss.BulletColor);
                    tuning.Boss.BulletSpawnOffset = ReadVec2(boss["bulletSpawnOffset"], tuning.Boss.BulletSpawnOffset);
                }

                if (const YAML::Node player = root["player"])
                {
                    tuning.Player.MoveSpeed = player["moveSpeed"].as<float>(tuning.Player.MoveSpeed);
                    tuning.Player.AutoAim = player["autoAim"].as<bool>(tuning.Player.AutoAim);
                    if (const YAML::Node weapons = player["weapons"])
                    {
                        if (weapons.IsMap())
                        {
                            for (const auto& entry : weapons)
                            {
                                if (!entry.first.IsScalar())
                                    continue;
                                const std::string weaponId = entry.first.as<std::string>();
                                const ArcadeWeaponTuning fallback =
                                    tuning.Player.Weapons.count(weaponId)
                                        ? tuning.Player.Weapons[weaponId]
                                        : ArcadeWeaponTuning{};
                                tuning.Player.Weapons[weaponId] = ReadWeapon(entry.second, fallback);
                            }
                        }
                    }
                }

                tuning.Loaded = true;
            }
            catch (const std::exception& e)
            {
                WT_CORE_WARN("ArcadeCombat tuning load failed for '{0}': {1}; using struct defaults only.",
                    path, e.what());
                tuning.Loaded = true;
            }

            return tuning;
        }

        struct ArcadeCombatTuningCacheEntry
        {
            ArcadeCombatTuning Tuning;
            std::filesystem::path ResolvedPath;
            std::filesystem::file_time_type WriteTime{};
            bool HasWriteTime = false;
            std::chrono::steady_clock::time_point LastChecked{};
        };

    } // namespace

    const ArcadeCombatTuning& GetTuning(const ArcadeCombatLevelComponent& level)
    {
        static std::unordered_map<std::string, ArcadeCombatTuningCacheEntry> cache;        static constexpr auto kCheckInterval = std::chrono::milliseconds(500);

        const std::string key = level.TuningPath.empty() ? "__default__" : level.TuningPath;
        const std::filesystem::path resolvedPath = ResolveTuningPath(level.TuningPath);
        auto it = cache.find(key);
        const auto now = std::chrono::steady_clock::now();
        if (it != cache.end() &&
            it->second.ResolvedPath == resolvedPath &&
            it->second.LastChecked.time_since_epoch().count() != 0 &&
            now - it->second.LastChecked < kCheckInterval)
        {
            return it->second.Tuning;
        }

        std::filesystem::file_time_type writeTime{};
        const bool hasWriteTime = TryGetWriteTime(resolvedPath, &writeTime);
        const bool shouldReload =
            it == cache.end() ||
            it->second.ResolvedPath != resolvedPath ||
            it->second.HasWriteTime != hasWriteTime ||
            (hasWriteTime && it->second.WriteTime != writeTime);

        if (shouldReload)
        {
            ArcadeCombatTuningCacheEntry entry;
            entry.Tuning = LoadTuning(level.TuningPath);
            entry.ResolvedPath = resolvedPath;
            entry.WriteTime = writeTime;
            entry.HasWriteTime = hasWriteTime;
            entry.LastChecked = now;
            it = cache.insert_or_assign(key, std::move(entry)).first;
        }
        else
        {
            it->second.LastChecked = now;
        }

        return it->second.Tuning;
    }

    const ArcadeWeaponTuning& GetWeaponTuning(const ArcadeCombatTuning& tuning,
        ArcadeWeaponType weapon)
    {
        static const ArcadeWeaponTuning kGun = [] {
            std::unordered_map<std::string, ArcadeWeaponTuning> defaults;
            PrefillDefaultWeapons(defaults);
            return defaults["gun"];
        }();
        static const ArcadeWeaponTuning kCannon = [] {
            std::unordered_map<std::string, ArcadeWeaponTuning> defaults;
            PrefillDefaultWeapons(defaults);
            return defaults["cannon"];
        }();
        static const ArcadeWeaponTuning kKatana = [] {
            std::unordered_map<std::string, ArcadeWeaponTuning> defaults;
            PrefillDefaultWeapons(defaults);
            return defaults["katana"];
        }();

        const char* id = weapon == ArcadeWeaponType::Cannon ? "cannon"
            : weapon == ArcadeWeaponType::Katana ? "katana" : "gun";
        if (auto it = tuning.Player.Weapons.find(id); it != tuning.Player.Weapons.end())
            return it->second;
        return weapon == ArcadeWeaponType::Cannon ? kCannon
            : weapon == ArcadeWeaponType::Katana ? kKatana : kGun;
    }

    void ApplyLevelTuning(const ArcadeCombatTuning& tuning, ArcadeCombatLevelComponent& level)
    {
        if (!tuning.Loaded)
            return;

        level.StartFadeDuration = tuning.Level.StartFadeDuration;
        level.VictoryReturnDelay = tuning.Level.VictoryReturnDelay;
        level.DefeatReturnDelay = tuning.Level.DefeatReturnDelay;
        level.ResultSceneFadeDuration = tuning.Level.ResultSceneFadeDuration;
        level.BossDefeatFadeDuration = tuning.Level.BossDefeatFadeDuration;
    }

    void ApplyBossTuning(const ArcadeCombatTuning& tuning,
        Scene* scene,
        const ArcadeCombatLevelComponent& level)
    {
        if (!tuning.Loaded || !scene)
            return;

        Entity boss = SceneQueries::FindEntityByName(scene, level.BossEntityName);
        if (!boss || !boss.HasComponent<ArcadeBossComponent>())
            return;

        auto& bossComponent = boss.GetComponent<ArcadeBossComponent>();
        bossComponent.IntroDuration = tuning.Boss.IntroDuration;
        bossComponent.ShootInterval = tuning.Boss.ShootInterval;
        bossComponent.JumpInterval = tuning.Boss.JumpInterval;
        bossComponent.JumpDuration = tuning.Boss.JumpDuration;
    }

    void ApplyPlayerTuning(const ArcadeCombatTuning& tuning,
        Scene* scene,
        const ArcadeCombatLevelComponent& level)
    {
        if (!tuning.Loaded || !scene)
            return;

        Entity player = SceneQueries::FindEntityByName(scene, level.PlayerEntityName);
        if (!player)
            return;

        if (player.HasComponent<ArcadeCombatantComponent>())
            player.GetComponent<ArcadeCombatantComponent>().MoveSpeed = tuning.Player.MoveSpeed;
        if (player.HasComponent<ArcadePlayerControllerComponent>())
            player.GetComponent<ArcadePlayerControllerComponent>().AutoAim = tuning.Player.AutoAim;
    }

} // namespace Wheatear::ArcadeCombatTuningService
