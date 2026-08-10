#include "wtpch.h"
#include "SideCombatTuningService.h"

#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Modules/Common/GameplayTextService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace Wheatear::SideCombatTuningService {
    float CalculateSortZ(float groundY, const SideCombatTuning& tuning)
        {
            return -0.08f + (tuning.LaneMaxY - groundY) * tuning.SortScale;
        }

        static std::string FormatFramePath(const std::string& pattern, int frame)
        {
            return GameplayTextService::FormatFramePath(pattern, frame);
        }

        static std::string ReadAtlasPath(const YAML::Node& node, const std::string& fallback)
        {
            if (!node || !node.IsMap())
                return fallback;

            const YAML::Node pathNode = node["sheetPath"] ? node["sheetPath"]
                : (node["sheet"] ? node["sheet"]
                    : (node["texture"] ? node["texture"]
                        : node["path"]));
            return AssetAliasRegistry::Resolve(pathNode.as<std::string>(fallback));
        }

        static GameplayVisualService::TextureAtlasFrameSpec ReadAtlasFrameSpec(
            const YAML::Node& node,
            const GameplayVisualService::TextureAtlasFrameSpec& fallback)
        {
            auto atlas = fallback;
            if (!node || !node.IsMap())
                return atlas;

            atlas.SheetPath = ReadAtlasPath(node, atlas.SheetPath);
            atlas.CellWidth = node["cellWidth"].as<int>(atlas.CellWidth);
            atlas.CellHeight = node["cellHeight"].as<int>(atlas.CellHeight);
            atlas.Columns = node["columns"].as<int>(atlas.Columns);
            atlas.StartFrame = node["startFrame"].as<int>(atlas.StartFrame);
            return atlas;
        }

        static GameplayVisualService::TextureAtlasFrameSpec MakeAtlas(
            const std::string& sheetPath,
            int cellWidth,
            int cellHeight,
            int columns,
            int startFrame = 0)
        {
            GameplayVisualService::TextureAtlasFrameSpec atlas;
            atlas.SheetPath = sheetPath;
            atlas.CellWidth = cellWidth;
            atlas.CellHeight = cellHeight;
            atlas.Columns = columns;
            atlas.StartFrame = startFrame;
            return atlas;
        }

        static SideAnimationClipTuning MakeAnimationClip(
            const std::string& pattern,
            int frameCount,
            float frameRate,
            bool loop = true,
            glm::vec2 renderScale = { 1.0f, 1.0f },
            glm::vec2 renderOffset = { 0.0f, 0.0f },
            GameplayVisualService::TextureAtlasFrameSpec atlas = {})
        {
            SideAnimationClipTuning clip;
            clip.Pattern = pattern;
            clip.Atlas = std::move(atlas);
            clip.FrameCount = std::max(1, frameCount);
            clip.FrameRate = std::max(1.0f, frameRate);
            clip.Loop = loop;
            clip.RenderScale = renderScale;
            clip.RenderOffset = renderOffset;
            return clip;
        }

        static void AddAnimationClip(
            SideAnimationSetTuning& set,
            const std::string& key,
            const std::string& pattern,
            int frameCount,
            float frameRate,
            bool loop = true,
            glm::vec2 renderScale = { 1.0f, 1.0f },
            glm::vec2 renderOffset = { 0.0f, 0.0f },
            GameplayVisualService::TextureAtlasFrameSpec atlas = {})
        {
            set.Clips[key] = MakeAnimationClip(pattern, frameCount, frameRate, loop, renderScale, renderOffset, std::move(atlas));
        }

        static std::filesystem::path ResolveTuningPath(const std::string& path)
        {
            if (path.empty())
                return {};

            const std::filesystem::path requested(AssetAliasRegistry::Resolve(path));
            return AssetPath::ResolveRuntimeData(requested);
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
            tuning.Startup = node["startup"].as<float>(tuning.Startup);
            tuning.Recovery = node["recovery"].as<float>(tuning.Recovery);
            tuning.CancelWindowStart = node["cancelWindowStart"].as<float>(tuning.CancelWindowStart);
            tuning.CancelWindowEnd = node["cancelWindowEnd"].as<float>(tuning.CancelWindowEnd);
            tuning.MovementScale = node["movementScale"].as<float>(tuning.MovementScale);
            tuning.HitStun = node["hitStun"].as<float>(tuning.HitStun);
            tuning.AttackerAirImpulse = node["attackerAirImpulse"].as<float>(tuning.AttackerAirImpulse);
            tuning.AttackerAirFallStep = node["attackerAirFallStep"].as<float>(tuning.AttackerAirFallStep);
            tuning.TargetAirFallStep = node["targetAirFallStep"].as<float>(tuning.TargetAirFallStep);
            tuning.ProtectionGain = node["protectionGain"].as<float>(tuning.ProtectionGain);
            tuning.DestroyOnHit = node["destroyOnHit"].as<bool>(tuning.DestroyOnHit);
            tuning.TextureFramePattern = AssetAliasRegistry::Resolve(
                node["textureFramePattern"].as<std::string>(tuning.TextureFramePattern));
            tuning.TextureAtlas = ReadAtlasFrameSpec(node["textureAtlas"], tuning.TextureAtlas);
            tuning.TextureFrameCount = node["textureFrameCount"].as<int>(tuning.TextureFrameCount);
            tuning.TextureFrameRate = node["textureFrameRate"].as<float>(tuning.TextureFrameRate);
            tuning.SwingSound = AssetAliasRegistry::Resolve(
                node["swingSound"].as<std::string>(tuning.SwingSound));
            tuning.HitSound = AssetAliasRegistry::Resolve(
                node["hitSound"].as<std::string>(tuning.HitSound));
            tuning.SoundVolume = node["soundVolume"].as<float>(tuning.SoundVolume);
            tuning.HitPause = node["hitPause"].as<float>(tuning.HitPause);
            tuning.CameraShake = node["cameraShake"].as<float>(tuning.CameraShake);
            tuning.CameraShakeDuration = node["cameraShakeDuration"].as<float>(tuning.CameraShakeDuration);
            return tuning;
        }

        static SideFeedbackTuning ReadFeedbackTuning(const YAML::Node& node, const SideFeedbackTuning& fallback)
        {
            SideFeedbackTuning tuning = fallback;
            if (!node)
                return tuning;

            tuning.HitPauseTimeScale = node["hitPauseTimeScale"].as<float>(tuning.HitPauseTimeScale);
            tuning.BreakLimitCinematicDuration = node["breakLimitCinematicDuration"].as<float>(
                tuning.BreakLimitCinematicDuration);
            tuning.BreakLimitCinematicTimeScale = node["breakLimitCinematicTimeScale"].as<float>(
                tuning.BreakLimitCinematicTimeScale);
            tuning.BreakLimitCameraZoom = node["breakLimitCameraZoom"].as<float>(
                tuning.BreakLimitCameraZoom);
            tuning.BreakLimitCameraOffset = ReadVec2(node["breakLimitCameraOffset"],
                tuning.BreakLimitCameraOffset);
            tuning.JumpSound = AssetAliasRegistry::Resolve(node["jumpSound"].as<std::string>(tuning.JumpSound));
            tuning.LandSound = AssetAliasRegistry::Resolve(node["landSound"].as<std::string>(tuning.LandSound));
            tuning.JumpSoundVolume = node["jumpSoundVolume"].as<float>(tuning.JumpSoundVolume);
            tuning.LandSoundVolume = node["landSoundVolume"].as<float>(tuning.LandSoundVolume);
            return tuning;
        }

        static SideAnimationClipTuning ReadAnimationClip(
            const YAML::Node& node,
            const SideAnimationClipTuning& fallback)
        {
            SideAnimationClipTuning clip = fallback;
            if (!node)
                return clip;

            clip.Pattern = node["pattern"].as<std::string>(clip.Pattern);
            clip.Atlas = ReadAtlasFrameSpec(node["atlas"] ? node["atlas"] : node, clip.Atlas);
            clip.FrameCount = node["frameCount"].as<int>(clip.FrameCount);
            clip.FrameRate = node["frameRate"].as<float>(clip.FrameRate);
            clip.Loop = node["loop"].as<bool>(clip.Loop);
            clip.RenderOffset = ReadVec2(node["renderOffset"], clip.RenderOffset);
            clip.RenderScale = ReadVec2(node["renderScale"], clip.RenderScale);
            clip.FrameCount = std::max(1, clip.FrameCount);
            clip.FrameRate = std::max(1.0f, clip.FrameRate);
            return clip;
        }

        static SideAnimationSetTuning ReadAnimationSet(
            const YAML::Node& node,
            const SideAnimationSetTuning& fallback)
        {
            SideAnimationSetTuning set = fallback;
            if (!node || !node.IsMap())
                return set;

            for (auto it = node.begin(); it != node.end(); ++it)
            {
                const std::string key = it->first.as<std::string>("");
                if (key.empty())
                    continue;

                const auto fallbackIt = set.Clips.find(key);
                const SideAnimationClipTuning fallbackClip = fallbackIt != set.Clips.end()
                    ? fallbackIt->second
                    : SideAnimationClipTuning{};
                set.Clips[key] = ReadAnimationClip(it->second, fallbackClip);
            }

            return set;
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
            tuning.JumpBufferTime = node["jumpBufferTime"].as<float>(tuning.JumpBufferTime);
            tuning.CoyoteTime = node["coyoteTime"].as<float>(tuning.CoyoteTime);
            tuning.LaneSpeedScale = node["laneSpeedScale"].as<float>(tuning.LaneSpeedScale);
            tuning.LaneAcceleration = node["laneAcceleration"].as<float>(tuning.LaneAcceleration);
            tuning.GroundAcceleration = node["groundAcceleration"].as<float>(tuning.GroundAcceleration);
            tuning.GroundFriction = node["groundFriction"].as<float>(tuning.GroundFriction);
            tuning.BasicCooldown = node["basicCooldown"].as<float>(tuning.BasicCooldown);
            tuning.BasicFinisherExtraCooldown = node["basicFinisherExtraCooldown"].as<float>(tuning.BasicFinisherExtraCooldown);
            tuning.LauncherCooldown = node["launcherCooldown"].as<float>(tuning.LauncherCooldown);
            tuning.MagicBoltCooldown = node["magicBoltCooldown"].as<float>(tuning.MagicBoltCooldown);
            tuning.AllySupportCooldown = node["allySupportCooldown"].as<float>(tuning.AllySupportCooldown);
            tuning.DashCooldown = node["dashCooldown"].as<float>(tuning.DashCooldown);
            tuning.HealItemCooldown = node["healItemCooldown"].as<float>(tuning.HealItemCooldown);
            tuning.ManaItemCooldown = node["manaItemCooldown"].as<float>(tuning.ManaItemCooldown);
            tuning.AttackBuffItemCooldown = node["attackBuffItemCooldown"].as<float>(tuning.AttackBuffItemCooldown);
            tuning.DashManaCost = node["dashManaCost"].as<float>(tuning.DashManaCost);
            tuning.DashSpeed = node["dashSpeed"].as<float>(tuning.DashSpeed);
            tuning.DashInvulnerableTime = node["dashInvulnerableTime"].as<float>(tuning.DashInvulnerableTime);
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
            AddDefaultSkill(tuning, "launcher", "裂空上挑", "S+J", "地面浮空起手", { "launcher" }, 2, true);
            AddDefaultSkill(tuning, "air_chase", "空中追斩", "空中 S+J", "空中续连 / 低空补救", { "air_chase" }, 2, true);
            AddDefaultSkill(tuning, "magic_bolt", "火球术", "U", "远程补 hit / 空中魔法续连", { "magic_bolt" }, 2, true);
            AddDefaultSkill(tuning, "ally_support", "真青梅支援", "I", "支援浮空 / 新手容错", { "ally_support" }, 2, true);
            AddDefaultSkill(tuning, "break_limit", "Break Limit", "L", "Clears boss protection during super armor", { "break_limit" }, 7, false);
            AddDefaultSkill(tuning, "dash", "冲刺", "O", "短暂无敌位移", { "dash" }, 2, true);
            const std::vector<std::string> chapterTwoSkills = {
                "basic_attack",
                "air_basic",
                "launcher",
                "air_chase",
                "dash",
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

        static void AddDefaultAnimationData(SideCombatTuning& tuning)
        {
            const glm::vec2 bodyScale = { 1.0f, 1.0f };
            const glm::vec2 bodyOffset = { 0.0f, 0.3398f };
            const glm::vec2 bodyTallScale = { 1.0f, 1.25f };
            const glm::vec2 bodyTallOffset = { 0.0f, 0.4453f };
            const glm::vec2 slashScale = { 1.25f, 1.0f };
            const glm::vec2 dashWideScale = { 2.0f, 1.0f };
            const glm::vec2 verticalScale = { 1.25f, 1.25f };
            const glm::vec2 launcherScale = { 1.25f, 1.5f };
            const glm::vec2 launcherOffset = { 0.0f, 0.5781f };
            const glm::vec2 dashTallScale = { 1.5f, 1.25f };
            const glm::vec2 actionTallScale = { 1.60f, 1.60f };
            const glm::vec2 actionTallOffset = { 0.0f, 0.4453f };
            const glm::vec2 breakLimitScale = { 1.25f, 1.25f };
            const glm::vec2 breakLimitOffset = { 0.0f, 0.3398f };
            const glm::vec2 floorScale = { 2.0f, 1.0f };

            const glm::vec2 gruntScale = { 1.0f, 1.0f };
            const glm::vec2 gruntOffset = { 0.0f, 0.2969f };
            const glm::vec2 gruntWideScale = { 1.5f, 1.0f };
            const glm::vec2 gruntAirWideScale = { 1.5f, 1.3333f };
            const glm::vec2 gruntAirOffset = { 0.0f, 0.3516f };
            const glm::vec2 bossScale = { 1.0f, 1.0f };
            const glm::vec2 bossHitScale = { 1.30f, 1.30f };
            const glm::vec2 bossOffset = { 0.0f, 0.3151f };
            const glm::vec2 bossHitOffset = { 0.0f, 0.4351f };
            const glm::vec2 bossClawScale = { 1.85f, 1.85f };
            const glm::vec2 bossShockwaveScale = { 1.16f, 0.96f };
            const glm::vec2 bossWideScale = { 1.25f, 1.0f };

            AddAnimationClip(tuning.PlayerAnimations, "idle", "", 8, 12.0f, true, bodyScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_idle_sheet.png", 512, 512, 8));
            AddAnimationClip(tuning.PlayerAnimations, "run", "", 4, 18.0f, true, bodyScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_run_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.PlayerAnimations, "jump", "", 1, 18.0f, false, bodyScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_jump_sheet.png", 512, 512, 1));
            AddAnimationClip(tuning.PlayerAnimations, "fall", "", 1, 12.0f, true, bodyTallScale, bodyTallOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_fall_sheet.png", 512, 512, 1));
            AddAnimationClip(tuning.PlayerAnimations, "hit", "", 4, 18.0f, false, bodyScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_hit_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.PlayerAnimations, "dead", "", 8, 12.0f, false, floorScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_dead_sheet.png", 1024, 512, 8));
            AddAnimationClip(tuning.PlayerAnimations, "basic1", "", 4, 24.0f, false, slashScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_basic1_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.PlayerAnimations, "basic2", "", 4, 24.0f, false, slashScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_basic2_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.PlayerAnimations, "basic3", "", 4, 24.0f, false, dashWideScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_basic3_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.PlayerAnimations, "air_basic", "", 4, 24.0f, false, verticalScale, bodyTallOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_air_basic_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.PlayerAnimations, "launcher", "", 2, 24.0f, false, launcherScale, launcherOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_launcher_sheet.png", 512, 512, 2));
            AddAnimationClip(tuning.PlayerAnimations, "air_chase", "", 8, 24.0f, false, dashTallScale, bodyTallOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_air_chase_sheet.png", 768, 640, 8));
            AddAnimationClip(tuning.PlayerAnimations, "dash", "", 1, 8.0f, false, actionTallScale, actionTallOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_dash_sheet.png", 512, 512, 1));
            AddAnimationClip(tuning.PlayerAnimations, "magic_bolt", "", 9, 20.0f, false, bodyScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_magic_bolt_sheet.png", 512, 512, 9));
            AddAnimationClip(tuning.PlayerAnimations, "ally_support", "", 8, 18.0f, false, slashScale, bodyOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_ally_support_sheet.png", 640, 512, 8));
            AddAnimationClip(tuning.PlayerAnimations, "break_limit", "", 2, 8.0f, false, breakLimitScale, breakLimitOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_characters/protag_break_limit_sheet.png", 512, 512, 2));

            AddAnimationClip(tuning.GruntAnimations, "idle", "", 4, 7.0f, true, gruntScale, gruntOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/en_claw_beast_idle_sheet.png", 512, 384, 4));
            AddAnimationClip(tuning.GruntAnimations, "run", "", 5, 11.0f, true, gruntScale, gruntOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/en_claw_beast_run_sheet.png", 512, 384, 5));
            AddAnimationClip(tuning.GruntAnimations, "hit", "", 3, 12.0f, false, gruntWideScale, gruntOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/en_claw_beast_hit_sheet.png", 768, 384, 3));
            AddAnimationClip(tuning.GruntAnimations, "fall", "", 3, 9.0f, true, gruntAirWideScale, gruntAirOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/en_claw_beast_fall_sheet.png", 768, 512, 3));
            AddAnimationClip(tuning.GruntAnimations, "dead", "", 4, 7.0f, false, gruntWideScale, gruntOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/en_claw_beast_dead_sheet.png", 768, 384, 4));
            AddAnimationClip(tuning.GruntAnimations, "enemy_claw", "", 4, 14.0f, false, gruntWideScale, gruntOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/en_claw_beast_attack_sheet.png", 768, 384, 4));

            AddAnimationClip(tuning.BossAnimations, "idle", "", 4, 6.0f, true, bossScale, bossOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_idle_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.BossAnimations, "run", "", 4, 8.0f, true, bossScale, bossOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_walk_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.BossAnimations, "hit", "", 4, 10.0f, false, bossHitScale, bossHitOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_hit_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.BossAnimations, "fall", "", 4, 10.0f, false, bossHitScale, bossHitOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_fall_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.BossAnimations, "dead", "", 4, 7.0f, false, bossWideScale, bossOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_dead_sheet.png", 1280, 768, 4));
            AddAnimationClip(tuning.BossAnimations, "enemy_claw", "", 3, 5.0f, false, bossClawScale, bossOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_attack_claw_sheet.png", 512, 512, 3));
            AddAnimationClip(tuning.BossAnimations, "bear_charge", "", 4, 12.0f, true, bossScale, bossOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_charge_sheet.png", 512, 512, 4));
            AddAnimationClip(tuning.BossAnimations, "bear_charge_windup", "", 6, 12.0f, false, bossScale, bossOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_charge_windup_sheet.png", 512, 512, 6));
            AddAnimationClip(tuning.BossAnimations, "bear_shockwave", "", 4, 12.0f, false, bossShockwaveScale, bossOffset, MakeAtlas("assets/vertical_slice/side_combat/sheets/runtime_enemies/boss_bear_husband_shockwave_sheet.png", 512, 512, 4));
        }

        static void ApplyDefaultAttackFeedback(SideCombatTuning& tuning)
        {
            auto apply = [&](const std::string& id,
                const std::string& swing,
                const std::string& hit,
                float volume,
                float hitPause,
                float cameraShake,
                float cameraShakeDuration)
            {
                auto attackIt = tuning.Attacks.find(id);
                if (attackIt == tuning.Attacks.end())
                    return;

                auto& attack = attackIt->second;
                attack.SwingSound = swing;
                attack.HitSound = hit;
                attack.SoundVolume = volume;
                attack.HitPause = hitPause;
                attack.CameraShake = cameraShake;
                attack.CameraShakeDuration = cameraShakeDuration;
            };

            const std::string audioRoot = AssetAliasRegistry::Path("side.path.audio");
            apply("basic1", audioRoot + "swing_light.wav", audioRoot + "hit_light.wav", 0.70f, 0.035f, 0.016f, 0.060f);
            apply("basic2", audioRoot + "swing_light.wav", audioRoot + "hit_light.wav", 0.72f, 0.040f, 0.018f, 0.065f);
            apply("basic3", audioRoot + "swing_heavy.wav", audioRoot + "hit_heavy.wav", 0.80f, 0.055f, 0.028f, 0.085f);
            apply("air_basic", audioRoot + "swing_air.wav", audioRoot + "hit_air.wav", 0.68f, 0.032f, 0.014f, 0.052f);
            apply("launcher", audioRoot + "swing_upper.wav", audioRoot + "hit_launcher.wav", 0.82f, 0.065f, 0.034f, 0.095f);
            apply("air_chase", audioRoot + "swing_air.wav", audioRoot + "hit_air.wav", 0.76f, 0.046f, 0.024f, 0.075f);
            apply("dash", audioRoot + "swing_air.wav", audioRoot + "hit_launcher.wav", 0.76f, 0.040f, 0.020f, 0.070f);
            apply("magic_bolt", audioRoot + "magic_cast.wav", audioRoot + "magic_hit.wav", 0.78f, 0.040f, 0.018f, 0.070f);
            apply("ally_support", audioRoot + "support_cast.wav", audioRoot + "support_hit.wav", 0.78f, 0.052f, 0.026f, 0.085f);
            apply("break_limit", audioRoot + "break_limit.wav", audioRoot + "hit_launcher.wav", 0.88f, 0.070f, 0.040f, 0.120f);
            apply("enemy_claw", audioRoot + "enemy_swing.wav", audioRoot + "player_hit.wav", 0.70f, 0.030f, 0.016f, 0.060f);
            apply("bear_charge", audioRoot + "bear_charge.wav", audioRoot + "player_hit.wav", 0.80f, 0.045f, 0.026f, 0.085f);
            apply("bear_shockwave", audioRoot + "shockwave_cast.wav", audioRoot + "player_hit.wav", 0.74f, 0.035f, 0.020f, 0.070f);
        }

        static void ApplyDefaultAttackVisuals(SideCombatTuning& tuning)
        {
            auto apply = [&](const std::string& id,
                GameplayVisualService::TextureAtlasFrameSpec atlas,
                int frameCount,
                float frameRate)
            {
                auto attackIt = tuning.Attacks.find(id);
                if (attackIt == tuning.Attacks.end())
                    return;

                auto& attack = attackIt->second;
                attack.TextureFramePattern.clear();
                attack.TextureAtlas = std::move(atlas);
                attack.TextureFrameCount = std::max(1, frameCount);
                attack.TextureFrameRate = std::max(1.0f, frameRate);
            };

            const auto basicSlash = MakeAtlas("assets/vertical_slice/side_combat/vfx_sheets/runtime_effects/vfx_basic_slash_sheet.png", 320, 192, 4);
            apply("basic1", basicSlash, 4, 26.0f);
            apply("basic2", basicSlash, 4, 26.0f);
            apply("basic3", basicSlash, 4, 26.0f);
            apply("air_basic", basicSlash, 4, 28.0f);

            const auto launcherSlash = MakeAtlas("assets/vertical_slice/side_combat/vfx_sheets/runtime_effects/vfx_launcher_slash_sheet.png", 240, 320, 5);
            apply("launcher", launcherSlash, 5, 26.0f);
            apply("air_chase", launcherSlash, 5, 26.0f);
            apply("dash", launcherSlash, 5, 26.0f);

            apply("magic_bolt", MakeAtlas("assets/vertical_slice/side_combat/vfx_sheets/runtime_effects/vfx_magic_bolt_sheet.png", 288, 160, 4), 4, 18.0f);

            const auto support = MakeAtlas("assets/vertical_slice/side_combat/vfx_sheets/runtime_effects/vfx_ally_support_sheet.png", 256, 320, 5);
            apply("ally_support", support, 5, 18.0f);
            apply("break_limit", support, 5, 22.0f);

            apply("enemy_claw", MakeAtlas("assets/vertical_slice/side_combat/vfx_sheets/runtime_effects/vfx_enemy_claw_sheet.png", 256, 192, 3), 3, 18.0f);
            apply("bear_charge", MakeAtlas("assets/vertical_slice/side_combat/vfx_sheets/runtime_effects/vfx_boss_bear_charge_sheet.png", 320, 192, 3), 3, 16.0f);
            apply("bear_shockwave", MakeAtlas("assets/vertical_slice/side_combat/vfx_sheets/runtime_effects/vfx_boss_bear_shockwave_sheet.png", 384, 192, 4), 4, 16.0f);
        }

        static SideCombatTuning BuildDefaultTuning()
        {
            SideCombatTuning tuning;
            tuning.Feedback.JumpSound = AssetAliasRegistry::Resolve(tuning.Feedback.JumpSound);
            tuning.Feedback.LandSound = AssetAliasRegistry::Resolve(tuning.Feedback.LandSound);
            AddDefaultProgressionData(tuning);
            AddDefaultAnimationData(tuning);

            SideAttackTuning basic1;
            basic1.Size = { 1.30f, 0.70f };
            basic1.Offset = { 0.88f, 0.0f };
            basic1.AirHeight = 0.58f;
            basic1.AirRange = 1.16f;
            basic1.DamageScale = 0.70f;
            basic1.DamageFlat = 10.0f;
            basic1.Lifetime = 0.15f;
            basic1.Startup = 0.06f;
            basic1.Recovery = 0.10f;
            basic1.CancelWindowStart = 0.12f;
            basic1.CancelWindowEnd = 0.26f;
            basic1.MovementScale = 0.58f;
            basic1.HitStun = 0.30f;
            basic1.LaunchVelocity = { 1.15f, 1.8f };
            basic1.ProtectionGain = 9.0f;
            tuning.Attacks["basic1"] = basic1;

            SideAttackTuning basic2 = basic1;
            basic2.Size = { 1.42f, 0.74f };
            basic2.Offset = { 0.94f, 0.02f };
            basic2.DamageScale = 0.78f;
            basic2.LaunchVelocity = { 1.35f, 2.2f };
            basic2.Startup = 0.07f;
            basic2.Recovery = 0.12f;
            basic2.CancelWindowStart = 0.13f;
            basic2.CancelWindowEnd = 0.30f;
            basic2.MovementScale = 0.52f;
            basic2.ProtectionGain = 11.0f;
            tuning.Attacks["basic2"] = basic2;

            SideAttackTuning basic3 = basic1;
            basic3.Size = { 1.58f, 0.86f };
            basic3.Offset = { 1.02f, 0.02f };
            basic3.AirHeight = 0.70f;
            basic3.AirRange = 1.35f;
            basic3.DamageScale = 0.96f;
            basic3.DamageFlat = 13.0f;
            basic3.Lifetime = 0.17f;
            basic3.Startup = 0.09f;
            basic3.Recovery = 0.18f;
            basic3.CancelWindowStart = 0.19f;
            basic3.CancelWindowEnd = 0.39f;
            basic3.MovementScale = 0.36f;
            basic3.HitStun = 0.36f;
            basic3.LaunchVelocity = { 2.2f, 4.2f };
            basic3.ProtectionGain = 16.0f;
            tuning.Attacks["basic3"] = basic3;

            SideAttackTuning airBasic = basic1;
            airBasic.Size = { 1.64f, 0.62f };
            airBasic.Offset = { 0.94f, 0.0f };
            airBasic.AirHeight = 0.38f;
            airBasic.AirRange = 0.98f;
            airBasic.DamageScale = 0.40f;
            airBasic.DamageFlat = 7.0f;
            airBasic.Lifetime = 0.14f;
            airBasic.Startup = 0.04f;
            airBasic.Recovery = 0.10f;
            airBasic.CancelWindowStart = 0.09f;
            airBasic.CancelWindowEnd = 0.24f;
            airBasic.MovementScale = 0.90f;
            airBasic.HitStun = 0.36f;
            airBasic.LaunchVelocity = { 0.85f, 2.15f };
            airBasic.AttackerAirImpulse = 0.85f;
            airBasic.AttackerAirFallStep = 0.10f;
            airBasic.TargetAirFallStep = 0.08f;
            airBasic.ProtectionGain = 14.0f;
            tuning.Attacks["air_basic"] = airBasic;

            SideAttackTuning launcher;
            launcher.Size = { 1.10f, 0.86f };
            launcher.Offset = { 0.68f, 0.02f };
            launcher.AirHeight = 0.98f;
            launcher.AirRange = 1.95f;
            launcher.DamageScale = 0.74f;
            launcher.DamageFlat = 12.0f;
            launcher.Lifetime = 0.22f;
            launcher.Startup = 0.11f;
            launcher.Recovery = 0.20f;
            launcher.CancelWindowStart = 0.20f;
            launcher.CancelWindowEnd = 0.44f;
            launcher.MovementScale = 0.35f;
            launcher.HitStun = 0.54f;
            launcher.LaunchVelocity = { 0.95f, 9.8f };
            launcher.AttackerAirImpulse = 0.0f;
            launcher.ProtectionGain = 22.0f;
            tuning.Attacks["launcher"] = launcher;

            SideAttackTuning airChase = launcher;
            airChase.Size = { 1.34f, 0.92f };
            airChase.Offset = { 0.78f, 0.02f };
            airChase.AirHeight = 0.16f;
            airChase.AirRange = 2.00f;
            airChase.DamageScale = 0.58f;
            airChase.DamageFlat = 10.0f;
            airChase.Lifetime = 0.19f;
            airChase.Startup = 0.06f;
            airChase.Recovery = 0.14f;
            airChase.CancelWindowStart = 0.11f;
            airChase.CancelWindowEnd = 0.32f;
            airChase.MovementScale = 0.86f;
            airChase.HitStun = 0.46f;
            airChase.LaunchVelocity = { 0.75f, 3.4f };
            airChase.AttackerAirImpulse = 1.20f;
            airChase.AttackerAirFallStep = 0.12f;
            airChase.TargetAirFallStep = 0.06f;
            airChase.ProtectionGain = 18.0f;
            tuning.Attacks["air_chase"] = airChase;

            SideAttackTuning dash;
            dash.Size = { 1.26f, 0.74f };
            dash.Offset = { 0.88f, 0.0f };
            dash.Velocity = { 10.5f, 0.0f };
            dash.AirHeight = 0.56f;
            dash.AirRange = 1.02f;
            dash.DamageScale = 0.60f;
            dash.DamageFlat = 6.0f;
            dash.Lifetime = 0.24f;
            dash.Startup = 0.05f;
            dash.Recovery = 0.08f;
            dash.CancelWindowStart = 0.14f;
            dash.CancelWindowEnd = 0.30f;
            dash.MovementScale = 0.0f;
            dash.HitStun = 0.34f;
            dash.LaunchVelocity = { 1.35f, 1.80f };
            dash.ProtectionGain = 10.0f;
            dash.DestroyOnHit = true;
            tuning.Attacks["dash"] = dash;

            SideAttackTuning magic;
            magic.Size = { 0.86f, 0.56f };
            magic.Offset = { 0.85f, 0.0f };
            magic.Velocity = { 8.8f, 0.0f };
            magic.AirHeight = 0.72f;
            magic.AirRange = 1.0f;
            magic.DamageScale = 0.70f;
            magic.DamageFlat = 11.0f;
            magic.Lifetime = 1.15f;
            magic.Startup = 0.10f;
            magic.Recovery = 0.20f;
            magic.CancelWindowStart = 0.18f;
            magic.CancelWindowEnd = 0.40f;
            magic.MovementScale = 0.72f;
            magic.HitStun = 0.34f;
            magic.LaunchVelocity = { 1.2f, 3.1f };
            magic.ProtectionGain = 12.0f;
            tuning.Attacks["magic_bolt"] = magic;

            SideAttackTuning support;
            support.Size = { 1.35f, 1.05f };
            support.Offset = { 0.0f, 0.0f };
            support.AirHeight = 1.05f;
            support.AirRange = 2.10f;
            support.DamageScale = 0.55f;
            support.DamageFlat = 9.0f;
            support.Lifetime = 0.34f;
            support.Startup = 0.18f;
            support.Recovery = 0.24f;
            support.CancelWindowStart = 0.28f;
            support.CancelWindowEnd = 0.60f;
            support.MovementScale = 0.70f;
            support.HitStun = 0.58f;
            support.LaunchVelocity = { 0.0f, 6.8f };
            support.DestroyOnHit = false;
            support.ProtectionGain = 20.0f;
            tuning.Attacks["ally_support"] = support;

            SideAttackTuning breakLimit = support;
            breakLimit.Size = { 1.65f, 1.28f };
            breakLimit.Offset = { 0.62f, 0.0f };
            breakLimit.AirHeight = 0.05f;
            breakLimit.AirRange = 1.72f;
            breakLimit.DamageScale = 0.34f;
            breakLimit.DamageFlat = 6.0f;
            breakLimit.Lifetime = 0.24f;
            breakLimit.Startup = 0.04f;
            breakLimit.Recovery = 0.12f;
            breakLimit.CancelWindowStart = 0.10f;
            breakLimit.CancelWindowEnd = 0.28f;
            breakLimit.MovementScale = 0.92f;
            breakLimit.HitStun = 0.22f;
            breakLimit.LaunchVelocity = { 0.0f, 0.0f };
            breakLimit.AttackerAirImpulse = 0.0f;
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
            enemyClaw.Startup = 0.32f;
            enemyClaw.Recovery = 0.42f;
            enemyClaw.MovementScale = 0.12f;
            enemyClaw.HitStun = 0.32f;
            enemyClaw.LaunchVelocity = { 3.0f, 2.0f };
            tuning.Attacks["enemy_claw"] = enemyClaw;

            SideAttackTuning bearCharge = enemyClaw;
            bearCharge.Size = { 1.82f, 0.92f };
            bearCharge.Offset = { 1.05f, 0.0f };
            bearCharge.Velocity = { 7.2f, 0.0f };
            bearCharge.DamageScale = 0.82f;
            bearCharge.DamageFlat = 12.0f;
            bearCharge.Lifetime = 0.88f;
            bearCharge.Startup = 0.50f;
            bearCharge.Recovery = 0.26f;
            bearCharge.MovementScale = 1.0f;
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
            shockwave.Startup = 0.52f;
            shockwave.Recovery = 0.20f;
            shockwave.MovementScale = 0.05f;
            shockwave.HitStun = 0.30f;
            shockwave.LaunchVelocity = { 2.5f, 1.6f };
            tuning.Attacks["bear_shockwave"] = shockwave;

            ApplyDefaultAttackFeedback(tuning);
            ApplyDefaultAttackVisuals(tuning);

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
                    tuning.ShadowOffset = ReadVec2(visuals["shadowOffset"], tuning.ShadowOffset);
                    tuning.ShadowColor = ReadVec4(visuals["shadowColor"], tuning.ShadowColor);
                    tuning.PlayerAnimations = ReadAnimationSet(visuals["playerAnimations"], tuning.PlayerAnimations);
                    tuning.GruntAnimations = ReadAnimationSet(visuals["gruntAnimations"], tuning.GruntAnimations);
                    tuning.BossAnimations = ReadAnimationSet(visuals["bossAnimations"], tuning.BossAnimations);
                }

                if (YAML::Node reactions = root["reactions"])
                    tuning.BossLaunchBonus = reactions["bossLaunchBonus"].as<float>(tuning.BossLaunchBonus);

                tuning.Player = ReadPlayerTuning(root["player"], tuning.Player);
                tuning.Combat = ReadCombatRuleTuning(root["combat"], tuning.Combat);
                tuning.Feedback = ReadFeedbackTuning(root["feedback"], tuning.Feedback);
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
            std::chrono::steady_clock::time_point LastChecked{};
        };

    const SideCombatTuning& GetTuning(const SideCombatLevelComponent& level)
        {
            static std::unordered_map<std::string, SideCombatTuningCacheEntry> cache;
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
                SideCombatTuningCacheEntry entry;
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

    const SideAttackTuning& GetAttack(const SideCombatTuning& tuning, const std::string& id)
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

    bool IsSkillUnlocked(
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            const std::string& skillId)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            if (!profile)
                return true;
            return ContainsId(profile->UnlockedSkills, skillId);
        }

    bool IsDebugSkillEnabled(
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            const std::string& skillId)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return profile && ContainsId(profile->DebugSkills, skillId);
        }

    bool IsSkillUsable(
            const SideCombatLevelComponent& level,
            const SideCombatTuning& tuning,
            const std::string& skillId)
        {
            return IsSkillUnlocked(level, tuning, skillId) || IsDebugSkillEnabled(level, tuning, skillId);
        }

    bool IsBreakLimitOfficiallyAvailable(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            return tuning.AirCombo.BreakLimitEnabled || IsSkillUnlocked(level, tuning, "break_limit");
        }

    bool IsBreakLimitDebugAvailable(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            return tuning.AirCombo.BreakLimitDebugKeyEnabled || IsDebugSkillEnabled(level, tuning, "break_limit");
        }

    bool ShouldShowBreakLimitUi(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return IsBreakLimitOfficiallyAvailable(level, tuning) ||
                IsBreakLimitDebugAvailable(level, tuning) ||
                tuning.AirCombo.ShowBreakLimitHint ||
                (profile && profile->ShowBreakLimitHint);
        }

    bool ShouldShowBossProtectionHud(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return tuning.Protection.ShowBossProtectionHud ||
                (profile && profile->ShowBossProtectionHud);
        }

    bool ShouldShowCombatStateHud(const SideCombatLevelComponent& level, const SideCombatTuning& tuning)
        {
            const SideUnlockProfile* profile = GetUnlockProfile(level, tuning);
            return tuning.Protection.ShowCombatStateHud ||
                (profile && profile->ShowCombatStateHud);
        }

    void ApplyPlayerTuning(const SideCombatTuning& tuning,
        SideCombatantComponent& combatant,
        SidePlayerControllerComponent& controller)
    {
        combatant.MoveSpeed = tuning.Player.MoveSpeed;
        controller.MaxJumps = std::max(0, tuning.Player.MaxJumps);
        controller.JumpImpulse = tuning.Player.JumpImpulse;
        controller.Gravity = tuning.Player.Gravity;
        controller.AirControl = tuning.Player.AirControl;
        controller.JumpBufferTime = std::max(0.0f, tuning.Player.JumpBufferTime);
        controller.CoyoteTime = std::max(0.0f, tuning.Player.CoyoteTime);
        controller.LaneSpeedScale = tuning.Player.LaneSpeedScale;
        controller.LaneAcceleration = tuning.Player.LaneAcceleration;
        controller.GroundFriction = tuning.Player.GroundFriction;
        controller.BasicCooldown = tuning.Player.BasicCooldown;
        controller.LauncherCooldown = tuning.Player.LauncherCooldown;
        controller.MagicBoltCooldown = tuning.Player.MagicBoltCooldown;
        controller.AllySupportCooldown = tuning.Player.AllySupportCooldown;
        controller.DashCooldown = std::max(0.0f, tuning.Player.DashCooldown);
        controller.HealItemCooldown = std::max(0.0f, tuning.Player.HealItemCooldown);
        controller.ManaItemCooldown = std::max(0.0f, tuning.Player.ManaItemCooldown);
        controller.AttackBuffItemCooldown = std::max(0.0f, tuning.Player.AttackBuffItemCooldown);
        controller.DashManaCost = std::max(0.0f, tuning.Player.DashManaCost);
        controller.DashSpeed = std::max(0.0f, tuning.Player.DashSpeed);
        controller.DashInvulnerableTime = std::max(0.0f, tuning.Player.DashInvulnerableTime);
        controller.RuntimeMagicSwordGaugeMax = std::max(1.0f, tuning.AirCombo.MagicSwordGaugeMax);
        controller.RuntimeMagicSwordGauge = std::clamp(
            controller.RuntimeMagicSwordGauge,
            0.0f,
            controller.RuntimeMagicSwordGaugeMax);
    }

    void ApplyBearBossTuning(const SideCombatTuning& tuning,
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


} // namespace Wheatear::SideCombatTuningService
