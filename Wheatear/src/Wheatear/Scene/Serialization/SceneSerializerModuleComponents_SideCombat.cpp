#include "wtpch.h"
#include "SceneSerializerComponentGroups.h"
#include "SceneSerializerComponentSupport.h"
#include "Wheatear/Modules/GameplayModuleComponents.h"
#include "Wheatear/Modules/SideCombat/SideCombatHudPreset.h"

namespace Wheatear {
    static void SerializeSideDeathRewards(YAML::Emitter& o, const std::vector<SideCombatLevelComponent::DeathReward>& rewards)
    {
        o << YAML::Key << "DeathRewards" << YAML::Value << YAML::BeginSeq;
        for (const auto& reward : rewards)
        {
            o << YAML::BeginMap;
            o << YAML::Key << "Enabled" << YAML::Value << reward.Enabled;
            o << YAML::Key << "EnemyKind" << YAML::Value << reward.EnemyKind;
            o << YAML::Key << "SourceEntityName" << YAML::Value << reward.SourceEntityName;
            o << YAML::Key << "SpawnEntityName" << YAML::Value << reward.SpawnEntityName;
            o << YAML::Key << "ItemId" << YAML::Value << reward.ItemId;
            o << YAML::Key << "DisplayName" << YAML::Value << reward.DisplayName;
            o << YAML::Key << "Amount" << YAML::Value << reward.Amount;
            o << YAML::Key << "Offset" << YAML::Value << reward.Offset;
            o << YAML::Key << "Scale" << YAML::Value << reward.Scale;
            o << YAML::Key << "TexturePath" << YAML::Value << reward.TexturePath;
            o << YAML::EndMap;
        }
        o << YAML::EndSeq;
    }

    static void DeserializeSideDeathRewards(const YAML::Node& node, std::vector<SideCombatLevelComponent::DeathReward>& rewards)
    {
        if (!node)
            return;

        rewards.clear();
        for (const auto& rewardNode : node)
        {
            SideCombatLevelComponent::DeathReward reward;
            reward.Enabled = rewardNode["Enabled"].as<bool>(reward.Enabled);
            reward.EnemyKind = rewardNode["EnemyKind"].as<int>(reward.EnemyKind);
            reward.SourceEntityName = rewardNode["SourceEntityName"].as<std::string>(reward.SourceEntityName);
            reward.SpawnEntityName = rewardNode["SpawnEntityName"].as<std::string>(reward.SpawnEntityName);
            reward.ItemId = rewardNode["ItemId"].as<std::string>(reward.ItemId);
            reward.DisplayName = rewardNode["DisplayName"].as<std::string>(reward.DisplayName);
            reward.Amount = rewardNode["Amount"].as<int>(reward.Amount);
            reward.Offset = rewardNode["Offset"].as<glm::vec3>(reward.Offset);
            reward.Scale = rewardNode["Scale"].as<glm::vec3>(reward.Scale);
            reward.TexturePath = rewardNode["TexturePath"].as<std::string>(reward.TexturePath);
            rewards.push_back(reward);
        }
    }

    static void SerializeSideWaveSpawns(YAML::Emitter& o, const std::vector<SideCombatLevelComponent::WaveSpawnDef>& spawns)
    {
        o << YAML::Key << "WaveSpawns" << YAML::Value << YAML::BeginSeq;
        for (const auto& spawn : spawns)
        {
            o << YAML::BeginMap;
            o << YAML::Key << "Enabled" << YAML::Value << spawn.Enabled;
            o << YAML::Key << "WaveIndex" << YAML::Value << spawn.WaveIndex;
            o << YAML::Key << "EnemyKind" << YAML::Value << spawn.EnemyKind;
            o << YAML::Key << "Count" << YAML::Value << spawn.Count;
            o << YAML::Key << "SpawnMinX" << YAML::Value << spawn.SpawnMinX;
            o << YAML::Key << "SpawnMaxX" << YAML::Value << spawn.SpawnMaxX;
            o << YAML::Key << "GroundYOffset" << YAML::Value << spawn.GroundYOffset;
            o << YAML::Key << "HpVariance" << YAML::Value << spawn.HpVariance;
            o << YAML::EndMap;
        }
        o << YAML::EndSeq;
    }

    static void DeserializeSideWaveSpawns(const YAML::Node& node, std::vector<SideCombatLevelComponent::WaveSpawnDef>& spawns)
    {
        if (!node)
            return;

        spawns.clear();
        for (const auto& spawnNode : node)
        {
            SideCombatLevelComponent::WaveSpawnDef spawn;
            spawn.Enabled = spawnNode["Enabled"].as<bool>(spawn.Enabled);
            spawn.WaveIndex = spawnNode["WaveIndex"].as<int>(spawn.WaveIndex);
            spawn.EnemyKind = spawnNode["EnemyKind"].as<int>(spawn.EnemyKind);
            spawn.Count = spawnNode["Count"].as<int>(spawn.Count);
            spawn.SpawnMinX = spawnNode["SpawnMinX"].as<float>(spawn.SpawnMinX);
            spawn.SpawnMaxX = spawnNode["SpawnMaxX"].as<float>(spawn.SpawnMaxX);
            spawn.GroundYOffset = spawnNode["GroundYOffset"].as<float>(spawn.GroundYOffset);
            spawn.HpVariance = spawnNode["HpVariance"].as<float>(spawn.HpVariance);
            spawns.push_back(spawn);
        }
    }

    static void SerializeSideHudRect(YAML::Emitter& o, const char* key, const SideCombatLevelComponent::HudRect& rect)
    {
        o << YAML::Key << key << YAML::Value << YAML::BeginMap;
        o << YAML::Key << "Position" << YAML::Value << rect.Position;
        o << YAML::Key << "Size" << YAML::Value << rect.Size;
        o << YAML::EndMap;
    }

    static void DeserializeSideHudRect(const YAML::Node& node, SideCombatLevelComponent::HudRect& rect)
    {
        if (!node)
            return;

        rect.Position = node["Position"].as<glm::vec2>(rect.Position);
        rect.Size = node["Size"].as<glm::vec2>(rect.Size);
    }

    static void SerializeSideStatusBadgeLayout(YAML::Emitter& o, const char* key, const SideCombatLevelComponent::StatusBadgeLayout& layout)
    {
        o << YAML::Key << key << YAML::Value << YAML::BeginMap;
        o << YAML::Key << "BuffStart" << YAML::Value << layout.BuffStart;
        o << YAML::Key << "DebuffStart" << YAML::Value << layout.DebuffStart;
        o << YAML::Key << "Size" << YAML::Value << layout.Size;
        o << YAML::Key << "Gap" << YAML::Value << layout.Gap;
        o << YAML::EndMap;
    }

    static void DeserializeSideStatusBadgeLayout(const YAML::Node& node, SideCombatLevelComponent::StatusBadgeLayout& layout)
    {
        if (!node)
            return;

        layout.BuffStart = node["BuffStart"].as<glm::vec2>(layout.BuffStart);
        layout.DebuffStart = node["DebuffStart"].as<glm::vec2>(layout.DebuffStart);
        layout.Size = node["Size"].as<glm::vec2>(layout.Size);
        layout.Gap = node["Gap"].as<float>(layout.Gap);
    }

    static void SerializeSideSkillHudSlots(YAML::Emitter& o, const std::vector<SideCombatLevelComponent::SkillHudSlot>& slots)
    {
        o << YAML::Key << "SkillHudSlots" << YAML::Value << YAML::BeginSeq;
        for (const auto& slot : slots)
        {
            o << YAML::BeginMap;
            o << YAML::Key << "Enabled" << YAML::Value << slot.Enabled;
            o << YAML::Key << "Key" << YAML::Value << slot.Key;
            o << YAML::Key << "KeyLabel" << YAML::Value << slot.KeyLabel;
            o << YAML::Key << "Command" << YAML::Value << YAML::DoubleQuoted << slot.Command;
            o << YAML::Key << "Position" << YAML::Value << slot.Position;
            o << YAML::Key << "Size" << YAML::Value << slot.Size;
            o << YAML::Key << "TooltipPosition" << YAML::Value << slot.TooltipPosition;
            o << YAML::Key << "UseSheetIcon" << YAML::Value << slot.UseSheetIcon;
            o << YAML::Key << "IconSheetPixels" << YAML::Value << slot.IconSheetPixels;
            o << YAML::Key << "IconTexturePath" << YAML::Value << slot.IconTexturePath;
            o << YAML::Key << "TooltipText" << YAML::Value << YAML::DoubleQuoted << slot.TooltipText;
            o << YAML::EndMap;
        }
        o << YAML::EndSeq;
    }

    static void DeserializeSideSkillHudSlots(const YAML::Node& node, std::vector<SideCombatLevelComponent::SkillHudSlot>& slots)
    {
        if (!node)
            return;

        slots.clear();
        for (const auto& slotNode : node)
        {
            SideCombatLevelComponent::SkillHudSlot slot;
            slot.Enabled = slotNode["Enabled"].as<bool>(slot.Enabled);
            slot.Key = slotNode["Key"].as<std::string>(slot.Key);
            slot.KeyLabel = slotNode["KeyLabel"].as<std::string>(slot.KeyLabel);
            slot.Command = slotNode["Command"].as<std::string>(slot.Command);
            slot.Position = slotNode["Position"].as<glm::vec2>(slot.Position);
            slot.Size = slotNode["Size"].as<glm::vec2>(slot.Size);
            slot.TooltipPosition = slotNode["TooltipPosition"].as<glm::vec2>(slot.TooltipPosition);
            slot.UseSheetIcon = slotNode["UseSheetIcon"].as<bool>(slot.UseSheetIcon);
            slot.IconSheetPixels = slotNode["IconSheetPixels"].as<glm::vec4>(slot.IconSheetPixels);
            slot.IconTexturePath = slotNode["IconTexturePath"].as<std::string>(slot.IconTexturePath);
            slot.TooltipText = slotNode["TooltipText"].as<std::string>(slot.TooltipText);
            slots.push_back(slot);
        }
    }

    static void SerializeSideCombatItemHudSlots(YAML::Emitter& o, const std::vector<SideCombatLevelComponent::CombatItemHudSlot>& slots)
    {
        o << YAML::Key << "CombatItemHudSlots" << YAML::Value << YAML::BeginSeq;
        for (const auto& slot : slots)
        {
            o << YAML::BeginMap;
            o << YAML::Key << "Enabled" << YAML::Value << slot.Enabled;
            o << YAML::Key << "Key" << YAML::Value << slot.Key;
            o << YAML::Key << "Shortcut" << YAML::Value << slot.Shortcut;
            o << YAML::Key << "Command" << YAML::Value << YAML::DoubleQuoted << slot.Command;
            o << YAML::Key << "Position" << YAML::Value << slot.Position;
            o << YAML::Key << "FrameSize" << YAML::Value << slot.FrameSize;
            o << YAML::Key << "IconInset" << YAML::Value << slot.IconInset;
            o << YAML::Key << "IconSize" << YAML::Value << slot.IconSize;
            o << YAML::Key << "TooltipPosition" << YAML::Value << slot.TooltipPosition;
            o << YAML::Key << "UseSheetIcon" << YAML::Value << slot.UseSheetIcon;
            o << YAML::Key << "IconSheetPixels" << YAML::Value << slot.IconSheetPixels;
            o << YAML::Key << "IconTexturePath" << YAML::Value << slot.IconTexturePath;
            o << YAML::Key << "DisplayName" << YAML::Value << YAML::DoubleQuoted << slot.DisplayName;
            o << YAML::Key << "UsageText" << YAML::Value << YAML::DoubleQuoted << slot.UsageText;
            o << YAML::EndMap;
        }
        o << YAML::EndSeq;
    }

    static void DeserializeSideCombatItemHudSlots(const YAML::Node& node, std::vector<SideCombatLevelComponent::CombatItemHudSlot>& slots)
    {
        if (!node)
            return;

        slots.clear();
        for (const auto& slotNode : node)
        {
            SideCombatLevelComponent::CombatItemHudSlot slot;
            slot.Enabled = slotNode["Enabled"].as<bool>(slot.Enabled);
            slot.Key = slotNode["Key"].as<std::string>(slot.Key);
            slot.Shortcut = slotNode["Shortcut"].as<std::string>(slot.Shortcut);
            slot.Command = slotNode["Command"].as<std::string>(slot.Command);
            slot.Position = slotNode["Position"].as<glm::vec2>(slot.Position);
            slot.FrameSize = slotNode["FrameSize"].as<glm::vec2>(slot.FrameSize);
            slot.IconInset = slotNode["IconInset"].as<glm::vec2>(slot.IconInset);
            slot.IconSize = slotNode["IconSize"].as<glm::vec2>(slot.IconSize);
            slot.TooltipPosition = slotNode["TooltipPosition"].as<glm::vec2>(slot.TooltipPosition);
            slot.UseSheetIcon = slotNode["UseSheetIcon"].as<bool>(slot.UseSheetIcon);
            slot.IconSheetPixels = slotNode["IconSheetPixels"].as<glm::vec4>(slot.IconSheetPixels);
            slot.IconTexturePath = slotNode["IconTexturePath"].as<std::string>(slot.IconTexturePath);
            slot.DisplayName = slotNode["DisplayName"].as<std::string>(slot.DisplayName);
            slot.UsageText = slotNode["UsageText"].as<std::string>(slot.UsageText);
            slots.push_back(slot);
        }
    }

    template<> struct ComponentSerializer<SideCombatLevelComponent> {
        static constexpr const char* Key = "SideCombatLevelComponent";
        static void Serialize(YAML::Emitter& o, const SideCombatLevelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "LevelId" << YAML::Value << c.LevelId;
            o << YAML::Key << "TuningPath" << YAML::Value << c.TuningPath;
            o << YAML::Key << "HudPresetPath" << YAML::Value << c.HudPresetPath;
            o << YAML::Key << "HudPresetOverridesEnabled" << YAML::Value << c.HudPresetOverridesEnabled;
            o << YAML::Key << "ArenaMin" << YAML::Value << c.ArenaMin;
            o << YAML::Key << "ArenaMax" << YAML::Value << c.ArenaMax;
            o << YAML::Key << "GroundY" << YAML::Value << c.GroundY;
            o << YAML::Key << "LaneMinY" << YAML::Value << c.LaneMinY;
            o << YAML::Key << "LaneMaxY" << YAML::Value << c.LaneMaxY;
            if (c.HudPresetOverridesEnabled)
            {
                o << YAML::Key << "PlayerEntityName" << YAML::Value << c.PlayerEntityName;
                o << YAML::Key << "BossEntityName" << YAML::Value << c.BossEntityName;
                o << YAML::Key << "FadeEntityName" << YAML::Value << c.FadeEntityName;
                o << YAML::Key << "MessageTextEntityName" << YAML::Value << c.MessageTextEntityName;
                o << YAML::Key << "ComboTextEntityName" << YAML::Value << c.ComboTextEntityName;
                o << YAML::Key << "SkillTextEntityName" << YAML::Value << c.SkillTextEntityName;
                o << YAML::Key << "RewardTextEntityName" << YAML::Value << c.RewardTextEntityName;
                o << YAML::Key << "PlayerHealthBarEntityName" << YAML::Value << c.PlayerHealthBarEntityName;
                o << YAML::Key << "PlayerHealthTextEntityName" << YAML::Value << c.PlayerHealthTextEntityName;
                o << YAML::Key << "BossHealthBarEntityName" << YAML::Value << c.BossHealthBarEntityName;
                o << YAML::Key << "BossHealthTextEntityName" << YAML::Value << c.BossHealthTextEntityName;
                o << YAML::Key << "CameraEntityName" << YAML::Value << c.CameraEntityName;
                o << YAML::Key << "TopPanelEntityName" << YAML::Value << c.TopPanelEntityName;
                o << YAML::Key << "ComboPanelEntityName" << YAML::Value << c.ComboPanelEntityName;
                o << YAML::Key << "ComboFrameEntityName" << YAML::Value << c.ComboFrameEntityName;
                o << YAML::Key << "ComboLabelEntityName" << YAML::Value << c.ComboLabelEntityName;
                o << YAML::Key << "ComboMultiplyEntityName" << YAML::Value << c.ComboMultiplyEntityName;
                o << YAML::Key << "ComboDigitPrefix" << YAML::Value << c.ComboDigitPrefix;
                o << YAML::Key << "SkillBarPanelEntityName" << YAML::Value << c.SkillBarPanelEntityName;
                o << YAML::Key << "SkillTooltipPanelEntityName" << YAML::Value << c.SkillTooltipPanelEntityName;
                o << YAML::Key << "SkillTooltipTextEntityName" << YAML::Value << c.SkillTooltipTextEntityName;
                o << YAML::Key << "JoystickBaseEntityName" << YAML::Value << c.JoystickBaseEntityName;
                o << YAML::Key << "JoystickThumbEntityName" << YAML::Value << c.JoystickThumbEntityName;
                o << YAML::Key << "PlayerManaEntityName" << YAML::Value << c.PlayerManaEntityName;
                o << YAML::Key << "PlayerUltimateFillEntityName" << YAML::Value << c.PlayerUltimateFillEntityName;
                o << YAML::Key << "PlayerUltimateMaskEntityName" << YAML::Value << c.PlayerUltimateMaskEntityName;
                o << YAML::Key << "BossProtectionEntityName" << YAML::Value << c.BossProtectionEntityName;
                o << YAML::Key << "PlayerStatusPrefix" << YAML::Value << c.PlayerStatusPrefix;
                o << YAML::Key << "EnemyStatusPrefix" << YAML::Value << c.EnemyStatusPrefix;
                o << YAML::Key << "SkillPrefix" << YAML::Value << c.SkillPrefix;
                o << YAML::Key << "ItemSlotPrefix" << YAML::Value << c.ItemSlotPrefix;
                SerializeSideHudRect(o, "TopPanelLayout", c.TopPanelLayout);
                SerializeSideHudRect(o, "PlayerHealthLayout", c.PlayerHealthLayout);
                SerializeSideHudRect(o, "PlayerManaLayout", c.PlayerManaLayout);
                SerializeSideHudRect(o, "PlayerUltimateLayout", c.PlayerUltimateLayout);
                SerializeSideHudRect(o, "PlayerHealthTextLayout", c.PlayerHealthTextLayout);
                SerializeSideHudRect(o, "BossPanelLayout", c.BossPanelLayout);
                SerializeSideHudRect(o, "BossHealthLayout", c.BossHealthLayout);
                SerializeSideHudRect(o, "BossProtectionLayout", c.BossProtectionLayout);
                SerializeSideHudRect(o, "BossHealthTextLayout", c.BossHealthTextLayout);
                SerializeSideHudRect(o, "ComboTextLayout", c.ComboTextLayout);
                SerializeSideHudRect(o, "ComboFrameLayout", c.ComboFrameLayout);
                SerializeSideHudRect(o, "SkillTooltipLayout", c.SkillTooltipLayout);
                o << YAML::Key << "SkillTooltipPadding" << YAML::Value << c.SkillTooltipPadding;
                SerializeSideHudRect(o, "JoystickBaseLayout", c.JoystickBaseLayout);
                o << YAML::Key << "JoystickThumbSize" << YAML::Value << c.JoystickThumbSize;
                o << YAML::Key << "JoystickThumbTravel" << YAML::Value << c.JoystickThumbTravel;
                SerializeSideStatusBadgeLayout(o, "PlayerStatusLayout", c.PlayerStatusLayout);
                SerializeSideStatusBadgeLayout(o, "EnemyStatusLayout", c.EnemyStatusLayout);
                SerializeSideSkillHudSlots(o, c.SkillHudSlots);
                SerializeSideCombatItemHudSlots(o, c.CombatItemHudSlots);
                o << YAML::Key << "HudLockedText" << YAML::Value << YAML::DoubleQuoted << c.HudLockedText;
                o << YAML::Key << "HudUnavailableText" << YAML::Value << YAML::DoubleQuoted << c.HudUnavailableText;
                o << YAML::Key << "HudInsufficientManaText" << YAML::Value << YAML::DoubleQuoted << c.HudInsufficientManaText;
                o << YAML::Key << "HudConditionText" << YAML::Value << YAML::DoubleQuoted << c.HudConditionText;
                o << YAML::Key << "HudGaugeText" << YAML::Value << YAML::DoubleQuoted << c.HudGaugeText;
                o << YAML::Key << "HudComboText" << YAML::Value << YAML::DoubleQuoted << c.HudComboText;
                o << YAML::Key << "HudArmorText" << YAML::Value << YAML::DoubleQuoted << c.HudArmorText;
                o << YAML::Key << "HudCooldownPrefix" << YAML::Value << YAML::DoubleQuoted << c.HudCooldownPrefix;
                o << YAML::Key << "HudSecondsSuffix" << YAML::Value << YAML::DoubleQuoted << c.HudSecondsSuffix;
                o << YAML::Key << "HudManaNotEnoughTooltip" << YAML::Value << YAML::DoubleQuoted << c.HudManaNotEnoughTooltip;
                o << YAML::Key << "HudNotUnlockedTooltip" << YAML::Value << YAML::DoubleQuoted << c.HudNotUnlockedTooltip;
                o << YAML::Key << "BreakLimitGaugeNotEnoughTooltip" << YAML::Value << YAML::DoubleQuoted << c.BreakLimitGaugeNotEnoughTooltip;
                o << YAML::Key << "BreakLimitComboNotEnoughTooltip" << YAML::Value << YAML::DoubleQuoted << c.BreakLimitComboNotEnoughTooltip;
                o << YAML::Key << "BreakLimitBossNotReadyTooltip" << YAML::Value << YAML::DoubleQuoted << c.BreakLimitBossNotReadyTooltip;
                o << YAML::Key << "HudDefaultMessage" << YAML::Value << YAML::DoubleQuoted << c.HudDefaultMessage;
                o << YAML::Key << "HudAirBasicMessage" << YAML::Value << YAML::DoubleQuoted << c.HudAirBasicMessage;
                o << YAML::Key << "HudMagicMessage" << YAML::Value << YAML::DoubleQuoted << c.HudMagicMessage;
                o << YAML::Key << "HudDashMessage" << YAML::Value << YAML::DoubleQuoted << c.HudDashMessage;
                o << YAML::Key << "HudReservedSkillMessage" << YAML::Value << YAML::DoubleQuoted << c.HudReservedSkillMessage;
                o << YAML::Key << "HudSupportMessage" << YAML::Value << YAML::DoubleQuoted << c.HudSupportMessage;
                o << YAML::Key << "HudBreakLimitInputMessage" << YAML::Value << YAML::DoubleQuoted << c.HudBreakLimitInputMessage;
                o << YAML::Key << "HudBreakLimitDebugInputMessage" << YAML::Value << YAML::DoubleQuoted << c.HudBreakLimitDebugInputMessage;
                o << YAML::Key << "HudVictoryMessage" << YAML::Value << YAML::DoubleQuoted << c.HudVictoryMessage;
                o << YAML::Key << "HudDefeatMessage" << YAML::Value << YAML::DoubleQuoted << c.HudDefeatMessage;
                o << YAML::Key << "HudHighAirMessage" << YAML::Value << YAML::DoubleQuoted << c.HudHighAirMessage;
                o << YAML::Key << "HudLowAirMessage" << YAML::Value << YAML::DoubleQuoted << c.HudLowAirMessage;
                o << YAML::Key << "HudBreakLimitHintMessage" << YAML::Value << YAML::DoubleQuoted << c.HudBreakLimitHintMessage;
                o << YAML::Key << "HudPlayerHealthLabel" << YAML::Value << YAML::DoubleQuoted << c.HudPlayerHealthLabel;
                o << YAML::Key << "HudBossHealthLabel" << YAML::Value << YAML::DoubleQuoted << c.HudBossHealthLabel;
                o << YAML::Key << "HudBossProtectionLabel" << YAML::Value << YAML::DoubleQuoted << c.HudBossProtectionLabel;
                o << YAML::Key << "HudManaGaugeLabel" << YAML::Value << YAML::DoubleQuoted << c.HudManaGaugeLabel;
                o << YAML::Key << "HudAirActionsLabel" << YAML::Value << YAML::DoubleQuoted << c.HudAirActionsLabel;
                o << YAML::Key << "HudRewardFallbackText" << YAML::Value << YAML::DoubleQuoted << c.HudRewardFallbackText;
                o << YAML::Key << "HudCollectedPrefix" << YAML::Value << YAML::DoubleQuoted << c.HudCollectedPrefix;
            }
            o << YAML::Key << "StartFadeDuration" << YAML::Value << c.StartFadeDuration;
            o << YAML::Key << "VictoryReturnDelay" << YAML::Value << c.VictoryReturnDelay;
            o << YAML::Key << "DefeatReturnDelay" << YAML::Value << c.DefeatReturnDelay;
            o << YAML::Key << "ResultSceneFadeDuration" << YAML::Value << c.ResultSceneFadeDuration;
            o << YAML::Key << "VictorySceneCommand" << YAML::Value << YAML::DoubleQuoted << c.VictorySceneCommand;
            o << YAML::Key << "DefeatSceneCommand" << YAML::Value << YAML::DoubleQuoted << c.DefeatSceneCommand;
            o << YAML::Key << "ComboDropDelay" << YAML::Value << c.ComboDropDelay;
            o << YAML::Key << "FirstClearRewardText" << YAML::Value << YAML::DoubleQuoted << c.FirstClearRewardText;
            SerializeSideDeathRewards(o, c.DeathRewards);
            SerializeSideWaveSpawns(o, c.WaveSpawns);
            o << YAML::Key << "WaveModeEnabled" << YAML::Value << c.WaveModeEnabled;
            o << YAML::Key << "WaveCount" << YAML::Value << c.WaveCount;
            o << YAML::Key << "Wave1RightWall" << YAML::Value << c.Wave1RightWall;
            o << YAML::Key << "Wave2RightWall" << YAML::Value << c.Wave2RightWall;
            o << YAML::Key << "Wave3RightWall" << YAML::Value << c.Wave3RightWall;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideCombatLevelComponent& c) {
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.LevelId = n["LevelId"].as<std::string>(c.LevelId);
            c.TuningPath = n["TuningPath"].as<std::string>(c.TuningPath);
            c.HudPresetPath = n["HudPresetPath"].as<std::string>(c.HudPresetPath);
            c.HudPresetOverridesEnabled = n["HudPresetOverridesEnabled"].as<bool>(c.HudPresetOverridesEnabled);
            SideCombatHudPreset::Apply(c);
            c.ArenaMin = n["ArenaMin"].as<glm::vec2>(c.ArenaMin);
            c.ArenaMax = n["ArenaMax"].as<glm::vec2>(c.ArenaMax);
            c.GroundY = n["GroundY"].as<float>(c.GroundY);
            c.LaneMinY = n["LaneMinY"].as<float>(c.LaneMinY);
            c.LaneMaxY = n["LaneMaxY"].as<float>(c.LaneMaxY);
            if (c.HudPresetOverridesEnabled)
            {
                c.PlayerEntityName = n["PlayerEntityName"].as<std::string>(c.PlayerEntityName);
                c.BossEntityName = n["BossEntityName"].as<std::string>(c.BossEntityName);
                c.FadeEntityName = n["FadeEntityName"].as<std::string>(c.FadeEntityName);
                c.MessageTextEntityName = n["MessageTextEntityName"].as<std::string>(c.MessageTextEntityName);
                c.ComboTextEntityName = n["ComboTextEntityName"].as<std::string>(c.ComboTextEntityName);
                c.SkillTextEntityName = n["SkillTextEntityName"].as<std::string>(c.SkillTextEntityName);
                c.RewardTextEntityName = n["RewardTextEntityName"].as<std::string>(c.RewardTextEntityName);
                c.PlayerHealthBarEntityName = n["PlayerHealthBarEntityName"].as<std::string>(c.PlayerHealthBarEntityName);
                c.PlayerHealthTextEntityName = n["PlayerHealthTextEntityName"].as<std::string>(c.PlayerHealthTextEntityName);
                c.BossHealthBarEntityName = n["BossHealthBarEntityName"].as<std::string>(c.BossHealthBarEntityName);
                c.BossHealthTextEntityName = n["BossHealthTextEntityName"].as<std::string>(c.BossHealthTextEntityName);
                c.CameraEntityName = n["CameraEntityName"].as<std::string>(c.CameraEntityName);
                c.TopPanelEntityName = n["TopPanelEntityName"].as<std::string>(c.TopPanelEntityName);
                c.ComboPanelEntityName = n["ComboPanelEntityName"].as<std::string>(c.ComboPanelEntityName);
                c.ComboFrameEntityName = n["ComboFrameEntityName"].as<std::string>(c.ComboFrameEntityName);
                c.ComboLabelEntityName = n["ComboLabelEntityName"].as<std::string>(c.ComboLabelEntityName);
                c.ComboMultiplyEntityName = n["ComboMultiplyEntityName"].as<std::string>(c.ComboMultiplyEntityName);
                c.ComboDigitPrefix = n["ComboDigitPrefix"].as<std::string>(c.ComboDigitPrefix);
                c.SkillBarPanelEntityName = n["SkillBarPanelEntityName"].as<std::string>(c.SkillBarPanelEntityName);
                c.SkillTooltipPanelEntityName = n["SkillTooltipPanelEntityName"].as<std::string>(c.SkillTooltipPanelEntityName);
                c.SkillTooltipTextEntityName = n["SkillTooltipTextEntityName"].as<std::string>(c.SkillTooltipTextEntityName);
                c.JoystickBaseEntityName = n["JoystickBaseEntityName"].as<std::string>(c.JoystickBaseEntityName);
                c.JoystickThumbEntityName = n["JoystickThumbEntityName"].as<std::string>(c.JoystickThumbEntityName);
                c.PlayerManaEntityName = n["PlayerManaEntityName"].as<std::string>(c.PlayerManaEntityName);
                c.PlayerUltimateFillEntityName = n["PlayerUltimateFillEntityName"].as<std::string>(c.PlayerUltimateFillEntityName);
                c.PlayerUltimateMaskEntityName = n["PlayerUltimateMaskEntityName"].as<std::string>(c.PlayerUltimateMaskEntityName);
                c.BossProtectionEntityName = n["BossProtectionEntityName"].as<std::string>(c.BossProtectionEntityName);
                c.PlayerStatusPrefix = n["PlayerStatusPrefix"].as<std::string>(c.PlayerStatusPrefix);
                c.EnemyStatusPrefix = n["EnemyStatusPrefix"].as<std::string>(c.EnemyStatusPrefix);
                c.SkillPrefix = n["SkillPrefix"].as<std::string>(c.SkillPrefix);
                c.ItemSlotPrefix = n["ItemSlotPrefix"].as<std::string>(c.ItemSlotPrefix);
                DeserializeSideHudRect(n["TopPanelLayout"], c.TopPanelLayout);
                DeserializeSideHudRect(n["PlayerHealthLayout"], c.PlayerHealthLayout);
                DeserializeSideHudRect(n["PlayerManaLayout"], c.PlayerManaLayout);
                DeserializeSideHudRect(n["PlayerUltimateLayout"], c.PlayerUltimateLayout);
                DeserializeSideHudRect(n["PlayerHealthTextLayout"], c.PlayerHealthTextLayout);
                DeserializeSideHudRect(n["BossPanelLayout"], c.BossPanelLayout);
                DeserializeSideHudRect(n["BossHealthLayout"], c.BossHealthLayout);
                DeserializeSideHudRect(n["BossProtectionLayout"], c.BossProtectionLayout);
                DeserializeSideHudRect(n["BossHealthTextLayout"], c.BossHealthTextLayout);
                DeserializeSideHudRect(n["ComboTextLayout"], c.ComboTextLayout);
                DeserializeSideHudRect(n["ComboFrameLayout"], c.ComboFrameLayout);
                DeserializeSideHudRect(n["SkillTooltipLayout"], c.SkillTooltipLayout);
                c.SkillTooltipPadding = n["SkillTooltipPadding"].as<glm::vec2>(c.SkillTooltipPadding);
                DeserializeSideHudRect(n["JoystickBaseLayout"], c.JoystickBaseLayout);
                c.JoystickThumbSize = n["JoystickThumbSize"].as<glm::vec2>(c.JoystickThumbSize);
                c.JoystickThumbTravel = n["JoystickThumbTravel"].as<glm::vec2>(c.JoystickThumbTravel);
                DeserializeSideStatusBadgeLayout(n["PlayerStatusLayout"], c.PlayerStatusLayout);
                DeserializeSideStatusBadgeLayout(n["EnemyStatusLayout"], c.EnemyStatusLayout);
                DeserializeSideSkillHudSlots(n["SkillHudSlots"], c.SkillHudSlots);
                DeserializeSideCombatItemHudSlots(n["CombatItemHudSlots"], c.CombatItemHudSlots);
                c.HudLockedText = n["HudLockedText"].as<std::string>(c.HudLockedText);
                c.HudUnavailableText = n["HudUnavailableText"].as<std::string>(c.HudUnavailableText);
                c.HudInsufficientManaText = n["HudInsufficientManaText"].as<std::string>(c.HudInsufficientManaText);
                c.HudConditionText = n["HudConditionText"].as<std::string>(c.HudConditionText);
                c.HudGaugeText = n["HudGaugeText"].as<std::string>(c.HudGaugeText);
                c.HudComboText = n["HudComboText"].as<std::string>(c.HudComboText);
                c.HudArmorText = n["HudArmorText"].as<std::string>(c.HudArmorText);
                c.HudCooldownPrefix = n["HudCooldownPrefix"].as<std::string>(c.HudCooldownPrefix);
                c.HudSecondsSuffix = n["HudSecondsSuffix"].as<std::string>(c.HudSecondsSuffix);
                c.HudManaNotEnoughTooltip = n["HudManaNotEnoughTooltip"].as<std::string>(c.HudManaNotEnoughTooltip);
                c.HudNotUnlockedTooltip = n["HudNotUnlockedTooltip"].as<std::string>(c.HudNotUnlockedTooltip);
                c.BreakLimitGaugeNotEnoughTooltip = n["BreakLimitGaugeNotEnoughTooltip"].as<std::string>(c.BreakLimitGaugeNotEnoughTooltip);
                c.BreakLimitComboNotEnoughTooltip = n["BreakLimitComboNotEnoughTooltip"].as<std::string>(c.BreakLimitComboNotEnoughTooltip);
                c.BreakLimitBossNotReadyTooltip = n["BreakLimitBossNotReadyTooltip"].as<std::string>(c.BreakLimitBossNotReadyTooltip);
                c.HudDefaultMessage = n["HudDefaultMessage"].as<std::string>(c.HudDefaultMessage);
                c.HudAirBasicMessage = n["HudAirBasicMessage"].as<std::string>(c.HudAirBasicMessage);
                c.HudMagicMessage = n["HudMagicMessage"].as<std::string>(c.HudMagicMessage);
                c.HudDashMessage = n["HudDashMessage"].as<std::string>(c.HudDashMessage);
                c.HudReservedSkillMessage = n["HudReservedSkillMessage"].as<std::string>(c.HudReservedSkillMessage);
                c.HudSupportMessage = n["HudSupportMessage"].as<std::string>(c.HudSupportMessage);
                c.HudBreakLimitInputMessage = n["HudBreakLimitInputMessage"].as<std::string>(c.HudBreakLimitInputMessage);
                c.HudBreakLimitDebugInputMessage = n["HudBreakLimitDebugInputMessage"].as<std::string>(c.HudBreakLimitDebugInputMessage);
                c.HudVictoryMessage = n["HudVictoryMessage"].as<std::string>(c.HudVictoryMessage);
                c.HudDefeatMessage = n["HudDefeatMessage"].as<std::string>(c.HudDefeatMessage);
                c.HudHighAirMessage = n["HudHighAirMessage"].as<std::string>(c.HudHighAirMessage);
                c.HudLowAirMessage = n["HudLowAirMessage"].as<std::string>(c.HudLowAirMessage);
                c.HudBreakLimitHintMessage = n["HudBreakLimitHintMessage"].as<std::string>(c.HudBreakLimitHintMessage);
                c.HudPlayerHealthLabel = n["HudPlayerHealthLabel"].as<std::string>(c.HudPlayerHealthLabel);
                c.HudBossHealthLabel = n["HudBossHealthLabel"].as<std::string>(c.HudBossHealthLabel);
                c.HudBossProtectionLabel = n["HudBossProtectionLabel"].as<std::string>(c.HudBossProtectionLabel);
                c.HudManaGaugeLabel = n["HudManaGaugeLabel"].as<std::string>(c.HudManaGaugeLabel);
                c.HudAirActionsLabel = n["HudAirActionsLabel"].as<std::string>(c.HudAirActionsLabel);
                c.HudRewardFallbackText = n["HudRewardFallbackText"].as<std::string>(c.HudRewardFallbackText);
                c.HudCollectedPrefix = n["HudCollectedPrefix"].as<std::string>(c.HudCollectedPrefix);
            }
            c.StartFadeDuration = n["StartFadeDuration"].as<float>(c.StartFadeDuration);
            c.VictoryReturnDelay = n["VictoryReturnDelay"].as<float>(c.VictoryReturnDelay);
            c.DefeatReturnDelay = n["DefeatReturnDelay"].as<float>(c.DefeatReturnDelay);
            c.ResultSceneFadeDuration = n["ResultSceneFadeDuration"].as<float>(c.ResultSceneFadeDuration);
            c.VictorySceneCommand = n["VictorySceneCommand"].as<std::string>(c.VictorySceneCommand);
            c.DefeatSceneCommand = n["DefeatSceneCommand"].as<std::string>(c.DefeatSceneCommand);
            c.ComboDropDelay = n["ComboDropDelay"].as<float>(c.ComboDropDelay);
            c.FirstClearRewardText = n["FirstClearRewardText"].as<std::string>(c.FirstClearRewardText);
            DeserializeSideDeathRewards(n["DeathRewards"], c.DeathRewards);
            DeserializeSideWaveSpawns(n["WaveSpawns"], c.WaveSpawns);
            c.WaveModeEnabled = n["WaveModeEnabled"].as<bool>(c.WaveModeEnabled);
            c.WaveCount = n["WaveCount"].as<int>(c.WaveCount);
            c.Wave1RightWall = n["Wave1RightWall"].as<float>(c.Wave1RightWall);
            c.Wave2RightWall = n["Wave2RightWall"].as<float>(c.Wave2RightWall);
            c.Wave3RightWall = n["Wave3RightWall"].as<float>(c.Wave3RightWall);
        }
    };

    template<> struct ComponentSerializer<SideCombatantComponent> {
        static constexpr const char* Key = "SideCombatantComponent";
        static void Serialize(YAML::Emitter& o, const SideCombatantComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "MaxHealth" << YAML::Value << c.MaxHealth;
            o << YAML::Key << "Health" << YAML::Value << c.Health;
            o << YAML::Key << "Attack" << YAML::Value << c.Attack;
            o << YAML::Key << "Defense" << YAML::Value << c.Defense;
            o << YAML::Key << "MoveSpeed" << YAML::Value << c.MoveSpeed;
            o << YAML::Key << "CollisionSize" << YAML::Value << c.CollisionSize;
            o << YAML::Key << "CollisionHeight" << YAML::Value << c.CollisionHeight;
            o << YAML::Key << "GravityScale" << YAML::Value << c.GravityScale;
            o << YAML::Key << "KnockbackResistance" << YAML::Value << c.KnockbackResistance;
            o << YAML::Key << "Invulnerable" << YAML::Value << c.Invulnerable;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideCombatantComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.MaxHealth = n["MaxHealth"].as<float>(c.MaxHealth);
            c.Health = n["Health"].as<float>(c.Health);
            c.Attack = n["Attack"].as<float>(c.Attack);
            c.Defense = n["Defense"].as<float>(c.Defense);
            c.MoveSpeed = n["MoveSpeed"].as<float>(c.MoveSpeed);
            c.CollisionSize = n["CollisionSize"].as<glm::vec2>(c.CollisionSize);
            c.CollisionHeight = n["CollisionHeight"].as<float>(c.CollisionHeight);
            c.GravityScale = n["GravityScale"].as<float>(c.GravityScale);
            c.KnockbackResistance = n["KnockbackResistance"].as<float>(c.KnockbackResistance);
            c.Invulnerable = n["Invulnerable"].as<bool>(c.Invulnerable);
        }
    };

    template<> struct ComponentSerializer<SidePlayerControllerComponent> {
        static constexpr const char* Key = "SidePlayerControllerComponent";
        static void Serialize(YAML::Emitter& o, const SidePlayerControllerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "MaxJumps" << YAML::Value << c.MaxJumps;
            o << YAML::Key << "JumpImpulse" << YAML::Value << c.JumpImpulse;
            o << YAML::Key << "Gravity" << YAML::Value << c.Gravity;
            o << YAML::Key << "AirControl" << YAML::Value << c.AirControl;
            o << YAML::Key << "JumpBufferTime" << YAML::Value << c.JumpBufferTime;
            o << YAML::Key << "CoyoteTime" << YAML::Value << c.CoyoteTime;
            o << YAML::Key << "LaneSpeedScale" << YAML::Value << c.LaneSpeedScale;
            o << YAML::Key << "LaneAcceleration" << YAML::Value << c.LaneAcceleration;
            o << YAML::Key << "GroundFriction" << YAML::Value << c.GroundFriction;
            o << YAML::Key << "BasicCooldown" << YAML::Value << c.BasicCooldown;
            o << YAML::Key << "LauncherCooldown" << YAML::Value << c.LauncherCooldown;
            o << YAML::Key << "MagicBoltCooldown" << YAML::Value << c.MagicBoltCooldown;
            o << YAML::Key << "AllySupportCooldown" << YAML::Value << c.AllySupportCooldown;
            o << YAML::Key << "DashCooldown" << YAML::Value << c.DashCooldown;
            o << YAML::Key << "DashManaCost" << YAML::Value << c.DashManaCost;
            o << YAML::Key << "DashSpeed" << YAML::Value << c.DashSpeed;
            o << YAML::Key << "DashInvulnerableTime" << YAML::Value << c.DashInvulnerableTime;
            o << YAML::Key << "MaxMana" << YAML::Value << c.MaxMana;
            o << YAML::Key << "LauncherManaCost" << YAML::Value << c.LauncherManaCost;
            o << YAML::Key << "MagicBoltManaCost" << YAML::Value << c.MagicBoltManaCost;
            o << YAML::Key << "AllySupportManaCost" << YAML::Value << c.AllySupportManaCost;
            o << YAML::Key << "HealItemAmount" << YAML::Value << c.HealItemAmount;
            o << YAML::Key << "ManaItemAmount" << YAML::Value << c.ManaItemAmount;
            o << YAML::Key << "AttackBuffMultiplier" << YAML::Value << c.AttackBuffMultiplier;
            o << YAML::Key << "AttackBuffDuration" << YAML::Value << c.AttackBuffDuration;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SidePlayerControllerComponent& c) {
            c.MaxJumps = n["MaxJumps"].as<int>(c.MaxJumps);
            c.JumpImpulse = n["JumpImpulse"].as<float>(c.JumpImpulse);
            c.Gravity = n["Gravity"].as<float>(c.Gravity);
            c.AirControl = n["AirControl"].as<float>(c.AirControl);
            c.JumpBufferTime = n["JumpBufferTime"].as<float>(c.JumpBufferTime);
            c.CoyoteTime = n["CoyoteTime"].as<float>(c.CoyoteTime);
            c.LaneSpeedScale = n["LaneSpeedScale"].as<float>(c.LaneSpeedScale);
            c.LaneAcceleration = n["LaneAcceleration"].as<float>(c.LaneAcceleration);
            c.GroundFriction = n["GroundFriction"].as<float>(c.GroundFriction);
            c.BasicCooldown = n["BasicCooldown"].as<float>(c.BasicCooldown);
            c.LauncherCooldown = n["LauncherCooldown"].as<float>(c.LauncherCooldown);
            c.MagicBoltCooldown = n["MagicBoltCooldown"].as<float>(c.MagicBoltCooldown);
            c.AllySupportCooldown = n["AllySupportCooldown"].as<float>(c.AllySupportCooldown);
            c.DashCooldown = n["DashCooldown"].as<float>(c.DashCooldown);
            c.DashManaCost = n["DashManaCost"].as<float>(c.DashManaCost);
            c.DashSpeed = n["DashSpeed"].as<float>(c.DashSpeed);
            c.DashInvulnerableTime = n["DashInvulnerableTime"].as<float>(c.DashInvulnerableTime);
            c.MaxMana = n["MaxMana"].as<float>(c.MaxMana);
            c.LauncherManaCost = n["LauncherManaCost"].as<float>(c.LauncherManaCost);
            c.MagicBoltManaCost = n["MagicBoltManaCost"].as<float>(c.MagicBoltManaCost);
            c.AllySupportManaCost = n["AllySupportManaCost"].as<float>(c.AllySupportManaCost);
            c.HealItemAmount = n["HealItemAmount"].as<float>(c.HealItemAmount);
            c.ManaItemAmount = n["ManaItemAmount"].as<float>(c.ManaItemAmount);
            c.AttackBuffMultiplier = n["AttackBuffMultiplier"].as<float>(c.AttackBuffMultiplier);
            c.AttackBuffDuration = n["AttackBuffDuration"].as<float>(c.AttackBuffDuration);
        }
    };

    template<> struct ComponentSerializer<SideEnemyAIComponent> {
        static constexpr const char* Key = "SideEnemyAIComponent";
        static void Serialize(YAML::Emitter& o, const SideEnemyAIComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Kind" << YAML::Value << (int)c.Kind;
            o << YAML::Key << "WaveIndex" << YAML::Value << c.WaveIndex;
            o << YAML::Key << "AggroRange" << YAML::Value << c.AggroRange;
            o << YAML::Key << "AttackRange" << YAML::Value << c.AttackRange;
            o << YAML::Key << "PreferredRange" << YAML::Value << c.PreferredRange;
            o << YAML::Key << "AttackInterval" << YAML::Value << c.AttackInterval;
            o << YAML::Key << "PatrolMinX" << YAML::Value << c.PatrolMinX;
            o << YAML::Key << "PatrolMaxX" << YAML::Value << c.PatrolMaxX;
            o << YAML::Key << "LaneTolerance" << YAML::Value << c.LaneTolerance;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideEnemyAIComponent& c) {
            c.Kind = (SideEnemyKind)n["Kind"].as<int>((int)c.Kind);
            c.WaveIndex = n["WaveIndex"].as<int>(c.WaveIndex);
            c.AggroRange = n["AggroRange"].as<float>(c.AggroRange);
            c.AttackRange = n["AttackRange"].as<float>(c.AttackRange);
            c.PreferredRange = n["PreferredRange"].as<float>(c.PreferredRange);
            c.AttackInterval = n["AttackInterval"].as<float>(c.AttackInterval);
            c.PatrolMinX = n["PatrolMinX"].as<float>(c.PatrolMinX);
            c.PatrolMaxX = n["PatrolMaxX"].as<float>(c.PatrolMaxX);
            c.LaneTolerance = n["LaneTolerance"].as<float>(c.LaneTolerance);
        }
    };

    template<> struct ComponentSerializer<SideHitboxComponent> {
        static constexpr const char* Key = "SideHitboxComponent";
        static void Serialize(YAML::Emitter& o, const SideHitboxComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Team" << YAML::Value << c.Team;
            o << YAML::Key << "AttackKind" << YAML::Value << (int)c.AttackKind;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Velocity" << YAML::Value << c.Velocity;
            o << YAML::Key << "LaunchVelocity" << YAML::Value << c.LaunchVelocity;
            o << YAML::Key << "AirHeight" << YAML::Value << c.AirHeight;
            o << YAML::Key << "AirRange" << YAML::Value << c.AirRange;
            o << YAML::Key << "Damage" << YAML::Value << c.Damage;
            o << YAML::Key << "Lifetime" << YAML::Value << c.Lifetime;
            o << YAML::Key << "HitStun" << YAML::Value << c.HitStun;
            o << YAML::Key << "AttackerAirImpulse" << YAML::Value << c.AttackerAirImpulse;
            o << YAML::Key << "AttackerAirFallStep" << YAML::Value << c.AttackerAirFallStep;
            o << YAML::Key << "TargetAirFallStep" << YAML::Value << c.TargetAirFallStep;
            o << YAML::Key << "ProtectionGain" << YAML::Value << c.ProtectionGain;
            o << YAML::Key << "DestroyOnHit" << YAML::Value << c.DestroyOnHit;
            o << YAML::Key << "TextureFramePattern" << YAML::Value << c.TextureFramePattern;
            o << YAML::Key << "TextureAtlasSheet" << YAML::Value << c.TextureAtlas.SheetPath;
            o << YAML::Key << "TextureAtlasCellWidth" << YAML::Value << c.TextureAtlas.CellWidth;
            o << YAML::Key << "TextureAtlasCellHeight" << YAML::Value << c.TextureAtlas.CellHeight;
            o << YAML::Key << "TextureAtlasColumns" << YAML::Value << c.TextureAtlas.Columns;
            o << YAML::Key << "TextureAtlasStartFrame" << YAML::Value << c.TextureAtlas.StartFrame;
            o << YAML::Key << "TextureFrameCount" << YAML::Value << c.TextureFrameCount;
            o << YAML::Key << "TextureFrameRate" << YAML::Value << c.TextureFrameRate;
            o << YAML::Key << "HitSound" << YAML::Value << c.HitSound;
            o << YAML::Key << "HitSoundVolume" << YAML::Value << c.HitSoundVolume;
            o << YAML::Key << "HitPause" << YAML::Value << c.HitPause;
            o << YAML::Key << "CameraShake" << YAML::Value << c.CameraShake;
            o << YAML::Key << "CameraShakeDuration" << YAML::Value << c.CameraShakeDuration;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SideHitboxComponent& c) {
            c.Team = n["Team"].as<int>(c.Team);
            c.AttackKind = (SideAttackKind)n["AttackKind"].as<int>((int)c.AttackKind);
            c.Size = n["Size"].as<glm::vec2>(c.Size);
            c.Velocity = n["Velocity"].as<glm::vec2>(c.Velocity);
            c.LaunchVelocity = n["LaunchVelocity"].as<glm::vec2>(c.LaunchVelocity);
            c.AirHeight = n["AirHeight"].as<float>(c.AirHeight);
            c.AirRange = n["AirRange"].as<float>(c.AirRange);
            c.Damage = n["Damage"].as<float>(c.Damage);
            c.Lifetime = n["Lifetime"].as<float>(c.Lifetime);
            c.HitStun = n["HitStun"].as<float>(c.HitStun);
            c.AttackerAirImpulse = n["AttackerAirImpulse"].as<float>(c.AttackerAirImpulse);
            c.AttackerAirFallStep = n["AttackerAirFallStep"].as<float>(c.AttackerAirFallStep);
            c.TargetAirFallStep = n["TargetAirFallStep"].as<float>(c.TargetAirFallStep);
            c.ProtectionGain = n["ProtectionGain"].as<float>(c.ProtectionGain);
            c.DestroyOnHit = n["DestroyOnHit"].as<bool>(c.DestroyOnHit);
            c.TextureFramePattern = n["TextureFramePattern"].as<std::string>(c.TextureFramePattern);
            c.TextureAtlas.SheetPath = n["TextureAtlasSheet"].as<std::string>(c.TextureAtlas.SheetPath);
            c.TextureAtlas.CellWidth = n["TextureAtlasCellWidth"].as<int>(c.TextureAtlas.CellWidth);
            c.TextureAtlas.CellHeight = n["TextureAtlasCellHeight"].as<int>(c.TextureAtlas.CellHeight);
            c.TextureAtlas.Columns = n["TextureAtlasColumns"].as<int>(c.TextureAtlas.Columns);
            c.TextureAtlas.StartFrame = n["TextureAtlasStartFrame"].as<int>(c.TextureAtlas.StartFrame);
            c.TextureFrameCount = n["TextureFrameCount"].as<int>(c.TextureFrameCount);
            c.TextureFrameRate = n["TextureFrameRate"].as<float>(c.TextureFrameRate);
            c.HitSound = n["HitSound"].as<std::string>(c.HitSound);
            c.HitSoundVolume = n["HitSoundVolume"].as<float>(c.HitSoundVolume);
            c.HitPause = n["HitPause"].as<float>(c.HitPause);
            c.CameraShake = n["CameraShake"].as<float>(c.CameraShake);
            c.CameraShakeDuration = n["CameraShakeDuration"].as<float>(c.CameraShakeDuration);
        }
    };

    template<> struct ComponentSerializer<SidePickupComponent> {
        static constexpr const char* Key = "SidePickupComponent";
        static void Serialize(YAML::Emitter& o, const SidePickupComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "ItemId" << YAML::Value << c.ItemId;
            o << YAML::Key << "DisplayName" << YAML::Value << c.DisplayName;
            o << YAML::Key << "Amount" << YAML::Value << c.Amount;
            o << YAML::Key << "PickupRadius" << YAML::Value << c.PickupRadius;
            o << YAML::Key << "AttractRadius" << YAML::Value << c.AttractRadius;
            o << YAML::Key << "AttractSpeed" << YAML::Value << c.AttractSpeed;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SidePickupComponent& c) {
            c.ItemId = n["ItemId"].as<std::string>(c.ItemId);
            c.DisplayName = n["DisplayName"].as<std::string>(c.DisplayName);
            c.Amount = n["Amount"].as<int>(c.Amount);
            c.PickupRadius = n["PickupRadius"].as<float>(c.PickupRadius);
            c.AttractRadius = n["AttractRadius"].as<float>(c.AttractRadius);
            c.AttractSpeed = n["AttractSpeed"].as<float>(c.AttractSpeed);
        }
    };


    using SideCombatModuleSceneComponents = ComponentGroup
    <
        SideCombatLevelComponent,
        SideCombatantComponent,
        SidePlayerControllerComponent,
        SideEnemyAIComponent,
        SideHitboxComponent,
        SidePickupComponent
    >;

    void SerializeSideCombatModuleSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(SideCombatModuleSceneComponents{}, out, entity);
    }

    void DeserializeSideCombatModuleSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(SideCombatModuleSceneComponents{}, node, entity);
    }

} // namespace Wheatear
