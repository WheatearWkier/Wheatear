#include "wepch.h"
#include "SideCombatDrawer.h"
#include "Wheatear/Modules/SideCombat/SideCombatComponents.h"

#include "Editor/CommandBuilder.h"
#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/TextAssetEditor.h"
#include "Modules/SideCombat/SideCombatTuningEditorPanel.h"
#include "Panels/SceneHierarchy/ComponentDrawers.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Scene/Components.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <unordered_map>

namespace Wheatear {

    namespace {

        using EditorCommandBuilder::DrawCommandBuilder;
        using EditorWidgets::DrawTeamCombo;
        using EditorWidgets::InputString;

        static void DrawEnemyKindCombo(SideEnemyKind& kind)
        {
            static const char* labels[] = { "Grunt", "Thrower", "Pouncer", "Bear Boss" };
            int index = std::clamp((int)kind, 0, 3);
            if (ImGui::Combo(EditorLocale::Text("Kind", "类型"), &index, labels, 4))
                kind = (SideEnemyKind)index;
        }

        static void DrawAttackKindCombo(SideAttackKind& kind)
        {
            static const char* labels[] = {
                "Basic",
                "Launcher",
                "Magic Bolt",
                "Ally Support",
                "Enemy Melee",
                "Enemy Projectile",
                "Enemy Shockwave",
                "Break Limit",
                "Dash"
            };
            int index = std::clamp((int)kind, 0, 8);
            if (ImGui::Combo(EditorLocale::Text("Attack Kind", "攻击类型"), &index, labels, 9))
                kind = (SideAttackKind)index;
        }

        static const char* GetStateLabel(SideCombatState state)
        {
            static const char* labels[] = {
                EditorLocale::Text("Normal", "普通"),
                "Hit Stun",
                "Launched",
                "Knockdown",
                "Recovery",
                "Super Armor",
                "Broken",
                "Dead"
            };
            return labels[std::clamp((int)state, 0, 7)];
        }

        static void DrawSceneBinding(Entity entity, const char* label, std::string& value)
        {
            EditorContentPickers::DrawSceneEntityField(label, entity, value, 260);
        }

        static void DrawDeathRewardRow(Entity entity, SideCombatLevelComponent::DeathReward& reward, int index)
        {
            ImGui::PushID(index);
            ImGui::Checkbox(EditorLocale::Text("Enabled", "启用"), &reward.Enabled);
            ImGui::DragInt(EditorLocale::Text("Enemy Kind", "敌人类型"), &reward.EnemyKind, 1.0f, -1, 3);
            DrawSceneBinding(entity, "Source Entity", reward.SourceEntityName);
            DrawSceneBinding(entity, "Spawn Entity", reward.SpawnEntityName);
            EditorContentPickers::DrawProgressionIdField("Item Id",
                reward.ItemId,
                EditorContentPickers::ProgressionIdKind::Material,
                260);
            InputString("Display Name", reward.DisplayName, 260);
            ImGui::DragInt(EditorLocale::Text("Amount", "数量"), &reward.Amount, 1.0f, 1, 999);
            ImGui::DragFloat3(EditorLocale::Text("Offset", "偏移"), glm::value_ptr(reward.Offset), 0.02f);
            ImGui::DragFloat3(EditorLocale::Text("Scale", "缩放"), glm::value_ptr(reward.Scale), 0.02f, 0.01f, 20.0f);
            EditorWidgets::DrawAssetReferenceField("Texture",
                reward.TexturePath,
                EditorWidgets::AssetReferenceKind::Texture,
                260);
            ImGui::PopID();
        }

        static void DrawWaveSpawnRow(Entity entity, SideCombatLevelComponent::WaveSpawnDef& spawn, int index)
        {
            ImGui::PushID(index);
            ImGui::Checkbox(EditorLocale::Text("Enabled", "启用"), &spawn.Enabled);
            ImGui::DragInt(EditorLocale::Text("Wave Index", "波次"), &spawn.WaveIndex, 1.0f, 0, 2);
            ImGui::DragInt(EditorLocale::Text("Enemy Kind", "敌人类型"), &spawn.EnemyKind, 1.0f, 0, 2);
            ImGui::SameLine();
            EditorWidgets::HelpTooltip("0 = Grunt, 1 = Thrower, 2 = Pouncer (boss stays scene-driven)");
            ImGui::DragInt(EditorLocale::Text("Count", "数量"), &spawn.Count, 1.0f, 1, 32);
            ImGui::DragFloat(EditorLocale::Text("Spawn Min X", "出生 X 最小"), &spawn.SpawnMinX, 0.1f, -30.0f, 30.0f);
            ImGui::DragFloat(EditorLocale::Text("Spawn Max X", "出生 X 最大"), &spawn.SpawnMaxX, 0.1f, -30.0f, 30.0f);
            ImGui::DragFloat(EditorLocale::Text("Ground Y Offset", "地面 Y 偏移"), &spawn.GroundYOffset, 0.05f, -10.0f, 10.0f);
            ImGui::DragFloat(EditorLocale::Text("HP Variance", "生命浮动"), &spawn.HpVariance, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::PopID();
        }

        static void DrawHudRect(const char* label, SideCombatLevelComponent::HudRect& rect)
        {
            ImGui::PushID(label);
            if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat2("Position", glm::value_ptr(rect.Position), 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat2("Size", glm::value_ptr(rect.Size), 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        static void DrawStatusIconLayout(const char* label, SideCombatLevelComponent::StatusIconLayout& layout)
        {
            ImGui::PushID(label);
            if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat2(EditorLocale::Text("Buff Start", "增益起始"), glm::value_ptr(layout.BuffStart), 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat2(EditorLocale::Text("Debuff Start", "减益起始"), glm::value_ptr(layout.DebuffStart), 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat2("Size", glm::value_ptr(layout.Size), 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat(EditorLocale::Text("Gap", "间距"), &layout.Gap, 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        static void DrawSkillHudSlotRow(SideCombatLevelComponent::SkillHudSlot& slot, int index)
        {
            ImGui::PushID(index);
            ImGui::Checkbox(EditorLocale::Text("Enabled", "启用"), &slot.Enabled);
            InputString("Key", slot.Key, 64);
            InputString("Key Label", slot.KeyLabel, 64);
            DrawCommandBuilder("Command", slot.Command, 260);
            ImGui::DragFloat2("Position", glm::value_ptr(slot.Position), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat2("Size", glm::value_ptr(slot.Size), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat2("Tooltip Position", glm::value_ptr(slot.TooltipPosition), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::Checkbox(EditorLocale::Text("Use Sheet Icon", "使用图集图标"), &slot.UseSheetIcon);
            ImGui::DragFloat4(EditorLocale::Text("Icon Sheet Pixels", "图标图集像素"), glm::value_ptr(slot.IconSheetPixels), 1.0f, 0.0f, 8192.0f, "%.1f");
            EditorWidgets::DrawAssetReferenceField("Icon Texture",
                slot.IconTexturePath,
                EditorWidgets::AssetReferenceKind::Texture,
                260);
            InputString("Tooltip", slot.TooltipText, 512);
            ImGui::PopID();
        }

        static void DrawCombatItemHudSlotRow(SideCombatLevelComponent::CombatItemHudSlot& slot, int index)
        {
            ImGui::PushID(index);
            ImGui::Checkbox(EditorLocale::Text("Enabled", "启用"), &slot.Enabled);
            InputString("Key", slot.Key, 64);
            InputString("Shortcut", slot.Shortcut, 64);
            DrawCommandBuilder("Command", slot.Command, 260);
            ImGui::DragFloat2("Position", glm::value_ptr(slot.Position), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat2("Frame Size", glm::value_ptr(slot.FrameSize), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat2("Icon Inset", glm::value_ptr(slot.IconInset), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat2("Icon Size", glm::value_ptr(slot.IconSize), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat2("Tooltip Position", glm::value_ptr(slot.TooltipPosition), 0.001f, 0.0f, 1.0f, "%.3f");
            ImGui::Checkbox(EditorLocale::Text("Use Sheet Icon", "使用图集图标"), &slot.UseSheetIcon);
            ImGui::DragFloat4(EditorLocale::Text("Icon Sheet Pixels", "图标图集像素"), glm::value_ptr(slot.IconSheetPixels), 1.0f, 0.0f, 8192.0f, "%.1f");
            EditorWidgets::DrawAssetReferenceField("Icon Texture",
                slot.IconTexturePath,
                EditorWidgets::AssetReferenceKind::Texture,
                260);
            InputString("Display Name", slot.DisplayName, 260);
            InputString("Usage Text", slot.UsageText, 512);
            ImGui::PopID();
        }

        static std::unordered_map<std::string, EditorUI::TextAssetEditorState> s_TuningEditors;

    } // namespace

    void DrawSideCombatLevelComponent(Entity entity)
    {
        DrawComponent<SideCombatLevelComponent>("Side Combat Level", entity, [entity](auto& level)
        {
            EditorWidgets::StatusBadge("Edits Scene", EditorWidgets::StatusKind::Success);
            ImGui::Checkbox(EditorLocale::Text("Play On Start", "开始时播放"), &level.PlayOnStart);
            InputString("Level Id / Unlock Profile", level.LevelId);
            ImGui::TextDisabled("Level Id selects progression.profiles in the tuning YAML.");
            EditorWidgets::DrawAssetReferenceField("Tuning",
                level.TuningPath,
                EditorWidgets::AssetReferenceKind::Data,
                260);
            if (ImGui::Button(EditorLocale::Text("Open Side Combat Tuning Editor", "打开横版战斗调参编辑器")))
                SideCombatEditorRequests::RequestOpenTuning(level.TuningPath);
            if (ImGui::CollapsingHeader("Advanced Raw Side Combat Tuning YAML"))
            {
                EditorWidgets::InlineStatus("Advanced raw preview. Prefer Side Combat Tuning Editor for normal authoring.", EditorWidgets::StatusKind::Warning);
                EditorUI::DrawTextAssetEditor("Side Combat Tuning YAML",
                    "SideCombatTuningEditor",
                    AssetAliasRegistry::Resolve(level.TuningPath),
                    s_TuningEditors,
                    512 * 1024,
                    ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);
            }
            ImGui::SameLine();
            ImGui::DragFloat2("Arena Min", glm::value_ptr(level.ArenaMin), 0.05f);
            ImGui::DragFloat2("Arena Max", glm::value_ptr(level.ArenaMax), 0.05f);
            ImGui::DragFloat("Ground Y", &level.GroundY, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Lane Min Y", &level.LaneMinY, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Lane Max Y", &level.LaneMaxY, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Start Fade", &level.StartFadeDuration, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Victory Return Delay", &level.VictoryReturnDelay, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat(EditorLocale::Text("Defeat Return Delay", "战败返回延迟"), &level.DefeatReturnDelay, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Result Fade", &level.ResultSceneFadeDuration, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Combo Drop Delay", &level.ComboDropDelay, 0.02f, 0.2f, 5.0f);
            DrawCommandBuilder("Victory Command", level.VictorySceneCommand, 260);
            DrawCommandBuilder("Defeat Command", level.DefeatSceneCommand, 260);
            InputString("First Clear Reward", level.FirstClearRewardText, 260);

            ImGui::Separator();
            ImGui::TextDisabled("Death Rewards");
            for (int i = 0; i < (int)level.DeathRewards.size(); ++i)
            {
                ImGui::PushID(i);
                const std::string header = "Reward " + std::to_string(i + 1);
                bool removeReward = false;
                if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    DrawDeathRewardRow(entity, level.DeathRewards[i], i);
                    if (ImGui::Button("Remove"))
                        removeReward = true;
                }
                ImGui::PopID();
                if (removeReward)
                {
                    level.DeathRewards.erase(level.DeathRewards.begin() + i);
                    break;
                }
            }
            if (ImGui::Button("Add Reward"))
                level.DeathRewards.emplace_back();

            ImGui::Separator();
            ImGui::TextDisabled("Wave Spawns (data-driven enemies)");
            ImGui::SameLine();
            EditorWidgets::HelpTooltip(
                "Enemies are generated from this table at runtime; scene-placed "
                "enemy entities (except the boss) are removed while it is non-empty.");
            for (int i = 0; i < (int)level.WaveSpawns.size(); ++i)
            {
                ImGui::PushID(i);
                const std::string header = "Wave Spawn " + std::to_string(i + 1);
                bool removeSpawn = false;
                if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    DrawWaveSpawnRow(entity, level.WaveSpawns[i], i);
                    if (ImGui::Button("Remove"))
                        removeSpawn = true;
                }
                ImGui::PopID();
                if (removeSpawn)
                {
                    level.WaveSpawns.erase(level.WaveSpawns.begin() + i);
                    break;
                }
            }
            if (ImGui::Button("Add Wave Spawn"))
                level.WaveSpawns.emplace_back();

            ImGui::Separator();
            ImGui::TextDisabled("Wave / Air Wall Flow");
            ImGui::Checkbox("Wave Mode Enabled", &level.WaveModeEnabled);
            ImGui::DragInt("Wave Count", &level.WaveCount, 1.0f, 1, 3);
            ImGui::DragFloat("Wave 1 Right Wall", &level.Wave1RightWall, 0.05f, -50.0f, 50.0f);
            ImGui::DragFloat("Wave 2 Right Wall", &level.Wave2RightWall, 0.05f, -50.0f, 50.0f);
            ImGui::DragFloat("Wave 3 Right Wall", &level.Wave3RightWall, 0.05f, -50.0f, 50.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Scene Bindings");
            DrawSceneBinding(entity, "Player", level.PlayerEntityName);
            DrawSceneBinding(entity, "Boss", level.BossEntityName);
            DrawSceneBinding(entity, "Fade", level.FadeEntityName);
            DrawSceneBinding(entity, "Message Text", level.MessageTextEntityName);
            DrawSceneBinding(entity, "Combo Text", level.ComboTextEntityName);
            DrawSceneBinding(entity, "Skill Text", level.SkillTextEntityName);
            DrawSceneBinding(entity, "Reward Text", level.RewardTextEntityName);
            DrawSceneBinding(entity, "Player Health Bar", level.PlayerHealthBarEntityName);
            DrawSceneBinding(entity, "Player Health Text", level.PlayerHealthTextEntityName);
            DrawSceneBinding(entity, "Boss Health Bar", level.BossHealthBarEntityName);
            DrawSceneBinding(entity, "Boss Health Text", level.BossHealthTextEntityName);
            DrawSceneBinding(entity, "Camera", level.CameraEntityName);
            DrawSceneBinding(entity, "Top Panel", level.TopPanelEntityName);
            DrawSceneBinding(entity, "Combo Panel", level.ComboPanelEntityName);
            DrawSceneBinding(entity, "Combo Frame", level.ComboFrameEntityName);
            DrawSceneBinding(entity, "Combo Label", level.ComboLabelEntityName);
            DrawSceneBinding(entity, "Combo Multiply", level.ComboMultiplyEntityName);
            InputString("Combo Digit Prefix", level.ComboDigitPrefix, 128);
            DrawSceneBinding(entity, "Skill Bar Panel", level.SkillBarPanelEntityName);
            DrawSceneBinding(entity, "Skill Tooltip Panel", level.SkillTooltipPanelEntityName);
            DrawSceneBinding(entity, "Skill Tooltip Text", level.SkillTooltipTextEntityName);
            DrawSceneBinding(entity, "Joystick Base", level.JoystickBaseEntityName);
            DrawSceneBinding(entity, "Joystick Thumb", level.JoystickThumbEntityName);
            DrawSceneBinding(entity, "Player Mana", level.PlayerManaEntityName);
            DrawSceneBinding(entity, "Player Ultimate Fill", level.PlayerUltimateFillEntityName);
            DrawSceneBinding(entity, "Player Ultimate Mask", level.PlayerUltimateMaskEntityName);
            DrawSceneBinding(entity, "Boss Protection", level.BossProtectionEntityName);
            InputString("Player Status Prefix", level.PlayerStatusPrefix, 128);
            InputString("Enemy Status Prefix", level.EnemyStatusPrefix, 128);
            InputString("Skill Prefix", level.SkillPrefix, 128);
            InputString("Item Slot Prefix", level.ItemSlotPrefix, 128);

            if (ImGui::CollapsingHeader("HUD Layout"))
            {
                DrawHudRect("Top Panel", level.TopPanelLayout);
                DrawHudRect("Player Health", level.PlayerHealthLayout);
                DrawHudRect("Player Mana", level.PlayerManaLayout);
                DrawHudRect("Player Ultimate", level.PlayerUltimateLayout);
                DrawHudRect("Player Health Text", level.PlayerHealthTextLayout);
                DrawHudRect("Boss Panel", level.BossPanelLayout);
                DrawHudRect("Boss Health", level.BossHealthLayout);
                DrawHudRect("Boss Protection", level.BossProtectionLayout);
                DrawHudRect("Boss Health Text", level.BossHealthTextLayout);
                DrawHudRect("Combo Text", level.ComboTextLayout);
                DrawHudRect("Combo Frame", level.ComboFrameLayout);
                DrawHudRect("Skill Tooltip", level.SkillTooltipLayout);
                ImGui::DragFloat2("Skill Tooltip Padding", glm::value_ptr(level.SkillTooltipPadding), 0.001f, 0.0f, 1.0f, "%.3f");
                DrawHudRect("Joystick Base", level.JoystickBaseLayout);
                ImGui::DragFloat2("Joystick Thumb Size", glm::value_ptr(level.JoystickThumbSize), 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat2("Joystick Thumb Travel", glm::value_ptr(level.JoystickThumbTravel), 0.001f, 0.0f, 1.0f, "%.3f");
                DrawStatusIconLayout("Player Status", level.PlayerStatusLayout);
                DrawStatusIconLayout("Enemy Status", level.EnemyStatusLayout);
            }

            if (ImGui::CollapsingHeader("Skill HUD Slots"))
            {
                for (int i = 0; i < (int)level.SkillHudSlots.size(); ++i)
                {
                    ImGui::PushID(i);
                    const std::string header = level.SkillHudSlots[i].Key.empty()
                        ? "Skill Slot " + std::to_string(i + 1)
                        : "Skill Slot " + level.SkillHudSlots[i].Key;
                    bool removeSlot = false;
                    if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        DrawSkillHudSlotRow(level.SkillHudSlots[i], i);
                        if (ImGui::Button("Remove"))
                            removeSlot = true;
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (removeSlot)
                    {
                        level.SkillHudSlots.erase(level.SkillHudSlots.begin() + i);
                        break;
                    }
                }
                if (ImGui::Button("Add Skill Slot"))
                    level.SkillHudSlots.emplace_back();
            }

            if (ImGui::CollapsingHeader("Combat Item HUD Slots"))
            {
                for (int i = 0; i < (int)level.CombatItemHudSlots.size(); ++i)
                {
                    ImGui::PushID(i);
                    const std::string header = level.CombatItemHudSlots[i].Key.empty()
                        ? "Item Slot " + std::to_string(i + 1)
                        : "Item Slot " + level.CombatItemHudSlots[i].Key;
                    bool removeSlot = false;
                    if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        DrawCombatItemHudSlotRow(level.CombatItemHudSlots[i], i);
                        if (ImGui::Button("Remove"))
                            removeSlot = true;
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (removeSlot)
                    {
                        level.CombatItemHudSlots.erase(level.CombatItemHudSlots.begin() + i);
                        break;
                    }
                }
                if (ImGui::Button("Add Item Slot"))
                    level.CombatItemHudSlots.emplace_back();
            }

            if (ImGui::CollapsingHeader("HUD Text"))
            {
                InputString("Locked", level.HudLockedText, 128);
                InputString("Unavailable", level.HudUnavailableText, 128);
                InputString("Insufficient Mana", level.HudInsufficientManaText, 128);
                InputString("Condition", level.HudConditionText, 128);
                InputString("Gauge", level.HudGaugeText, 128);
                InputString("Combo", level.HudComboText, 128);
                InputString("Armor", level.HudArmorText, 128);
                InputString("Cooldown Prefix", level.HudCooldownPrefix, 128);
                InputString("Seconds Suffix", level.HudSecondsSuffix, 128);
                InputString("Mana Not Enough Tooltip", level.HudManaNotEnoughTooltip, 260);
                InputString("Not Unlocked Tooltip", level.HudNotUnlockedTooltip, 260);
                InputString("Break Gauge Tooltip", level.BreakLimitGaugeNotEnoughTooltip, 260);
                InputString("Break Combo Tooltip", level.BreakLimitComboNotEnoughTooltip, 260);
                InputString("Break Boss Tooltip", level.BreakLimitBossNotReadyTooltip, 260);
                InputString("Default Message", level.HudDefaultMessage, 512);
                InputString("Air Basic Message", level.HudAirBasicMessage, 260);
                InputString("Magic Message", level.HudMagicMessage, 260);
                InputString("Dash Message", level.HudDashMessage, 260);
                InputString("Reserved Skill Message", level.HudReservedSkillMessage, 260);
                InputString("Support Message", level.HudSupportMessage, 260);
                InputString("Break Limit Input", level.HudBreakLimitInputMessage, 260);
                InputString("Break Limit Debug", level.HudBreakLimitDebugInputMessage, 260);
                InputString("Victory Message", level.HudVictoryMessage, 260);
                InputString("Defeat Message", level.HudDefeatMessage, 260);
                InputString("High Air Message", level.HudHighAirMessage, 260);
                InputString("Low Air Message", level.HudLowAirMessage, 260);
                InputString("Break Limit Hint", level.HudBreakLimitHintMessage, 260);
                InputString("Player Health Label", level.HudPlayerHealthLabel, 128);
                InputString("Boss Health Label", level.HudBossHealthLabel, 128);
                InputString("Boss Protection Label", level.HudBossProtectionLabel, 128);
                InputString("Mana Gauge Label", level.HudManaGaugeLabel, 128);
                InputString("Air Actions Label", level.HudAirActionsLabel, 128);
                InputString("Reward Fallback", level.HudRewardFallbackText, 260);
                InputString("Collected Prefix", level.HudCollectedPrefix, 128);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Result: %s", level.RuntimeVictory ? "Victory" : (level.RuntimeDefeat ? "Defeat" : "Playing"));
            ImGui::Text("Wave: %d / %d, Wall X: %.2f",
                level.RuntimeWaveIndex + 1,
                std::max(1, level.WaveCount),
                level.RuntimeWaveRightWall);
            ImGui::Text("Combo: %d / Best: %d", level.RuntimeComboCount, level.RuntimeBestCombo);
            ImGui::Text("Collected Pickups: %d", level.RuntimeCollectedPickups);
        });
    }

    void DrawSideCombatantComponent(Entity entity)
    {
        DrawComponent<SideCombatantComponent>("Side Combatant", entity, [](auto& combatant)
        {
            DrawTeamCombo(combatant.Team);
            ImGui::DragFloat("Max Health", &combatant.MaxHealth, 1.0f, 1.0f, 99999.0f);
            ImGui::DragFloat("Health", &combatant.Health, 1.0f, 0.0f, combatant.MaxHealth);
            ImGui::DragFloat(EditorLocale::Text("Attack", "攻击"), &combatant.Attack, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat(EditorLocale::Text("Defense", "防御"), &combatant.Defense, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Move Speed", &combatant.MoveSpeed, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat2("Collision Size", glm::value_ptr(combatant.CollisionSize), 0.02f, 0.05f, 20.0f);
            ImGui::DragFloat("Collision Height", &combatant.CollisionHeight, 0.02f, 0.05f, 20.0f);
            ImGui::DragFloat("Gravity Scale", &combatant.GravityScale, 0.02f, 0.0f, 5.0f);
            ImGui::DragFloat("Knockback Resistance", &combatant.KnockbackResistance, 0.01f, 0.0f, 0.95f);
            ImGui::Checkbox(EditorLocale::Text("Invulnerable", "无敌"), &combatant.Invulnerable);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Alive: %s", combatant.Alive ? "true" : "false");
            ImGui::Text("State: %s / %.2f", GetStateLabel(combatant.RuntimeState), combatant.RuntimeStateTimer);
            ImGui::Text("On Ground: %s", combatant.RuntimeOnGround ? "true" : "false");
            ImGui::Text("Velocity: %.2f, %.2f", combatant.RuntimeVelocity.x, combatant.RuntimeVelocity.y);
            ImGui::Text("Ground: %.2f, %.2f / Air %.2f", combatant.RuntimeGroundPosition.x, combatant.RuntimeGroundPosition.y, combatant.RuntimeAirHeight);
            ImGui::Text("Hit Stun: %.2f", combatant.RuntimeHitStun);
            ImGui::Text("Protection: %.1f / %.1f", combatant.RuntimeProtection, combatant.RuntimeProtectionMax);
        });
    }

    void DrawSidePlayerControllerComponent(Entity entity)
    {
        DrawComponent<SidePlayerControllerComponent>("Side Player Controller", entity, [](auto& controller)
        {
            ImGui::DragInt("Max Jumps", &controller.MaxJumps, 1.0f, 1, 3);
            ImGui::DragFloat("Jump Impulse", &controller.JumpImpulse, 0.05f, 0.0f, 40.0f);
            ImGui::DragFloat("Gravity", &controller.Gravity, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Air Control", &controller.AirControl, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Jump Buffer Time", &controller.JumpBufferTime, 0.005f, 0.0f, 0.5f);
            ImGui::DragFloat("Coyote Time", &controller.CoyoteTime, 0.005f, 0.0f, 0.5f);
            ImGui::DragFloat("Lane Speed Scale", &controller.LaneSpeedScale, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("Lane Acceleration", &controller.LaneAcceleration, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Ground Friction", &controller.GroundFriction, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat("Basic Cooldown", &controller.BasicCooldown, 0.01f, 0.01f, 5.0f);
            ImGui::DragFloat("Launcher Cooldown", &controller.LauncherCooldown, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Magic Bolt Cooldown", &controller.MagicBoltCooldown, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Ally Support Cooldown", &controller.AllySupportCooldown, 0.01f, 0.01f, 30.0f);
            ImGui::DragFloat("Dash Cooldown", &controller.DashCooldown, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Dash Mana Cost", &controller.DashManaCost, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("Dash Speed", &controller.DashSpeed, 0.05f, 0.0f, 40.0f);
            ImGui::DragFloat("Dash Invulnerable Time", &controller.DashInvulnerableTime, 0.005f, 0.0f, 2.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Items / Mana / Buffs");
            ImGui::DragFloat("Heal Item Cooldown", &controller.HealItemCooldown, 0.1f, 0.0f, 30.0f);
            ImGui::DragFloat("Mana Item Cooldown", &controller.ManaItemCooldown, 0.1f, 0.0f, 30.0f);
            ImGui::DragFloat("Attack Buff Item Cooldown", &controller.AttackBuffItemCooldown, 0.1f, 0.0f, 60.0f);
            ImGui::DragFloat("Max Mana", &controller.MaxMana, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat("Launcher Mana Cost", &controller.LauncherManaCost, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("Magic Bolt Mana Cost", &controller.MagicBoltManaCost, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("Ally Support Mana Cost", &controller.AllySupportManaCost, 0.5f, 0.0f, 100.0f);
            ImGui::DragFloat("Heal Item Amount", &controller.HealItemAmount, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat("Mana Item Amount", &controller.ManaItemAmount, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat("Attack Buff Multiplier", &controller.AttackBuffMultiplier, 0.01f, 1.0f, 3.0f);
            ImGui::DragFloat("Attack Buff Duration", &controller.AttackBuffDuration, 0.1f, 0.0f, 30.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime Cooldowns");
            ImGui::Text("Basic %.2f / Launcher %.2f / Magic %.2f / Support %.2f / Dash %.2f",
                controller.RuntimeBasicCooldown,
                controller.RuntimeLauncherCooldown,
                controller.RuntimeMagicBoltCooldown,
                controller.RuntimeAllySupportCooldown,
                controller.RuntimeDashCooldown);
            ImGui::Text("Jumps: %d", controller.RuntimeJumpsRemaining);
            ImGui::Text("Jump Buffer %.2f / Coyote %.2f",
                controller.RuntimeJumpBufferTimer,
                controller.RuntimeCoyoteTimer);
            ImGui::Text("Air Actions: %d / Break Limit %.2f",
                controller.RuntimeAirActionsRemaining,
                controller.RuntimeBreakLimitCooldown);
            ImGui::Text("Magic Sword Gauge: %.2f / %.2f",
                controller.RuntimeMagicSwordGauge,
                controller.RuntimeMagicSwordGaugeMax);
        });
    }

    void DrawSideEnemyAIComponent(Entity entity)
    {
        DrawComponent<SideEnemyAIComponent>("Side Enemy AI", entity, [](auto& ai)
        {
            DrawEnemyKindCombo(ai.Kind);
            ImGui::DragInt("Wave Index", &ai.WaveIndex, 1.0f, -1, 3);
            ImGui::DragFloat("Aggro Range", &ai.AggroRange, 0.05f, 0.0f, 80.0f);
            ImGui::DragFloat(EditorLocale::Text("Attack Range", "攻击范围"), &ai.AttackRange, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Preferred Range", &ai.PreferredRange, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Attack Interval", &ai.AttackInterval, 0.02f, 0.05f, 20.0f);
            ImGui::DragFloat("Patrol Min X", &ai.PatrolMinX, 0.05f);
            ImGui::DragFloat("Patrol Max X", &ai.PatrolMaxX, 0.05f);
            ImGui::DragFloat("Lane Tolerance", &ai.LaneTolerance, 0.02f, 0.0f, 5.0f);

            ImGui::Separator();
            ImGui::TextDisabled("Runtime");
            ImGui::Text("Attack Timer: %.2f", ai.RuntimeAttackTimer);
            ImGui::Text("Awake: %s", ai.RuntimeAwake ? "true" : "false");
        });
    }

    void DrawSideHitboxComponent(Entity entity)
    {
        DrawComponent<SideHitboxComponent>("Side Hitbox", entity, [](auto& hitbox)
        {
            DrawTeamCombo(hitbox.Team);
            DrawAttackKindCombo(hitbox.AttackKind);
            ImGui::DragFloat2("Size", glm::value_ptr(hitbox.Size), 0.02f, 0.01f, 20.0f);
            ImGui::DragFloat2("Velocity", glm::value_ptr(hitbox.Velocity), 0.05f);
            ImGui::DragFloat2("Launch Velocity", glm::value_ptr(hitbox.LaunchVelocity), 0.05f);
            ImGui::DragFloat("Air Height", &hitbox.AirHeight, 0.02f, 0.0f, 20.0f);
            ImGui::DragFloat("Air Range", &hitbox.AirRange, 0.02f, 0.01f, 20.0f);
            ImGui::DragFloat(EditorLocale::Text("Damage", "伤害"), &hitbox.Damage, 0.5f, 0.0f, 9999.0f);
            ImGui::DragFloat("Lifetime", &hitbox.Lifetime, 0.01f, 0.0f, 20.0f);
            ImGui::DragFloat("Hit Stun", &hitbox.HitStun, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Attacker Air Impulse", &hitbox.AttackerAirImpulse, 0.02f, -20.0f, 20.0f);
            ImGui::DragFloat("Attacker Air Fall Step", &hitbox.AttackerAirFallStep, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Target Air Fall Step", &hitbox.TargetAirFallStep, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Protection Gain", &hitbox.ProtectionGain, 0.1f, 0.0f, 200.0f);
            ImGui::Checkbox("Destroy On Hit", &hitbox.DestroyOnHit);
            InputString("Frame Pattern", hitbox.TextureFramePattern, 260);
            ImGui::DragInt("Frame Count", &hitbox.TextureFrameCount, 1.0f, 1, 64);
            ImGui::DragFloat("Frame Rate", &hitbox.TextureFrameRate, 0.5f, 1.0f, 60.0f);
        });
    }

    void DrawSidePickupComponent(Entity entity)
    {
        DrawComponent<SidePickupComponent>("Side Pickup", entity, [](auto& pickup)
        {
            EditorContentPickers::DrawProgressionIdField("Item Id",
                pickup.ItemId,
                EditorContentPickers::ProgressionIdKind::Material);
            InputString("Display Name", pickup.DisplayName);
            ImGui::DragInt(EditorLocale::Text("Amount", "数量"), &pickup.Amount, 1.0f, 1, 999);
            ImGui::DragFloat("Pickup Radius", &pickup.PickupRadius, 0.01f, 0.01f, 5.0f);
            ImGui::DragFloat("Attract Radius", &pickup.AttractRadius, 0.05f, 0.0f, 20.0f);
            ImGui::DragFloat("Attract Speed", &pickup.AttractSpeed, 0.05f, 0.0f, 40.0f);
        });
    }

} // namespace Wheatear
