#include "wepch.h"
#include "ProgressionContentEditorPanel.h"
#include "ProgressionContentEditorPanelInternal.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <imgui/imgui.h>

#include <string>
#include <vector>

namespace Wheatear {

    using namespace ProgressionContentEditorInternal;

    void ProgressionContentEditorPanel::DrawContentTab()
    {
        if (!m_SelectedDocument.IsParseValid())
        {
            EditorWidgets::InlineStatus("YAML parse failed. Fix the source file before content editing.", EditorWidgets::StatusKind::Error);
            DrawRawPreview(m_SelectedDocument.GetRawPreview(), "##ProgressionContentRawPreview");
            return;
        }

        if (m_SelectedKey == "skillNodes")
        {
            DrawSkillTreeTab();
            return;
        }

        bool changed = false;
        if (const char* rootKey = RootKeyForSelectedAsset(m_SelectedKey))
            changed |= DrawSequenceContentEditor(rootKey);
        else if (m_SelectedKey == "defaults")
        {
            YAML::Node root = m_SelectedDocument.Root();
            YAML::Node defaults = root["defaults"];
            if (!defaults || !defaults.IsMap())
            {
                root["defaults"] = YAML::Node(YAML::NodeType::Map);
                defaults = root["defaults"];
                changed = true;
            }
            changed |= DrawDefaultsEditor(defaults);
        }
        else if (m_SelectedKey == "upgrades")
        {
            YAML::Node root = m_SelectedDocument.Root();
            YAML::Node upgrades = root["upgrades"];
            if (!upgrades || !upgrades.IsMap())
            {
                root["upgrades"] = YAML::Node(YAML::NodeType::Map);
                upgrades = root["upgrades"];
                changed = true;
            }
            changed |= DrawUpgradesEditor(upgrades);
        }
        else if (m_SelectedKey == "dungeonRewardSummary")
        {
            YAML::Node root = m_SelectedDocument.Root();
            changed |= DrawRewardSummaryListEditor(root);
        }
        else
        {
            EditorWidgets::EmptyState("No dedicated editor for this asset yet.", "Use Advanced YAML for now.");
        }

        if (changed)
            m_SelectedDocument.MarkDirty();
    }
    bool ProgressionContentEditorPanel::DrawSequenceContentEditor(const char* rootKey)
    {
        YAML::Node root = m_SelectedDocument.Root();
        YAML::Node records = root[rootKey];
        bool changed = false;

        if (!records || !records.IsSequence())
        {
            EditorWidgets::InlineStatus("This asset does not contain the expected list root.", EditorWidgets::StatusKind::Warning);
            if (ImGui::Button("Create List Root"))
            {
                root[rootKey] = YAML::Node(YAML::NodeType::Sequence);
                m_SelectedContentRecordId.clear();
                return true;
            }
            return false;
        }

        if (m_SelectedContentRecordId.empty() && records.size() > 0)
            m_SelectedContentRecordId = RecordId(records[0], 0);

        int selectedIndex = -1;
        for (size_t i = 0; i < records.size(); ++i)
        {
            if (RecordId(records[i], i) == m_SelectedContentRecordId)
            {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }

        const float listWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.30f);
        ImGui::BeginChild("##ProgressionRecordList", ImVec2(listWidth, 0.0f), true);
        EditorWidgets::SectionHeader(HumanizeKey(m_SelectedKey).c_str(), "Records in this progression asset.");

        if (ImGui::Button("Add Record"))
        {
            YAML::Node record = MakeDefaultContentRecord(m_SelectedKey);
            const std::string baseId = ScalarText(record["id"]).empty() ? "new_record" : ScalarText(record["id"]);
            const std::string newId = MakeUniqueRecordId(records, baseId);
            record["id"] = newId;
            records.push_back(record);
            m_SelectedContentRecordId = newId;
            changed = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(selectedIndex < 0);
        if (ImGui::Button("Duplicate") && selectedIndex >= 0)
        {
            YAML::Node clone = YAML::Clone(records[static_cast<size_t>(selectedIndex)]);
            const std::string baseId = ScalarText(clone["id"]).empty()
                ? "new_record_copy"
                : ScalarText(clone["id"]) + "_copy";
            const std::string newId = MakeUniqueRecordId(records, baseId);
            clone["id"] = newId;
            records.push_back(clone);
            m_SelectedContentRecordId = newId;
            changed = true;
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        for (size_t i = 0; i < records.size(); ++i)
        {
            YAML::Node record = records[i];
            const std::string id = RecordId(record, i);
            const std::string label = EditorWidgets::LabelWithId(RecordTitle(record, i), "progression_record:" + std::to_string(i));
            if (ImGui::Selectable(label.c_str(), id == m_SelectedContentRecordId))
                m_SelectedContentRecordId = id;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##ProgressionRecordDetails", ImVec2(0.0f, 0.0f), true);
        EditorWidgets::SectionHeader(
            EditorLocale::Text("Details", "详情"),
            EditorLocale::Text("Edit fields and save the asset when ready.", "编辑字段，准备好后保存。"));

        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(records.size()))
        {
            EditorWidgets::EmptyState("Select a record to edit.");
            ImGui::EndChild();
            return changed;
        }

        YAML::Node record = records[static_cast<size_t>(selectedIndex)];
        const std::string originalId = ScalarText(record["id"]);
        if (DrawTextField(record, "Id", "id", 160))
        {
            const std::string newId = ScalarText(record["id"]);
            if (!newId.empty() && newId != originalId)
                m_SelectedContentRecordId = newId;
            changed = true;
        }

        if (m_SelectedKey == "materials")
        {
            changed |= DrawTextField(record, "Name", "name", 256);
        }
        else if (m_SelectedKey == "equipment")
        {
            changed |= DrawTextField(record, "Name", "name", 256);
            changed |= DrawTextField(record, "Slot Label", "slot", 128);
            changed |= DrawIntField(record, "Page", "page", 1, 1, 99);
            changed |= DrawTextField(record, "Status", "status", 256);
            changed |= DrawTextField(record, EditorLocale::Text("Stats", "属性"), "stats", 512);
            changed |= DrawTextField(record, EditorLocale::Text("Source", "来源"), "source", 512);
            changed |= DrawMultilineField(record, EditorLocale::Text("Description", "描述"), "description");
            changed |= DrawIdField(record, "Slot Id", "slotId", "equipmentSlot");
            changed |= DrawAssetField(record, EditorLocale::Text("Icon", "图标"), "icon", EditorWidgets::AssetReferenceKind::Texture, 512);
        }
        else if (m_SelectedKey == "equipmentSlots")
        {
            changed |= DrawTextField(record, "Name", "name", 256);
        }
        else if (m_SelectedKey == "dungeons")
        {
            changed |= DrawTextField(record, "Name", "name", 256);
            changed |= DrawChoiceField(record, "Category", "category", DungeonCategoryChoices(), 256);
            changed |= DrawIntField(record, EditorLocale::Text("Recommended Level", "推荐等级"), "recommendedLevel", 1, 1, 99);
            changed |= DrawTextField(record, "Locked Status", "statusWhenLocked", 512);
            changed |= DrawTextField(record, "Unlocked Status", "statusWhenUnlocked", 512);
            changed |= DrawRewardTextField(record, EditorLocale::Text("First Clear Reward", "首通奖励"), "firstClearRewardText");
            changed |= DrawRewardTextField(record, EditorLocale::Text("Repeat Reward", "重复奖励"), "repeatRewardText");
            changed |= DrawStringListField(record, "Unlocks On First Clear", "unlocksOnFirstClear", BuildContentIds("dungeon"));
            changed |= DrawStoryFlagListField(record, "Flags On Clear", "flagsOnClear");
            changed |= DrawMultilineField(record, "Objective On Clear", "objectiveOnClear");
            changed |= DrawTextField(record, "First Clear Notification", "firstClearNotification", 512);
        }
        else if (m_SelectedKey == "relationships")
        {
            changed |= DrawTextField(record, "Name", "name", 256);
            changed |= DrawIntField(record, EditorLocale::Text("Affinity", "好感度"), "affinity", 0, -999, 999);
            changed |= DrawIntField(record, EditorLocale::Text("Support Level", "支援等级"), "supportLevel", 0, 0, 99);
            changed |= DrawBoolField(record, EditorLocale::Text("Unlocked", "已解锁"), "unlocked", false);
            changed |= DrawTextField(record, EditorLocale::Text("Role", "角色定位"), "role", 512);
            changed |= DrawMultilineField(record, EditorLocale::Text("Next Milestone", "下一里程碑"), "nextMilestone");
        }

        ImGui::Separator();
        if (ImGui::Button("Delete Record"))
        {
            records.remove(static_cast<size_t>(selectedIndex));
            m_SelectedContentRecordId.clear();
            changed = true;
        }

        ImGui::EndChild();
        return changed;
    }
    bool ProgressionContentEditorPanel::DrawDefaultsEditor(YAML::Node defaults)
    {
        bool changed = false;
        EditorWidgets::SectionHeader(
            EditorLocale::Text("Default Progression State", "默认成长状态"),
            EditorLocale::Text("Initial unlocks, objective text, and save-load defaults.", "初始解锁、目标文本和存读档默认值。"));
        changed |= DrawIdField(defaults, "Main Dungeon", "mainDungeon", "dungeon");
        changed |= DrawIdField(defaults, "Material Dungeon", "materialDungeon", "dungeon");
        changed |= DrawMultilineField(defaults, "Objective", "objective");
        changed |= DrawMultilineField(defaults, "Last Result Message", "lastResultMessage");
        changed |= DrawStringListField(defaults, "Initial Unlocked Skills", "initialUnlockedSkills", BuildContentIds("skill"));
        changed |= DrawStringListField(defaults, "Initial Owned Equipment", "initialOwnedEquipment", BuildContentIds("equipment"));
        changed |= DrawStringMapField(defaults, "Initial Equipped Items", "initialEquippedItems",
            BuildContentIds("equipmentSlot"),
            BuildContentIds("equipment"));
        changed |= DrawIdField(defaults, "Initial Selected Equipment", "initialSelectedEquipment", "equipment");
        changed |= DrawIdField(defaults, "Traveler Armor Upgrade Equipment", "travelerArmorUpgradeEquipment", "equipment");
        changed |= DrawStringListField(defaults, "Initial Unlocked Dungeons", "initialUnlockedDungeons", BuildContentIds("dungeon"));
        changed |= DrawStoryFlagListField(defaults, "Initial Story Flags", "initialStoryFlags");
        return changed;
    }
    bool ProgressionContentEditorPanel::DrawUpgradesEditor(YAML::Node upgrades)
    {
        bool changed = false;
        const std::vector<std::string> keys = MapKeysInOrder(upgrades);
        if (m_SelectedContentRecordId.empty() && !keys.empty())
            m_SelectedContentRecordId = keys.front();

        const float listWidth = std::max(260.0f, ImGui::GetContentRegionAvail().x * 0.28f);
        ImGui::BeginChild("##ProgressionUpgradeList", ImVec2(listWidth, 0.0f), true);
        EditorWidgets::SectionHeader(
            EditorLocale::Text(EditorLocale::Text("Upgrades", "升级"), "升级"),
            EditorLocale::Text("Named upgrade recipes.", "带名称的升级配方。"));
        if (ImGui::Button("Add Upgrade"))
        {
            std::string key = "newUpgrade";
            for (int i = 2; MapContainsKey(upgrades, key); ++i)
                key = "newUpgrade" + std::to_string(i);

            YAML::Node upgrade(YAML::NodeType::Map);
            upgrade["costs"] = YAML::Node(YAML::NodeType::Sequence);
            upgrade["attributeBonus"] = YAML::Node(YAML::NodeType::Map);
            upgrade["unlockSkills"] = YAML::Node(YAML::NodeType::Sequence);
            upgrades[key] = upgrade;
            m_SelectedContentRecordId = key;
            changed = true;
        }
        ImGui::Separator();
        for (const std::string& key : keys)
        {
            const std::string label = EditorWidgets::LabelWithId(key, "progression_upgrade:" + key);
            if (ImGui::Selectable(label.c_str(), key == m_SelectedContentRecordId))
                m_SelectedContentRecordId = key;
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##ProgressionUpgradeDetails", ImVec2(0.0f, 0.0f), true);
        EditorWidgets::SectionHeader(
            EditorLocale::Text("Upgrade Details", "升级详情"),
            EditorLocale::Text("Costs, attribute bonuses, and unlocked skills.", "成本、属性加成和解锁技能。"));
        if (m_SelectedContentRecordId.empty() || !MapContainsKey(upgrades, m_SelectedContentRecordId))
        {
            EditorWidgets::EmptyState("Select an upgrade to edit.");
            ImGui::EndChild();
            return changed;
        }

        YAML::Node upgrade = upgrades[m_SelectedContentRecordId];
        std::string upgradeKey = m_SelectedContentRecordId;
        if (EditorWidgets::InputString("Upgrade Key", upgradeKey, 160)
            && !upgradeKey.empty()
            && upgradeKey != m_SelectedContentRecordId
            && !MapContainsKey(upgrades, upgradeKey))
        {
            upgrades[upgradeKey] = YAML::Clone(upgrade);
            upgrades.remove(m_SelectedContentRecordId);
            m_SelectedContentRecordId = upgradeKey;
            upgrade = upgrades[m_SelectedContentRecordId];
            changed = true;
        }

        changed |= DrawCostsField(upgrade);
        changed |= DrawAttributeBonusField(upgrade);
        changed |= DrawStringListField(upgrade, "Unlock Skills", "unlockSkills", BuildContentIds("skill"));

        ImGui::Separator();
        if (ImGui::Button("Duplicate Upgrade"))
        {
            std::string key = m_SelectedContentRecordId + "_copy";
            for (int i = 2; MapContainsKey(upgrades, key); ++i)
                key = m_SelectedContentRecordId + "_copy" + std::to_string(i);
            upgrades[key] = YAML::Clone(upgrade);
            m_SelectedContentRecordId = key;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Upgrade"))
        {
            upgrades.remove(m_SelectedContentRecordId);
            m_SelectedContentRecordId.clear();
            changed = true;
        }

        ImGui::EndChild();
        return changed;
    }
} // namespace Wheatear
