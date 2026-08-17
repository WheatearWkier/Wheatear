#include "wtpch.h"
#include "TacticalCombatTuningService.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Scene/Components/CoreComponents.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <chrono>
#include <filesystem>
#include <set>
#include <unordered_map>
#include <utility>

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

        static TacticalFrameTuning ReadFrameTuning(const YAML::Node& node)
        {
            TacticalFrameTuning frame;
            if (!node || !node.IsMap())
                return frame;
            frame.Sheet = node["sheet"].as<std::string>(frame.Sheet);
            frame.CellWidth = node["cellWidth"].as<int>(frame.CellWidth);
            frame.CellHeight = node["cellHeight"].as<int>(frame.CellHeight);
            frame.Columns = node["columns"].as<int>(frame.Columns);
            frame.StartFrame = node["startFrame"].as<int>(frame.StartFrame);
            frame.Count = node["count"].as<int>(frame.Count);
            return frame;
        }

        static void ReadUnitTuning(const YAML::Node& node, TacticalUnitTuning& unit)
        {
            unit.Tag = node["tag"].as<std::string>(unit.Tag);
            unit.Team = node["team"].as<int>(unit.Team);
            unit.Slot = node["slot"].as<int>(unit.Slot);

            // Grid is authored as "grid: [x, y]"; also accept legacy
            // "gridX"/"gridY" scalar keys.
            if (const YAML::Node grid = node["grid"])
            {
                if (grid.IsSequence() && grid.size() >= 2)
                {
                    unit.GridX = grid[0].as<int>(unit.GridX);
                    unit.GridY = grid[1].as<int>(unit.GridY);
                }
            }
            else
            {
                unit.GridX = node["gridX"].as<int>(unit.GridX);
                unit.GridY = node["gridY"].as<int>(unit.GridY);
            }

            unit.DisplayName = node["name"].as<std::string>(unit.DisplayName);
            unit.ClassName = node["class"].as<std::string>(unit.ClassName);
            unit.MaxHealth = node["maxHealth"].as<float>(unit.MaxHealth);
            unit.Attack = node["attack"].as<float>(unit.Attack);
            unit.Magic = node["magic"].as<float>(unit.Magic);
            unit.Defense = node["defense"].as<float>(unit.Defense);
            unit.MoveRange = node["move"].as<int>(unit.MoveRange);
            unit.AttackRange = node["range"].as<int>(unit.AttackRange);
            unit.Controllable = node["controllable"].as<bool>(unit.Controllable);
            unit.BasicSkillId = node["basic"].as<std::string>(unit.BasicSkillId);
            unit.Skill1Id = node["skill1"].as<std::string>(unit.Skill1Id);
            unit.Skill2Id = node["skill2"].as<std::string>(unit.Skill2Id);
            unit.IdleFrames = ReadFrameTuning(node["idleFrames"]);
            unit.AttackFrames = ReadFrameTuning(node["attackFrames"]);
            unit.HitFrames = ReadFrameTuning(node["hitFrames"]);
            unit.DownFrames = ReadFrameTuning(node["downFrames"]);
            unit.AnimationFrameRate = node["frameRate"].as<float>(unit.AnimationFrameRate);
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

                if (const YAML::Node units = root["units"])
                {
                    if (units.IsSequence())
                    {
                        for (const YAML::Node& unitNode : units)
                        {
                            TacticalUnitTuning unit;
                            ReadUnitTuning(unitNode, unit);
                            if (!unit.Tag.empty())
                                tuning.Units.push_back(std::move(unit));
                        }
                    }
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

    std::filesystem::path TuningSourcePath(const TacticalCombatLevelComponent& level)
    {
        return ResolveTuningPath(level.TuningPath);
    }

    bool IsFieldManagedByTuning(std::string_view fieldId)
    {
        return fieldId == "StartFadeDuration"
            || fieldId == "IntroDuration"
            || fieldId == "ActionDuration"
            || fieldId == "EnemyStepDuration"
            || fieldId == "VictoryReturnDelay"
            || fieldId == "DefeatReturnDelay"
            || fieldId == "GridWidth"
            || fieldId == "GridHeight"
            || fieldId == "BoardOrigin"
            || fieldId == "CellSize"
            || fieldId == "TileNormalColor"
            || fieldId == "TileMoveColor"
            || fieldId == "TileAttackColor"
            || fieldId == "TileSelectedColor";
    }

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

    size_t ApplyUnitTuningToScene(Scene* scene, const TacticalCombatTuning& tuning)
    {
        if (!scene || !tuning.Loaded || tuning.Units.empty())
            return 0;

        // Match tactical unit entity tags against the tuning table.
        std::unordered_map<std::string, const TacticalUnitTuning*> byTag;
        for (const TacticalUnitTuning& unit : tuning.Units)
            byTag[unit.Tag] = &unit;

        // Units must never overlap on the board: track occupied cells and
        // relocate colliding units (including the default 0,0) to the nearest
        // free cell via a deterministic spiral search.
        std::set<std::pair<int, int>> occupied;
        const auto findFreeGrid = [&occupied](int px, int py)
        {
            constexpr int kSearchLimit = 64;
            if (px < 0) px = 0;
            if (py < 0) py = 0;
            if (!occupied.count({ px, py }))
                return std::pair<int, int>{ px, py };
            for (int r = 1; r < kSearchLimit; ++r)
            {
                for (int dx = -r; dx <= r; ++dx)
                {
                    for (int dy = -r; dy <= r; ++dy)
                    {
                        if (std::max(std::abs(dx), std::abs(dy)) != r)
                            continue;
                        const int x = px + dx;
                        const int y = py + dy;
                        if (x < 0 || y < 0)
                            continue;
                        if (!occupied.count({ x, y }))
                            return std::pair<int, int>{ x, y };
                    }
                }
            }
            return std::pair<int, int>{ px, py };
        };

        constexpr const char* kUnitPrefix = SystemBindings::Tactical::UnitPrefix;
        const size_t kPrefixLength = std::char_traits<char>::length(kUnitPrefix);

        size_t applied = 0;
        size_t relocated = 0;
        auto& registry = scene->GetRegistry();
        for (auto entityID : registry.view<TacticalUnitComponent>())
        {
            Entity entity{ entityID, scene };
            if (!entity.HasComponent<TagComponent>())
                continue;

            const std::string& tag = entity.GetComponent<TagComponent>().Tag;
            if (tag.rfind(kUnitPrefix, 0) != 0 || tag.size() <= kPrefixLength)
                continue;

            const std::string unitTag = tag.substr(kPrefixLength);
            auto it = byTag.find(unitTag);
            if (it == byTag.end())
                continue;

            const TacticalUnitTuning& data = *it->second;
            auto& unit = entity.GetComponent<TacticalUnitComponent>();

            // Resolve the board cell, relocating on collision with an
            // already-placed unit.
            const auto cell = findFreeGrid(data.GridX, data.GridY);
            occupied.insert(cell);
            if (cell.first != data.GridX || cell.second != data.GridY)
                ++relocated;
            unit.Team = data.Team;
            unit.Slot = data.Slot;
            unit.GridX = cell.first;
            unit.GridY = cell.second;
            unit.DisplayName = data.DisplayName;
            unit.ClassName = data.ClassName;
            unit.MaxHealth = data.MaxHealth;
            unit.Health = data.MaxHealth;
            unit.Attack = data.Attack;
            unit.Magic = data.Magic;
            unit.Defense = data.Defense;
            unit.MoveRange = data.MoveRange;
            unit.AttackRange = data.AttackRange;
            unit.Controllable = data.Controllable;
            unit.BasicSkillId = data.BasicSkillId;
            unit.Skill1Id = data.Skill1Id;
            unit.Skill2Id = data.Skill2Id;

            unit.IdleFrameAtlas.SheetPath = data.IdleFrames.Sheet;
            unit.IdleFrameAtlas.CellWidth = data.IdleFrames.CellWidth;
            unit.IdleFrameAtlas.CellHeight = data.IdleFrames.CellHeight;
            unit.IdleFrameAtlas.Columns = data.IdleFrames.Columns;
            unit.IdleFrameAtlas.StartFrame = data.IdleFrames.StartFrame;
            unit.IdleFrameCount = data.IdleFrames.Count;

            unit.AttackFrameAtlas.SheetPath = data.AttackFrames.Sheet;
            unit.AttackFrameAtlas.CellWidth = data.AttackFrames.CellWidth;
            unit.AttackFrameAtlas.CellHeight = data.AttackFrames.CellHeight;
            unit.AttackFrameAtlas.Columns = data.AttackFrames.Columns;
            unit.AttackFrameAtlas.StartFrame = data.AttackFrames.StartFrame;
            unit.AttackFrameCount = data.AttackFrames.Count;

            unit.HitFrameAtlas.SheetPath = data.HitFrames.Sheet;
            unit.HitFrameAtlas.CellWidth = data.HitFrames.CellWidth;
            unit.HitFrameAtlas.CellHeight = data.HitFrames.CellHeight;
            unit.HitFrameAtlas.Columns = data.HitFrames.Columns;
            unit.HitFrameAtlas.StartFrame = data.HitFrames.StartFrame;
            unit.HitFrameCount = data.HitFrames.Count;

            unit.DownFrameAtlas.SheetPath = data.DownFrames.Sheet;
            unit.DownFrameAtlas.CellWidth = data.DownFrames.CellWidth;
            unit.DownFrameAtlas.CellHeight = data.DownFrames.CellHeight;
            unit.DownFrameAtlas.Columns = data.DownFrames.Columns;
            unit.DownFrameAtlas.StartFrame = data.DownFrames.StartFrame;
            unit.DownFrameCount = data.DownFrames.Count;

            unit.AnimationFrameRate = data.AnimationFrameRate;

            ++applied;
        }

        if (applied > 0)
        {
            WT_CORE_INFO("TacticalCombatTuningService: applied {0} unit(s) from tuning table ({1} relocated to avoid overlap).",
                applied, relocated);
        }
        return applied;
    }

} // namespace Wheatear::TacticalCombatTuningService
