#include "wepch.h"
#include "VisualNovelScriptEditorPanel.h"
#include "VisualNovelScriptEditorPanelInternal.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <string>

namespace Wheatear {

    using namespace VisualNovelScriptEditorPanelInternal;

    void VisualNovelScriptEditorPanel::DrawTimeline()
    {
        const float leftWidth = 390.0f;
        ImGui::BeginChild("##VNRows", ImVec2(leftWidth, 0.0f), true);
        EditorWidgets::SectionHeader("Timeline", "Add, select, and reorder script rows.");

        if (ImGui::Button(EditorLocale::Text("+ Dialogue", "+ 对白"))) AddRow(RowKind::Dialogue);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ BGM", "+ BGM"))) AddRow(RowKind::Music);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Choice", "+ 选项"))) AddRow(RowKind::Choice);
        if (ImGui::Button(EditorLocale::Text("+ Character", "+ 角色"))) AddRow(RowKind::Character);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Show", "+ 显示"))) AddRow(RowKind::Show);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Hide", "+ 隐藏"))) AddRow(RowKind::Hide);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Expression", "+ 表情"))) AddRow(RowKind::Expression);
        if (ImGui::Button(EditorLocale::Text("+ Goto", "+ 跳转"))) AddRow(RowKind::Goto);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Background", "+ 背景"))) AddRow(RowKind::Background);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Label", "+ 标签"))) AddRow(RowKind::Label);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ End", "+ 结束"))) AddRow(RowKind::End);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Set", "+ 变量"))) AddRow(RowKind::Set);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ If", "+ 条件"))) AddRow(RowKind::If);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Speed", "+ 速度"))) AddRow(RowKind::Speed);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Sheet", "+ 图集"))) AddRow(RowKind::Sheet);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Char", "+ 角色格"))) AddRow(RowKind::Char);
        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("+ Raw", "+ 原始"))) AddRow(RowKind::Raw);

        ImGui::Separator();
        if (m_Rows.empty())
        {
            EditorWidgets::EmptyState("No rows yet.", "Add dialogue, choice, character, background, or BGM rows to start this script.");
        }
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            const Row& row = m_Rows[i];
            std::string label = std::to_string(i + 1) + "  [" + KindName(row.Kind) + "]  " + RowSummary(row);
            if (label.size() > 96)
                label = label.substr(0, 93) + "...";
            label = EditorWidgets::LabelWithId(label, "vn_row:" + std::to_string(i));
            if (ImGui::Selectable(label.c_str(), m_SelectedRow == i))
                m_SelectedRow = i;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##VNRowEditor", ImVec2(0.0f, 0.0f), true);
        if (m_SelectedRow >= 0 && m_SelectedRow < static_cast<int>(m_Rows.size()))
        {
            Row& row = m_Rows[m_SelectedRow];
            const std::string header = std::string("Row ") + std::to_string(m_SelectedRow + 1) + "  " + KindName(row.Kind);
            EditorWidgets::SectionHeader(header.c_str(), "Edit the selected script row.");
            DrawRowEditor(row);

            ImGui::Separator();
            if (ImGui::Button(EditorLocale::Text("Move Up", "上移")) && m_SelectedRow > 0)
            {
                std::swap(m_Rows[m_SelectedRow], m_Rows[m_SelectedRow - 1]);
                --m_SelectedRow;
                m_Dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Move Down", "下移")) && m_SelectedRow + 1 < static_cast<int>(m_Rows.size()))
            {
                std::swap(m_Rows[m_SelectedRow], m_Rows[m_SelectedRow + 1]);
                ++m_SelectedRow;
                m_Dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Duplicate", "复制行")))
            {
                m_Rows.insert(m_Rows.begin() + m_SelectedRow + 1, row);
                ++m_SelectedRow;
                m_Dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Delete", "删除")))
            {
                m_Rows.erase(m_Rows.begin() + m_SelectedRow);
                if (m_SelectedRow >= static_cast<int>(m_Rows.size()))
                    m_SelectedRow = static_cast<int>(m_Rows.size()) - 1;
                m_Dirty = true;
            }
        }
        else
        {
            EditorWidgets::EmptyState("No row selected.", "Select a row from the timeline or add a new one.");
        }
        ImGui::EndChild();
    }
    void VisualNovelScriptEditorPanel::DrawRowEditor(Row& row)
    {
        switch (row.Kind)
        {
            case RowKind::Raw:
                if (InputMultiline("Raw", row.Raw, ImVec2(-1.0f, 160.0f)))
                    m_Dirty = true;
                break;
            case RowKind::Label:
                if (InputString("Label", row.Name))
                    m_Dirty = true;
                break;
            case RowKind::Background:
                if (EditorContentPickers::DrawAssetField("Background", row.Value, EditorWidgets::AssetReferenceKind::Texture, 512))
                    m_Dirty = true;
                break;
            case RowKind::Music:
                if (!m_BGMAssets.empty())
                {
                    const char* preview = row.Value.empty() ? "(stop)" : row.Value.c_str();
                    if (ImGui::BeginCombo("BGM Asset", preview))
                    {
                        if (ImGui::Selectable("(stop)", row.Value.empty()))
                        {
                            row.Value.clear();
                            m_Dirty = true;
                        }
                        for (size_t i = 0; i < m_BGMAssets.size(); ++i)
                        {
                            const std::string& asset = m_BGMAssets[i];
                            const std::string label = EditorWidgets::LabelWithId(
                                asset,
                                "vn_bgm:" + std::to_string(i));
                            if (ImGui::Selectable(label.c_str(), asset == row.Value))
                            {
                                row.Value = asset;
                                m_Dirty = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (EditorContentPickers::DrawAssetField("BGM Path", row.Value, EditorWidgets::AssetReferenceKind::Audio, 512))
                    m_Dirty = true;
                if (InputString("Display Name", row.Text, 256))
                    m_Dirty = true;
                break;
            case RowKind::Speed:
                if (InputString("Characters / Second", row.Value, 64))
                    m_Dirty = true;
                break;
            case RowKind::Character:
                if (InputString("Id", row.Name, 128))
                    m_Dirty = true;
                if (InputString("Display Name", row.Text, 128))
                    m_Dirty = true;
                if (EditorContentPickers::DrawStringPicker("Portrait Style", row.Value, PortraitStyleChoices(), 512))
                    m_Dirty = true;
                break;
            case RowKind::Show:
                if (EditorContentPickers::DrawStringPicker("Characters", row.Value, CollectCharacterIds(m_Rows), 512))
                    m_Dirty = true;
                break;
            case RowKind::Hide:
                if (EditorContentPickers::DrawStringPicker("Characters / all", row.Value, CollectCharacterIds(m_Rows), 512))
                    m_Dirty = true;
                break;
            case RowKind::Expression:
                if (EditorContentPickers::DrawStringPicker("Character", row.Name, CollectCharacterIds(m_Rows), 128))
                    m_Dirty = true;
                if (EditorContentPickers::DrawStringPicker("Expression", row.Value, CollectExpressionIds(m_Rows, row.Name), 128))
                    m_Dirty = true;
                break;
            case RowKind::Dialogue:
                if (EditorContentPickers::DrawStringPicker("Speaker", row.Name, CollectCharacterIds(m_Rows), 128))
                    m_Dirty = true;
                if (InputMultiline("Text", row.Text, ImVec2(-1.0f, 130.0f)))
                    m_Dirty = true;
                break;
            case RowKind::Choice:
                if (InputString(EditorLocale::Text("Prompt", "提示语"), row.Value, 256))
                    m_Dirty = true;
                EditorWidgets::HelpTooltip("Text shown while waiting for a choice. Empty uses the default.");
                ImGui::Separator();
                for (int i = 0; i < static_cast<int>(row.Choices.size()); ++i)
                {
                    ImGui::PushID(i);
                    ImGui::Text(EditorLocale::Text("Choice %d", "选项 %d"), i + 1);
                    if (InputString(EditorLocale::Text("Text", "文本"), row.Choices[i].Text, 512))
                        m_Dirty = true;
                    if (EditorContentPickers::DrawStringPicker("Target", row.Choices[i].Target, CollectLabels(m_Rows), 512))
                        m_Dirty = true;
                    if (EditorContentPickers::DrawStoryFlagField("Required Flag", row.Choices[i].RequiredFlag, 256))
                        m_Dirty = true;
                    EditorWidgets::HelpTooltip("Optional. The choice only renders when this story flag is set.");
                    if (EditorWidgets::InputString(EditorLocale::Text("Required Condition", "条件表达式"), row.Choices[i].RequiredCondition, 256))
                        m_Dirty = true;
                    EditorWidgets::HelpTooltip("Optional expression, e.g. \"gold >= 5\" or \"not flag FLAG_X\". Evaluated against script variables when Required Flag is empty.");
                    ImGui::SameLine();
                    if (ImGui::Button(EditorLocale::Text("Remove", "移除")))
                    {
                        row.Choices.erase(row.Choices.begin() + i);
                        m_Dirty = true;
                        ImGui::PopID();
                        break;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (ImGui::Button(EditorLocale::Text("+ Choice Option", "+ 选项项")))
                {
                    row.Choices.push_back({ EditorLocale::Text("New option", "新选项"), "target_label", {}, {} });
                    m_Dirty = true;
                }
                break;
            case RowKind::Goto:
                if (EditorContentPickers::DrawStringPicker("Target Label", row.Value, CollectLabels(m_Rows), 256))
                    m_Dirty = true;
                break;
            case RowKind::End:
                ImGui::TextDisabled("Ends this VN script.");
                break;
            case RowKind::Set:
                if (InputString(EditorLocale::Text("Variable", "变量"), row.Name, 128))
                    m_Dirty = true;
                if (InputString(EditorLocale::Text("Value", "值"), row.Value, 128))
                    m_Dirty = true;
                EditorWidgets::HelpTooltip("Assigns a literal number or copies another variable (e.g. \"5\" or \"maxhp\").");
                break;
            case RowKind::If:
                if (EditorWidgets::InputString(EditorLocale::Text("Condition", "条件"), row.Value, 256))
                    m_Dirty = true;
                EditorWidgets::HelpTooltip("Expression: always | never | flag <id> | <var> OP <number> | <number> OP <number>, optional leading \"not \".");
                if (EditorContentPickers::DrawStringPicker("Jump To", row.Text, CollectLabels(m_Rows), 256))
                    m_Dirty = true;
                break;
            case RowKind::Sheet:
                if (EditorContentPickers::DrawAssetField("Sheet Texture", row.Value, EditorWidgets::AssetReferenceKind::Texture, 512))
                    m_Dirty = true;
                if (InputString(EditorLocale::Text("Cell Width", "单元格宽"), row.Name, 32))
                    m_Dirty = true;
                if (InputString(EditorLocale::Text("Cell Height", "单元格高"), row.Text, 32))
                    m_Dirty = true;
                break;
            case RowKind::Char:
                if (InputString(EditorLocale::Text("Name", "名称"), row.Name, 128))
                    m_Dirty = true;
                if (InputString(EditorLocale::Text("Sheet X", "图集 X"), row.Value, 32))
                    m_Dirty = true;
                if (InputString(EditorLocale::Text("Sheet Y", "图集 Y"), row.Text, 32))
                    m_Dirty = true;
                break;
        }
    }
    void VisualNovelScriptEditorPanel::DrawLibraries()
    {
        EditorWidgets::SectionHeader("Characters", "Rows declared with @character and used by dialogue playback.");
        bool hasCharacters = false;
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            Row& row = m_Rows[i];
            if (row.Kind != RowKind::Character)
                continue;
            hasCharacters = true;
            ImGui::PushID(i);
            ImGui::Separator();
            ImGui::Text("Row %d", i + 1);
            if (InputString("Id", row.Name, 128)) m_Dirty = true;
            if (InputString("Display", row.Text, 128)) m_Dirty = true;
            if (InputString("Style", row.Value, 512)) m_Dirty = true;
            ImGui::PopID();
        }
        if (!hasCharacters)
            EditorWidgets::EmptyState("No characters declared.", "Add a Character row in Timeline to register speaker metadata.");

        ImGui::Separator();
        EditorWidgets::SectionHeader("BGM Cues", "Rows declared with @music / @bgm.");
        bool hasBgm = false;
        for (int i = 0; i < static_cast<int>(m_Rows.size()); ++i)
        {
            Row& row = m_Rows[i];
            if (row.Kind != RowKind::Music)
                continue;
            hasBgm = true;
            ImGui::PushID(10000 + i);
            ImGui::Separator();
            ImGui::Text("Row %d", i + 1);
            if (EditorContentPickers::DrawAssetField("Path", row.Value, EditorWidgets::AssetReferenceKind::Audio, 512)) m_Dirty = true;
            if (InputString("Display Name", row.Text, 256)) m_Dirty = true;
            ImGui::PopID();
        }
        if (!hasBgm)
            EditorWidgets::EmptyState("No BGM cues declared.", "Add a BGM row in Timeline to bind music cues.");
    }
} // namespace Wheatear
