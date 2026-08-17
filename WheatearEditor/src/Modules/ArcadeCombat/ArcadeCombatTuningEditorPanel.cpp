#include "wepch.h"
#include "ArcadeCombatTuningEditorPanel.h"

#include "Modules/CombatUnitOverview.h"
#include "Editor/EditorCommands.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/GameplayEditorShell.h"
#include "Editor/YamlTreeEditor.h"
#include "Panels/DataFileEditorPanel.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatComponents.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <vector>

namespace Wheatear {

    namespace {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;

        using namespace EditorWidgets;

        static void DrawRawPreview(const std::string& text, const char* sourcePath)
        {
            std::string preview = text;
            EditorWidgets::InputMultilineString("##ArcadeCombatRawPreview",
                preview,
                ImVec2(-1.0f, -1.0f),
                std::max<size_t>(text.size() + 1, 4096),
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);

            ImGui::Spacing();
            if (ImGui::Button(EditorLocale::Text("Edit in Data File Editor", "在数据文件编辑器中编辑")))
                DataFileEditorRequests::RequestOpen(sourcePath);
        }

    } // namespace

    namespace ArcadeCombatEditorRequests {

        void RequestOpenTuning(const std::string& sourcePath)
        {
            s_PendingOpenPath = sourcePath;
            s_HasPendingOpen = true;
        }

        bool ConsumeOpenTuningRequest(std::string& sourcePath)
        {
            if (!s_HasPendingOpen)
                return false;

            sourcePath = s_PendingOpenPath;
            s_PendingOpenPath.clear();
            s_HasPendingOpen = false;
            return true;
        }

    } // namespace ArcadeCombatEditorRequests

    ArcadeCombatTuningEditorPanel::ArcadeCombatTuningEditorPanel()
        : m_Root(std::make_unique<YAML::Node>(YAML::NodeType::Map))
    {
    }

    ArcadeCombatTuningEditorPanel::~ArcadeCombatTuningEditorPanel() = default;

    void ArcadeCombatTuningEditorPanel::Open(const std::string& sourcePath, Scene* scene)
    {
        m_Scene = scene;
        m_Open = true;
        if (!sourcePath.empty() && sourcePath != m_SourcePath)
        {
            m_SourcePath = sourcePath;
            m_Loaded = false;
        }
    }

    void ArcadeCombatTuningEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        if (ArcadeCombatEditorRequests::ConsumeOpenTuningRequest(requestedPath))
            Open(requestedPath);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        EditorFloatingWindow::Begin("Arcade Combat Tuning Editor", &m_Open, 0, { 900.0f, 640.0f });
        EditorWidgets::PanelHeader("Arcade Combat Tuning",
            "Structured YAML authoring for arcade (bullet-hell) combat flow timings, boss behaviour and player feel.");
        EditorFloatingWindow::DrawToggleButton("Arcade Combat Tuning Editor");
        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_Dirty,
            m_ParseValid,
            m_SourcePath,
            m_Status
        });
        DrawToolbar();

        if (!m_ParseValid)
        {
            ImGui::Separator();
            EditorWidgets::InlineStatus(
                "YAML parse failed. Use the raw YAML editor to fix the file first.",
                EditorWidgets::StatusKind::Error);
            DrawRawPreview(m_RawPreview, m_SourcePath.c_str());
            EditorFloatingWindow::End();
            return;
        }

        if (ImGui::BeginTabBar("##ArcadeCombatTuningTabs"))
        {
            if (ImGui::BeginTabItem(EditorLocale::Text("Level", "关卡流程")))
            {
                DrawLevelTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Boss", "Boss 行为")))
            {
                DrawBossTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Player", "玩家手感")))
            {
                DrawPlayerTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Scene Units", "场景单位")))
            {
                DrawSceneUnitsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Advanced", "高级")))
            {
                if (YamlTreeEditor::DrawYamlNode(
                        *m_Root, "tuning", 0, m_NewScalarValues, m_NewMapKeys))
                {
                    m_Dirty = true;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Raw YAML", "原始 YAML")))
            {
                DrawRawPreviewTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        EditorFloatingWindow::End();
    }

    void ArcadeCombatTuningEditorPanel::DrawToolbar()
    {
        ImGui::Separator();
        bool reloadClicked = false;
        EditorWidgets::DirtySaveBar(m_Dirty, m_Status,
            EditorLocale::Text("Save", "保存"),
            EditorLocale::Text("Reload", "重载"),
            &reloadClicked);
        if (reloadClicked)
        {
            m_Loaded = false;
            Load();
        }
        ImGui::SameLine();
        EditorWidgets::InputString("##ArcadeCombatSourcePath", m_SourcePath, 512);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            m_Loaded = false;
            Load();
        }
        ImGui::Separator();
    }

    void ArcadeCombatTuningEditorPanel::Load()
    {
        m_Loaded = true;
        m_Dirty = false;
        m_Status.clear();
        m_ParseValid = false;

        m_ResolvedPath = EditorWidgets::ResolveWritableProjectAsset(m_SourcePath);
        std::string text;
        if (!EditorWidgets::ReadFileText(m_ResolvedPath, text))
        {
            m_Status = "File not found: " + m_ResolvedPath.string();
            return;
        }

        try
        {
            *m_Root = YAML::Load(text);
        }
        catch (const std::exception& e)
        {
            m_Status = std::string("YAML parse error: ") + e.what();
            m_RawPreview = text;
            return;
        }

        if (!m_Root->IsMap())
        {
            m_Status = "Root node is not a map.";
            m_RawPreview = text;
            return;
        }

        m_ParseValid = true;
        m_RawPreview = text;
        m_Status = "Loaded";
    }

    void ArcadeCombatTuningEditorPanel::Save()
    {
        if (!m_ParseValid)
        {
            m_Status = "Cannot save an invalid document.";
            return;
        }

        RefreshRawPreview();
        if (!EditorWidgets::WriteFileText(m_ResolvedPath, m_RawPreview))
        {
            m_Status = "Write failed.";
            return;
        }

        m_Dirty = false;
        m_Status = "Saved";
        WT_CORE_INFO("ArcadeCombat tuning saved to '{}'", m_ResolvedPath.string());
    }

    void ArcadeCombatTuningEditorPanel::RefreshRawPreview()
    {
        if (!m_Root || !m_ParseValid)
            return;

        YAML::Emitter out;
        out.SetIndent(2);
        out << *m_Root;
        m_RawPreview = out.c_str();
    }

    void ArcadeCombatTuningEditorPanel::DrawLevelTab()
    {
        YAML::Node level = EnsureMap(*m_Root, "level");
        m_Dirty |= DrawFloat(level, "startFadeDuration", "Start Fade (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "victoryReturnDelay", "Victory Return Delay (s)", 0.01f, 0.0f, 30.0f);
        m_Dirty |= DrawFloat(level, "defeatReturnDelay", "Defeat Return Delay (s)", 0.01f, 0.0f, 30.0f);
        m_Dirty |= DrawFloat(level, "resultSceneFadeDuration", "Result Scene Fade (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "bossDefeatFadeDuration", "Boss Defeat Fade (s)", 0.01f, 0.0f, 10.0f);
    }

    void ArcadeCombatTuningEditorPanel::DrawBossTab()
    {
        YAML::Node boss = EnsureMap(*m_Root, "boss");
        m_Dirty |= DrawFloat(boss, "introDuration", "Intro (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "shootInterval", "Shoot Interval (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpInterval", "Jump Interval (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpDuration", "Jump Duration (s)", 0.01f, 0.0f, 10.0f);

        ImGui::Separator();
        ImGui::TextDisabled("%s", EditorLocale::Text("Jump path (Lissajous inside the arena)", "跳跃轨迹（场地内利萨茹曲线）"));
        m_Dirty |= DrawFloat(boss, "jumpXFrequency", "Jump X Frequency", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpYFrequency", "Jump Y Frequency", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpXAmplitude", "Jump X Amplitude", 0.05f, 0.0f, 20.0f);
        m_Dirty |= DrawFloat(boss, "jumpYAmplitude", "Jump Y Amplitude", 0.05f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpYBase", "Jump Y Base", 0.05f, -5.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpArcHeight", "Jump Arc Height", 0.01f, 0.0f, 5.0f);
        m_Dirty |= DrawFloat(boss, "jumpMarginX", "Jump Margin X", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpMarginTop", "Jump Margin Top", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(boss, "jumpMarginBottom", "Jump Margin Bottom", 0.01f, 0.0f, 10.0f);

        ImGui::Separator();
        ImGui::TextDisabled("%s", EditorLocale::Text("Bullet payload", "子弹弹道"));
        m_Dirty |= DrawString(boss, "bulletEntityName", "Bullet Entity Name", 160);
        m_Dirty |= DrawFloat(boss, "bulletSpeed", "Bullet Speed", 0.05f, 0.0f, 30.0f);
        m_Dirty |= DrawFloat(boss, "bulletLifetime", "Bullet Lifetime (s)", 0.01f, 0.05f, 10.0f);
        m_Dirty |= DrawFloat(boss, "bulletRadius", "Bullet Radius", 0.01f, 0.01f, 5.0f);
        m_Dirty |= DrawVec2(boss, "bulletSpawnOffset", "Bullet Spawn Offset", 0.01f);
    }

    void ArcadeCombatTuningEditorPanel::DrawPlayerTab()
    {
        YAML::Node player = EnsureMap(*m_Root, "player");
        m_Dirty |= DrawFloat(player, "moveSpeed", "Move Speed", 0.05f, 0.0f, 30.0f);
        m_Dirty |= DrawBool(player, "autoAim", "Auto Aim");

        ImGui::Separator();
        ImGui::TextDisabled("%s", EditorLocale::Text(
            "Weapon payloads (cooldown/damage come from WAO recipes)",
            "武器弹道（冷却/伤害来自 WAO recipe）"));
        YAML::Node weapons = EnsureMap(player, "weapons");
        if (m_SelectedWeapon.empty())
            m_SelectedWeapon = "gun";
        if (ImGui::BeginCombo(EditorLocale::Text("Weapon", "武器"), m_SelectedWeapon.c_str()))
        {
            for (const char* key : { "gun", "cannon", "katana" })
            {
                if (ImGui::Selectable(key, m_SelectedWeapon == key))
                    m_SelectedWeapon = key;
            }
            ImGui::EndCombo();
        }

        YAML::Node weapon = EnsureMap(weapons, m_SelectedWeapon.c_str());
        m_Dirty |= DrawString(weapon, "entityName", "Entity Name", 160);
        m_Dirty |= DrawFloat(weapon, "speed", "Speed", 0.05f, 0.0f, 40.0f);
        m_Dirty |= DrawFloat(weapon, "lifetime", "Lifetime (s)", 0.01f, 0.01f, 10.0f);
        m_Dirty |= DrawFloat(weapon, "radius", "Radius", 0.01f, 0.01f, 5.0f);
        m_Dirty |= DrawVec2(weapon, "muzzleOffset", "Muzzle Offset", 0.01f);
        m_Dirty |= DrawFloat(weapon, "slashOffset", "Slash Offset", 0.01f, 0.0f, 5.0f);
        m_Dirty |= DrawBool(weapon, "heavy", "Heavy");
        m_Dirty |= DrawBool(weapon, "melee", "Melee");
    }

    void ArcadeCombatTuningEditorPanel::DrawSceneUnitsTab()
    {
        if (m_Scene)
        {
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "Edits the open scene (Ctrl+Z undo, save the scene to persist).",
                "编辑当前打开的场景（Ctrl+Z 撤销，保存场景后生效）。"));

            auto& registry = m_Scene->GetRegistry();
            int index = 0;
            for (auto entityID : registry.view<ArcadeCombatantComponent>())
            {
                Entity entity{ entityID, m_Scene };
                auto& unit = entity.GetComponent<ArcadeCombatantComponent>();
                ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entityID)));

                const std::string headerText =
                    (entity.HasComponent<TagComponent>()
                        ? entity.GetComponent<TagComponent>().Tag
                        : "?") + "  Team " + std::to_string(unit.Team);
                const bool open = ImGui::CollapsingHeader(headerText.c_str());

                if (open)
                {
                    const uint32_t unitKey =
                        static_cast<uint32_t>(static_cast<entt::entity>(entityID));

                    auto beginEdit = [&]()
                    {
                        if (ImGui::IsItemActivated())
                            m_UnitEditSnapshots[unitKey] = unit;
                    };
                    auto endEdit = [&]()
                    {
                        if (!ImGui::IsItemDeactivatedAfterEdit())
                            return;
                        auto snapshotIt = m_UnitEditSnapshots.find(unitKey);
                        if (snapshotIt != m_UnitEditSnapshots.end())
                        {
                            auto command = MakeComponentValueCommand(entity, snapshotIt->second, unit);
                            command->Execute();
                            CommandHistory::Get().Push(std::move(command));
                            m_UnitEditSnapshots.erase(snapshotIt);
                        }
                    };

                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Max Health", "最大生命"), &unit.MaxHealth, 1.0f, 1.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Health", "生命"), &unit.Health, 1.0f, 0.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Move Speed", "移动速度"), &unit.MoveSpeed, 0.1f, 0.0f, 30.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Collision Radius", "碰撞半径"), &unit.CollisionRadius, 0.01f, 0.05f, 5.0f);
                    endEdit();
                    beginEdit();
                    ImGui::Checkbox(EditorLocale::Text("Invulnerable", "无敌"), &unit.Invulnerable);
                    endEdit();
                }
                ++index;
                ImGui::PopID();
            }
            if (index == 0)
                ImGui::TextDisabled("%s", EditorLocale::Text(
                    "No arcade combatants in the open scene.", "当前场景没有街机战斗单位。"));
            ImGui::Separator();
        }
        else
        {
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "No scene open; the project-wide overview below is read-only.",
                "未打开场景；下方项目总览为只读。"));
        }

        ImGui::Spacing();
        CombatUnitOverview::DrawProjectUnitsTable("ArcadeCombatantComponent",
            {
                { "MaxHealth", EditorLocale::Text("HP", "生命") },
                { "MoveSpeed", EditorLocale::Text("Speed", "速度") },
                { "CollisionRadius", EditorLocale::Text("Radius", "半径") },
            },
            true);
    }

    void ArcadeCombatTuningEditorPanel::DrawRawPreviewTab()
    {
        RefreshRawPreview();
        DrawRawPreview(m_RawPreview, m_SourcePath.c_str());
    }

} // namespace Wheatear
