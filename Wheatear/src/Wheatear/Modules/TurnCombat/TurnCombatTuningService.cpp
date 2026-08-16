#include "wtpch.h"
#include "TurnCombatTuningService.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"

#include <chrono>
#include <filesystem>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace Wheatear::TurnCombatTuningService {

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

        static TurnCombatTuning LoadTuning(const std::string& path)
        {
            TurnCombatTuning tuning;
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
                    tuning.Level.IntroDuration = level["introDuration"].as<float>(tuning.Level.IntroDuration);
                    tuning.Level.ActionDuration = level["actionDuration"].as<float>(tuning.Level.ActionDuration);
                    tuning.Level.VictoryReturnDelay = level["victoryReturnDelay"].as<float>(tuning.Level.VictoryReturnDelay);
                    tuning.Level.DefeatReturnDelay = level["defeatReturnDelay"].as<float>(tuning.Level.DefeatReturnDelay);
                }

                if (const YAML::Node formula = root["formula"])
                {
                    tuning.Formula.DefenseMultiplier = formula["defenseMultiplier"].as<float>(tuning.Formula.DefenseMultiplier);
                    tuning.Formula.MinDamage = formula["minDamage"].as<float>(tuning.Formula.MinDamage);
                }

                tuning.Loaded = true;
            }
            catch (const std::exception& e)
            {
                WT_CORE_WARN("TurnCombat tuning load failed for '{0}': {1}; using struct defaults only.",
                    path, e.what());
                tuning.Loaded = true;
            }

            return tuning;
        }

        struct TurnCombatTuningCacheEntry
        {
            TurnCombatTuning Tuning;
            std::filesystem::path ResolvedPath;
            std::filesystem::file_time_type WriteTime{};
            bool HasWriteTime = false;
            std::chrono::steady_clock::time_point LastChecked{};
        };

    } // namespace

    const TurnCombatTuning& GetTuning(const TurnCombatLevelComponent& level)
    {
        static std::unordered_map<std::string, TurnCombatTuningCacheEntry> cache;
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
            TurnCombatTuningCacheEntry entry;
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

    void ApplyLevelTuning(const TurnCombatTuning& tuning, TurnCombatLevelComponent& level)
    {
        if (!tuning.Loaded)
            return;

        level.StartFadeDuration = tuning.Level.StartFadeDuration;
        level.IntroDuration = tuning.Level.IntroDuration;
        level.ActionDuration = tuning.Level.ActionDuration;
        level.VictoryReturnDelay = tuning.Level.VictoryReturnDelay;
        level.DefeatReturnDelay = tuning.Level.DefeatReturnDelay;
    }

} // namespace Wheatear::TurnCombatTuningService
