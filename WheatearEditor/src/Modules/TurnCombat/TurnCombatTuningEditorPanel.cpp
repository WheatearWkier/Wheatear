#include "wepch.h"
#include "TurnCombatTuningEditorPanel.h"

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
#include "Wheatear/Modules/TurnCombat/TurnCombatComponents.h"
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
            EditorWidgets::InputMultilineString("##TurnCombatRawPreview",
                preview,
                ImVec2(-1.0f, -1.0f),
                std::max<size_t>(text.size() + 1, 4096),
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);

            ImGui::Spacing();
            if (ImGui::Button(EditorLocale::Text("Edit in Data File Editor", "在数据文件编辑器中编辑")))
                DataFileEditorRequests::RequestOpen(sourcePath);
        }

    } // namespace

    namespace TurnCombatEditorRequests {

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

    } // namespace TurnCombatEditorRequests

    TurnCombatTuningEditorPanel::TurnCombatTuningEditorPanel()
        : m_Root(std::make_unique<YAML::Node>(YAML::NodeType::Map))
    {
    }

    TurnCombatTuningEditorPanel::~TurnCombatTuningEditorPanel() = default;

    void TurnCombatTuningEditorPanel::Open(const std::string& sourcePath, Scene* scene)
    {
        m_Scene = scene;
        m_Open = true;
        if (!sourcePath.empty() && sourcePath != m_SourcePath)
        {
            m_SourcePath = sourcePath;
            m_Loaded = false;
        }
    }

    void TurnCombatTuningEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        if (TurnCombatEditorRequests::ConsumeOpenTuningRequest(requestedPath))
            Open(requestedPath);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        EditorFloatingWindow::Begin("Turn Combat Tuning Editor", &m_Open, 0, { 900.0f, 640.0f });
        EditorWidgets::PanelHeader("Turn Combat Tuning",
            "Structured YAML authoring for turn combat flow timings and damage formula.");
        EditorFloatingWindow::DrawToggleButton("Turn Combat Tuning Editor");
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

        if (ImGui::BeginTabBar("##TurnCombatTuningTabs"))
        {
            if (ImGui::BeginTabItem(EditorLocale::Text("Level", "关卡流程")))
            {
                DrawLevelTab();
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

    void TurnCombatTuningEditorPanel::DrawToolbar()
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
        EditorWidgets::InputString("##TurnCombatSourcePath", m_SourcePath, 512);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            m_Loaded = false;
            Load();
        }
        ImGui::Separator();
    }

    void TurnCombatTuningEditorPanel::Load()
    {
        m_Loaded = true;
        m_Dirty = false;
        m_Status.clear();
        m_ParseValid = false;

        m_ResolvedPath = AssetPath::Resolve(m_SourcePath);
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

    void TurnCombatTuningEditorPanel::Save()
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
        WT_CORE_INFO("TurnCombat tuning saved to '{}'", m_ResolvedPath.string());
    }

    void TurnCombatTuningEditorPanel::RefreshRawPreview()
    {
        if (!m_Root || !m_ParseValid)
            return;

        YAML::Emitter out;
        out.SetIndent(2);
        out << *m_Root;
        m_RawPreview = out.c_str();
    }

    void TurnCombatTuningEditorPanel::DrawLevelTab()
    {
        YAML::Node level = EnsureMap(*m_Root, "level");
        m_Dirty |= DrawFloat(level, "startFadeDuration", "Start Fade (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "introDuration", "Intro (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "actionDuration", "Action (s)", 0.01f, 0.0f, 10.0f);
        m_Dirty |= DrawFloat(level, "victoryReturnDelay", "Victory Return Delay (s)", 0.01f, 0.0f, 30.0f);
        m_Dirty |= DrawFloat(level, "defeatReturnDelay", "Defeat Return Delay (s)", 0.01f, 0.0f, 30.0f);
    }

    void TurnCombatTuningEditorPanel::DrawFormulaTab()
    {
        YAML::Node formula = EnsureMap(*m_Root, "formula");
        m_Dirty |= DrawFloat(formula, "defenseMultiplier", "Defense Multiplier", 0.01f, 0.0f, 2.0f);
        m_Dirty |= DrawFloat(formula, "minDamage", "Min Damage", 0.1f, 0.0f, 100.0f);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("damage = offense * power + 6 - defense * multiplier");
    }

    void TurnCombatTuningEditorPanel::DrawSceneUnitsTab()
    {
        // Editable list of the turn-combat units in the scene currently open
        // in the editor (undoable through the shared command history; saved
        // together with the scene).
        if (m_Scene)
        {
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "Edits the open scene (Ctrl+Z undo, save the scene to persist).",
                "编辑当前打开的场景（Ctrl+Z 撤销，保存场景后生效）。"));

            auto& registry = m_Scene->GetRegistry();
            int index = 0;
            for (auto entityID : registry.view<TurnCombatantComponent>())
            {
                Entity entity{ entityID, m_Scene };
                auto& unit = entity.GetComponent<TurnCombatantComponent>();
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

                    // One undo step per drag session: snapshot when the first
                    // field of this unit becomes active, commit on release.
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

                    ImGui::TextDisabled("Team %d / Slot %d", unit.Team, unit.Slot);
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Max Health", "最大生命"), &unit.MaxHealth, 1.0f, 1.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Health", "生命"), &unit.Health, 1.0f, 0.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Max Mana", "最大魔力"), &unit.MaxMana, 1.0f, 0.0f, 9999.0f);
                    endEdit();
                    beginEdit();
                    ImGui::DragFloat(EditorLocale::Text("Mana", "魔力"), &unit.Mana, 1.0f, 0.0f, 9999.0f);
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
                    ImGui::DragFloat(EditorLocale::Text("Speed", "速度"), &unit.Speed, 0.5f, 0.0f, 9999.0f);
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
                    "No turn-combat units in the open scene.", "当前场景没有回合制单位。"));
            ImGui::Separator();
        }
        else
        {
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "No scene open; the project-wide overview below is read-only.",
                "未打开场景；下方项目总览为只读。"));
        }

        ImGui::Spacing();
        CombatUnitOverview::DrawProjectUnitsTable("TurnCombatantComponent",
            {
                { "MaxHealth", EditorLocale::Text("HP", "生命") },
                { "Attack", EditorLocale::Text("ATK", "攻击") },
                { "Magic", EditorLocale::Text("MAG", "魔力") },
                { "Defense", EditorLocale::Text("DEF", "防御") },
                { "Speed", EditorLocale::Text("SPD", "速度") },
                { "BasicSkillId", EditorLocale::Text("Basic Skill", "基础技能") },
            },
            true);
    }

    void TurnCombatTuningEditorPanel::DrawRawPreviewTab()
    {
        RefreshRawPreview();
        DrawRawPreview(m_RawPreview, m_SourcePath.c_str());
    }

} // namespace Wheatear
