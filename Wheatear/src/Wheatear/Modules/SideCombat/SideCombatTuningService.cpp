#include "wtpch.h"
#include "SideCombatTuningService.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Gameplay/Services/GameplayTextService.h"

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

        static EnemyTypeDefinition ReadEnemyType(const YAML::Node& node, const EnemyTypeDefinition& fallback)
        {
            EnemyTypeDefinition definition = fallback;
            if (!node)
                return definition;

            definition.Id = node["id"].as<std::string>(definition.Id);
            definition.MaxHealth = node["maxHealth"].as<float>(definition.MaxHealth);
            definition.Attack = node["attack"].as<float>(definition.Attack);
            definition.Defense = node["defense"].as<float>(definition.Defense);
            definition.MoveSpeed = node["moveSpeed"].as<float>(definition.MoveSpeed);
            definition.CollisionSize = ReadVec2(node["collisionSize"], definition.CollisionSize);
            definition.CollisionHeight = node["collisionHeight"].as<float>(definition.CollisionHeight);
            definition.KnockbackResistance = node["knockbackResistance"].as<float>(definition.KnockbackResistance);
            definition.AggroRange = node["aggroRange"].as<float>(definition.AggroRange);
            definition.AttackRange = node["attackRange"].as<float>(definition.AttackRange);
            definition.PreferredRange = node["preferredRange"].as<float>(definition.PreferredRange);
            definition.AttackInterval = node["attackInterval"].as<float>(definition.AttackInterval);
            definition.LaneTolerance = node["laneTolerance"].as<float>(definition.LaneTolerance);
            if (const YAML::Node scale = node["renderScale"])
            {
                if (scale && scale.IsSequence() && scale.size() >= 3)
                {
                    definition.RenderScale = {
                        scale[0].as<float>(definition.RenderScale.x),
                        scale[1].as<float>(definition.RenderScale.y),
                        scale[2].as<float>(definition.RenderScale.z)
                    };
                }
            }
            if (const YAML::Node shadow = node["shadowScale"])
            {
                if (shadow && shadow.IsSequence() && shadow.size() >= 3)
                {
                    definition.ShadowScale = {
                        shadow[0].as<float>(definition.ShadowScale.x),
                        shadow[1].as<float>(definition.ShadowScale.y),
                        shadow[2].as<float>(definition.ShadowScale.z)
                    };
                }
            }
            return definition;
        }

        static SideAttackTuning ReadAttackTuning(const YAML::Node& node, const SideAttackTuning& fallback)
        {            SideAttackTuning tuning = fallback;
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

        static SideItemSlotKind ReadItemSlotKind(const std::string& value, SideItemSlotKind fallback)
        {
            if (value == "mana")
                return SideItemSlotKind::Mana;
            if (value == "attack_buff")
                return SideItemSlotKind::AttackBuff;
            return fallback;
        }

        static std::vector<SideItemSlotTuning> ReadItemSlots(const YAML::Node& node)
        {
            // Built-in three slots mirror the legacy hard-coded behaviour
            // (heal / mana / attack buff with the component-authored amounts).
            std::vector<SideItemSlotTuning> slots = {
                { 1, "side.item1", SideItemSlotKind::Heal, 5.0f, {} },
                { 2, "side.item2", SideItemSlotKind::Mana, 5.0f, {} },
                { 3, "side.item3", SideItemSlotKind::AttackBuff, 10.0f, {} }
            };
            if (!node || !node.IsSequence())
                return slots;

            slots.clear();
            for (const YAML::Node& entry : node)
            {
                if (!entry.IsMap())
                    continue;
                SideItemSlotTuning slot;
                slot.Slot = entry["slot"].as<int>(0);
                slot.ActionId = entry["actionId"].as<std::string>("side.item" + std::to_string(slot.Slot));
                slot.Kind = ReadItemSlotKind(entry["kind"].as<std::string>(""),
                    SideItemSlotKind::Heal);
                slot.Cooldown = entry["cooldown"].as<float>(5.0f);
                slot.RecipeId = entry["recipeId"].as<std::string>("");
                if (slot.Slot > 0)
                    slots.push_back(std::move(slot));
            }
            return slots;
        }

        static SideSkillSlotKind ReadSkillSlotKind(const std::string& value, SideSkillSlotKind fallback)
        {
            if (value == "launcher")
                return SideSkillSlotKind::Launcher;
            if (value == "magic_bolt" || value == "magic")
                return SideSkillSlotKind::MagicBolt;
            if (value == "dash")
                return SideSkillSlotKind::Dash;
            if (value == "ally_support" || value == "support")
                return SideSkillSlotKind::AllySupport;
            if (value == "break_limit")
                return SideSkillSlotKind::BreakLimit;
            if (value == "custom")
                return SideSkillSlotKind::Custom;
            return fallback;
        }

        static std::vector<SideSkillSlotTuning> ReadSkillSlots(const YAML::Node& node)
        {
            // Built-in six slots mirror the legacy hard-coded action polling.
            std::vector<SideSkillSlotTuning> slots = {
                { "basic",        "side.basic",        SideSkillSlotKind::Basic,       true, {} },
                { "launcher",     "side.launcher",     SideSkillSlotKind::Launcher,    true, {} },
                { "magic",        "side.magic",        SideSkillSlotKind::MagicBolt,   true, {} },
                { "dash",         "side.dash",         SideSkillSlotKind::Dash,        true, {} },
                { "support",      "side.support",      SideSkillSlotKind::AllySupport, true, {} },
                { "break_limit",  "side.break_limit",  SideSkillSlotKind::BreakLimit,  true, {} }
            };
            if (!node || !node.IsSequence())
                return slots;

            slots.clear();
            for (const YAML::Node& entry : node)
            {
                if (!entry.IsMap())
                    continue;
                SideSkillSlotTuning slot;
                slot.SlotId = entry["slot"].as<std::string>("");
                slot.ActionId = entry["actionId"].as<std::string>(
                    slot.SlotId.empty() ? "side.basic" : "side." + slot.SlotId);
                slot.Kind = ReadSkillSlotKind(entry["kind"].as<std::string>(""),
                    SideSkillSlotKind::Basic);
                slot.Enabled = entry["enabled"].as<bool>(true);
                slot.CustomBehavior = entry["customBehavior"].as<std::string>("");
                if (!slot.SlotId.empty())
                    slots.push_back(std::move(slot));
            }
            return slots;
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

        static SideCombatTuning LoadTuning(const std::string& path)
        {
            SideCombatTuning tuning;
            tuning.Feedback.JumpSound = AssetAliasRegistry::Resolve(tuning.Feedback.JumpSound);
            tuning.Feedback.LandSound = AssetAliasRegistry::Resolve(tuning.Feedback.LandSound);
            if (path.empty())
            {
                WT_CORE_WARN("SideCombat tuning path is empty; using struct defaults only.");
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
                tuning.ItemSlots = ReadItemSlots(root["itemSlots"]);
                tuning.SkillSlots = ReadSkillSlots(root["skillSlots"]);
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

                if (YAML::Node enemyTypes = root["enemyTypes"])
                {
                    for (auto it = enemyTypes.begin(); it != enemyTypes.end(); ++it)
                    {
                        const std::string id = it->first.as<std::string>();
                        const auto fallbackIt = tuning.EnemyTypes.find(id);
                        const EnemyTypeDefinition fallback = fallbackIt != tuning.EnemyTypes.end()
                            ? fallbackIt->second
                            : EnemyTypeDefinition{};
                        tuning.EnemyTypes[id] = ReadEnemyType(it->second, fallback);
                    }
                }
                tuning.Loaded = true;
            }
            catch (const std::exception& e)
            {
                WT_CORE_WARN("SideCombat tuning load failed for '{0}': {1}; using struct defaults only.", path, e.what());
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
