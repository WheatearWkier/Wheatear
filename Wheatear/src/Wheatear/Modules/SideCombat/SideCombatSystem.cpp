#include "wtpch.h"
#include "SideCombatSystem.h"

#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/KeyCodes.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <yaml-cpp/yaml.h>

namespace Wheatear {

    namespace {

        constexpr float GravityDefault = 22.0f;
        constexpr float DefaultLaneMinY = -3.55f;
        constexpr float DefaultLaneMaxY = -1.30f;

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
            float HitStun = 0.30f;
            float AttackerAirImpulse = 0.0f;
            float AttackerAirFallStep = 0.0f;
            float TargetAirFallStep = 0.0f;
            float ProtectionGain = 0.0f;
            bool DestroyOnHit = true;
            std::string TextureFramePattern;
            int TextureFrameCount = 1;
            float TextureFrameRate = 16.0f;
        };

        struct SidePlayerTuning
        {
            float MoveSpeed = 5.4f;
            int MaxJumps = 1;
            float JumpImpulse = 7.8f;
            float Gravity = 20.0f;
            float AirControl = 13.0f;
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
            float LaneMinY = DefaultLaneMinY;
            float LaneMaxY = DefaultLaneMaxY;
            float LaneSpeedScale = 0.72f;
            float LaneAcceleration = 28.0f;
            float SortScale = 0.015f;
            float ShadowMinAlpha = 0.22f;
            float ShadowMaxAlpha = 0.46f;
            float ShadowAirFadeHeight = 3.8f;
            float BossLaunchBonus = 1.35f;
            std::unordered_map<std::string, SideAttackTuning> Attacks;
            std::unordered_map<std::string, SideSkillDefinition> Skills;
            SideProgressionTuning Progression;
        };

        static Entity FindEntityByName(Scene* scene, const std::string& name)
        {
            if (!scene || name.empty())
                return {};

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TagComponent>())
            {
                if (registry.get<TagComponent>(e).Tag == name)
                    return { e, scene };
            }
            return {};
        }

        static float SignNonZero(float value)
        {
            return value < 0.0f ? -1.0f : 1.0f;
        }

        static float Approach(float value, float target, float delta)
        {
            if (value < target)
                return std::min(value + delta, target);
            return std::max(value - delta, target);
        }

        static glm::vec2 ToVec2(const glm::vec3& value)
        {
            return { value.x, value.y };
        }

        static float CalculateSortZ(float groundY, const SideCombatTuning& tuning)
        {
            return -0.08f + (tuning.LaneMaxY - groundY) * tuning.SortScale;
        }

        static std::string FormatFramePath(const std::string& pattern, int frame)
        {
            if (pattern.empty())
                return {};

            const size_t marker = pattern.find("{frame}");
            if (marker == std::string::npos)
                return pattern;

            std::ostringstream index;
            index << std::setw(2) << std::setfill('0') << std::max(1, frame);
            std::string result = pattern;
            result.replace(marker, 7, index.str());
            return result;
        }

        static std::filesystem::path FindLooseTuningPath(const std::filesystem::path& path)
        {
            if (path.empty() || path.is_absolute())
                return path;

            std::error_code error;
            std::filesystem::path cursor = std::filesystem::current_path(error);
            while (!error && !cursor.empty())
            {
                const std::filesystem::path candidate = (cursor / path).lexically_normal();
                if (std::filesystem::is_regular_file(candidate, error) && !error)
                    return candidate;

                error.clear();
                const std::filesystem::path parent = cursor.parent_path();
                if (parent == cursor)
                    break;
                cursor = parent;
            }

            return {};
        }

        static std::filesystem::path ResolveTuningPath(const std::string& path)
        {
            if (path.empty())
                return {};

            const std::filesystem::path requested(path);
            const std::filesystem::path loosePath = FindLooseTuningPath(requested);
            if (!loosePath.empty())
                return loosePath;

            return AssetPath::Resolve(requested);
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

        static std::vector<std::string> ReadStringList(const YAML::Node& node, const std::vector<std::string>& fallback = {})
        {
            if (!node || !node.IsSequence())
                return fallback;

            std::vector<std::string> values;
            values.reserve(node.size());
            for (const auto& item : node)
            {
                const std::string value = item.as<std::string>("");
                if (!value.empty())
                    values.push_back(value);
            }
            return values;
        }

        static SideAttackTuning ReadAttackTuning(const YAML::Node& node, const SideAttackTuning& fallback)
        {
            SideAttackTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.Size = ReadVec2(node["size"], tuning.Size);
            tuning.Offset = ReadVec2(node["offset"], tuning.Offset);
            tuning.Velocity = ReadVec2(node["velocity"], tuning.Velocity);
            tuning.LaunchVelocity = ReadVec2(node["launchVelocity"], tuning.LaunchVelocity);
            tuning.AirHeight = node["airHeight"].as<float>(tuning.AirHeight);
            tuning.AirRange = node["airRange"].as<float>(tuning.AirRange);
            tuning.DamageScale = node["damageScale"].as<float>(tuning.DamageScale);
            tuning.DamageFlat = node["damageFlat"].as<float>(tuning.DamageFlat);
            tuning.Lifetime = node["lifetime"].as<float>(tuning.Lifetime);
            tuning.HitStun = node["hitStun"].as<float>(tuning.HitStun);
            tuning.AttackerAirImpulse = node["attackerAirImpulse"].as<float>(tuning.AttackerAirImpulse);
            tuning.AttackerAirFallStep = node["attackerAirFallStep"].as<float>(tuning.AttackerAirFallStep);
            tuning.TargetAirFallStep = node["targetAirFallStep"].as<float>(tuning.TargetAirFallStep);
            tuning.ProtectionGain = node["protectionGain"].as<float>(tuning.ProtectionGain);
            tuning.DestroyOnHit = node["destroyOnHit"].as<bool>(tuning.DestroyOnHit);
            tuning.TextureFramePattern = node["textureFramePattern"].as<std::string>(tuning.TextureFramePattern);
            tuning.TextureFrameCount = node["textureFrameCount"].as<int>(tuning.TextureFrameCount);
            tuning.TextureFrameRate = node["textureFrameRate"].as<float>(tuning.TextureFrameRate);
            return tuning;
        }

        static SidePlayerTuning ReadPlayerTuning(const YAML::Node& node, const SidePlayerTuning& fallback)
        {
            SidePlayerTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.MoveSpeed = node["moveSpeed"].as<float>(tuning.MoveSpeed);
            tuning.MaxJumps = node["maxJumps"].as<int>(tuning.MaxJumps);
            tuning.JumpImpulse = node["jumpImpulse"].as<float>(tuning.JumpImpulse);
            tuning.Gravity = node["gravity"].as<float>(tuning.Gravity);
            tuning.AirControl = node["airControl"].as<float>(tuning.AirControl);
            tuning.LaneSpeedScale = node["laneSpeedScale"].as<float>(tuning.LaneSpeedScale);
            tuning.LaneAcceleration = node["laneAcceleration"].as<float>(tuning.LaneAcceleration);
            tuning.GroundAcceleration = node["groundAcceleration"].as<float>(tuning.GroundAcceleration);
            tuning.GroundFriction = node["groundFriction"].as<float>(tuning.GroundFriction);
            tuning.BasicCooldown = node["basicCooldown"].as<float>(tuning.BasicCooldown);
            tuning.BasicFinisherExtraCooldown = node["basicFinisherExtraCooldown"].as<float>(tuning.BasicFinisherExtraCooldown);
            tuning.LauncherCooldown = node["launcherCooldown"].as<float>(tuning.LauncherCooldown);
            tuning.MagicBoltCooldown = node["magicBoltCooldown"].as<float>(tuning.MagicBoltCooldown);
            tuning.AllySupportCooldown = node["allySupportCooldown"].as<float>(tuning.AllySupportCooldown);
            tuning.BasicChainWindow = node["basicChainWindow"].as<float>(tuning.BasicChainWindow);
            tuning.LauncherChainWindow = node["launcherChainWindow"].as<float>(tuning.LauncherChainWindow);
            tuning.MagicChainWindow = node["magicChainWindow"].as<float>(tuning.MagicChainWindow);
            tuning.SupportChainWindow = node["supportChainWindow"].as<float>(tuning.SupportChainWindow);
            return tuning;
        }

        static SideCombatRuleTuning ReadCombatRuleTuning(const YAML::Node& node, const SideCombatRuleTuning& fallback)
        {
            SideCombatRuleTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.ComboDropDelay = node["comboDropDelay"].as<float>(tuning.ComboDropDelay);
            tuning.HitInvulnerableTime = node["hitInvulnerableTime"].as<float>(tuning.HitInvulnerableTime);
            tuning.DefenseBase = node["defenseBase"].as<float>(tuning.DefenseBase);
            tuning.MinDamage = node["minDamage"].as<float>(tuning.MinDamage);
            return tuning;
        }

        static SideAirComboTuning ReadAirComboTuning(const YAML::Node& node, const SideAirComboTuning& fallback)
        {
            SideAirComboTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.AirActionLimit = node["airActionLimit"].as<int>(tuning.AirActionLimit);
            tuning.AirActionLimitAfterBreak = node["airActionLimitAfterBreak"].as<int>(tuning.AirActionLimitAfterBreak);
            tuning.BreakLimitEnabled = node["breakLimitEnabled"].as<bool>(tuning.BreakLimitEnabled);
            tuning.BreakLimitDebugKeyEnabled = node["breakLimitDebugKeyEnabled"].as<bool>(tuning.BreakLimitDebugKeyEnabled);
            tuning.ShowBreakLimitHint = node["showBreakLimitHint"].as<bool>(tuning.ShowBreakLimitHint);
            tuning.AirBasicCooldown = node["airBasicCooldown"].as<float>(tuning.AirBasicCooldown);
            tuning.AirChaseCooldown = node["airChaseCooldown"].as<float>(tuning.AirChaseCooldown);
            tuning.BreakLimitCooldown = node["breakLimitCooldown"].as<float>(tuning.BreakLimitCooldown);
            tuning.BreakLimitMinCombo = node["breakLimitMinCombo"].as<int>(tuning.BreakLimitMinCombo);
            tuning.BreakLimitMaxHeight = node["breakLimitMaxHeight"].as<float>(tuning.BreakLimitMaxHeight);
            tuning.BreakLimitFallingVelocity = node["breakLimitFallingVelocity"].as<float>(tuning.BreakLimitFallingVelocity);
            tuning.BreakLimitHangImpulse = node["breakLimitHangImpulse"].as<float>(tuning.BreakLimitHangImpulse);
            tuning.BreakLimitHeightBoost = node["breakLimitHeightBoost"].as<float>(tuning.BreakLimitHeightBoost);
            tuning.BreakLimitGaugeCost = node["breakLimitGaugeCost"].as<float>(tuning.BreakLimitGaugeCost);
            tuning.MagicSwordGaugeMax = node["magicSwordGaugeMax"].as<float>(tuning.MagicSwordGaugeMax);
            tuning.GaugeGainGroundHit = node["gaugeGainGroundHit"].as<float>(tuning.GaugeGainGroundHit);
            tuning.GaugeGainAirHit = node["gaugeGainAirHit"].as<float>(tuning.GaugeGainAirHit);
            tuning.GroundThreatHeight = node["groundThreatHeight"].as<float>(tuning.GroundThreatHeight);
            tuning.HighAirSafetyHeight = node["highAirSafetyHeight"].as<float>(tuning.HighAirSafetyHeight);
            return tuning;
        }

        static SideProtectionTuning ReadProtectionTuning(const YAML::Node& node, const SideProtectionTuning& fallback)
        {
            SideProtectionTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.BossProtectionMax = node["bossProtectionMax"].as<float>(tuning.BossProtectionMax);
            tuning.BossProtectionDecayPerSecond = node["bossProtectionDecayPerSecond"].as<float>(tuning.BossProtectionDecayPerSecond);
            tuning.BossProtectionLimitTime = node["bossProtectionLimitTime"].as<float>(tuning.BossProtectionLimitTime);
            tuning.BossProtectionForceFallVelocity = node["bossProtectionForceFallVelocity"].as<float>(tuning.BossProtectionForceFallVelocity);
            tuning.BossProtectionBreakLimitThreshold = node["bossProtectionBreakLimitThreshold"].as<float>(tuning.BossProtectionBreakLimitThreshold);
            tuning.BreakLimitProtectionReduce = node["breakLimitProtectionReduce"].as<float>(tuning.BreakLimitProtectionReduce);
            tuning.GroundResetDelay = node["groundResetDelay"].as<float>(tuning.GroundResetDelay);
            tuning.ShowBossProtectionHud = node["showBossProtectionHud"].as<bool>(tuning.ShowBossProtectionHud);
            tuning.ShowCombatStateHud = node["showCombatStateHud"].as<bool>(tuning.ShowCombatStateHud);
            return tuning;
        }

        static SideSkillDefinition ReadSkillDefinition(const YAML::Node& node, const SideSkillDefinition& fallback)
        {
            SideSkillDefinition definition = fallback;
            if (!node)
                return definition;

            definition.DisplayName = node["displayName"].as<std::string>(definition.DisplayName);
            definition.InputLabel = node["input"].as<std::string>(definition.InputLabel);
            definition.ComboRole = node["comboRole"].as<std::string>(definition.ComboRole);
            definition.AttackIds = ReadStringList(node["attackIds"], definition.AttackIds);
            definition.UnlockChapter = node["unlockChapter"].as<int>(definition.UnlockChapter);
            definition.CoreMove = node["coreMove"].as<bool>(definition.CoreMove);
            return definition;
        }

        static std::unordered_map<std::string, SideSkillDefinition> ReadSkillDefinitions(
            const YAML::Node& node,
            const std::unordered_map<std::string, SideSkillDefinition>& fallback)
        {
            auto definitions = fallback;
            if (!node || !node.IsMap())
                return definitions;

            for (auto it = node.begin(); it != node.end(); ++it)
            {
                const std::string id = it->first.as<std::string>("");
                if (id.empty())
                    continue;

                const auto fallbackIt = definitions.find(id);
                const SideSkillDefinition fallbackDefinition = fallbackIt != definitions.end()
                    ? fallbackIt->second
                    : SideSkillDefinition{};
                definitions[id] = ReadSkillDefinition(it->second, fallbackDefinition);
            }

            return definitions;
        }

        static SideUnlockProfile ReadUnlockProfile(
            const std::string& id,
            const YAML::Node& node,
            const SideUnlockProfile& fallback)
        {
            SideUnlockProfile profile = fallback;
            profile.Id = id.empty() ? profile.Id : id;
            if (!node)
                return profile;

            profile.DisplayName = node["displayName"].as<std::string>(profile.DisplayName);
            profile.Chapter = node["chapter"].as<int>(profile.Chapter);
            profile.UnlockedSkills = ReadStringList(node["unlockedSkills"], profile.UnlockedSkills);
            profile.DebugSkills = ReadStringList(node["debugSkills"], profile.DebugSkills);

            if (YAML::Node visible = node["visibleSystems"])
            {
                profile.ShowBossProtectionHud = visible["bossProtectionHud"].as<bool>(profile.ShowBossProtectionHud);
                profile.ShowCombatStateHud = visible["combatStateHud"].as<bool>(profile.ShowCombatStateHud);
                profile.ShowBreakLimitHint = visible["breakLimitHint"].as<bool>(profile.ShowBreakLimitHint);
            }

            return profile;
        }

        static SideProgressionTuning ReadProgressionTuning(const YAML::Node& node, const SideProgressionTuning& fallback)
        {
            SideProgressionTuning progression = fallback;
            if (!node)
                return progression;

            progression.DefaultProfileId = node["defaultProfile"].as<std::string>(progression.DefaultProfileId);
            progression.DefaultProfileId = node["defaultProfileId"].as<std::string>(progression.DefaultProfileId);

            if (YAML::Node profiles = node["profiles"])
            {
                for (auto it = profiles.begin(); it != profiles.end(); ++it)
                {
                    const std::string id = it->first.as<std::string>("");
                    if (id.empty())
                        continue;

                    const auto fallbackIt = progression.Profiles.find(id);
                    SideUnlockProfile fallbackProfile;
                    if (fallbackIt != progression.Profiles.end())
                        fallbackProfile = fallbackIt->second;
                    else
                        fallbackProfile.Id = id;
                    progression.Profiles[id] = ReadUnlockProfile(id, it->second, fallbackProfile);
                }
            }

            return progression;
        }

        static SideEnemyTuning ReadEnemyTuning(const YAML::Node& node, const SideEnemyTuning& fallback)
        {
            SideEnemyTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.InitialAttackDelay = node["initialAttackDelay"].as<float>(tuning.InitialAttackDelay);
            tuning.AttackRangePadding = node["attackRangePadding"].as<float>(tuning.AttackRangePadding);
            tuning.LaneAttackPadding = node["laneAttackPadding"].as<float>(tuning.LaneAttackPadding);
            tuning.BossPreferredRangeBonus = node["bossPreferredRangeBonus"].as<float>(tuning.BossPreferredRangeBonus);
            tuning.GruntMoveSpeedScale = node["gruntMoveSpeedScale"].as<float>(tuning.GruntMoveSpeedScale);
            tuning.BossMoveSpeedScale = node["bossMoveSpeedScale"].as<float>(tuning.BossMoveSpeedScale);
            tuning.GruntLaneSpeedScale = node["gruntLaneSpeedScale"].as<float>(tuning.GruntLaneSpeedScale);
            tuning.BossLaneSpeedScale = node["bossLaneSpeedScale"].as<float>(tuning.BossLaneSpeedScale);
            tuning.XApproachAcceleration = node["xApproachAcceleration"].as<float>(tuning.XApproachAcceleration);
            tuning.XBrakeAcceleration = node["xBrakeAcceleration"].as<float>(tuning.XBrakeAcceleration);
            tuning.LaneApproachAcceleration = node["laneApproachAcceleration"].as<float>(tuning.LaneApproachAcceleration);
            tuning.LaneBrakeAcceleration = node["laneBrakeAcceleration"].as<float>(tuning.LaneBrakeAcceleration);

            if (YAML::Node bearBoss = node["bearBoss"])
            {
                tuning.BearBossMoveSpeed = bearBoss["moveSpeed"].as<float>(tuning.BearBossMoveSpeed);
                tuning.BearBossAggroRange = bearBoss["aggroRange"].as<float>(tuning.BearBossAggroRange);
                tuning.BearBossAttackRange = bearBoss["attackRange"].as<float>(tuning.BearBossAttackRange);
                tuning.BearBossPreferredRange = bearBoss["preferredRange"].as<float>(tuning.BearBossPreferredRange);
                tuning.BearBossAttackInterval = bearBoss["attackInterval"].as<float>(tuning.BearBossAttackInterval);
                tuning.BearBossLaneTolerance = bearBoss["laneTolerance"].as<float>(tuning.BearBossLaneTolerance);
                tuning.BearBossMidHealthThreshold = bearBoss["midHealthThreshold"].as<float>(tuning.BearBossMidHealthThreshold);
                tuning.BearBossLowHealthThreshold = bearBoss["lowHealthThreshold"].as<float>(tuning.BearBossLowHealthThreshold);
                tuning.BearBossMidAttackInterval = bearBoss["midAttackInterval"].as<float>(tuning.BearBossMidAttackInterval);
                tuning.BearBossLowAttackInterval = bearBoss["lowAttackInterval"].as<float>(tuning.BearBossLowAttackInterval);
                tuning.BearBossChargeDistance = bearBoss["chargeDistance"].as<float>(tuning.BearBossChargeDistance);
                tuning.BearBossShockwaveDistance = bearBoss["shockwaveDistance"].as<float>(tuning.BearBossShockwaveDistance);
                tuning.BearBossChargeSpeed = bearBoss["chargeSpeed"].as<float>(tuning.BearBossChargeSpeed);
            }

            return tuning;
        }

        static SidePickupTuning ReadPickupTuning(const YAML::Node& node, const SidePickupTuning& fallback)
        {
            SidePickupTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.PickupRadius = node["pickupRadius"].as<float>(tuning.PickupRadius);
            tuning.AttractRadius = node["attractRadius"].as<float>(tuning.AttractRadius);
            tuning.AttractSpeed = node["attractSpeed"].as<float>(tuning.AttractSpeed);
            return tuning;
        }

        static void AddDefaultSkill(
            SideCombatTuning& tuning,
            const std::string& id,
            const std::string& displayName,
            const std::string& inputLabel,
            const std::string& comboRole,
            std::vector<std::string> attackIds,
            int unlockChapter,
            bool coreMove)
        {
            SideSkillDefinition definition;
            definition.DisplayName = displayName;
            definition.InputLabel = inputLabel;
            definition.ComboRole = comboRole;
            definition.AttackIds = std::move(attackIds);
            definition.UnlockChapter = unlockChapter;
            definition.CoreMove = coreMove;
            tuning.Skills[id] = std::move(definition);
        }

        static void AddDefaultUnlockProfile(
            SideCombatTuning& tuning,
            const std::string& id,
            const std::string& displayName,
            int chapter,
            std::vector<std::string> unlockedSkills,
            std::vector<std::string> debugSkills,
            bool showBossProtectionHud,
            bool showCombatStateHud,
            bool showBreakLimitHint)
        {
            SideUnlockProfile profile;
            profile.Id = id;
            profile.DisplayName = displayName;
            profile.Chapter = chapter;
            profile.UnlockedSkills = std::move(unlockedSkills);
            profile.DebugSkills = std::move(debugSkills);
            profile.ShowBossProtectionHud = showBossProtectionHud;
            profile.ShowCombatStateHud = showCombatStateHud;
            profile.ShowBreakLimitHint = showBreakLimitHint;
            tuning.Progression.Profiles[id] = std::move(profile);
        }

        static void AddDefaultProgressionData(SideCombatTuning& tuning)
        {
            AddDefaultSkill(tuning, "basic_attack", "三段斩", "J", "地面起手 / 基础续连", { "basic1", "basic2", "basic3" }, 2, true);
            AddDefaultSkill(tuning, "air_basic", "跳斩", "空中 J", "前期默认空中续连", { "air_basic" }, 2, true);
            AddDefaultSkill(tuning, "launcher", "裂空上挑", "K", "地面浮空起手", { "launcher" }, 2, true);
            AddDefaultSkill(tuning, "air_chase", "空中追斩", "空中 K", "空中续连 / 低空补救", { "air_chase" }, 2, true);
            AddDefaultSkill(tuning, "magic_bolt", "火球术", "U", "远程补 hit / 空中魔法续连", { "magic_bolt" }, 2, true);
            AddDefaultSkill(tuning, "ally_support", "真青梅支援", "I", "支援浮空 / 新手容错", { "ally_support" }, 2, true);
            AddDefaultSkill(tuning, "break_limit", "断限追击", "L", "Boss 保护临界时刷新空中行动", { "break_limit" }, 7, false);

            const std::vector<std::string> chapterTwoSkills = {
                "basic_attack",
                "air_basic",
                "launcher",
                "air_chase",
                "magic_bolt",
                "ally_support"
            };
            AddDefaultUnlockProfile(tuning,
                "CH02_MAIN_BearAwakening",
                "第二章：魔剑觉醒",
                2,
                chapterTwoSkills,
                { "break_limit" },
                false,
                false,
                false);
            AddDefaultUnlockProfile(tuning,
                "CH02_MAT_BeastPath",
                "第二章：黑林兽道",
                2,
                chapterTwoSkills,
                { "break_limit" },
                false,
                false,
                false);
            AddDefaultUnlockProfile(tuning,
                "CH06_MAIN_MageLair",
                "第六章：魔法师老巢",
                6,
                chapterTwoSkills,
                {},
                true,
                false,
                false);

            std::vector<std::string> chapterSevenSkills = chapterTwoSkills;
            chapterSevenSkills.push_back("break_limit");
            AddDefaultUnlockProfile(tuning,
                "CH07_MAIN_PalaceAssault",
                "第七章：进攻王宫",
                7,
                chapterSevenSkills,
                {},
                true,
                false,
                true);
        }

        static SideCombatTuning BuildDefaultTuning()
        {
            SideCombatTuning tuning;
            AddDefaultProgressionData(tuning);

            SideAttackTuning basic1;
            basic1.Size = { 1.30f, 0.70f };
            basic1.Offset = { 0.88f, 0.0f };
            basic1.AirHeight = 0.58f;
            basic1.AirRange = 1.16f;
            basic1.DamageScale = 0.70f;
            basic1.DamageFlat = 10.0f;
            basic1.Lifetime = 0.15f;
            basic1.HitStun = 0.30f;
            basic1.LaunchVelocity = { 1.15f, 1.8f };
            basic1.ProtectionGain = 4.0f;
            tuning.Attacks["basic1"] = basic1;

            SideAttackTuning basic2 = basic1;
            basic2.Size = { 1.42f, 0.74f };
            basic2.Offset = { 0.94f, 0.02f };
            basic2.DamageScale = 0.78f;
            basic2.LaunchVelocity = { 1.35f, 2.2f };
            basic2.ProtectionGain = 5.0f;
            tuning.Attacks["basic2"] = basic2;

            SideAttackTuning basic3 = basic1;
            basic3.Size = { 1.58f, 0.86f };
            basic3.Offset = { 1.02f, 0.02f };
            basic3.AirHeight = 0.70f;
            basic3.AirRange = 1.35f;
            basic3.DamageScale = 0.96f;
            basic3.DamageFlat = 13.0f;
            basic3.Lifetime = 0.17f;
            basic3.HitStun = 0.36f;
            basic3.LaunchVelocity = { 2.2f, 4.2f };
            basic3.ProtectionGain = 8.0f;
            tuning.Attacks["basic3"] = basic3;

            SideAttackTuning airBasic = basic1;
            airBasic.Size = { 1.36f, 0.78f };
            airBasic.Offset = { 0.82f, 0.04f };
            airBasic.AirHeight = 0.18f;
            airBasic.AirRange = 1.70f;
            airBasic.DamageScale = 0.40f;
            airBasic.DamageFlat = 7.0f;
            airBasic.Lifetime = 0.14f;
            airBasic.HitStun = 0.36f;
            airBasic.LaunchVelocity = { 0.85f, 2.15f };
            airBasic.AttackerAirImpulse = 0.85f;
            airBasic.AttackerAirFallStep = 0.10f;
            airBasic.TargetAirFallStep = 0.08f;
            airBasic.ProtectionGain = 7.0f;
            tuning.Attacks["air_basic"] = airBasic;

            SideAttackTuning launcher;
            launcher.Size = { 1.10f, 0.86f };
            launcher.Offset = { 0.68f, 0.02f };
            launcher.AirHeight = 0.98f;
            launcher.AirRange = 1.95f;
            launcher.DamageScale = 0.74f;
            launcher.DamageFlat = 12.0f;
            launcher.Lifetime = 0.22f;
            launcher.HitStun = 0.54f;
            launcher.LaunchVelocity = { 0.95f, 9.8f };
            launcher.AttackerAirImpulse = 3.2f;
            launcher.ProtectionGain = 12.0f;
            tuning.Attacks["launcher"] = launcher;

            SideAttackTuning airChase = launcher;
            airChase.Size = { 1.34f, 0.92f };
            airChase.Offset = { 0.78f, 0.02f };
            airChase.AirHeight = 0.16f;
            airChase.AirRange = 2.00f;
            airChase.DamageScale = 0.58f;
            airChase.DamageFlat = 10.0f;
            airChase.Lifetime = 0.19f;
            airChase.HitStun = 0.46f;
            airChase.LaunchVelocity = { 0.75f, 3.4f };
            airChase.AttackerAirImpulse = 1.20f;
            airChase.AttackerAirFallStep = 0.12f;
            airChase.TargetAirFallStep = 0.06f;
            airChase.ProtectionGain = 9.0f;
            tuning.Attacks["air_chase"] = airChase;

            SideAttackTuning magic;
            magic.Size = { 0.86f, 0.56f };
            magic.Offset = { 0.85f, 0.0f };
            magic.Velocity = { 8.8f, 0.0f };
            magic.AirHeight = 0.72f;
            magic.AirRange = 1.0f;
            magic.DamageScale = 0.70f;
            magic.DamageFlat = 11.0f;
            magic.Lifetime = 1.15f;
            magic.HitStun = 0.34f;
            magic.LaunchVelocity = { 1.2f, 3.1f };
            magic.ProtectionGain = 5.0f;
            tuning.Attacks["magic_bolt"] = magic;

            SideAttackTuning support;
            support.Size = { 1.35f, 1.05f };
            support.Offset = { 0.0f, 0.0f };
            support.AirHeight = 1.05f;
            support.AirRange = 2.10f;
            support.DamageScale = 0.55f;
            support.DamageFlat = 9.0f;
            support.Lifetime = 0.34f;
            support.HitStun = 0.58f;
            support.LaunchVelocity = { 0.0f, 6.8f };
            support.DestroyOnHit = false;
            support.ProtectionGain = 10.0f;
            tuning.Attacks["ally_support"] = support;

            SideAttackTuning breakLimit = support;
            breakLimit.Size = { 1.65f, 1.28f };
            breakLimit.Offset = { 0.62f, 0.0f };
            breakLimit.AirHeight = 0.05f;
            breakLimit.AirRange = 1.72f;
            breakLimit.DamageScale = 0.34f;
            breakLimit.DamageFlat = 6.0f;
            breakLimit.Lifetime = 0.24f;
            breakLimit.HitStun = 0.52f;
            breakLimit.LaunchVelocity = { 0.30f, 5.4f };
            breakLimit.AttackerAirImpulse = 2.25f;
            breakLimit.AttackerAirFallStep = 0.0f;
            breakLimit.TargetAirFallStep = 0.0f;
            breakLimit.DestroyOnHit = false;
            breakLimit.ProtectionGain = 0.0f;
            tuning.Attacks["break_limit"] = breakLimit;

            SideAttackTuning enemyClaw;
            enemyClaw.Size = { 1.18f, 0.82f };
            enemyClaw.Offset = { 0.78f, 0.0f };
            enemyClaw.AirHeight = 0.62f;
            enemyClaw.AirRange = 1.20f;
            enemyClaw.DamageScale = 0.64f;
            enemyClaw.DamageFlat = 7.0f;
            enemyClaw.Lifetime = 0.19f;
            enemyClaw.HitStun = 0.32f;
            enemyClaw.LaunchVelocity = { 3.0f, 2.0f };
            tuning.Attacks["enemy_claw"] = enemyClaw;

            SideAttackTuning bearCharge = enemyClaw;
            bearCharge.Size = { 1.82f, 0.92f };
            bearCharge.Offset = { 1.05f, 0.0f };
            bearCharge.Velocity = { 3.7f, 0.0f };
            bearCharge.DamageScale = 0.82f;
            bearCharge.DamageFlat = 12.0f;
            bearCharge.Lifetime = 0.32f;
            bearCharge.HitStun = 0.44f;
            bearCharge.DestroyOnHit = false;
            tuning.Attacks["bear_charge"] = bearCharge;

            SideAttackTuning shockwave;
            shockwave.Size = { 0.98f, 0.62f };
            shockwave.Offset = { 1.08f, 0.0f };
            shockwave.Velocity = { 5.4f, 0.0f };
            shockwave.AirHeight = 0.30f;
            shockwave.AirRange = 0.78f;
            shockwave.DamageScale = 0.56f;
            shockwave.DamageFlat = 8.0f;
            shockwave.Lifetime = 1.5f;
            shockwave.HitStun = 0.30f;
            shockwave.LaunchVelocity = { 2.5f, 1.6f };
            tuning.Attacks["bear_shockwave"] = shockwave;

            return tuning;
        }

        static SideCombatTuning LoadTuning(const std::string& path)
        {
            SideCombatTuning tuning = BuildDefaultTuning();
            if (path.empty())
            {
                tuning.Loaded = true;
                return tuning;
            }

            try
            {
                const std::filesystem::path resolvedPath = ResolveTuningPath(path);
                YAML::Node root = YAML::LoadFile(resolvedPath.string());
                if (YAML::Node movement = root["movement"])
                {
                    tuning.LaneMinY = movement["laneMinY"].as<float>(tuning.LaneMinY);
                    tuning.LaneMaxY = movement["laneMaxY"].as<float>(tuning.LaneMaxY);
                    tuning.LaneSpeedScale = movement["laneSpeedScale"].as<float>(tuning.LaneSpeedScale);
                    tuning.LaneAcceleration = movement["laneAcceleration"].as<float>(tuning.LaneAcceleration);
                    tuning.SortScale = movement["sortScale"].as<float>(tuning.SortScale);
                }

                if (YAML::Node visuals = root["visuals"])
                {
                    tuning.ShadowMinAlpha = visuals["shadowMinAlpha"].as<float>(tuning.ShadowMinAlpha);
                    tuning.ShadowMaxAlpha = visuals["shadowMaxAlpha"].as<float>(tuning.ShadowMaxAlpha);
                    tuning.ShadowAirFadeHeight = visuals["shadowAirFadeHeight"].as<float>(tuning.ShadowAirFadeHeight);
                }

                if (YAML::Node reactions = root["reactions"])
                    tuning.BossLaunchBonus = reactions["bossLaunchBonus"].as<float>(tuning.BossLaunchBonus);

                tuning.Player = ReadPlayerTuning(root["player"], tuning.Player);
                tuning.Combat = ReadCombatRuleTuning(root["combat"], tuning.Combat);
                tuning.AirCombo = ReadAirComboTuning(root["airCombo"], tuning.AirCombo);
                tuning.Protection = ReadProtectionTuning(root["protection"], tuning.Protection);
                tuning.Enemy = ReadEnemyTuning(root["enemy"], tuning.Enemy);
                tuning.Pickup = ReadPickupTuning(root["pickup"], tuning.Pickup);
                tuning.Skills = ReadSkillDefinitions(root["skills"], tuning.Skills);
                tuning.Progression = ReadProgressionTuning(root["progression"], tuning.Progression);

                if (YAML::Node attacks = root["attacks"])
                {
                    for (auto it = attacks.begin(); it != attacks.end(); ++it)
                    {
                        const std::string id = it->first.as<std::string>();
                        const auto fallbackIt = tuning.Attacks.find(id);
                        const SideAttackTuning fallback = fallbackIt != tuning.Attacks.end()
                            ? fallbackIt->second
                            : SideAttackTuning{};
                        tuning.Attacks[id] = ReadAttackTuning(it->second, fallback);
                    }
                }
                tuning.Loaded = true;
            }
            catch (const std::exception& e)
            {
                WT_CORE_WARN("SideCombat tuning load failed for '{0}': {1}", path, e.what());
                tuning.Loaded = true;
            }

            return tuning;
        }

        struct SideCombatTuningCacheEntry
        {
            SideCombatTuning Tuning;
            std::filesystem::path ResolvedPath;
            std::filesystem::file_time_type WriteTime{};
            bool HasWriteTime = false;
        };

        static const SideCombatTuning& GetTuning(const SideCombatLevelComponent& level)
        {
            static std::unordered_map<std::string, SideCombatTuningCacheEntry> cache;
            const std::string key = level.TuningPath.empty() ? "__default__" : level.TuningPath;
            const std::filesystem::path resolvedPath = ResolveTuningPath(level.TuningPath);
            std::filesystem::file_time_type writeTime{};
            const bool hasWriteTime = TryGetWriteTime(resolvedPath, &writeTime);

            auto it = cache.find(key);
            const bool shouldReload =
                it == cache.end() ||
                it->second.ResolvedPath != resolvedPath ||
                it->second.HasWriteTime != hasWriteTime ||
                (hasWriteTime && it->second.WriteTime != writeTime);

            if (shouldReload)
            {
                SideCombatTuningCacheEntry entry;
                entry.Tuning = LoadTuning(level.TuningPath);
                entry.ResolvedPath = resolvedPath;
                entry.WriteTime = writeTime;
                entry.HasWriteTime = hasWriteTime;
                it = cache.insert_or_assign(key, std::move(entry)).first;
            }
            return it->second.Tuning;
        }

        static const SideAttackTuning& GetAttack(const SideCombatTuning& tuning, const std::string& id)
        {
            static const SideAttackTuning fallback;
            if (auto it = tuning.Attacks.find(id); it != tuning.Attacks.end())
                return it->second;
            return fallback;
        }

        static bool ContainsId(const std::vector<std::string>& ids, const std::string& id)
        {
            return std::find(ids.begin(), ids.end(), id) != ids.end();
        }

        static const SideUnlockProfile* GetUnlockProfile(
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning)
        {
            if (!level.LevelId.empty())
            {
                if (auto it = tuning.Progression.Profiles.find(level.LevelId); it != tuning.Progression.Profiles.end())
                    return &it->second;
            }

            if (!tuning.Progression.DefaultProfileId.empty())
            {
                if (auto it = tuning.Progression.Profiles.find(tuning.Progression.DefaultProfileId);
                    it != tuning.Progression.Profiles.end())
                    return &it->second;
            }

            if (!tuning.Progression.Profiles.empty())
                return &tuning.Progression.Profiles.begin()->second;

            return nullptr;
        }

        static bool IsSkillUnlocked(
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            const std::string& skillId)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            if (!profile)
                return true;
            return ContainsId(profile->UnlockedSkills, skillId);
        }

        static bool IsDebugSkillEnabled(
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            const std::string& skillId)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return profile && ContainsId(profile->DebugSkills, skillId);
        }

        static bool IsSkillUsable(
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            const std::string& skillId)
        {
            return IsSkillUnlocked(level, tuning, skillId) || IsDebugSkillEnabled(level, tuning, skillId);
        }

        static bool IsBreakLimitOfficiallyAvailable(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            return tuning.AirCombo.BreakLimitEnabled || IsSkillUnlocked(level, tuning, "break_limit");
        }

        static bool IsBreakLimitDebugAvailable(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            return tuning.AirCombo.BreakLimitDebugKeyEnabled || IsDebugSkillEnabled(level, tuning, "break_limit");
        }

        static bool ShouldShowBreakLimitUi(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return IsBreakLimitOfficiallyAvailable(level, tuning) ||
                tuning.AirCombo.ShowBreakLimitHint ||
                (profile && profile->ShowBreakLimitHint);
        }

        static bool ShouldShowBossProtectionHud(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return tuning.Protection.ShowBossProtectionHud ||
                (profile && profile->ShowBossProtectionHud);
        }

        static bool ShouldShowCombatStateHud(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return tuning.Protection.ShowCombatStateHud ||
                (profile && profile->ShowCombatStateHud);
        }

        static void SetWidgetVisible(Scene* scene, const std::string& entityName, bool visible)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<UIWidgetComponent>())
                entity.GetComponent<UIWidgetComponent>().Visible = visible;
        }

        static void SetText(Scene* scene, const std::string& entityName, const std::string& text)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<UITextComponent>())
                entity.GetComponent<UITextComponent>().Text = text;
        }

        static void SetProgress(Scene* scene,
            const std::string& entityName,
            float value,
            float maxValue)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UIProgressBarComponent>())
                return;

            auto& bar = entity.GetComponent<UIProgressBarComponent>();
            bar.MaxValue = std::max(0.01f, maxValue);
            bar.Value = std::clamp(value, 0.0f, bar.MaxValue);
        }

        static void SetImageAlpha(Scene* scene, const std::string& entityName, float alpha)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity)
                return;

            alpha = std::clamp(alpha, 0.0f, 1.0f);
            if (entity.HasComponent<UIImageComponent>())
                entity.GetComponent<UIImageComponent>().Color.a = alpha;
            if (entity.HasComponent<UIWidgetComponent>())
                entity.GetComponent<UIWidgetComponent>().Visible = alpha > 0.01f;
        }

        static Ref<Texture2D> LoadTexture(const std::string& texturePath)
        {
            if (texturePath.empty())
                return nullptr;

            static std::unordered_map<std::string, Ref<Texture2D>> textureCache;
            if (auto it = textureCache.find(texturePath); it != textureCache.end())
                return it->second;

            Ref<Texture2D> texture = Texture2D::Create(texturePath);
            if (!texture || !texture->IsLoaded())
                return nullptr;

            textureCache[texturePath] = texture;
            return texture;
        }

        static const char* ResolveAttackTexture(SideAttackKind kind, int team)
        {
            if (team == (int)SideCombatTeam::Enemy)
            {
                if (kind == SideAttackKind::EnemyProjectile || kind == SideAttackKind::EnemyShockwave)
                    return "assets/vertical_slice/side_combat/effects/enemy_projectile.png";
                return "assets/vertical_slice/side_combat/effects/enemy_claw.png";
            }

            switch (kind)
            {
            case SideAttackKind::Launcher:
                return "assets/vertical_slice/side_combat/effects/slash_launcher.png";
            case SideAttackKind::MagicBolt:
                return "assets/vertical_slice/side_combat/effects/magic_bolt.png";
            case SideAttackKind::AllySupport:
                return "assets/vertical_slice/side_combat/effects/ally_support.png";
            case SideAttackKind::BreakLimit:
                return "assets/vertical_slice/side_combat/effects/ally_support.png";
            case SideAttackKind::Basic:
            default:
                return "assets/vertical_slice/side_combat/effects/slash_basic.png";
            }
        }

        static void ApplyFrameTexture(SpriteRendererComponent& sprite, const SideHitboxComponent& hitbox)
        {
            const int frameCount = std::max(1, hitbox.TextureFrameCount);
            const float frameRate = std::max(1.0f, hitbox.TextureFrameRate);
            const int frame = 1 + std::min(frameCount - 1, (int)std::floor(hitbox.RuntimeAge * frameRate));
            const std::string path = FormatFramePath(hitbox.TextureFramePattern, frame);
            if (!path.empty())
            {
                if (Ref<Texture2D> texture = LoadTexture(path))
                    sprite.Texture = texture;
            }
        }

        static void UpdateCombatantVisual(Scene* scene,
            Entity entity,
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning)
        {
            if (!entity || !entity.HasComponent<TransformComponent>() || !entity.HasComponent<SideCombatantComponent>())
                return;

            auto& transform = entity.GetComponent<TransformComponent>();
            auto& combatant = entity.GetComponent<SideCombatantComponent>();
            transform.Translation.x = combatant.RuntimeGroundPosition.x;
            transform.Translation.y = combatant.RuntimeGroundPosition.y + combatant.RuntimeAirHeight;
            transform.Translation.z = CalculateSortZ(combatant.RuntimeGroundPosition.y, tuning);

            if (entity.HasComponent<SpriteRendererComponent>())
            {
                auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                sprite.FlipX = combatant.RuntimeFacing < 0.0f;
                if (!combatant.Alive)
                {
                    sprite.Color.a = 0.18f;
                }
                else if (combatant.RuntimeState == SideCombatState::SuperArmor)
                {
                    sprite.Color.r = 1.0f;
                    sprite.Color.g = 0.88f;
                    sprite.Color.b = 0.34f;
                    sprite.Color.a = 1.0f;
                }
                else if (combatant.RuntimeState == SideCombatState::Broken)
                {
                    sprite.Color.r = 0.76f;
                    sprite.Color.g = 0.92f;
                    sprite.Color.b = 1.0f;
                    sprite.Color.a = 1.0f;
                }
                else if (combatant.RuntimeInvulnerableTimer > 0.0f ||
                    combatant.RuntimeHitStun > 0.0f ||
                    (combatant.Team == (int)SideCombatTeam::Enemy && !combatant.RuntimeOnGround))
                {
                    sprite.Color.r = 1.0f;
                    sprite.Color.g = 0.78f;
                    sprite.Color.b = 0.72f;
                    sprite.Color.a = 1.0f;
                }
                else
                {
                    sprite.Color = { 1.0f, 1.0f, 1.0f, sprite.Color.a };
                }
            }

            if (!entity.HasComponent<TagComponent>())
                return;

            const std::string shadowName = entity.GetComponent<TagComponent>().Tag + "_Shadow";
            Entity shadow = FindEntityByName(scene, shadowName);
            if (!shadow || !shadow.HasComponent<TransformComponent>() || !shadow.HasComponent<SpriteRendererComponent>())
                return;

            auto& shadowTransform = shadow.GetComponent<TransformComponent>();
            shadowTransform.Translation = {
                combatant.RuntimeGroundPosition.x,
                combatant.RuntimeGroundPosition.y - 0.10f,
                CalculateSortZ(combatant.RuntimeGroundPosition.y, tuning) - 0.02f
            };

            const float airFade = 1.0f - std::clamp(combatant.RuntimeAirHeight / std::max(0.01f, tuning.ShadowAirFadeHeight), 0.0f, 1.0f);
            const float alpha = tuning.ShadowMinAlpha + (tuning.ShadowMaxAlpha - tuning.ShadowMinAlpha) * airFade;
            shadow.GetComponent<SpriteRendererComponent>().Color.a = combatant.Alive ? alpha : 0.0f;
        }

        static bool OverlapsHitbox(const SideHitboxComponent& hitbox, const SideCombatantComponent& target)
        {
            const glm::vec2 groundDelta = glm::abs(hitbox.RuntimeGroundPosition - target.RuntimeGroundPosition);
            const bool overlapsGround =
                groundDelta.x <= (hitbox.Size.x + target.CollisionSize.x) * 0.5f &&
                groundDelta.y <= (hitbox.Size.y + target.CollisionSize.y) * 0.5f;
            if (!overlapsGround)
                return false;

            const float hitMin = hitbox.AirHeight - hitbox.AirRange * 0.5f;
            const float hitMax = hitbox.AirHeight + hitbox.AirRange * 0.5f;
            const float targetMin = target.RuntimeAirHeight;
            const float targetMax = target.RuntimeAirHeight + std::max(0.1f, target.CollisionHeight);
            return hitMax >= targetMin && hitMin <= targetMax;
        }

        static std::string FormatFloat(float value, int precision = 0)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(precision) << value;
            return stream.str();
        }

        static float CalculateDamage(float rawDamage, float defense, const SideCombatRuleTuning& rules)
        {
            const float defenseBase = std::max(1.0f, rules.DefenseBase);
            const float defenseFactor = defenseBase / (defenseBase + std::max(0.0f, defense));
            return std::max(rules.MinDamage, rawDamage * defenseFactor);
        }

        static const char* GetCombatStateLabel(SideCombatState state)
        {
            switch (state)
            {
            case SideCombatState::HitStun: return "受击";
            case SideCombatState::Launched: return "浮空";
            case SideCombatState::Knockdown: return "倒地";
            case SideCombatState::Recovery: return "恢复";
            case SideCombatState::SuperArmor: return "霸体脱离";
            case SideCombatState::Broken: return "破防";
            case SideCombatState::Dead: return "死亡";
            case SideCombatState::Normal:
            default: return "通常";
            }
        }

        static bool IsBossEntity(entt::registry& registry, entt::entity entity)
        {
            return registry.has<SideEnemyAIComponent>(entity) &&
                registry.get<SideEnemyAIComponent>(entity).Kind == SideEnemyKind::BearBoss;
        }

        static bool IsControlledAirborne(const SideCombatantComponent& combatant)
        {
            return !combatant.RuntimeOnGround || combatant.RuntimeAirHeight > 0.05f;
        }

        static bool CanEnemyAct(const SideCombatantComponent& combatant)
        {
            return combatant.Alive &&
                combatant.RuntimeState == SideCombatState::Normal &&
                combatant.RuntimeHitStun <= 0.0f &&
                !IsControlledAirborne(combatant);
        }

        static void SetCombatState(SideCombatantComponent& combatant,
            SideCombatState state,
            float duration = 0.0f)
        {
            combatant.RuntimeState = state;
            combatant.RuntimeStateTimer = std::max(0.0f, duration);
        }

        static void EnterBossProtectionRecovery(SideCombatantComponent& boss,
            const SideProtectionTuning& protection)
        {
            boss.RuntimeProtection = boss.RuntimeProtectionMax;
            boss.RuntimeHitStun = 0.0f;
            boss.RuntimeInvulnerableTimer = std::max(
                boss.RuntimeInvulnerableTimer,
                protection.BossProtectionLimitTime);
            boss.RuntimeVelocity = { 0.0f, 0.0f };
            boss.RuntimeAirVelocity = std::min(
                boss.RuntimeAirVelocity,
                protection.BossProtectionForceFallVelocity);
            SetCombatState(boss, SideCombatState::SuperArmor, protection.BossProtectionLimitTime);
        }

        static void ApplyPlayerTuning(const SideCombatTuning& tuning,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            combatant.MoveSpeed = tuning.Player.MoveSpeed;
            controller.MaxJumps = std::max(0, tuning.Player.MaxJumps);
            controller.JumpImpulse = tuning.Player.JumpImpulse;
            controller.Gravity = tuning.Player.Gravity;
            controller.AirControl = tuning.Player.AirControl;
            controller.LaneSpeedScale = tuning.Player.LaneSpeedScale;
            controller.LaneAcceleration = tuning.Player.LaneAcceleration;
            controller.GroundFriction = tuning.Player.GroundFriction;
            controller.BasicCooldown = tuning.Player.BasicCooldown;
            controller.LauncherCooldown = tuning.Player.LauncherCooldown;
            controller.MagicBoltCooldown = tuning.Player.MagicBoltCooldown;
            controller.AllySupportCooldown = tuning.Player.AllySupportCooldown;
            controller.RuntimeMagicSwordGaugeMax = std::max(1.0f, tuning.AirCombo.MagicSwordGaugeMax);
            controller.RuntimeMagicSwordGauge = std::clamp(
                controller.RuntimeMagicSwordGauge,
                0.0f,
                controller.RuntimeMagicSwordGaugeMax);
        }

        static void ApplyBearBossTuning(const SideCombatTuning& tuning,
            SideCombatantComponent& combatant,
            SideEnemyAIComponent& ai)
        {
            if (ai.Kind != SideEnemyKind::BearBoss)
                return;

            combatant.MoveSpeed = tuning.Enemy.BearBossMoveSpeed;
            ai.AggroRange = tuning.Enemy.BearBossAggroRange;
            ai.AttackRange = tuning.Enemy.BearBossAttackRange;
            ai.PreferredRange = tuning.Enemy.BearBossPreferredRange;
            ai.AttackInterval = tuning.Enemy.BearBossAttackInterval;
            ai.LaneTolerance = tuning.Enemy.BearBossLaneTolerance;
        }

        static void ResetLevelRuntime(Scene* scene, SideCombatLevelComponent& level)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            level.ComboDropDelay = std::max(0.0f, tuning.Combat.ComboDropDelay);
            level.RuntimeElapsed = 0.0f;
            level.RuntimeFadeAlpha = 1.0f;
            level.RuntimePaused = false;
            level.RuntimeVictory = false;
            level.RuntimeDefeat = false;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeResultCommandIssued = false;
            level.RuntimeRequestedCommand.clear();
            level.RuntimeComboCount = 0;
            level.RuntimeBestCombo = 0;
            level.RuntimeComboTimer = 0.0f;
            level.RuntimeCollectedPickups = 0;
            level.RuntimeRewardsSpawned = false;

            SetImageAlpha(scene, level.FadeEntityName, 1.0f);
        }

        static void ResetCombatants(Scene* scene, const SideCombatLevelComponent& level)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TransformComponent, SideCombatantComponent>())
            {
                auto& transform = registry.get<TransformComponent>(e);
                auto& combatant = registry.get<SideCombatantComponent>(e);
                if (combatant.Team == (int)SideCombatTeam::Player)
                {
                    const auto& progress = GameProgress::GetState();
                    combatant.MaxHealth = std::max(combatant.MaxHealth, static_cast<float>(progress.Attributes.HP));
                    combatant.Attack = std::max(combatant.Attack, static_cast<float>(progress.Attributes.ATK));
                    combatant.Defense = std::max(combatant.Defense, static_cast<float>(progress.Attributes.DEF));
                }

                combatant.Health = std::max(0.0f, combatant.MaxHealth);
                combatant.Alive = combatant.Health > 0.0f;
                combatant.ControlsLocked = false;
                combatant.RuntimeVelocity = { 0.0f, 0.0f };
                combatant.RuntimeGroundPosition = { transform.Translation.x, transform.Translation.y };
                combatant.RuntimeAirHeight = 0.0f;
                combatant.RuntimeAirVelocity = 0.0f;
                combatant.RuntimeHitStun = 0.0f;
                combatant.RuntimeInvulnerableTimer = 0.0f;
                combatant.RuntimeProtection = 0.0f;
                combatant.RuntimeProtectionMax = registry.has<SideEnemyAIComponent>(e) &&
                    registry.get<SideEnemyAIComponent>(e).Kind == SideEnemyKind::BearBoss
                    ? std::max(1.0f, tuning.Protection.BossProtectionMax)
                    : 100.0f;
                combatant.RuntimeState = combatant.Alive ? SideCombatState::Normal : SideCombatState::Dead;
                combatant.RuntimeStateTimer = 0.0f;
                combatant.RuntimeDeathProcessed = false;
                combatant.RuntimeOnGround = true;

                if (registry.has<SidePlayerControllerComponent>(e))
                {
                    auto& controller = registry.get<SidePlayerControllerComponent>(e);
                    ApplyPlayerTuning(tuning, combatant, controller);
                }

                if (registry.has<SideEnemyAIComponent>(e))
                {
                    auto& ai = registry.get<SideEnemyAIComponent>(e);
                    ApplyBearBossTuning(tuning, combatant, ai);
                }

                if (combatant.Team == (int)SideCombatTeam::Enemy && transform.Translation.x != 0.0f)
                    combatant.RuntimeFacing = transform.Translation.x > 0.0f ? -1.0f : 1.0f;

                if (registry.has<SpriteRendererComponent>(e))
                    registry.get<SpriteRendererComponent>(e).Color.a = 1.0f;
                UpdateCombatantVisual(scene, { e, scene }, level, tuning);
            }

            for (auto e : registry.view<SidePlayerControllerComponent>())
            {
                auto& controller = registry.get<SidePlayerControllerComponent>(e);
                controller.RuntimeBasicCooldown = 0.0f;
                controller.RuntimeLauncherCooldown = 0.0f;
                controller.RuntimeMagicBoltCooldown = 0.0f;
                controller.RuntimeAllySupportCooldown = 0.0f;
                controller.RuntimeBreakLimitCooldown = 0.0f;
                controller.RuntimeMagicSwordGaugeMax = std::max(1.0f, tuning.AirCombo.MagicSwordGaugeMax);
                controller.RuntimeMagicSwordGauge = controller.RuntimeMagicSwordGaugeMax;
                controller.RuntimeJumpsRemaining = controller.MaxJumps;
                controller.RuntimeAttackChain = 0;
                controller.RuntimeAttackChainTimer = 0.0f;
                controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;
            }

            for (auto e : registry.view<SideEnemyAIComponent>())
            {
                auto& ai = registry.get<SideEnemyAIComponent>(e);
                ai.RuntimeAttackTimer = tuning.Enemy.InitialAttackDelay;
                ai.RuntimeDecisionTimer = 0.0f;
                ai.RuntimeAwake = true;
            }
        }

        static void UpdateStartFade(Scene* scene, SideCombatLevelComponent& level, float dt)
        {
            if (level.StartFadeDuration <= 0.0f)
            {
                level.RuntimeFadeAlpha = 0.0f;
                SetImageAlpha(scene, level.FadeEntityName, 0.0f);
                return;
            }

            level.RuntimeFadeAlpha = std::max(0.0f,
                level.RuntimeFadeAlpha - dt / level.StartFadeDuration);
            SetImageAlpha(scene, level.FadeEntityName, level.RuntimeFadeAlpha);
        }

        static Entity CreateHitbox(Scene* scene,
            const std::string& name,
            const glm::vec2& sourceGroundPosition,
            float sourceAirHeight,
            float facing,
            SideAttackKind kind,
            int team,
            const SideAttackTuning& tuning,
            float damage,
            const SideCombatTuning& combatTuning)
        {
            Entity hitbox = scene->CreateEntity(name);
            const glm::vec2 groundPosition = {
                sourceGroundPosition.x + facing * tuning.Offset.x,
                sourceGroundPosition.y + tuning.Offset.y
            };
            const float airHeight = std::max(0.0f, sourceAirHeight + tuning.AirHeight);

            auto& transform = hitbox.GetComponent<TransformComponent>();
            transform.Translation = {
                groundPosition.x,
                groundPosition.y + airHeight,
                CalculateSortZ(groundPosition.y, combatTuning) + 0.04f
            };
            transform.Scale = { tuning.Size.x, std::max(tuning.AirRange, tuning.Size.y), 1.0f };

            auto& sprite = hitbox.AddComponent<SpriteRendererComponent>();
            sprite.Color = { 1.0f, 1.0f, 1.0f, 0.88f };
            sprite.FlipX = facing < 0.0f;

            auto& component = hitbox.AddComponent<SideHitboxComponent>();
            component.Team = team;
            component.AttackKind = kind;
            component.Size = tuning.Size;
            component.Velocity = { facing * tuning.Velocity.x, tuning.Velocity.y };
            component.Damage = damage;
            component.Lifetime = tuning.Lifetime;
            component.HitStun = tuning.HitStun;
            component.LaunchVelocity = { facing * tuning.LaunchVelocity.x, tuning.LaunchVelocity.y };
            component.AttackerAirImpulse = tuning.AttackerAirImpulse;
            component.AttackerAirFallStep = tuning.AttackerAirFallStep;
            component.TargetAirFallStep = tuning.TargetAirFallStep;
            component.ProtectionGain = tuning.ProtectionGain;
            component.AirHeight = airHeight;
            component.AirRange = tuning.AirRange;
            component.DestroyOnHit = tuning.DestroyOnHit;
            component.TextureFramePattern = tuning.TextureFramePattern;
            component.TextureFrameCount = std::max(1, tuning.TextureFrameCount);
            component.TextureFrameRate = std::max(1.0f, tuning.TextureFrameRate);
            component.RuntimeGroundPosition = groundPosition;
            ApplyFrameTexture(sprite, component);
            if (!sprite.Texture)
            {
                if (Ref<Texture2D> texture = LoadTexture(ResolveAttackTexture(kind, team)))
                    sprite.Texture = texture;
            }
            return hitbox;
        }

        static void CreatePickup(Scene* scene,
            const std::string& name,
            const glm::vec3& position,
            const std::string& itemId,
            const std::string& displayName,
            int amount,
            const std::string& texturePath,
            const SidePickupTuning& tuning)
        {
            Entity pickup = scene->CreateEntity(name);
            auto& transform = pickup.GetComponent<TransformComponent>();
            transform.Translation = position;
            transform.Scale = { 0.38f, 0.38f, 1.0f };

            auto& sprite = pickup.AddComponent<SpriteRendererComponent>();
            sprite.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
            if (Ref<Texture2D> texture = LoadTexture(texturePath))
                sprite.Texture = texture;

            auto& component = pickup.AddComponent<SidePickupComponent>();
            component.ItemId = itemId;
            component.DisplayName = displayName;
            component.Amount = amount;
            component.PickupRadius = tuning.PickupRadius;
            component.AttractRadius = tuning.AttractRadius;
            component.AttractSpeed = tuning.AttractSpeed;
        }

        static void RegisterPlayerHit(SideCombatLevelComponent& level)
        {
            ++level.RuntimeComboCount;
            level.RuntimeBestCombo = std::max(level.RuntimeBestCombo, level.RuntimeComboCount);
            level.RuntimeComboTimer = level.ComboDropDelay;
        }

        static Entity FindNearestAliveEnemy(Scene* scene, const glm::vec3& origin)
        {
            Entity best;
            float bestDistanceSq = std::numeric_limits<float>::max();
            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TransformComponent, SideCombatantComponent>())
            {
                auto& combatant = registry.get<SideCombatantComponent>(e);
                if (combatant.Team != (int)SideCombatTeam::Enemy || !combatant.Alive)
                    continue;

                const float distanceSq = glm::length2(
                    combatant.RuntimeGroundPosition - ToVec2(origin));
                if (distanceSq < bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    best = { e, scene };
                }
            }
            return best;
        }

        static void CreatePlayerBasic(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            if (!combatant.RuntimeOnGround)
            {
                if (!IsSkillUnlocked(level, tuning, "air_basic"))
                    return;
                if (controller.RuntimeAirActionsRemaining <= 0)
                    return;

                --controller.RuntimeAirActionsRemaining;
                controller.RuntimeAttackChain = 0;
                controller.RuntimeAttackChainTimer = tuning.Player.BasicChainWindow;
                controller.RuntimeBasicCooldown = tuning.AirCombo.AirBasicCooldown;

                const SideAttackTuning& attack = GetAttack(tuning, "air_basic");
                const float facing = combatant.RuntimeFacing;
                CreateHitbox(scene, "Side_PlayerAirSlash",
                    combatant.RuntimeGroundPosition,
                    combatant.RuntimeAirHeight,
                    facing,
                    SideAttackKind::Basic,
                    (int)SideCombatTeam::Player,
                    attack,
                    combatant.Attack * attack.DamageScale + attack.DamageFlat,
                    tuning);
                return;
            }

            if (!IsSkillUnlocked(level, tuning, "basic_attack"))
                return;

            const int chain = controller.RuntimeAttackChainTimer > 0.0f
                ? (controller.RuntimeAttackChain % 3) + 1
                : 1;
            controller.RuntimeAttackChain = chain;
            controller.RuntimeAttackChainTimer = tuning.Player.BasicChainWindow;
            controller.RuntimeBasicCooldown = chain == 3
                ? controller.BasicCooldown + tuning.Player.BasicFinisherExtraCooldown
                : controller.BasicCooldown;

            const SideAttackTuning& attack = GetAttack(tuning, "basic" + std::to_string(chain));
            const float facing = combatant.RuntimeFacing;
            const float damage = combatant.Attack * attack.DamageScale + attack.DamageFlat;

            CreateHitbox(scene, "Side_PlayerSlash",
                combatant.RuntimeGroundPosition,
                combatant.RuntimeAirHeight,
                facing,
                SideAttackKind::Basic,
                (int)SideCombatTeam::Player,
                attack,
                damage,
                tuning);
        }

        static void CreatePlayerLauncher(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            const bool airborne = !combatant.RuntimeOnGround;
            if (!IsSkillUnlocked(level, tuning, airborne ? "air_chase" : "launcher"))
                return;
            if (airborne && controller.RuntimeAirActionsRemaining <= 0)
                return;

            controller.RuntimeLauncherCooldown = airborne
                ? tuning.AirCombo.AirChaseCooldown
                : controller.LauncherCooldown;
            controller.RuntimeAttackChainTimer = tuning.Player.LauncherChainWindow;

            const SideAttackTuning& attack = GetAttack(tuning, airborne ? "air_chase" : "launcher");
            if (airborne)
            {
                --controller.RuntimeAirActionsRemaining;
            }
            else
            {
                controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;
                combatant.RuntimeOnGround = false;
                combatant.RuntimeAirVelocity = std::max(combatant.RuntimeAirVelocity, attack.AttackerAirImpulse);
            }

            const float facing = combatant.RuntimeFacing;
            CreateHitbox(scene, airborne ? "Side_PlayerAirChase" : "Side_PlayerLauncher",
                combatant.RuntimeGroundPosition,
                combatant.RuntimeAirHeight,
                facing,
                SideAttackKind::Launcher,
                (int)SideCombatTeam::Player,
                attack,
                combatant.Attack * attack.DamageScale + attack.DamageFlat,
                tuning);
        }

        static bool CanUseBreakLimit(Scene* scene,
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            Entity target,
            const SideCombatantComponent& combatant,
            const SidePlayerControllerComponent& controller)
        {
            if (!scene || !target || !target.HasComponent<SideCombatantComponent>())
                return false;
            if (!IsBreakLimitOfficiallyAvailable(level, tuning) && !IsBreakLimitDebugAvailable(level, tuning))
                return false;
            if (combatant.RuntimeOnGround || controller.RuntimeBreakLimitCooldown > 0.0f)
                return false;
            if (controller.RuntimeMagicSwordGauge + 0.001f < tuning.AirCombo.BreakLimitGaugeCost)
                return false;
            if (level.RuntimeComboCount < tuning.AirCombo.BreakLimitMinCombo)
                return false;
            if (combatant.RuntimeAirHeight > tuning.AirCombo.BreakLimitMaxHeight)
                return false;
            if (combatant.RuntimeAirVelocity > tuning.AirCombo.BreakLimitFallingVelocity)
                return false;

            auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
            if (!targetCombatant.Alive || !IsControlledAirborne(targetCombatant))
                return false;
            if (targetCombatant.RuntimeAirVelocity > tuning.AirCombo.BreakLimitFallingVelocity)
                return false;

            auto& registry = scene->GetRegistry();
            if (IsBossEntity(registry, (entt::entity)target))
            {
                if (targetCombatant.RuntimeProtection < tuning.Protection.BossProtectionBreakLimitThreshold)
                    return false;
            }

            return true;
        }

        static void CreateBreakLimitChase(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            Entity target = FindNearestAliveEnemy(scene, player.GetComponent<TransformComponent>().Translation);
            if (!CanUseBreakLimit(scene, level, tuning, target, combatant, controller))
                return;

            const SideAttackTuning& attack = GetAttack(tuning, "break_limit");
            controller.RuntimeBreakLimitCooldown = tuning.AirCombo.BreakLimitCooldown;
            controller.RuntimeMagicSwordGauge = std::max(0.0f,
                controller.RuntimeMagicSwordGauge - tuning.AirCombo.BreakLimitGaugeCost);
            controller.RuntimeJumpsRemaining = std::max(controller.RuntimeJumpsRemaining, 1);
            controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimitAfterBreak;
            controller.RuntimeAttackChainTimer = tuning.Player.LauncherChainWindow;
            combatant.RuntimeAirHeight = std::max(0.05f,
                combatant.RuntimeAirHeight + tuning.AirCombo.BreakLimitHeightBoost);
            combatant.RuntimeAirVelocity = std::max(combatant.RuntimeAirVelocity,
                tuning.AirCombo.BreakLimitHangImpulse);

            glm::vec2 origin = combatant.RuntimeGroundPosition;
            float sourceAirHeight = combatant.RuntimeAirHeight;
            if (target && target.HasComponent<SideCombatantComponent>())
            {
                const auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
                origin = targetCombatant.RuntimeGroundPosition;
                sourceAirHeight = targetCombatant.RuntimeAirHeight;
                combatant.RuntimeFacing = SignNonZero(origin.x - combatant.RuntimeGroundPosition.x);
            }

            CreateHitbox(scene, "Side_BreakLimitChase",
                origin,
                sourceAirHeight,
                combatant.RuntimeFacing,
                SideAttackKind::BreakLimit,
                (int)SideCombatTeam::Player,
                attack,
                combatant.Attack * attack.DamageScale + attack.DamageFlat,
                tuning);
        }

        static void CreatePlayerMagicBolt(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            if (!IsSkillUnlocked(level, tuning, "magic_bolt"))
                return;
            controller.RuntimeMagicBoltCooldown = controller.MagicBoltCooldown;
            controller.RuntimeAttackChainTimer = tuning.Player.MagicChainWindow;

            const SideAttackTuning& attack = GetAttack(tuning, "magic_bolt");
            const float facing = combatant.RuntimeFacing;
            CreateHitbox(scene, "Side_PlayerMagicBolt",
                combatant.RuntimeGroundPosition,
                combatant.RuntimeAirHeight,
                facing,
                SideAttackKind::MagicBolt,
                (int)SideCombatTeam::Player,
                attack,
                combatant.Attack * attack.DamageScale + attack.DamageFlat,
                tuning);
        }

        static void CreateAllySupport(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            SideCombatantComponent& combatant,
            SidePlayerControllerComponent& controller)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            if (!IsSkillUnlocked(level, tuning, "ally_support"))
                return;
            controller.RuntimeAllySupportCooldown = controller.AllySupportCooldown;
            controller.RuntimeAttackChainTimer = tuning.Player.SupportChainWindow;

            const SideAttackTuning& attack = GetAttack(tuning, "ally_support");
            Entity target = FindNearestAliveEnemy(scene, player.GetComponent<TransformComponent>().Translation);
            glm::vec2 origin = combatant.RuntimeGroundPosition + glm::vec2{ combatant.RuntimeFacing * 2.0f, 0.0f };
            float targetAirHeight = combatant.RuntimeAirHeight;
            if (target && target.HasComponent<SideCombatantComponent>())
            {
                const auto& targetCombatant = target.GetComponent<SideCombatantComponent>();
                origin = targetCombatant.RuntimeGroundPosition;
                targetAirHeight = targetCombatant.RuntimeAirHeight;
            }

            CreateHitbox(scene, "Side_AllySupport",
                origin,
                targetAirHeight,
                combatant.RuntimeFacing,
                SideAttackKind::AllySupport,
                (int)SideCombatTeam::Player,
                attack,
                combatant.Attack * attack.DamageScale + attack.DamageFlat,
                tuning);
        }

        static void UpdatePlayer(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            float dt,
            bool jumpPressed,
            bool basicPressed,
            bool launcherPressed,
            bool magicPressed,
            bool supportPressed,
            bool breakLimitPressed,
            bool previousJumpPressed,
            bool previousLauncherPressed,
            bool previousMagicPressed,
            bool previousSupportPressed,
            bool previousBreakLimitPressed)
        {
            if (!player || !player.HasComponent<TransformComponent>() ||
                !player.HasComponent<SideCombatantComponent>() ||
                !player.HasComponent<SidePlayerControllerComponent>())
                return;

            auto& transform = player.GetComponent<TransformComponent>();
            auto& combatant = player.GetComponent<SideCombatantComponent>();
            auto& controller = player.GetComponent<SidePlayerControllerComponent>();
            const SideCombatTuning& tuning = GetTuning(level);
            ApplyPlayerTuning(tuning, combatant, controller);

            if (!combatant.Alive)
            {
                if (!level.RuntimeDefeat)
                {
                    level.RuntimeDefeat = true;
                    level.RuntimeResultTimer = 0.0f;
                    level.RuntimeResultCommandIssued = false;
                    level.RuntimeComboCount = 0;
                    combatant.ControlsLocked = true;
                }
                return;
            }

            controller.RuntimeBasicCooldown = std::max(0.0f, controller.RuntimeBasicCooldown - dt);
            controller.RuntimeLauncherCooldown = std::max(0.0f, controller.RuntimeLauncherCooldown - dt);
            controller.RuntimeMagicBoltCooldown = std::max(0.0f, controller.RuntimeMagicBoltCooldown - dt);
            controller.RuntimeAllySupportCooldown = std::max(0.0f, controller.RuntimeAllySupportCooldown - dt);
            controller.RuntimeBreakLimitCooldown = std::max(0.0f, controller.RuntimeBreakLimitCooldown - dt);
            controller.RuntimeAttackChainTimer = std::max(0.0f, controller.RuntimeAttackChainTimer - dt);
            if (controller.RuntimeAttackChainTimer <= 0.0f)
                controller.RuntimeAttackChain = 0;

            if (combatant.ControlsLocked || level.RuntimeVictory || level.RuntimeDefeat)
                return;

            float horizontal = 0.0f;
            if (Input::IsKeyPressed(WT_KEY_A) || Input::IsKeyPressed(WT_KEY_LEFT))
                horizontal -= 1.0f;
            if (Input::IsKeyPressed(WT_KEY_D) || Input::IsKeyPressed(WT_KEY_RIGHT))
                horizontal += 1.0f;

            float lane = 0.0f;
            if (Input::IsKeyPressed(WT_KEY_S) || Input::IsKeyPressed(WT_KEY_DOWN))
                lane -= 1.0f;
            if (Input::IsKeyPressed(WT_KEY_W) || Input::IsKeyPressed(WT_KEY_UP))
                lane += 1.0f;

            if (horizontal != 0.0f)
            {
                combatant.RuntimeFacing = SignNonZero(horizontal);
                const float targetSpeed = horizontal * combatant.MoveSpeed;
                const float accel = combatant.RuntimeOnGround ? tuning.Player.GroundAcceleration : controller.AirControl;
                combatant.RuntimeVelocity.x = Approach(combatant.RuntimeVelocity.x, targetSpeed, accel * dt);
            }
            else if (combatant.RuntimeOnGround)
            {
                combatant.RuntimeVelocity.x = Approach(
                    combatant.RuntimeVelocity.x,
                    0.0f,
                    controller.GroundFriction * dt);
            }

            if (lane != 0.0f)
            {
                const float targetLaneSpeed = lane * combatant.MoveSpeed * controller.LaneSpeedScale * tuning.LaneSpeedScale;
                combatant.RuntimeVelocity.y = Approach(
                    combatant.RuntimeVelocity.y,
                    targetLaneSpeed,
                    std::max(controller.LaneAcceleration, tuning.LaneAcceleration) * dt);
            }
            else if (combatant.RuntimeOnGround)
            {
                combatant.RuntimeVelocity.y = Approach(
                    combatant.RuntimeVelocity.y,
                    0.0f,
                    controller.GroundFriction * dt);
            }

            if (combatant.RuntimeOnGround)
            {
                controller.RuntimeJumpsRemaining = controller.MaxJumps;
                controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;
            }

            if (jumpPressed && !previousJumpPressed && controller.RuntimeJumpsRemaining > 0)
            {
                combatant.RuntimeOnGround = false;
                combatant.RuntimeAirVelocity = controller.JumpImpulse;
                --controller.RuntimeJumpsRemaining;
                controller.RuntimeAirActionsRemaining = tuning.AirCombo.AirActionLimit;
            }

            if (basicPressed && controller.RuntimeBasicCooldown <= 0.0f)
                CreatePlayerBasic(scene, level, player, combatant, controller);

            if (launcherPressed && !previousLauncherPressed && controller.RuntimeLauncherCooldown <= 0.0f)
                CreatePlayerLauncher(scene, level, player, combatant, controller);

            if (magicPressed && !previousMagicPressed && controller.RuntimeMagicBoltCooldown <= 0.0f)
                CreatePlayerMagicBolt(scene, level, player, combatant, controller);

            if (supportPressed && !previousSupportPressed && controller.RuntimeAllySupportCooldown <= 0.0f)
                CreateAllySupport(scene, level, player, combatant, controller);

            if (breakLimitPressed && !previousBreakLimitPressed)
                CreateBreakLimitChase(scene, level, player, combatant, controller);

            if (player.HasComponent<SpriteRendererComponent>())
                player.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
        }

        static void IssueEnemyAttack(Scene* scene,
            Entity enemy,
            Entity player,
            SideCombatLevelComponent& level,
            SideEnemyAIComponent& ai,
            SideCombatantComponent& combatant)
        {
            if (!enemy || !player)
                return;
            if (!CanEnemyAct(combatant))
                return;

            auto& playerCombatant = player.GetComponent<SideCombatantComponent>();
            const glm::vec2 playerPosition = playerCombatant.RuntimeGroundPosition;
            combatant.RuntimeFacing = SignNonZero(playerPosition.x - combatant.RuntimeGroundPosition.x);
            const float facing = combatant.RuntimeFacing;
            const float healthRatio = combatant.MaxHealth > 0.0f ? combatant.Health / combatant.MaxHealth : 0.0f;
            const SideCombatTuning& tuning = GetTuning(level);

            if (ai.Kind == SideEnemyKind::BearBoss)
            {
                const bool lowHealth = healthRatio <= tuning.Enemy.BearBossLowHealthThreshold;
                const bool midHealth = healthRatio <= tuning.Enemy.BearBossMidHealthThreshold;
                const float distance = std::abs(playerPosition.x - combatant.RuntimeGroundPosition.x);
                ai.RuntimeAttackTimer = lowHealth
                    ? tuning.Enemy.BearBossLowAttackInterval
                    : (midHealth ? tuning.Enemy.BearBossMidAttackInterval : ai.AttackInterval);

                if (lowHealth && distance > tuning.Enemy.BearBossChargeDistance)
                {
                    combatant.RuntimeVelocity.x = facing * tuning.Enemy.BearBossChargeSpeed;
                    const SideAttackTuning& attack = GetAttack(tuning, "bear_charge");
                    CreateHitbox(scene, "Side_BearCharge",
                        combatant.RuntimeGroundPosition,
                        combatant.RuntimeAirHeight,
                        facing,
                        SideAttackKind::EnemyMelee,
                        (int)SideCombatTeam::Enemy,
                        attack,
                        combatant.Attack * attack.DamageScale + attack.DamageFlat,
                        tuning);
                    return;
                }

                if (midHealth && distance > tuning.Enemy.BearBossShockwaveDistance)
                {
                    const SideAttackTuning& attack = GetAttack(tuning, "bear_shockwave");
                    CreateHitbox(scene, "Side_BearShockwave",
                        combatant.RuntimeGroundPosition,
                        combatant.RuntimeAirHeight,
                        facing,
                        SideAttackKind::EnemyShockwave,
                        (int)SideCombatTeam::Enemy,
                        attack,
                        combatant.Attack * attack.DamageScale + attack.DamageFlat,
                        tuning);
                    return;
                }

                const SideAttackTuning& attack = GetAttack(tuning, "enemy_claw");
                CreateHitbox(scene, "Side_BearClaw",
                    combatant.RuntimeGroundPosition,
                    combatant.RuntimeAirHeight,
                    facing,
                    SideAttackKind::EnemyMelee,
                    (int)SideCombatTeam::Enemy,
                    attack,
                    combatant.Attack * attack.DamageScale + attack.DamageFlat,
                    tuning);
                return;
            }

            ai.RuntimeAttackTimer = ai.AttackInterval;
            const SideAttackTuning& attack = GetAttack(tuning, "enemy_claw");
            CreateHitbox(scene, "Side_EnemyClaw",
                combatant.RuntimeGroundPosition,
                combatant.RuntimeAirHeight,
                facing,
                SideAttackKind::EnemyMelee,
                (int)SideCombatTeam::Enemy,
                attack,
                combatant.Attack * attack.DamageScale + attack.DamageFlat,
                tuning);
        }

        static void UpdateEnemyAI(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            float dt)
        {
            if (!player || !player.HasComponent<TransformComponent>() || !player.HasComponent<SideCombatantComponent>())
                return;

            const glm::vec2 playerPosition = player.GetComponent<SideCombatantComponent>().RuntimeGroundPosition;
            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TransformComponent, SideCombatantComponent, SideEnemyAIComponent>())
            {
                Entity enemy = { e, scene };
                auto& transform = registry.get<TransformComponent>(e);
                auto& combatant = registry.get<SideCombatantComponent>(e);
                auto& ai = registry.get<SideEnemyAIComponent>(e);
                const SideCombatTuning& tuning = GetTuning(level);
                ApplyBearBossTuning(tuning, combatant, ai);

                if (combatant.Team != (int)SideCombatTeam::Enemy || !combatant.Alive || !ai.RuntimeAwake)
                    continue;
                if (level.RuntimeVictory || level.RuntimeDefeat)
                    continue;

                if (!CanEnemyAct(combatant))
                {
                    combatant.RuntimeVelocity.x = Approach(
                        combatant.RuntimeVelocity.x,
                        0.0f,
                        tuning.Enemy.XBrakeAcceleration * dt);
                    combatant.RuntimeVelocity.y = Approach(
                        combatant.RuntimeVelocity.y,
                        0.0f,
                        tuning.Enemy.LaneBrakeAcceleration * dt);
                    continue;
                }

                const glm::vec2 delta = playerPosition - combatant.RuntimeGroundPosition;
                const float distanceX = delta.x;
                const float distanceY = delta.y;
                const float distanceAbs = std::abs(distanceX);
                const float laneAbs = std::abs(distanceY);
                if (distanceAbs > ai.AggroRange)
                    continue;

                combatant.RuntimeFacing = SignNonZero(distanceX);
                ai.RuntimeAttackTimer = std::max(0.0f, ai.RuntimeAttackTimer - dt);

                const float preferred = ai.Kind == SideEnemyKind::BearBoss
                    ? ai.PreferredRange + tuning.Enemy.BossPreferredRangeBonus
                    : ai.PreferredRange;
                if (distanceAbs > preferred)
                {
                    const float speedScale = ai.Kind == SideEnemyKind::BearBoss
                        ? tuning.Enemy.BossMoveSpeedScale
                        : tuning.Enemy.GruntMoveSpeedScale;
                    combatant.RuntimeVelocity.x = Approach(
                        combatant.RuntimeVelocity.x,
                        combatant.RuntimeFacing * combatant.MoveSpeed * speedScale,
                        tuning.Enemy.XApproachAcceleration * dt);
                }
                else
                {
                    combatant.RuntimeVelocity.x = Approach(
                        combatant.RuntimeVelocity.x,
                        0.0f,
                        tuning.Enemy.XBrakeAcceleration * dt);
                }

                if (laneAbs > ai.LaneTolerance)
                {
                    const float laneDirection = SignNonZero(distanceY);
                    const float laneSpeedScale = ai.Kind == SideEnemyKind::BearBoss
                        ? tuning.Enemy.BossLaneSpeedScale
                        : tuning.Enemy.GruntLaneSpeedScale;
                    combatant.RuntimeVelocity.y = Approach(
                        combatant.RuntimeVelocity.y,
                        laneDirection * combatant.MoveSpeed * laneSpeedScale,
                        tuning.Enemy.LaneApproachAcceleration * dt);
                }
                else
                {
                    combatant.RuntimeVelocity.y = Approach(
                        combatant.RuntimeVelocity.y,
                        0.0f,
                        tuning.Enemy.LaneBrakeAcceleration * dt);
                }

                if (ai.RuntimeAttackTimer <= 0.0f &&
                    distanceAbs <= ai.AttackRange + tuning.Enemy.AttackRangePadding &&
                    laneAbs <= ai.LaneTolerance + tuning.Enemy.LaneAttackPadding)
                {
                    IssueEnemyAttack(scene, enemy, player, level, ai, combatant);
                }

                if (enemy.HasComponent<SpriteRendererComponent>())
                    enemy.GetComponent<SpriteRendererComponent>().FlipX = combatant.RuntimeFacing < 0.0f;
            }
        }

        static void ApplyPlayerAirHitReward(Scene* scene,
            const SideCombatLevelComponent& level,
            const SideHitboxComponent& hitbox,
            SideCombatantComponent& target)
        {
            if (hitbox.Team != (int)SideCombatTeam::Player)
                return;

            Entity player = FindEntityByName(scene, level.PlayerEntityName);
            if (!player || !player.HasComponent<SideCombatantComponent>())
                return;

            auto& attacker = player.GetComponent<SideCombatantComponent>();
            if (attacker.Alive && !attacker.RuntimeOnGround)
            {
                if (hitbox.AttackerAirImpulse > 0.0f)
                    attacker.RuntimeAirVelocity = std::max(attacker.RuntimeAirVelocity, hitbox.AttackerAirImpulse);
                if (hitbox.AttackerAirFallStep > 0.0f)
                    attacker.RuntimeAirHeight = std::max(0.05f, attacker.RuntimeAirHeight - hitbox.AttackerAirFallStep);
            }

            if (target.RuntimeAirHeight > 0.0f && hitbox.TargetAirFallStep > 0.0f)
                target.RuntimeAirHeight = std::max(0.05f, target.RuntimeAirHeight - hitbox.TargetAirFallStep);
        }

        static void AwardMagicSwordGauge(Scene* scene,
            const SideCombatLevelComponent& level,
            const SideHitboxComponent& hitbox,
            const SideCombatantComponent& target)
        {
            if (hitbox.Team != (int)SideCombatTeam::Player ||
                hitbox.AttackKind == SideAttackKind::BreakLimit)
                return;

            Entity player = FindEntityByName(scene, level.PlayerEntityName);
            if (!player || !player.HasComponent<SidePlayerControllerComponent>())
                return;

            const SideCombatTuning& tuning = GetTuning(level);
            auto& controller = player.GetComponent<SidePlayerControllerComponent>();
            const float gain = IsControlledAirborne(target)
                ? tuning.AirCombo.GaugeGainAirHit
                : tuning.AirCombo.GaugeGainGroundHit;
            controller.RuntimeMagicSwordGauge = std::clamp(
                controller.RuntimeMagicSwordGauge + gain,
                0.0f,
                controller.RuntimeMagicSwordGaugeMax);
        }

        static void ApplyBossProtectionOnHit(Scene* scene,
            SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            entt::entity targetEntity,
            SideCombatantComponent& target,
            const SideHitboxComponent& hitbox)
        {
            if (hitbox.Team != (int)SideCombatTeam::Player)
                return;

            auto& registry = scene->GetRegistry();
            if (!IsBossEntity(registry, targetEntity))
                return;

            target.RuntimeProtectionMax = std::max(1.0f, tuning.Protection.BossProtectionMax);
            if (hitbox.AttackKind == SideAttackKind::BreakLimit)
            {
                target.RuntimeProtection = std::max(
                    0.0f,
                    target.RuntimeProtection - tuning.Protection.BreakLimitProtectionReduce);
                SetCombatState(target, SideCombatState::Broken, std::max(0.18f, hitbox.HitStun * 0.5f));
                return;
            }

            float gain = std::max(0.0f, hitbox.ProtectionGain);
            if (!IsControlledAirborne(target) && hitbox.LaunchVelocity.y <= 0.0f)
                gain *= 0.5f;

            target.RuntimeProtection = std::clamp(
                target.RuntimeProtection + gain,
                0.0f,
                target.RuntimeProtectionMax);

            if (target.RuntimeProtection >= target.RuntimeProtectionMax - 0.001f)
            {
                EnterBossProtectionRecovery(target, tuning.Protection);
                level.RuntimeComboCount = 0;
                level.RuntimeComboTimer = 0.0f;
            }
        }

        static void UpdateHitboxes(Scene* scene,
            SideCombatLevelComponent& level,
            float dt)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            auto& registry = scene->GetRegistry();
            std::vector<entt::entity> hitboxesToDestroy;

            for (auto e : registry.view<TransformComponent, SideHitboxComponent>())
            {
                auto& hitboxTransform = registry.get<TransformComponent>(e);
                auto& hitbox = registry.get<SideHitboxComponent>(e);

                hitbox.RuntimeAge += dt;
                hitbox.Lifetime -= dt;
                hitbox.RuntimeGroundPosition += hitbox.Velocity * dt;
                hitboxTransform.Translation = {
                    hitbox.RuntimeGroundPosition.x,
                    hitbox.RuntimeGroundPosition.y + hitbox.AirHeight,
                    CalculateSortZ(hitbox.RuntimeGroundPosition.y, tuning) + 0.04f
                };
                if (registry.has<SpriteRendererComponent>(e))
                    ApplyFrameTexture(registry.get<SpriteRendererComponent>(e), hitbox);

                if (hitbox.Lifetime <= 0.0f)
                {
                    hitboxesToDestroy.push_back(e);
                    continue;
                }

                for (auto targetEntity : registry.view<TransformComponent, SideCombatantComponent>())
                {
                    auto& targetTransform = registry.get<TransformComponent>(targetEntity);
                    auto& target = registry.get<SideCombatantComponent>(targetEntity);
                    if (!target.Alive || target.Team == hitbox.Team || target.Team == (int)SideCombatTeam::Neutral)
                        continue;
                    if (target.RuntimeState == SideCombatState::SuperArmor)
                        continue;
                    if (target.Invulnerable || target.RuntimeInvulnerableTimer > 0.0f)
                        continue;
                    if (!OverlapsHitbox(hitbox, target))
                        continue;

                    const float damage = CalculateDamage(hitbox.Damage, target.Defense, tuning.Combat);
                    target.Health = std::max(0.0f, target.Health - damage);
                    target.Alive = target.Health > 0.0f;
                    target.RuntimeHitStun = std::max(target.RuntimeHitStun, hitbox.HitStun);
                    target.RuntimeInvulnerableTimer = tuning.Combat.HitInvulnerableTime;
                    if (!target.Alive)
                    {
                        SetCombatState(target, SideCombatState::Dead);
                    }

                    const float resistanceScale = std::clamp(1.0f - target.KnockbackResistance, 0.25f, 1.0f);
                    target.RuntimeVelocity.x = hitbox.LaunchVelocity.x * resistanceScale;
                    float launchY = hitbox.LaunchVelocity.y;
                    if (hitbox.Team == (int)SideCombatTeam::Player &&
                        registry.has<SideEnemyAIComponent>(targetEntity) &&
                        registry.get<SideEnemyAIComponent>(targetEntity).Kind == SideEnemyKind::BearBoss &&
                        launchY > 0.0f)
                    {
                        launchY *= tuning.BossLaunchBonus;
                    }
                    target.RuntimeAirVelocity = std::max(target.RuntimeAirVelocity, launchY * resistanceScale);
                    if (launchY > 0.0f)
                    {
                        target.RuntimeAirHeight = std::max(target.RuntimeAirHeight, 0.05f);
                        target.RuntimeOnGround = false;
                        if (target.Alive)
                            SetCombatState(target, SideCombatState::Launched);
                    }
                    else if (target.Alive)
                    {
                        SetCombatState(target, SideCombatState::HitStun, hitbox.HitStun);
                    }

                    ApplyPlayerAirHitReward(scene, level, hitbox, target);
                    AwardMagicSwordGauge(scene, level, hitbox, target);
                    ApplyBossProtectionOnHit(scene, level, tuning, targetEntity, target, hitbox);
                    const bool bossProtectionTriggered =
                        hitbox.Team == (int)SideCombatTeam::Player &&
                        IsBossEntity(registry, targetEntity) &&
                        target.RuntimeState == SideCombatState::SuperArmor;

                    if (hitbox.Team == (int)SideCombatTeam::Player && !bossProtectionTriggered)
                        RegisterPlayerHit(level);
                    else
                        level.RuntimeComboCount = 0;

                    hitbox.RuntimeHitSomething = true;
                    if (hitbox.DestroyOnHit)
                    {
                        hitboxesToDestroy.push_back(e);
                        break;
                    }
                }
            }

            for (auto e : hitboxesToDestroy)
            {
                if (registry.valid(e))
                    scene->DestroyEntity({ e, scene });
            }
        }

        static void UpdateCombatantPhysics(Scene* scene,
            const SideCombatLevelComponent& level,
            float dt)
        {
            const SideCombatTuning& tuning = GetTuning(level);
            const float laneMinY = level.LaneMinY < level.LaneMaxY ? level.LaneMinY : tuning.LaneMinY;
            const float laneMaxY = level.LaneMinY < level.LaneMaxY ? level.LaneMaxY : tuning.LaneMaxY;

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TransformComponent, SideCombatantComponent>())
            {
                auto& transform = registry.get<TransformComponent>(e);
                auto& combatant = registry.get<SideCombatantComponent>(e);

                combatant.RuntimeHitStun = std::max(0.0f, combatant.RuntimeHitStun - dt);
                combatant.RuntimeInvulnerableTimer = std::max(0.0f, combatant.RuntimeInvulnerableTimer - dt);
                combatant.RuntimeStateTimer = std::max(0.0f, combatant.RuntimeStateTimer - dt);

                if (!combatant.Alive)
                {
                    SetCombatState(combatant, SideCombatState::Dead);
                    continue;
                }

                float gravity = GravityDefault;
                if (registry.has<SidePlayerControllerComponent>(e))
                    gravity = registry.get<SidePlayerControllerComponent>(e).Gravity;

                if (!combatant.RuntimeOnGround)
                {
                    combatant.RuntimeAirVelocity -= gravity * combatant.GravityScale * dt;
                    combatant.RuntimeAirHeight += combatant.RuntimeAirVelocity * dt;
                }

                if (combatant.RuntimeAirHeight <= 0.0f)
                {
                    combatant.RuntimeAirHeight = 0.0f;
                    combatant.RuntimeAirVelocity = 0.0f;
                    combatant.RuntimeOnGround = true;
                }
                else
                {
                    combatant.RuntimeOnGround = false;
                }

                combatant.RuntimeGroundPosition += combatant.RuntimeVelocity * dt;
                combatant.RuntimeGroundPosition.x = std::clamp(
                    combatant.RuntimeGroundPosition.x,
                    level.ArenaMin.x + combatant.CollisionSize.x * 0.5f,
                    level.ArenaMax.x - combatant.CollisionSize.x * 0.5f);
                combatant.RuntimeGroundPosition.y = std::clamp(
                    combatant.RuntimeGroundPosition.y,
                    laneMinY + combatant.CollisionSize.y * 0.5f,
                    laneMaxY - combatant.CollisionSize.y * 0.5f);
                combatant.RuntimeAirHeight = std::min(combatant.RuntimeAirHeight, std::max(0.0f, level.ArenaMax.y - laneMinY));

                const bool boss = IsBossEntity(registry, e);
                if (boss)
                    combatant.RuntimeProtectionMax = std::max(1.0f, tuning.Protection.BossProtectionMax);

                if (combatant.RuntimeState == SideCombatState::SuperArmor)
                {
                    combatant.RuntimeInvulnerableTimer = std::max(combatant.RuntimeInvulnerableTimer, 0.05f);
                    if (combatant.RuntimeStateTimer <= 0.0f && combatant.RuntimeOnGround)
                    {
                        combatant.RuntimeProtection = 0.0f;
                        SetCombatState(combatant, SideCombatState::Recovery, tuning.Protection.GroundResetDelay);
                    }
                }
                else if (combatant.RuntimeState == SideCombatState::Recovery ||
                    combatant.RuntimeState == SideCombatState::Broken)
                {
                    if (combatant.RuntimeStateTimer <= 0.0f)
                    {
                        if (combatant.RuntimeHitStun > 0.0f)
                            SetCombatState(combatant, SideCombatState::HitStun, combatant.RuntimeHitStun);
                        else if (!combatant.RuntimeOnGround)
                            SetCombatState(combatant, SideCombatState::Launched);
                        else
                            SetCombatState(combatant, SideCombatState::Normal);
                    }
                }
                else if (combatant.RuntimeHitStun > 0.0f)
                {
                    combatant.RuntimeState = SideCombatState::HitStun;
                    combatant.RuntimeStateTimer = std::max(combatant.RuntimeStateTimer, combatant.RuntimeHitStun);
                }
                else if (!combatant.RuntimeOnGround)
                {
                    combatant.RuntimeState = SideCombatState::Launched;
                }
                else
                {
                    combatant.RuntimeState = SideCombatState::Normal;
                }

                if (boss &&
                    combatant.RuntimeOnGround &&
                    combatant.RuntimeState == SideCombatState::Normal &&
                    combatant.RuntimeProtection > 0.0f)
                {
                    combatant.RuntimeProtection = std::max(
                        0.0f,
                        combatant.RuntimeProtection - tuning.Protection.BossProtectionDecayPerSecond * dt);
                }

                UpdateCombatantVisual(scene, { e, scene }, level, tuning);
            }
        }

        static void SpawnDeathRewards(Scene* scene,
            const SideCombatLevelComponent& level,
            const TransformComponent& transform,
            const SideEnemyAIComponent* ai)
        {
            const bool boss = ai && ai->Kind == SideEnemyKind::BearBoss;
            const SidePickupTuning& pickupTuning = GetTuning(level).Pickup;
            if (boss)
            {
                CreatePickup(scene, "Drop_MagicCore",
                    transform.Translation + glm::vec3(-0.42f, 0.55f, 0.03f),
                    "MAT-MAGIC-CORE-T0", "魔核碎片", 1,
                    "assets/vertical_slice/side_combat/ui/mat_magic_core.png",
                    pickupTuning);
                CreatePickup(scene, "Drop_BeastSinew",
                    transform.Translation + glm::vec3(0.0f, 0.72f, 0.03f),
                    "MAT-BEAST-SINEW", "兽筋", 2,
                    "assets/vertical_slice/side_combat/ui/mat_beast_sinew.png",
                    pickupTuning);
                CreatePickup(scene, "Drop_BeastClaw",
                    transform.Translation + glm::vec3(0.42f, 0.55f, 0.03f),
                    "MAT-BEAST-CLAW", "熊爪", 1,
                    "assets/vertical_slice/side_combat/ui/mat_beast_claw.png",
                    pickupTuning);
                return;
            }

            CreatePickup(scene, "Drop_BeastSinew",
                transform.Translation + glm::vec3(0.0f, 0.45f, 0.03f),
                "MAT-BEAST-SINEW", "兽筋", 1,
                "assets/vertical_slice/side_combat/ui/mat_beast_sinew.png",
                pickupTuning);
        }

        static void UpdateDeathsAndVictory(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player)
        {
            bool anyAliveEnemy = false;
            auto& registry = scene->GetRegistry();

            for (auto e : registry.view<TransformComponent, SideCombatantComponent>())
            {
                auto& transform = registry.get<TransformComponent>(e);
                auto& combatant = registry.get<SideCombatantComponent>(e);
                if (combatant.Team != (int)SideCombatTeam::Enemy)
                    continue;

                if (combatant.Alive)
                {
                    anyAliveEnemy = true;
                    continue;
                }

                if (combatant.RuntimeDeathProcessed)
                    continue;

                combatant.RuntimeDeathProcessed = true;
                combatant.RuntimeVelocity = { 0.0f, 0.0f };
                const SideEnemyAIComponent* ai = registry.has<SideEnemyAIComponent>(e)
                    ? &registry.get<SideEnemyAIComponent>(e)
                    : nullptr;
                SpawnDeathRewards(scene, level, transform, ai);

                if (registry.has<SpriteRendererComponent>(e))
                    registry.get<SpriteRendererComponent>(e).Color.a = 0.18f;
            }

            if (!anyAliveEnemy && !level.RuntimeVictory && !level.RuntimeDefeat)
            {
                level.RuntimeVictory = true;
                level.RuntimeResultTimer = 0.0f;
                level.RuntimeResultCommandIssued = false;
                level.RuntimeRewardsSpawned = true;
                GameProgress::RecordDungeonClear(level.LevelId, level.RuntimeBestCombo, 120, 45);
                if (player && player.HasComponent<SideCombatantComponent>())
                    player.GetComponent<SideCombatantComponent>().ControlsLocked = true;
            }
        }

        static void UpdatePickups(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            float dt)
        {
            if (!player || !player.HasComponent<TransformComponent>())
                return;

            const glm::vec3 playerPosition = player.GetComponent<TransformComponent>().Translation;
            auto& registry = scene->GetRegistry();
            std::vector<entt::entity> picked;

            for (auto e : registry.view<TransformComponent, SidePickupComponent>())
            {
                auto& transform = registry.get<TransformComponent>(e);
                auto& pickup = registry.get<SidePickupComponent>(e);
                const glm::vec2 toPlayer = ToVec2(playerPosition - transform.Translation);
                const float distance = glm::length(toPlayer);
                if (distance <= pickup.PickupRadius)
                {
                    level.RuntimeCollectedPickups += pickup.Amount;
                    GameProgress::AddMaterial(pickup.ItemId, pickup.DisplayName, pickup.Amount);
                    picked.push_back(e);
                    continue;
                }

                if (distance <= pickup.AttractRadius || level.RuntimeVictory)
                {
                    const glm::vec2 direction = distance > 0.001f
                        ? toPlayer / distance
                        : glm::vec2{ 0.0f, 0.0f };
                    transform.Translation += glm::vec3(direction * pickup.AttractSpeed * dt, 0.0f);
                }
            }

            for (auto e : picked)
            {
                if (registry.valid(e))
                    scene->DestroyEntity({ e, scene });
            }
        }

        static void UpdateCombo(SideCombatLevelComponent& level, float dt)
        {
            if (level.RuntimeComboTimer <= 0.0f)
                return;

            level.RuntimeComboTimer = std::max(0.0f, level.RuntimeComboTimer - dt);
            if (level.RuntimeComboTimer <= 0.0f)
                level.RuntimeComboCount = 0;
        }

        static void UpdateUI(Scene* scene,
            SideCombatLevelComponent& level,
            Entity player,
            Entity boss)
        {
            const SideCombatantComponent* playerCombatant =
                player && player.HasComponent<SideCombatantComponent>()
                ? &player.GetComponent<SideCombatantComponent>()
                : nullptr;
            const SideCombatantComponent* bossCombatant =
                boss && boss.HasComponent<SideCombatantComponent>()
                ? &boss.GetComponent<SideCombatantComponent>()
                : nullptr;
            const SidePlayerControllerComponent* controller =
                player && player.HasComponent<SidePlayerControllerComponent>()
                ? &player.GetComponent<SidePlayerControllerComponent>()
                : nullptr;
            const SideCombatTuning& tuning = GetTuning(level);

            if (playerCombatant)
            {
                SetProgress(scene, level.PlayerHealthBarEntityName, playerCombatant->Health, playerCombatant->MaxHealth);
                SetText(scene, level.PlayerHealthTextEntityName,
                    "生命 " + FormatFloat(playerCombatant->Health) + "/" + FormatFloat(playerCombatant->MaxHealth));
            }
            if (bossCombatant)
            {
                SetProgress(scene, level.BossHealthBarEntityName, bossCombatant->Health, bossCombatant->MaxHealth);
                std::string bossText = "黑熊丈夫 " + FormatFloat(bossCombatant->Health) + "/" + FormatFloat(bossCombatant->MaxHealth);
                if (ShouldShowCombatStateHud(level, tuning))
                {
                    bossText += "  " + std::string(GetCombatStateLabel(bossCombatant->RuntimeState));
                }
                if (ShouldShowBossProtectionHud(level, tuning) && bossCombatant->RuntimeProtectionMax > 0.0f)
                {
                    bossText += " 保护 " + FormatFloat(bossCombatant->RuntimeProtection) + "/" + FormatFloat(bossCombatant->RuntimeProtectionMax);
                }
                SetText(scene, level.BossHealthTextEntityName, bossText);
            }

            const bool showBreakLimitUi = ShouldShowBreakLimitUi(level, tuning);
            const std::string breakLimitInputText = IsBreakLimitOfficiallyAvailable(level, tuning) ? "L断限" : "L调试断";
            std::string message = "A/D推进  W/S纵深  Space跳";
            if (IsSkillUnlocked(level, tuning, "basic_attack") || IsSkillUnlocked(level, tuning, "air_basic"))
                message += "  J斩/空斩";
            if (IsSkillUnlocked(level, tuning, "launcher") || IsSkillUnlocked(level, tuning, "air_chase"))
                message += "  K挑/追";
            if (IsSkillUnlocked(level, tuning, "magic_bolt"))
                message += "  U魔";
            if (IsSkillUnlocked(level, tuning, "ally_support"))
                message += "  I支";
            if (showBreakLimitUi)
                message += "  " + breakLimitInputText;
            if (level.RuntimeVictory)
                message = "黑熊丈夫倒下了。材料会自动靠近魔剑。";
            else if (level.RuntimeDefeat)
                message = "被打倒了，重新来过。";
            else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
                playerCombatant->RuntimeAirHeight >= tuning.AirCombo.HighAirSafetyHeight)
                message = "高空连段成立，普通地面攻击够不到你。";
            else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
                playerCombatant->RuntimeAirHeight <= tuning.AirCombo.GroundThreatHeight)
                message = "低空危险，地面攻击仍可能打断空连。";
            else if (level.RuntimeComboCount >= 6 &&
                showBreakLimitUi)
                message = "保护临界且快坠落时可以断限，刷新跳跃和空中动作。";

            SetText(scene, level.MessageTextEntityName, message);

            if (level.RuntimeComboCount > 0)
            {
                SetText(scene, level.ComboTextEntityName,
                    "连击 x" + std::to_string(level.RuntimeComboCount)
                    + "  最佳 x" + std::to_string(level.RuntimeBestCombo));
            }
            else
            {
                SetText(scene, level.ComboTextEntityName,
                    "最佳连击 x" + std::to_string(level.RuntimeBestCombo));
            }

            if (controller)
            {
                std::string skillText;
                if (IsSkillUnlocked(level, tuning, "launcher") || IsSkillUnlocked(level, tuning, "air_chase"))
                    skillText += "K " + FormatFloat(controller->RuntimeLauncherCooldown, 1);
                if (IsSkillUnlocked(level, tuning, "magic_bolt"))
                {
                    if (!skillText.empty())
                        skillText += "  ";
                    skillText += "U " + FormatFloat(controller->RuntimeMagicBoltCooldown, 1);
                }
                if (IsSkillUnlocked(level, tuning, "ally_support"))
                {
                    if (!skillText.empty())
                        skillText += "  ";
                    skillText += "I " + FormatFloat(controller->RuntimeAllySupportCooldown, 1);
                }
                if (showBreakLimitUi)
                {
                    if (!skillText.empty())
                        skillText += "  ";
                    skillText += "L " + FormatFloat(controller->RuntimeBreakLimitCooldown, 1);
                }
                if (!skillText.empty())
                    skillText += "  ";
                skillText += "槽 " + FormatFloat(controller->RuntimeMagicSwordGauge, 1) + "/" + FormatFloat(controller->RuntimeMagicSwordGaugeMax, 0)
                    + "  空击 " + std::to_string(controller->RuntimeAirActionsRemaining);
                SetText(scene, level.SkillTextEntityName, skillText);
            }

            std::string reward = level.RuntimeVictory
                ? level.FirstClearRewardText
                : "掉落: 魔核碎片 / 兽筋 / 熊爪";
            if (level.RuntimeCollectedPickups > 0)
                reward += "  已吸收 " + std::to_string(level.RuntimeCollectedPickups);
            SetText(scene, level.RewardTextEntityName, reward);
        }

        static void UpdateResultTransition(Scene* scene,
            SideCombatLevelComponent& level,
            float dt)
        {
            if (!level.RuntimeVictory && !level.RuntimeDefeat)
                return;

            level.RuntimeResultTimer += dt;
            const bool victory = level.RuntimeVictory;
            const std::string& command = victory ? level.VictorySceneCommand : level.DefeatSceneCommand;
            if (command.empty())
                return;

            const float delay = std::max(0.0f, victory ? level.VictoryReturnDelay : level.DefeatReturnDelay);
            const float fadeDuration = std::max(0.01f, level.ResultSceneFadeDuration);
            const float fadeStart = std::max(0.0f, delay - fadeDuration);
            const float fade = std::clamp((level.RuntimeResultTimer - fadeStart) / fadeDuration, 0.0f, 1.0f);
            SetImageAlpha(scene, level.FadeEntityName, fade);

            if (!level.RuntimeResultCommandIssued && level.RuntimeResultTimer >= delay)
            {
                level.RuntimeResultCommandIssued = true;
                level.RuntimeRequestedCommand = command;
            }
        }

    } // namespace

    void SideCombatSystem::ResetInputState()
    {
        m_PreviousPausePressed = false;
        m_PreviousJumpPressed = false;
        m_PreviousBasicPressed = false;
        m_PreviousLauncherPressed = false;
        m_PreviousMagicPressed = false;
        m_PreviousSupportPressed = false;
        m_PreviousBreakLimitPressed = false;
    }

    void SideCombatSystem::OnRuntimeStart(Scene* scene)
    {
        ResetInputState();
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<SideCombatLevelComponent>())
        {
            auto& level = registry.get<SideCombatLevelComponent>(e);
            if (!level.PlayOnStart)
                continue;

            ResetLevelRuntime(scene, level);
            ResetCombatants(scene, level);
        }
    }

    void SideCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        const float dt = std::min(0.05f, ts.GetSeconds());
        auto& registry = scene->GetRegistry();

        const bool pausePressed = Input::IsKeyPressed(WT_KEY_ESCAPE) || Input::IsKeyPressed(WT_KEY_P);
        const bool jumpPressed = Input::IsKeyPressed(WT_KEY_SPACE);
        const bool basicPressed = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT) || Input::IsKeyPressed(WT_KEY_J);
        const bool launcherPressed = Input::IsKeyPressed(WT_KEY_K);
        const bool magicPressed = Input::IsKeyPressed(WT_KEY_U);
        const bool supportPressed = Input::IsKeyPressed(WT_KEY_I);
        const bool breakLimitPressed = Input::IsKeyPressed(WT_KEY_L);

        for (auto levelEntity : registry.view<SideCombatLevelComponent>())
        {
            auto& level = registry.get<SideCombatLevelComponent>(levelEntity);
            if (!level.PlayOnStart)
                continue;

            level.RuntimeElapsed += dt;
            UpdateStartFade(scene, level, dt);

            if (pausePressed && !m_PreviousPausePressed)
                level.RuntimePaused = !level.RuntimePaused;

            Entity player = FindEntityByName(scene, level.PlayerEntityName);
            Entity boss = FindEntityByName(scene, level.BossEntityName);

            if (!level.RuntimePaused)
            {
                UpdateCombo(level, dt);
                UpdatePlayer(scene, level, player, dt,
                    jumpPressed,
                    basicPressed,
                    launcherPressed,
                    magicPressed,
                    supportPressed,
                    breakLimitPressed,
                    m_PreviousJumpPressed,
                    m_PreviousLauncherPressed,
                    m_PreviousMagicPressed,
                    m_PreviousSupportPressed,
                    m_PreviousBreakLimitPressed);
                UpdateEnemyAI(scene, level, player, dt);
                UpdateHitboxes(scene, level, dt);
                UpdateCombatantPhysics(scene, level, dt);
                UpdateDeathsAndVictory(scene, level, player);
                UpdatePickups(scene, level, player, dt);
                UpdateResultTransition(scene, level, dt);
            }

            UpdateUI(scene, level, player, boss);
        }

        m_PreviousPausePressed = pausePressed;
        m_PreviousJumpPressed = jumpPressed;
        m_PreviousBasicPressed = basicPressed;
        m_PreviousLauncherPressed = launcherPressed;
        m_PreviousMagicPressed = magicPressed;
        m_PreviousSupportPressed = supportPressed;
        m_PreviousBreakLimitPressed = breakLimitPressed;
    }

} // namespace Wheatear
