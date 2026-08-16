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

        static ArcadeCombatTuning LoadTuning(const std::string& path)
        {
            ArcadeCombatTuning tuning;
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
                }

                if (const YAML::Node player = root["player"])
                {
                    tuning.Player.MoveSpeed = player["moveSpeed"].as<float>(tuning.Player.MoveSpeed);
                    tuning.Player.AutoAim = player["autoAim"].as<bool>(tuning.Player.AutoAim);
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
        static std::unordered_map<std::string, ArcadeCombatTuningCacheEntry> cache;
        static constexpr auto kCheckInterval = std::chrono::milliseconds(500);

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
