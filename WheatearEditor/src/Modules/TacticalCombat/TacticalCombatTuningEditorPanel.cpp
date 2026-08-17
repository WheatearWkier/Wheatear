#include "wepch.h"
#include "TacticalCombatTuningEditorPanel.h"

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
#include "Wheatear/Modules/TacticalCombat/TacticalCombatComponents.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatTuningService.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
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
            EditorWidgets::InputMultilineString("##TacticalCombatRawPreview",
                preview,
                ImVec2(-1.0f, -1.0f),
                std::max<size_t>(text.size() + 1, 4096),
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);

            ImGui::Spacing();
            if (ImGui::Button(EditorLocale::Text("Edit in Data File Editor", "在数据文件编辑器中编辑")))
                DataFileEditorRequests::RequestOpen(sourcePath);
        }

        static bool DrawVec4Node(YAML::Node map,
            const char* key,
            const char* label,
            bool& dirty)
        {
            YAML::Node node = map[key];
            if (!node || !node.IsSequence() || node.size() < 4)
                return false;

            glm::vec4 value = {
                node[0].as<float>(0.0f),
                node[1].as<float>(0.0f),
                node[2].as<float>(0.0f),
                node[3].as<float>(1.0f)
            };
            const bool changed = ImGui::ColorEdit4(label, glm::value_ptr(value));
            if (changed)
            {
                node[0] = value.r;
                node[1] = value.g;
                node[2] = value.b;
                node[3] = value.a;
                dirty = true;
            }
            return changed;
        }

    } // namespace

    namespace TacticalCombatEditorRequests {

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

    } // namespace TacticalCombatEditorRequests

    TacticalCombatTuningEditorPanel::TacticalCombatTuningEditorPanel()
        : m_Root(std::make_unique<YAML::Node>(YAML::NodeType::Map))
    {
    }

    TacticalCombatTuningEditorPanel::~TacticalCombatTuningEditorPanel() = default;

    void TacticalCombatTuningEditorPanel::Open(const std::string& sourcePath, Scene* scene)
    {
        m_Scene = scene;
        m_Open = true;
        if (!sourcePath.empty() && sourcePath != m_SourcePath)
        {
            m_SourcePath = sourcePath;
            m_Loaded = false;
        }
    }

    void TacticalCombatTuningEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        if (TacticalCombatEditorRequests::ConsumeOpenTuningRequest(requestedPath))
            Open(requestedPath);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        EditorFloatingWindow::Begin("Tactical Combat Tuning Editor", &m_Open, 0, { 960.0f, 680.0f });
        EditorWidgets::PanelHeader("Tactical Combat Tuning",
            "Structured YAML authoring for tactical (grid) combat flow timings, board layout, tile colors and formula.");
        EditorFloatingWindow::DrawToggleButton("Tactical Combat Tuning Editor");
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

        if (ImGui::BeginTabBar("##TacticalCombatTuningTabs"))
        {
            if (ImGui::BeginTabItem(EditorLocale::Text("Level", "关卡流程")))
            {
                DrawLevelTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Board", "棋盘")))
            {
                DrawBoardTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(EditorLocale::Text("Formula", "伤害公式")))
            {
                DrawFormulaTab();
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

    void TacticalCombatTuningEditorPanel::DrawToolbar()
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
        EditorWidgets::InputString("##TacticalCombatSourcePath", m_SourcePath, 512);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            m_Loaded = false;
            Load();
        }
        ImGui::Separator();
    }

    void TacticalCombatTuningEditorPanel::Load()
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

    void TacticalCombatTuningEditorPanel::Save()
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
        WT_CORE_INFO("TacticalCombat tuning saved to '{}'", m_ResolvedPath.string());
    }

    void TacticalCombatTuningEditorPanel::RefreshRawPreview()
    {
        if (!m_Root || !m_ParseValid)
            return;

        YAML::Emitter out;
        out.SetIndent(2);
        out << *m_Root;
        m_RawPreview = out.c_str();
    }

    void TacticalCombatTuningEditorPanel::DrawLevelTab()
    {
        YAML::Node level = EnsureMap(*m_Root, "level");
        m_Dirty |= DrawFloat(level, "startFadeDuration", "Start Fade (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "introDuration", "Intro (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "actionDuration", "Action (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "enemyStepDuration", "Enemy Step (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "victoryReturnDelay", "Victory Return Delay (s)", 0.01f, 0.0f, 30.0f);
        m_Dirty |= DrawFloat(level, "defeatReturnDelay", "Defeat Return Delay (s)", 0.01f, 0.0f, 30.0f);
    }

    void TacticalCombatTuningEditorPanel::DrawBoardTab()
    {
        YAML::Node level = EnsureMap(*m_Root, "level");
        m_Dirty |= DrawInt(level, "gridWidth", "Grid Width", 2, 32);
        m_Dirty |= DrawInt(level, "gridHeight", "Grid Height", 2, 32);
        m_Dirty |= DrawVec2(level, "boardOrigin", "Board Origin");
        m_Dirty |= DrawVec2(level, "cellSize", "Cell Size");
        ImGui::Separator();
        DrawVec4Node(level, "tileNormalColor", "Tile Normal Color", m_Dirty);
        DrawVec4Node(level, "tileMoveColor", "Tile Move Color", m_Dirty);
        DrawVec4Node(level, "tileAttackColor", "Tile Attack Color", m_Dirty);
        DrawVec4Node(level, "tileSelectedColor", "Tile Selected Color", m_Dirty);
    }

    void TacticalCombatTuningEditorPanel::DrawFormulaTab()
    {
        YAML::Node formula = EnsureMap(*m_Root, "formula");
        m_Dirty |= DrawFloat(formula, "magicDefenseMultiplier", "Magic Defense Multiplier", 0.01f, 0.0f, 2.0f);
        m_Dirty |= DrawFloat(formula, "minDamage", "Min Damage", 0.1f, 0.0f, 100.0f);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("damage = max(min, offense * power - defense * (magic ? multiplier : 1))");
    }

    void TacticalCombatTuningEditorPanel::DrawSceneUnitsTab()
    {
        if (m_Scene)
        {
            // Apply the tuning unit table to the open scene for edit-time
            // preview (same operation the runtime performs at start).
            auto levelView = m_Scene->GetRegistry().view<TacticalCombatLevelComponent>();
            if (!levelView.empty())
            {
                const auto& level = m_Scene->GetRegistry()
                    .get<TacticalCombatLevelComponent>(levelView.front());
                const auto& tuning = TacticalCombatTuningService::GetTuning(level);

                ImGui::TextDisabled(
                    EditorLocale::Text(
                        "Tuning unit table: %zu entries. Matches entities by Tag %s<tag>.",
                        "调参单位表：%zu 条。按实体 Tag %s<tag> 匹配。"),
                    tuning.Units.size(),
                    SystemBindings::Tactical::UnitPrefix);
                if (ImGui::Button(EditorLocale::Text(
                    "Apply Tuning Units To Scene", "把调参单位表应用到场景")))
                {
                    const size_t applied =
                        TacticalCombatTuningService::ApplyUnitTuningToScene(m_Scene, tuning);
                    m_Status = std::to_string(applied) + " unit(s) applied from tuning table.";
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", EditorLocale::Text(
                        "Overwrites matching TacticalUnitComponent values from the tuning table "
                        "(same as runtime start). Save the scene to persist.",
                        "用调参表的数值覆盖匹配的战棋单位组件（与运行时启动一致）。保存场景后生效。"));
                ImGui::Separator();
            }

            ImGui::TextDisabled("%s", EditorLocale::Text(
                "Edits the open scene (Ctrl+Z undo, save the scene to persist).",
                "编辑当前打开的场景（Ctrl+Z 撤销，保存场景后生效）。"));

            auto& registry = m_Scene->GetRegistry();
            int index = 0;
            for (auto entityID : registry.view<TacticalUnitComponent>())
            {
                Entity entity{ entityID, m_Scene };
                auto& unit = entity.GetComponent<TacticalUnitComponent>();
                ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entityID)));

                const std::string headerText = unit.DisplayName + "  ["
                    + (entity.HasComponent<TagComponent>()
                        ? entity.GetComponent<TagComponent>().Tag
                        : "?") + "]";
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

                    ImGui::TextDisabled("Team %d / Grid (%d, %d)", unit.Team, unit.GridX, unit.GridY);
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Max Health", "最大生命"), &unit.MaxHealth, 1.0f, 1.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Health", "生命"), &unit.Health, 1.0f, 0.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Attack", "攻击"), &unit.Attack, 0.5f, 0.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Magic", "魔力属性"), &unit.Magic, 0.5f, 0.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Defense", "防御"), &unit.Defense, 0.5f, 0.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragInt(EditorLocale::Text("Move Range", "移动范围"), &unit.MoveRange, 0, 16);
                    endEdit();
                    beginEdit();
                    ImGui::DragInt(EditorLocale::Text("Attack Range", "攻击范围"), &unit.AttackRange, 0, 16);
                    endEdit();
                    beginEdit();
                    ImGui::Checkbox(EditorLocale::Text("Controllable", "可控"), &unit.Controllable);
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
                    "No tactical units in the open scene.", "当前场景没有战棋单位。"));
            ImGui::Separator();
        }
        else
        {
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "No scene open; the project-wide overview below is read-only.",
                "未打开场景；下方项目总览为只读。"));
        }

        ImGui::Spacing();
        CombatUnitOverview::DrawProjectUnitsTable("TacticalUnitComponent",
            {
                { "MaxHealth", EditorLocale::Text("HP", "生命") },
                { "Attack", EditorLocale::Text("ATK", "攻击") },
                { "Magic", EditorLocale::Text("MAG", "魔力") },
                { "Defense", EditorLocale::Text("DEF", "防御") },
                { "MoveRange", EditorLocale::Text("Move", "移动") },
                { "AttackRange", EditorLocale::Text("Range", "射程") },
            },
            true);
    }

    void TacticalCombatTuningEditorPanel::DrawRawPreviewTab()
    {
        RefreshRawPreview();
        DrawRawPreview(m_RawPreview, m_SourcePath.c_str());
    }

} // namespace Wheatear
