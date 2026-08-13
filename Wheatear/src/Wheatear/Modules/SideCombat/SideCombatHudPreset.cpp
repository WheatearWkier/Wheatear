#include "wtpch.h"
#include "SideCombatHudPreset.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace YAML {

    template<> struct convert<glm::vec2> {
        static Node encode(const glm::vec2& value) {
            Node node; node.push_back(value.x); node.push_back(value.y); return node;
        }
        static bool decode(const Node& node, glm::vec2& value) {
            if (!node.IsSequence() || node.size() != 2) return false;
            value = { node[0].as<float>(), node[1].as<float>() }; return true;
        }
    };

    template<> struct convert<glm::vec4> {
        static Node encode(const glm::vec4& value) {
            Node node; node.push_back(value.x); node.push_back(value.y); node.push_back(value.z); node.push_back(value.w); return node;
        }
        static bool decode(const Node& node, glm::vec4& value) {
            if (!node.IsSequence() || node.size() != 4) return false;
            value = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() }; return true;
        }
    };

} // namespace YAML

namespace Wheatear {

    namespace {

        static std::string ResolveHudPresetPath(const std::string& sourcePath)
        {
            if (sourcePath.empty() || sourcePath == "side.hud.preset")
                return AssetAliasRegistry::Path("side.hud.preset", SideCombatHudPreset::DefaultPath());
            return AssetAliasRegistry::Resolve(sourcePath);
        }

        static void SetStatus(std::string* status, const std::string& message)
        {
            if (status)
                *status = message;
        }

        static YAML::Emitter& EmitVec2(YAML::Emitter& o, const glm::vec2& value)
        {
            return o << YAML::Flow << YAML::BeginSeq << value.x << value.y << YAML::EndSeq;
        }

        static YAML::Emitter& EmitVec4(YAML::Emitter& o, const glm::vec4& value)
        {
            return o << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
        }

        static void EmitString(YAML::Emitter& o, const char* key, const std::string& value)
        {
            o << YAML::Key << key << YAML::Value << YAML::DoubleQuoted << value;
        }

        static void EmitHudRect(YAML::Emitter& o,
            const char* key,
            const SideCombatLevelComponent::HudRect& rect)
        {
            o << YAML::Key << key << YAML::Value << YAML::BeginMap;
            o << YAML::Key << "Position" << YAML::Value;
            EmitVec2(o, rect.Position);
            o << YAML::Key << "Size" << YAML::Value;
            EmitVec2(o, rect.Size);
            o << YAML::EndMap;
        }

        static void EmitStatusBadgeLayout(YAML::Emitter& o,
            const char* key,
            const SideCombatLevelComponent::StatusBadgeLayout& layout)
        {
            o << YAML::Key << key << YAML::Value << YAML::BeginMap;
            o << YAML::Key << "BuffStart" << YAML::Value;
            EmitVec2(o, layout.BuffStart);
            o << YAML::Key << "DebuffStart" << YAML::Value;
            EmitVec2(o, layout.DebuffStart);
            o << YAML::Key << "Size" << YAML::Value;
            EmitVec2(o, layout.Size);
            o << YAML::Key << "Gap" << YAML::Value << layout.Gap;
            o << YAML::EndMap;
        }

        static void EmitSkillHudSlots(YAML::Emitter& o,
            const std::vector<SideCombatLevelComponent::SkillHudSlot>& slots)
        {
            o << YAML::Key << "SkillHudSlots" << YAML::Value << YAML::BeginSeq;
            for (const auto& slot : slots)
            {
                o << YAML::BeginMap;
                o << YAML::Key << "Enabled" << YAML::Value << slot.Enabled;
                EmitString(o, "Key", slot.Key);
                EmitString(o, "KeyLabel", slot.KeyLabel);
                EmitString(o, "Command", slot.Command);
                o << YAML::Key << "Position" << YAML::Value;
                EmitVec2(o, slot.Position);
                o << YAML::Key << "Size" << YAML::Value;
                EmitVec2(o, slot.Size);
                o << YAML::Key << "TooltipPosition" << YAML::Value;
                EmitVec2(o, slot.TooltipPosition);
                o << YAML::Key << "UseSheetIcon" << YAML::Value << slot.UseSheetIcon;
                o << YAML::Key << "IconSheetPixels" << YAML::Value;
                EmitVec4(o, slot.IconSheetPixels);
                EmitString(o, "IconTexturePath", slot.IconTexturePath);
                EmitString(o, "TooltipText", slot.TooltipText);
                o << YAML::EndMap;
            }
            o << YAML::EndSeq;
        }

        static void EmitCombatItemHudSlots(YAML::Emitter& o,
            const std::vector<SideCombatLevelComponent::CombatItemHudSlot>& slots)
        {
            o << YAML::Key << "CombatItemHudSlots" << YAML::Value << YAML::BeginSeq;
            for (const auto& slot : slots)
            {
                o << YAML::BeginMap;
                o << YAML::Key << "Enabled" << YAML::Value << slot.Enabled;
                EmitString(o, "Key", slot.Key);
                EmitString(o, "Shortcut", slot.Shortcut);
                EmitString(o, "Command", slot.Command);
                o << YAML::Key << "Position" << YAML::Value;
                EmitVec2(o, slot.Position);
                o << YAML::Key << "FrameSize" << YAML::Value;
                EmitVec2(o, slot.FrameSize);
                o << YAML::Key << "IconInset" << YAML::Value;
                EmitVec2(o, slot.IconInset);
                o << YAML::Key << "IconSize" << YAML::Value;
                EmitVec2(o, slot.IconSize);
                o << YAML::Key << "TooltipPosition" << YAML::Value;
                EmitVec2(o, slot.TooltipPosition);
                o << YAML::Key << "UseSheetIcon" << YAML::Value << slot.UseSheetIcon;
                o << YAML::Key << "IconSheetPixels" << YAML::Value;
                EmitVec4(o, slot.IconSheetPixels);
                EmitString(o, "IconTexturePath", slot.IconTexturePath);
                EmitString(o, "DisplayName", slot.DisplayName);
                EmitString(o, "UsageText", slot.UsageText);
                o << YAML::EndMap;
            }
            o << YAML::EndSeq;
        }

        static void EmitHudPreset(YAML::Emitter& o,
            const SideCombatLevelComponent& level)
        {
            o << YAML::BeginMap;
            o << YAML::Key << "schema" << YAML::Value << "wheatear.side_combat.hud_preset.v1";
            o << YAML::Key << "version" << YAML::Value << 1;
            o << YAML::Key << "SideCombatHudPreset" << YAML::Value << YAML::BeginMap;

            EmitString(o, "PlayerEntityName", level.PlayerEntityName);
            EmitString(o, "BossEntityName", level.BossEntityName);
            EmitString(o, "FadeEntityName", level.FadeEntityName);
            EmitString(o, "MessageTextEntityName", level.MessageTextEntityName);
            EmitString(o, "ComboTextEntityName", level.ComboTextEntityName);
            EmitString(o, "SkillTextEntityName", level.SkillTextEntityName);
            EmitString(o, "RewardTextEntityName", level.RewardTextEntityName);
            EmitString(o, "PlayerHealthBarEntityName", level.PlayerHealthBarEntityName);
            EmitString(o, "PlayerHealthTextEntityName", level.PlayerHealthTextEntityName);
            EmitString(o, "BossHealthBarEntityName", level.BossHealthBarEntityName);
            EmitString(o, "BossHealthTextEntityName", level.BossHealthTextEntityName);
            EmitString(o, "CameraEntityName", level.CameraEntityName);
            EmitString(o, "TopPanelEntityName", level.TopPanelEntityName);
            EmitString(o, "ComboPanelEntityName", level.ComboPanelEntityName);
            EmitString(o, "ComboFrameEntityName", level.ComboFrameEntityName);
            EmitString(o, "ComboLabelEntityName", level.ComboLabelEntityName);
            EmitString(o, "ComboMultiplyEntityName", level.ComboMultiplyEntityName);
            EmitString(o, "ComboDigitPrefix", level.ComboDigitPrefix);
            EmitString(o, "SkillBarPanelEntityName", level.SkillBarPanelEntityName);
            EmitString(o, "SkillTooltipPanelEntityName", level.SkillTooltipPanelEntityName);
            EmitString(o, "SkillTooltipTextEntityName", level.SkillTooltipTextEntityName);
            EmitString(o, "JoystickBaseEntityName", level.JoystickBaseEntityName);
            EmitString(o, "JoystickThumbEntityName", level.JoystickThumbEntityName);
            EmitString(o, "PlayerManaEntityName", level.PlayerManaEntityName);
            EmitString(o, "PlayerUltimateFillEntityName", level.PlayerUltimateFillEntityName);
            EmitString(o, "PlayerUltimateMaskEntityName", level.PlayerUltimateMaskEntityName);
            EmitString(o, "BossProtectionEntityName", level.BossProtectionEntityName);
            EmitString(o, "PlayerStatusPrefix", level.PlayerStatusPrefix);
            EmitString(o, "EnemyStatusPrefix", level.EnemyStatusPrefix);
            EmitString(o, "SkillPrefix", level.SkillPrefix);
            EmitString(o, "ItemSlotPrefix", level.ItemSlotPrefix);

            EmitHudRect(o, "TopPanelLayout", level.TopPanelLayout);
            EmitHudRect(o, "PlayerHealthLayout", level.PlayerHealthLayout);
            EmitHudRect(o, "PlayerManaLayout", level.PlayerManaLayout);
            EmitHudRect(o, "PlayerUltimateLayout", level.PlayerUltimateLayout);
            EmitHudRect(o, "PlayerHealthTextLayout", level.PlayerHealthTextLayout);
            EmitHudRect(o, "BossPanelLayout", level.BossPanelLayout);
            EmitHudRect(o, "BossHealthLayout", level.BossHealthLayout);
            EmitHudRect(o, "BossProtectionLayout", level.BossProtectionLayout);
            EmitHudRect(o, "BossHealthTextLayout", level.BossHealthTextLayout);
            EmitHudRect(o, "ComboTextLayout", level.ComboTextLayout);
            EmitHudRect(o, "ComboFrameLayout", level.ComboFrameLayout);
            EmitHudRect(o, "SkillTooltipLayout", level.SkillTooltipLayout);
            o << YAML::Key << "SkillTooltipPadding" << YAML::Value;
            EmitVec2(o, level.SkillTooltipPadding);
            EmitHudRect(o, "JoystickBaseLayout", level.JoystickBaseLayout);
            o << YAML::Key << "JoystickThumbSize" << YAML::Value;
            EmitVec2(o, level.JoystickThumbSize);
            o << YAML::Key << "JoystickThumbTravel" << YAML::Value;
            EmitVec2(o, level.JoystickThumbTravel);
            EmitStatusBadgeLayout(o, "PlayerStatusLayout", level.PlayerStatusLayout);
            EmitStatusBadgeLayout(o, "EnemyStatusLayout", level.EnemyStatusLayout);
            EmitSkillHudSlots(o, level.SkillHudSlots);
            EmitCombatItemHudSlots(o, level.CombatItemHudSlots);

            EmitString(o, "HudLockedText", level.HudLockedText);
            EmitString(o, "HudUnavailableText", level.HudUnavailableText);
            EmitString(o, "HudInsufficientManaText", level.HudInsufficientManaText);
            EmitString(o, "HudConditionText", level.HudConditionText);
            EmitString(o, "HudGaugeText", level.HudGaugeText);
            EmitString(o, "HudComboText", level.HudComboText);
            EmitString(o, "HudArmorText", level.HudArmorText);
            EmitString(o, "HudCooldownPrefix", level.HudCooldownPrefix);
            EmitString(o, "HudSecondsSuffix", level.HudSecondsSuffix);
            EmitString(o, "HudManaNotEnoughTooltip", level.HudManaNotEnoughTooltip);
            EmitString(o, "HudNotUnlockedTooltip", level.HudNotUnlockedTooltip);
            EmitString(o, "BreakLimitGaugeNotEnoughTooltip", level.BreakLimitGaugeNotEnoughTooltip);
            EmitString(o, "BreakLimitComboNotEnoughTooltip", level.BreakLimitComboNotEnoughTooltip);
            EmitString(o, "BreakLimitBossNotReadyTooltip", level.BreakLimitBossNotReadyTooltip);
            EmitString(o, "HudDefaultMessage", level.HudDefaultMessage);
            EmitString(o, "HudAirBasicMessage", level.HudAirBasicMessage);
            EmitString(o, "HudMagicMessage", level.HudMagicMessage);
            EmitString(o, "HudDashMessage", level.HudDashMessage);
            EmitString(o, "HudReservedSkillMessage", level.HudReservedSkillMessage);
            EmitString(o, "HudSupportMessage", level.HudSupportMessage);
            EmitString(o, "HudBreakLimitInputMessage", level.HudBreakLimitInputMessage);
            EmitString(o, "HudBreakLimitDebugInputMessage", level.HudBreakLimitDebugInputMessage);
            EmitString(o, "HudVictoryMessage", level.HudVictoryMessage);
            EmitString(o, "HudDefeatMessage", level.HudDefeatMessage);
            EmitString(o, "HudHighAirMessage", level.HudHighAirMessage);
            EmitString(o, "HudLowAirMessage", level.HudLowAirMessage);
            EmitString(o, "HudBreakLimitHintMessage", level.HudBreakLimitHintMessage);
            EmitString(o, "HudPlayerHealthLabel", level.HudPlayerHealthLabel);
            EmitString(o, "HudBossHealthLabel", level.HudBossHealthLabel);
            EmitString(o, "HudBossProtectionLabel", level.HudBossProtectionLabel);
            EmitString(o, "HudManaGaugeLabel", level.HudManaGaugeLabel);
            EmitString(o, "HudAirActionsLabel", level.HudAirActionsLabel);
            EmitString(o, "HudRewardFallbackText", level.HudRewardFallbackText);
            EmitString(o, "HudCollectedPrefix", level.HudCollectedPrefix);

            o << YAML::EndMap;
            o << YAML::EndMap;
        }

        static bool CaptureWidgetRect(Scene* scene,
            const std::string& name,
            glm::vec2& position,
            glm::vec2& size)
        {
            Entity entity = SceneQueries::FindEntityByName(scene, name);
            if (!entity || !entity.HasComponent<UIWidgetComponent>())
                return false;

            const auto& widget = entity.GetComponent<UIWidgetComponent>();
            position = widget.Position;
            size = widget.Size;
            return true;
        }

        static bool CaptureWidgetRect(Scene* scene,
            const std::string& name,
            SideCombatLevelComponent::HudRect& rect)
        {
            return CaptureWidgetRect(scene, name, rect.Position, rect.Size);
        }

        static int CaptureStatusBadgeLayout(Scene* scene,
            const std::string& prefix,
            SideCombatLevelComponent::StatusBadgeLayout& layout)
        {
            if (prefix.empty())
                return 0;

            int captured = 0;
            glm::vec2 position = layout.BuffStart;
            glm::vec2 size = layout.Size;
            if (CaptureWidgetRect(scene, prefix + "_Buff_0", position, size))
            {
                layout.BuffStart = position;
                layout.Size = size;
                captured++;
            }

            position = layout.DebuffStart;
            size = layout.Size;
            if (CaptureWidgetRect(scene, prefix + "_Debuff_0", position, size))
            {
                layout.DebuffStart = position;
                layout.Size = size;
                captured++;
            }

            glm::vec2 secondPosition = {};
            glm::vec2 secondSize = {};
            if (CaptureWidgetRect(scene, prefix + "_Buff_1", secondPosition, secondSize))
            {
                layout.Gap = secondPosition.x - layout.BuffStart.x;
                captured++;
            }

            return captured;
        }

        static void ApplyHudRect(const YAML::Node& node,
            SideCombatLevelComponent::HudRect& rect)
        {
            if (!node)
                return;

            rect.Position = node["Position"].as<glm::vec2>(rect.Position);
            rect.Size = node["Size"].as<glm::vec2>(rect.Size);
        }

        static void ApplyStatusBadgeLayout(const YAML::Node& node,
            SideCombatLevelComponent::StatusBadgeLayout& layout)
        {
            if (!node)
                return;

            layout.BuffStart = node["BuffStart"].as<glm::vec2>(layout.BuffStart);
            layout.DebuffStart = node["DebuffStart"].as<glm::vec2>(layout.DebuffStart);
            layout.Size = node["Size"].as<glm::vec2>(layout.Size);
            layout.Gap = node["Gap"].as<float>(layout.Gap);
        }

        static void ApplySkillHudSlots(const YAML::Node& node,
            std::vector<SideCombatLevelComponent::SkillHudSlot>& slots)
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

        static void ApplyCombatItemHudSlots(const YAML::Node& node,
            std::vector<SideCombatLevelComponent::CombatItemHudSlot>& slots)
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

        static void ApplyHudPresetNode(const YAML::Node& preset,
            SideCombatLevelComponent& level)
        {
            level.PlayerEntityName = preset["PlayerEntityName"].as<std::string>(level.PlayerEntityName);
            level.BossEntityName = preset["BossEntityName"].as<std::string>(level.BossEntityName);
            level.FadeEntityName = preset["FadeEntityName"].as<std::string>(level.FadeEntityName);
            level.MessageTextEntityName = preset["MessageTextEntityName"].as<std::string>(level.MessageTextEntityName);
            level.ComboTextEntityName = preset["ComboTextEntityName"].as<std::string>(level.ComboTextEntityName);
            level.SkillTextEntityName = preset["SkillTextEntityName"].as<std::string>(level.SkillTextEntityName);
            level.RewardTextEntityName = preset["RewardTextEntityName"].as<std::string>(level.RewardTextEntityName);
            level.PlayerHealthBarEntityName = preset["PlayerHealthBarEntityName"].as<std::string>(level.PlayerHealthBarEntityName);
            level.PlayerHealthTextEntityName = preset["PlayerHealthTextEntityName"].as<std::string>(level.PlayerHealthTextEntityName);
            level.BossHealthBarEntityName = preset["BossHealthBarEntityName"].as<std::string>(level.BossHealthBarEntityName);
            level.BossHealthTextEntityName = preset["BossHealthTextEntityName"].as<std::string>(level.BossHealthTextEntityName);
            level.CameraEntityName = preset["CameraEntityName"].as<std::string>(level.CameraEntityName);
            level.TopPanelEntityName = preset["TopPanelEntityName"].as<std::string>(level.TopPanelEntityName);
            level.ComboPanelEntityName = preset["ComboPanelEntityName"].as<std::string>(level.ComboPanelEntityName);
            level.ComboFrameEntityName = preset["ComboFrameEntityName"].as<std::string>(level.ComboFrameEntityName);
            level.ComboLabelEntityName = preset["ComboLabelEntityName"].as<std::string>(level.ComboLabelEntityName);
            level.ComboMultiplyEntityName = preset["ComboMultiplyEntityName"].as<std::string>(level.ComboMultiplyEntityName);
            level.ComboDigitPrefix = preset["ComboDigitPrefix"].as<std::string>(level.ComboDigitPrefix);
            level.SkillBarPanelEntityName = preset["SkillBarPanelEntityName"].as<std::string>(level.SkillBarPanelEntityName);
            level.SkillTooltipPanelEntityName = preset["SkillTooltipPanelEntityName"].as<std::string>(level.SkillTooltipPanelEntityName);
            level.SkillTooltipTextEntityName = preset["SkillTooltipTextEntityName"].as<std::string>(level.SkillTooltipTextEntityName);
            level.JoystickBaseEntityName = preset["JoystickBaseEntityName"].as<std::string>(level.JoystickBaseEntityName);
            level.JoystickThumbEntityName = preset["JoystickThumbEntityName"].as<std::string>(level.JoystickThumbEntityName);
            level.PlayerManaEntityName = preset["PlayerManaEntityName"].as<std::string>(level.PlayerManaEntityName);
            level.PlayerUltimateFillEntityName = preset["PlayerUltimateFillEntityName"].as<std::string>(level.PlayerUltimateFillEntityName);
            level.PlayerUltimateMaskEntityName = preset["PlayerUltimateMaskEntityName"].as<std::string>(level.PlayerUltimateMaskEntityName);
            level.BossProtectionEntityName = preset["BossProtectionEntityName"].as<std::string>(level.BossProtectionEntityName);
            level.PlayerStatusPrefix = preset["PlayerStatusPrefix"].as<std::string>(level.PlayerStatusPrefix);
            level.EnemyStatusPrefix = preset["EnemyStatusPrefix"].as<std::string>(level.EnemyStatusPrefix);
            level.SkillPrefix = preset["SkillPrefix"].as<std::string>(level.SkillPrefix);
            level.ItemSlotPrefix = preset["ItemSlotPrefix"].as<std::string>(level.ItemSlotPrefix);

            ApplyHudRect(preset["TopPanelLayout"], level.TopPanelLayout);
            ApplyHudRect(preset["PlayerHealthLayout"], level.PlayerHealthLayout);
            ApplyHudRect(preset["PlayerManaLayout"], level.PlayerManaLayout);
            ApplyHudRect(preset["PlayerUltimateLayout"], level.PlayerUltimateLayout);
            ApplyHudRect(preset["PlayerHealthTextLayout"], level.PlayerHealthTextLayout);
            ApplyHudRect(preset["BossPanelLayout"], level.BossPanelLayout);
            ApplyHudRect(preset["BossHealthLayout"], level.BossHealthLayout);
            ApplyHudRect(preset["BossProtectionLayout"], level.BossProtectionLayout);
            ApplyHudRect(preset["BossHealthTextLayout"], level.BossHealthTextLayout);
            ApplyHudRect(preset["ComboTextLayout"], level.ComboTextLayout);
            ApplyHudRect(preset["ComboFrameLayout"], level.ComboFrameLayout);
            ApplyHudRect(preset["SkillTooltipLayout"], level.SkillTooltipLayout);
            level.SkillTooltipPadding = preset["SkillTooltipPadding"].as<glm::vec2>(level.SkillTooltipPadding);
            ApplyHudRect(preset["JoystickBaseLayout"], level.JoystickBaseLayout);
            level.JoystickThumbSize = preset["JoystickThumbSize"].as<glm::vec2>(level.JoystickThumbSize);
            level.JoystickThumbTravel = preset["JoystickThumbTravel"].as<glm::vec2>(level.JoystickThumbTravel);
            ApplyStatusBadgeLayout(preset["PlayerStatusLayout"], level.PlayerStatusLayout);
            ApplyStatusBadgeLayout(preset["EnemyStatusLayout"], level.EnemyStatusLayout);
            ApplySkillHudSlots(preset["SkillHudSlots"], level.SkillHudSlots);
            ApplyCombatItemHudSlots(preset["CombatItemHudSlots"], level.CombatItemHudSlots);

            level.HudLockedText = preset["HudLockedText"].as<std::string>(level.HudLockedText);
            level.HudUnavailableText = preset["HudUnavailableText"].as<std::string>(level.HudUnavailableText);
            level.HudInsufficientManaText = preset["HudInsufficientManaText"].as<std::string>(level.HudInsufficientManaText);
            level.HudConditionText = preset["HudConditionText"].as<std::string>(level.HudConditionText);
            level.HudGaugeText = preset["HudGaugeText"].as<std::string>(level.HudGaugeText);
            level.HudComboText = preset["HudComboText"].as<std::string>(level.HudComboText);
            level.HudArmorText = preset["HudArmorText"].as<std::string>(level.HudArmorText);
            level.HudCooldownPrefix = preset["HudCooldownPrefix"].as<std::string>(level.HudCooldownPrefix);
            level.HudSecondsSuffix = preset["HudSecondsSuffix"].as<std::string>(level.HudSecondsSuffix);
            level.HudManaNotEnoughTooltip = preset["HudManaNotEnoughTooltip"].as<std::string>(level.HudManaNotEnoughTooltip);
            level.HudNotUnlockedTooltip = preset["HudNotUnlockedTooltip"].as<std::string>(level.HudNotUnlockedTooltip);
            level.BreakLimitGaugeNotEnoughTooltip = preset["BreakLimitGaugeNotEnoughTooltip"].as<std::string>(level.BreakLimitGaugeNotEnoughTooltip);
            level.BreakLimitComboNotEnoughTooltip = preset["BreakLimitComboNotEnoughTooltip"].as<std::string>(level.BreakLimitComboNotEnoughTooltip);
            level.BreakLimitBossNotReadyTooltip = preset["BreakLimitBossNotReadyTooltip"].as<std::string>(level.BreakLimitBossNotReadyTooltip);
            level.HudDefaultMessage = preset["HudDefaultMessage"].as<std::string>(level.HudDefaultMessage);
            level.HudAirBasicMessage = preset["HudAirBasicMessage"].as<std::string>(level.HudAirBasicMessage);
            level.HudMagicMessage = preset["HudMagicMessage"].as<std::string>(level.HudMagicMessage);
            level.HudDashMessage = preset["HudDashMessage"].as<std::string>(level.HudDashMessage);
            level.HudReservedSkillMessage = preset["HudReservedSkillMessage"].as<std::string>(level.HudReservedSkillMessage);
            level.HudSupportMessage = preset["HudSupportMessage"].as<std::string>(level.HudSupportMessage);
            level.HudBreakLimitInputMessage = preset["HudBreakLimitInputMessage"].as<std::string>(level.HudBreakLimitInputMessage);
            level.HudBreakLimitDebugInputMessage = preset["HudBreakLimitDebugInputMessage"].as<std::string>(level.HudBreakLimitDebugInputMessage);
            level.HudVictoryMessage = preset["HudVictoryMessage"].as<std::string>(level.HudVictoryMessage);
            level.HudDefeatMessage = preset["HudDefeatMessage"].as<std::string>(level.HudDefeatMessage);
            level.HudHighAirMessage = preset["HudHighAirMessage"].as<std::string>(level.HudHighAirMessage);
            level.HudLowAirMessage = preset["HudLowAirMessage"].as<std::string>(level.HudLowAirMessage);
            level.HudBreakLimitHintMessage = preset["HudBreakLimitHintMessage"].as<std::string>(level.HudBreakLimitHintMessage);
            level.HudPlayerHealthLabel = preset["HudPlayerHealthLabel"].as<std::string>(level.HudPlayerHealthLabel);
            level.HudBossHealthLabel = preset["HudBossHealthLabel"].as<std::string>(level.HudBossHealthLabel);
            level.HudBossProtectionLabel = preset["HudBossProtectionLabel"].as<std::string>(level.HudBossProtectionLabel);
            level.HudManaGaugeLabel = preset["HudManaGaugeLabel"].as<std::string>(level.HudManaGaugeLabel);
            level.HudAirActionsLabel = preset["HudAirActionsLabel"].as<std::string>(level.HudAirActionsLabel);
            level.HudRewardFallbackText = preset["HudRewardFallbackText"].as<std::string>(level.HudRewardFallbackText);
            level.HudCollectedPrefix = preset["HudCollectedPrefix"].as<std::string>(level.HudCollectedPrefix);
        }

    } // namespace

    SideCombatLevelComponent::SideCombatLevelComponent()
    {
        SideCombatHudPreset::Apply(*this);
    }

} // namespace Wheatear

namespace Wheatear::SideCombatHudPreset {

    const char* DefaultPath()
    {
        return "assets/vertical_slice/data/side_combat_hud_preset.yaml";
    }

    bool Apply(SideCombatLevelComponent& level,
        const std::string& sourcePath)
    {
        const std::string requestedPath = sourcePath.empty()
            ? level.HudPresetPath
            : sourcePath;
        const std::string assetPath = ResolveHudPresetPath(requestedPath);
        if (assetPath.empty())
            return false;

        const std::filesystem::path resolvedPath = AssetPath::ResolveRuntimeData(assetPath);
        if (!std::filesystem::is_regular_file(resolvedPath))
        {
            WT_CORE_WARN("SideCombat HUD preset not found: {0}", resolvedPath.string());
            return false;
        }

        try
        {
            const YAML::Node root = YAML::LoadFile(resolvedPath.string());
            const YAML::Node preset = root["SideCombatHudPreset"] ? root["SideCombatHudPreset"] : root;
            if (!preset || !preset.IsMap())
                return false;

            ApplyHudPresetNode(preset, level);
            level.HudPresetPath = requestedPath.empty() ? "side.hud.preset" : requestedPath;
            return true;
        }
        catch (const YAML::Exception& exception)
        {
            WT_CORE_WARN("SideCombat HUD preset load failed for '{0}': {1}", resolvedPath.string(), exception.what());
        }

        return false;
    }

    int CaptureSceneLayout(SideCombatLevelComponent& level,
        Scene* scene)
    {
        if (!scene)
            return 0;

        int captured = 0;
        captured += CaptureWidgetRect(scene, level.TopPanelEntityName, level.TopPanelLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerHealthBarEntityName, level.PlayerHealthLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerManaEntityName, level.PlayerManaLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerUltimateMaskEntityName, level.PlayerUltimateLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerHealthTextEntityName, level.PlayerHealthTextLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.ComboPanelEntityName, level.BossPanelLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.BossHealthBarEntityName, level.BossHealthLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.BossProtectionEntityName, level.BossProtectionLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.BossHealthTextEntityName, level.BossHealthTextLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.ComboTextEntityName, level.ComboTextLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.ComboFrameEntityName, level.ComboFrameLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.SkillTooltipPanelEntityName, level.SkillTooltipLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.JoystickBaseEntityName, level.JoystickBaseLayout) ? 1 : 0;

        glm::vec2 thumbPosition = {};
        glm::vec2 thumbSize = {};
        if (CaptureWidgetRect(scene, level.JoystickThumbEntityName, thumbPosition, thumbSize))
        {
            level.JoystickThumbSize = thumbSize;
            captured++;
        }

        const std::string skillPrefix = level.SkillPrefix.empty() ? "SC_Skill" : level.SkillPrefix;
        for (auto& slot : level.SkillHudSlots)
        {
            if (slot.Key.empty())
                continue;

            if (CaptureWidgetRect(scene, skillPrefix + "Icon_" + slot.Key, slot.Position, slot.Size)
                || CaptureWidgetRect(scene, skillPrefix + "Slot_" + slot.Key, slot.Position, slot.Size))
            {
                captured++;
            }
        }

        const std::string itemSlotPrefix = level.ItemSlotPrefix.empty() ? "SC_ItemSlot_" : level.ItemSlotPrefix;
        for (auto& slot : level.CombatItemHudSlots)
        {
            if (slot.Key.empty())
                continue;

            const std::string prefix = itemSlotPrefix + slot.Key;
            glm::vec2 framePosition = slot.Position;
            glm::vec2 frameSize = slot.FrameSize;
            if (CaptureWidgetRect(scene, prefix + "_Frame", framePosition, frameSize)
                || CaptureWidgetRect(scene, prefix + "_Button", framePosition, frameSize))
            {
                slot.Position = framePosition;
                slot.FrameSize = frameSize;
                captured++;
            }

            glm::vec2 iconPosition = slot.Position + slot.IconInset;
            glm::vec2 iconSize = slot.IconSize;
            if (CaptureWidgetRect(scene, prefix + "_Icon", iconPosition, iconSize))
            {
                slot.IconInset = iconPosition - slot.Position;
                slot.IconSize = iconSize;
                captured++;
            }
        }

        captured += CaptureStatusBadgeLayout(scene, level.PlayerStatusPrefix, level.PlayerStatusLayout);
        captured += CaptureStatusBadgeLayout(scene, level.EnemyStatusPrefix, level.EnemyStatusLayout);

        return captured;
    }

    bool Save(const SideCombatLevelComponent& level,
        const std::string& sourcePath,
        std::string* status)
    {
        const std::string requestedPath = sourcePath.empty()
            ? level.HudPresetPath
            : sourcePath;
        const std::string assetPath = ResolveHudPresetPath(requestedPath);
        if (assetPath.empty())
        {
            SetStatus(status, "HUD preset path is empty.");
            return false;
        }

        const std::filesystem::path resolvedPath = AssetPath::Resolve(assetPath);
        try
        {
            const std::filesystem::path parent = resolvedPath.parent_path();
            if (!parent.empty())
                std::filesystem::create_directories(parent);

            YAML::Emitter out;
            out.SetIndent(2);
            EmitHudPreset(out, level);
            if (!out.good())
            {
                SetStatus(status, std::string("HUD preset YAML emit failed: ") + out.GetLastError());
                return false;
            }

            std::ofstream output(resolvedPath, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                SetStatus(status, "Failed to open HUD preset for writing: " + resolvedPath.string());
                return false;
            }

            output << out.c_str() << "\n";
            output.close();
            if (!output)
            {
                SetStatus(status, "Failed to finish writing HUD preset: " + resolvedPath.string());
                return false;
            }

            SetStatus(status, "Saved HUD preset: " + AssetPath::ToProjectRelative(resolvedPath).generic_string());
            return true;
        }
        catch (const std::exception& exception)
        {
            SetStatus(status, std::string("HUD preset save failed: ") + exception.what());
        }

        return false;
    }

} // namespace Wheatear::SideCombatHudPreset
