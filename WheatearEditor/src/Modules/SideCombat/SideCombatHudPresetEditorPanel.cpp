#include "wepch.h"
#include "SideCombatHudPresetEditorPanel.h"

#include "Editor/CommandBuilder.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorWidgets.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Modules/SideCombat/SideCombatHudPreset.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace Wheatear {

    namespace {

        static bool s_HasPendingHudPresetOpen = false;
        static std::string s_PendingHudPresetPath;

        static void DrawStringField(const char* label, std::string& value, bool& dirty, size_t capacity = 256)
        {
            if (EditorWidgets::InputString(label, value, capacity))
                dirty = true;
        }

        static void DrawMultilineField(const char* label, std::string& value, bool& dirty)
        {
            if (EditorWidgets::InputMultilineString(label, value, ImVec2(-1.0f, 68.0f), std::max<size_t>(2048, value.size() + 256)))
                dirty = true;
        }

        static void DrawVec2Field(const char* label, glm::vec2& value, bool& dirty)
        {
            if (ImGui::DragFloat2(label, glm::value_ptr(value), 0.001f, 0.0f, 1.0f, "%.3f"))
                dirty = true;
        }

        static void DrawVec4PixelsField(const char* label, glm::vec4& value, bool& dirty)
        {
            if (ImGui::DragFloat4(label, glm::value_ptr(value), 1.0f, 0.0f, 8192.0f, "%.1f"))
                dirty = true;
        }

        static void DrawHudRect(const char* label, SideCombatLevelComponent::HudRect& rect, bool& dirty)
        {
            ImGui::PushID(label);
            if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawVec2Field("Position", rect.Position, dirty);
                DrawVec2Field("Size", rect.Size, dirty);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        static void DrawStatusBadgeLayout(const char* label,
            SideCombatLevelComponent::StatusBadgeLayout& layout,
            bool& dirty)
        {
            ImGui::PushID(label);
            if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawVec2Field("Buff Start", layout.BuffStart, dirty);
                DrawVec2Field("Debuff Start", layout.DebuffStart, dirty);
                DrawVec2Field("Size", layout.Size, dirty);
                if (ImGui::DragFloat("Gap", &layout.Gap, 0.001f, 0.0f, 1.0f, "%.3f"))
                    dirty = true;
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        static void DrawSkillSlot(SideCombatLevelComponent::SkillHudSlot& slot, int index, bool& dirty)
        {
            ImGui::PushID(index);
            if (ImGui::Checkbox("Enabled", &slot.Enabled))
                dirty = true;
            DrawStringField("Key", slot.Key, dirty, 64);
            DrawStringField("Key Label", slot.KeyLabel, dirty, 64);
            if (EditorCommandBuilder::DrawCommandBuilder("Command", slot.Command, 256))
                dirty = true;
            DrawVec2Field("Position", slot.Position, dirty);
            DrawVec2Field("Size", slot.Size, dirty);
            DrawVec2Field("Tooltip Position", slot.TooltipPosition, dirty);
            if (ImGui::Checkbox("Use Sheet Icon", &slot.UseSheetIcon))
                dirty = true;
            DrawVec4PixelsField("Icon Sheet Pixels", slot.IconSheetPixels, dirty);
            if (EditorWidgets::DrawAssetReferenceField("Icon Texture",
                slot.IconTexturePath,
                EditorWidgets::AssetReferenceKind::Texture,
                512))
                dirty = true;
            DrawMultilineField("Tooltip", slot.TooltipText, dirty);
            ImGui::PopID();
        }

        static void DrawCombatItemSlot(SideCombatLevelComponent::CombatItemHudSlot& slot, int index, bool& dirty)
        {
            ImGui::PushID(index);
            if (ImGui::Checkbox("Enabled", &slot.Enabled))
                dirty = true;
            DrawStringField("Key", slot.Key, dirty, 64);
            DrawStringField("Shortcut", slot.Shortcut, dirty, 64);
            if (EditorCommandBuilder::DrawCommandBuilder("Command", slot.Command, 256))
                dirty = true;
            DrawVec2Field("Position", slot.Position, dirty);
            DrawVec2Field("Frame Size", slot.FrameSize, dirty);
            DrawVec2Field("Icon Inset", slot.IconInset, dirty);
            DrawVec2Field("Icon Size", slot.IconSize, dirty);
            DrawVec2Field("Tooltip Position", slot.TooltipPosition, dirty);
            if (ImGui::Checkbox("Use Sheet Icon", &slot.UseSheetIcon))
                dirty = true;
            DrawVec4PixelsField("Icon Sheet Pixels", slot.IconSheetPixels, dirty);
            if (EditorWidgets::DrawAssetReferenceField("Icon Texture",
                slot.IconTexturePath,
                EditorWidgets::AssetReferenceKind::Texture,
                512))
                dirty = true;
            DrawStringField("Display Name", slot.DisplayName, dirty, 256);
            DrawMultilineField("Usage Text", slot.UsageText, dirty);
            ImGui::PopID();
        }

        static std::filesystem::path ResolveHudPresetFile(const std::string& sourcePath)
        {
            const std::string resolved = sourcePath.empty()
                ? AssetAliasRegistry::Path("side.hud.preset", SideCombatHudPreset::DefaultPath())
                : AssetAliasRegistry::Resolve(sourcePath);
            return AssetPath::Resolve(resolved);
        }

    } // namespace

    namespace SideCombatEditorRequests {

        void RequestOpenHudPreset(const std::string& sourcePath)
        {
            s_PendingHudPresetPath = sourcePath;
            s_HasPendingHudPresetOpen = true;
        }

        bool ConsumeOpenHudPresetRequest(std::string& sourcePath)
        {
            if (!s_HasPendingHudPresetOpen)
                return false;

            sourcePath = s_PendingHudPresetPath;
            s_PendingHudPresetPath.clear();
            s_HasPendingHudPresetOpen = false;
            return true;
        }

    } // namespace SideCombatEditorRequests

    void SideCombatHudPresetEditorPanel::Open(const std::string& sourcePath)
    {
        m_Open = true;
        if (!sourcePath.empty() && sourcePath != m_SourcePath)
        {
            m_SourcePath = sourcePath;
            m_Loaded = false;
        }
    }

    void SideCombatHudPresetEditorPanel::Load()
    {
        m_Level = SideCombatLevelComponent{};
        m_Level.HudPresetPath = m_SourcePath;
        const bool loaded = SideCombatHudPreset::Apply(m_Level, m_SourcePath);
        m_Loaded = true;
        m_Dirty = false;
        m_Valid = loaded;
        m_Status = loaded ? "Loaded HUD preset." : "Failed to load HUD preset.";
        RefreshRawPreview();
    }

    void SideCombatHudPresetEditorPanel::Save()
    {
        m_Level.HudPresetPath = m_SourcePath;
        std::string status;
        const bool saved = SideCombatHudPreset::Save(m_Level, m_SourcePath, &status);
        m_Status = status.empty() ? (saved ? "Saved HUD preset." : "Failed to save HUD preset.") : status;
        if (saved)
        {
            m_Dirty = false;
            m_Valid = true;
            RefreshRawPreview();
        }
    }

    void SideCombatHudPresetEditorPanel::RefreshRawPreview()
    {
        m_RawPreview.clear();
        const std::filesystem::path path = ResolveHudPresetFile(m_SourcePath);
        std::ifstream input(path, std::ios::binary);
        if (input)
            m_RawPreview.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    void SideCombatHudPresetEditorPanel::DrawToolbar()
    {
        if (EditorWidgets::DrawAssetReferenceField("HUD Preset",
            m_SourcePath,
            EditorWidgets::AssetReferenceKind::Data,
            512))
        {
            m_Loaded = false;
            m_Dirty = false;
        }

        if (ImGui::Button("Reload Preset"))
            Load();
        ImGui::SameLine();
        bool reload = false;
        if (EditorWidgets::DirtySaveBar(m_Dirty, m_Status, "Save Preset", "Discard Preset", &reload))
            Save();
        if (reload)
            Load();
    }

    void SideCombatHudPresetEditorPanel::DrawBindingsTab()
    {
        EditorWidgets::SectionHeader("Entity Bindings", "Scene entity names and widget prefixes consumed by the HUD runtime.");
        DrawStringField("Player Entity", m_Level.PlayerEntityName, m_Dirty);
        DrawStringField("Boss Entity", m_Level.BossEntityName, m_Dirty);
        DrawStringField("Fade Entity", m_Level.FadeEntityName, m_Dirty);
        DrawStringField("Message Text", m_Level.MessageTextEntityName, m_Dirty);
        DrawStringField("Combo Text", m_Level.ComboTextEntityName, m_Dirty);
        DrawStringField("Skill Text", m_Level.SkillTextEntityName, m_Dirty);
        DrawStringField("Reward Text", m_Level.RewardTextEntityName, m_Dirty);
        DrawStringField("Player Health Bar", m_Level.PlayerHealthBarEntityName, m_Dirty);
        DrawStringField("Player Health Text", m_Level.PlayerHealthTextEntityName, m_Dirty);
        DrawStringField("Boss Health Bar", m_Level.BossHealthBarEntityName, m_Dirty);
        DrawStringField("Boss Health Text", m_Level.BossHealthTextEntityName, m_Dirty);
        DrawStringField("Camera", m_Level.CameraEntityName, m_Dirty);
        DrawStringField("Skill Prefix", m_Level.SkillPrefix, m_Dirty);
        DrawStringField("Item Slot Prefix", m_Level.ItemSlotPrefix, m_Dirty);
        DrawStringField("Player Status Prefix", m_Level.PlayerStatusPrefix, m_Dirty);
        DrawStringField("Enemy Status Prefix", m_Level.EnemyStatusPrefix, m_Dirty);
    }

    void SideCombatHudPresetEditorPanel::DrawLayoutsTab()
    {
        EditorWidgets::SectionHeader("HUD Layout", "Normalized widget positions and sizes saved into the preset asset.");
        DrawHudRect("Top Panel", m_Level.TopPanelLayout, m_Dirty);
        DrawHudRect("Player Health", m_Level.PlayerHealthLayout, m_Dirty);
        DrawHudRect("Player Mana", m_Level.PlayerManaLayout, m_Dirty);
        DrawHudRect("Player Ultimate", m_Level.PlayerUltimateLayout, m_Dirty);
        DrawHudRect("Player Health Text", m_Level.PlayerHealthTextLayout, m_Dirty);
        DrawHudRect("Boss Panel", m_Level.BossPanelLayout, m_Dirty);
        DrawHudRect("Boss Health", m_Level.BossHealthLayout, m_Dirty);
        DrawHudRect("Boss Protection", m_Level.BossProtectionLayout, m_Dirty);
        DrawHudRect("Boss Health Text", m_Level.BossHealthTextLayout, m_Dirty);
        DrawHudRect("Combo Text", m_Level.ComboTextLayout, m_Dirty);
        DrawHudRect("Combo Frame", m_Level.ComboFrameLayout, m_Dirty);
        DrawHudRect("Skill Tooltip", m_Level.SkillTooltipLayout, m_Dirty);
        DrawVec2Field("Skill Tooltip Padding", m_Level.SkillTooltipPadding, m_Dirty);
        DrawHudRect("Joystick Base", m_Level.JoystickBaseLayout, m_Dirty);
        DrawVec2Field("Joystick Thumb Size", m_Level.JoystickThumbSize, m_Dirty);
        DrawVec2Field("Joystick Thumb Travel", m_Level.JoystickThumbTravel, m_Dirty);
        DrawStatusBadgeLayout("Player Status", m_Level.PlayerStatusLayout, m_Dirty);
        DrawStatusBadgeLayout("Enemy Status", m_Level.EnemyStatusLayout, m_Dirty);
    }

    void SideCombatHudPresetEditorPanel::DrawSlotsTab()
    {
        EditorWidgets::SectionHeader("Skill HUD Slots", "Skill buttons, commands, icons, and tooltip text.");
        for (int i = 0; i < static_cast<int>(m_Level.SkillHudSlots.size()); ++i)
        {
            ImGui::PushID(i);
            const std::string header = "Skill Slot " + std::to_string(i + 1);
            bool remove = false;
            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawSkillSlot(m_Level.SkillHudSlots[static_cast<size_t>(i)], i, m_Dirty);
                if (ImGui::SmallButton("Remove Skill Slot"))
                    remove = true;
            }
            ImGui::PopID();
            if (remove)
            {
                m_Level.SkillHudSlots.erase(m_Level.SkillHudSlots.begin() + i);
                m_Dirty = true;
                break;
            }
        }
        if (ImGui::Button("Add Skill Slot"))
        {
            m_Level.SkillHudSlots.emplace_back();
            m_Dirty = true;
        }

        ImGui::Separator();
        EditorWidgets::SectionHeader("Combat Item Slots", "Consumable/action item slots and tooltip text.");
        for (int i = 0; i < static_cast<int>(m_Level.CombatItemHudSlots.size()); ++i)
        {
            ImGui::PushID(i);
            const std::string header = "Item Slot " + std::to_string(i + 1);
            bool remove = false;
            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawCombatItemSlot(m_Level.CombatItemHudSlots[static_cast<size_t>(i)], i, m_Dirty);
                if (ImGui::SmallButton("Remove Item Slot"))
                    remove = true;
            }
            ImGui::PopID();
            if (remove)
            {
                m_Level.CombatItemHudSlots.erase(m_Level.CombatItemHudSlots.begin() + i);
                m_Dirty = true;
                break;
            }
        }
        if (ImGui::Button("Add Item Slot"))
        {
            m_Level.CombatItemHudSlots.emplace_back();
            m_Dirty = true;
        }
    }

    void SideCombatHudPresetEditorPanel::DrawTextTab()
    {
        EditorWidgets::SectionHeader("HUD Text", "Labels, messages, and tooltip copy shown by the side-combat HUD.");
        DrawStringField("Locked Text", m_Level.HudLockedText, m_Dirty);
        DrawStringField("Unavailable Text", m_Level.HudUnavailableText, m_Dirty);
        DrawStringField("Insufficient Mana Text", m_Level.HudInsufficientManaText, m_Dirty);
        DrawStringField("Condition Text", m_Level.HudConditionText, m_Dirty);
        DrawStringField("Gauge Text", m_Level.HudGaugeText, m_Dirty);
        DrawStringField("Combo Text", m_Level.HudComboText, m_Dirty);
        DrawStringField("Armor Text", m_Level.HudArmorText, m_Dirty);
        DrawStringField("Cooldown Prefix", m_Level.HudCooldownPrefix, m_Dirty);
        DrawStringField("Seconds Suffix", m_Level.HudSecondsSuffix, m_Dirty);
        DrawMultilineField("Mana Tooltip", m_Level.HudManaNotEnoughTooltip, m_Dirty);
        DrawMultilineField("Locked Tooltip", m_Level.HudNotUnlockedTooltip, m_Dirty);
        DrawMultilineField("Break Gauge Tooltip", m_Level.BreakLimitGaugeNotEnoughTooltip, m_Dirty);
        DrawMultilineField("Break Combo Tooltip", m_Level.BreakLimitComboNotEnoughTooltip, m_Dirty);
        DrawMultilineField("Break Boss Tooltip", m_Level.BreakLimitBossNotReadyTooltip, m_Dirty);
        DrawMultilineField("Default Message", m_Level.HudDefaultMessage, m_Dirty);
        DrawMultilineField("Air Basic Message", m_Level.HudAirBasicMessage, m_Dirty);
        DrawMultilineField("Magic Message", m_Level.HudMagicMessage, m_Dirty);
        DrawMultilineField("Dash Message", m_Level.HudDashMessage, m_Dirty);
        DrawMultilineField("Reserved Skill Message", m_Level.HudReservedSkillMessage, m_Dirty);
        DrawMultilineField("Support Message", m_Level.HudSupportMessage, m_Dirty);
        DrawMultilineField("Break Limit Input Message", m_Level.HudBreakLimitInputMessage, m_Dirty);
        DrawMultilineField("Break Limit Debug Message", m_Level.HudBreakLimitDebugInputMessage, m_Dirty);
        DrawMultilineField("Victory Message", m_Level.HudVictoryMessage, m_Dirty);
        DrawMultilineField("Defeat Message", m_Level.HudDefeatMessage, m_Dirty);
        DrawMultilineField("High Air Message", m_Level.HudHighAirMessage, m_Dirty);
        DrawMultilineField("Low Air Message", m_Level.HudLowAirMessage, m_Dirty);
        DrawMultilineField("Break Limit Hint", m_Level.HudBreakLimitHintMessage, m_Dirty);
        DrawStringField("Player Health Label", m_Level.HudPlayerHealthLabel, m_Dirty);
        DrawStringField("Boss Health Label", m_Level.HudBossHealthLabel, m_Dirty);
        DrawStringField("Boss Protection Label", m_Level.HudBossProtectionLabel, m_Dirty);
        DrawStringField("Mana Gauge Label", m_Level.HudManaGaugeLabel, m_Dirty);
        DrawStringField("Air Actions Label", m_Level.HudAirActionsLabel, m_Dirty);
        DrawStringField("Reward Fallback Text", m_Level.HudRewardFallbackText, m_Dirty);
        DrawStringField("Collected Prefix", m_Level.HudCollectedPrefix, m_Dirty);
    }

    void SideCombatHudPresetEditorPanel::DrawRawPreviewTab()
    {
        RefreshRawPreview();
        EditorWidgets::InlineStatus("Read-only raw YAML preview. Use the structured tabs for normal HUD preset authoring.", EditorWidgets::StatusKind::Info);
        EditorGameplayShell::DrawRawPreview(m_RawPreview, "##SideCombatHudPresetRawPreview");
    }

    void SideCombatHudPresetEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        if (SideCombatEditorRequests::ConsumeOpenHudPresetRequest(requestedPath))
            Open(requestedPath);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        EditorFloatingWindow::Begin("Side Combat HUD Preset Editor", &m_Open, 0, { 1180.0f, 760.0f });
        EditorWidgets::PanelHeader("Side Combat HUD Preset", "Structured asset editor for HUD bindings, layout, slots, and text.");
        EditorFloatingWindow::DrawToggleButton("Side Combat HUD Preset Editor");
        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_Dirty,
            m_Valid,
            m_SourcePath,
            m_Status
        });
        DrawToolbar();

        if (ImGui::BeginTabBar("##SideCombatHudPresetTabs"))
        {
            if (ImGui::BeginTabItem("Bindings"))
            {
                DrawBindingsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Layout"))
            {
                DrawLayoutsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Slots"))
            {
                DrawSlotsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Text"))
            {
                DrawTextTab();
                ImGui::EndTabItem();
            }
            if (EditorGameplayShell::BeginRawPreviewTab("Advanced Raw"))
            {
                DrawRawPreviewTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        EditorFloatingWindow::End();
    }

} // namespace Wheatear
