#include "wtpch.h"
#include "TacticalCombatTuningService.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"

#include <chrono>
#include <filesystem>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

namespace Wheatear::TacticalCombatTuningService {

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
            return {
                node[0].as<float>(fallback.x),
                node[1].as<float>(fallback.y)
            };
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

        static TacticalCombatTuning LoadTuning(const std::string& path)
        {
            TacticalCombatTuning tuning;
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
                    tuning.Level.EnemyStepDuration = level["enemyStepDuration"].as<float>(tuning.Level.EnemyStepDuration);
                    tuning.Level.VictoryReturnDelay = level["victoryReturnDelay"].as<float>(tuning.Level.VictoryReturnDelay);
                    tuning.Level.DefeatReturnDelay = level["defeatReturnDelay"].as<float>(tuning.Level.DefeatReturnDelay);
                    tuning.Level.GridWidth = level["gridWidth"].as<int>(tuning.Level.GridWidth);
                    tuning.Level.GridHeight = level["gridHeight"].as<int>(tuning.Level.GridHeight);
                    tuning.Level.BoardOrigin = ReadVec2(level["boardOrigin"], tuning.Level.BoardOrigin);
                    tuning.Level.CellSize = ReadVec2(level["cellSize"], tuning.Level.CellSize);
                    tuning.Level.TileNormalColor = ReadVec4(level["tileNormalColor"], tuning.Level.TileNormalColor);
                    tuning.Level.TileMoveColor = ReadVec4(level["tileMoveColor"], tuning.Level.TileMoveColor);
                    tuning.Level.TileAttackColor = ReadVec4(level["tileAttackColor"], tuning.Level.TileAttackColor);
                    tuning.Level.TileSelectedColor = ReadVec4(level["tileSelectedColor"], tuning.Level.TileSelectedColor);
                }

                if (const YAML::Node formula = root["formula"])
                {
                    tuning.Formula.MagicDefenseMultiplier = formula["magicDefenseMultiplier"].as<float>(tuning.Formula.MagicDefenseMultiplier);
                    tuning.Formula.MinDamage = formula["minDamage"].as<float>(tuning.Formula.MinDamage);
                }

                tuning.Loaded = true;
            }
            catch (const std::exception& e)
            {
                WT_CORE_WARN("TacticalCombat tuning load failed for '{0}': {1}; using struct defaults only.",
                    path, e.what());
                tuning.Loaded = true;
            }

            return tuning;
        }

        struct TacticalCombatTuningCacheEntry
        {
            TacticalCombatTuning Tuning;
            std::filesystem::path ResolvedPath;
            std::filesystem::file_time_type WriteTime{};
            bool HasWriteTime = false;
            std::chrono::steady_clock::time_point LastChecked{};
        };

    } // namespace

    const TacticalCombatTuning& GetTuning(const TacticalCombatLevelComponent& level)
    {
        static std::unordered_map<std::string, TacticalCombatTuningCacheEntry> cache;
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
            TacticalCombatTuningCacheEntry entry;
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

    void ApplyLevelTuning(const TacticalCombatTuning& tuning, TacticalCombatLevelComponent& level)
    {
        if (!tuning.Loaded)
            return;

        level.StartFadeDuration = tuning.Level.StartFadeDuration;
        level.IntroDuration = tuning.Level.IntroDuration;
        level.ActionDuration = tuning.Level.ActionDuration;
        level.EnemyStepDuration = tuning.Level.EnemyStepDuration;
        level.VictoryReturnDelay = tuning.Level.VictoryReturnDelay;
        level.DefeatReturnDelay = tuning.Level.DefeatReturnDelay;
        level.GridWidth = tuning.Level.GridWidth;
        level.GridHeight = tuning.Level.GridHeight;
        level.BoardOrigin = tuning.Level.BoardOrigin;
        level.CellSize = tuning.Level.CellSize;
        level.TileNormalColor = tuning.Level.TileNormalColor;
        level.TileMoveColor = tuning.Level.TileMoveColor;
        level.TileAttackColor = tuning.Level.TileAttackColor;
        level.TileSelectedColor = tuning.Level.TileSelectedColor;
    }

} // namespace Wheatear::TacticalCombatTuningService
