#include "wepch.h"
#include "ProgressionContentEditorPanel.h"
#include "ProgressionContentEditorPanelInternal.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"

#include <yaml-cpp/yaml.h>

#include <imgui/imgui.h>

#include <string>
#include <vector>

namespace Wheatear {

    using namespace ProgressionContentEditorInternal;

    void ProgressionContentEditorPanel::DrawValidationTab()
    {
        if (!m_SelectedDocument.IsParseValid())
        {
            EditorWidgets::InlineStatus("YAML parse failed. Fix parsing before reference validation.", EditorWidgets::StatusKind::Error);
            return;
        }

        YAML::Node root = m_SelectedDocument.Root();
        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        auto existsIn = [](const std::vector<std::string>& ids, const std::string& value)
        {
            return value.empty()
                || std::find(ids.begin(), ids.end(), value) != ids.end();
        };

        auto validateId = [&](const std::string& label,
            const std::string& value,
            const std::vector<std::string>& ids,
            bool required)
        {
            if (value.empty())
            {
                if (required)
                    errors.push_back(label + " is empty.");
                return;
            }
            if (!existsIn(ids, value))
                errors.push_back(label + " references missing id: " + value);
        };

        const std::vector<std::string> skillIds = BuildContentIds("skill");
        const std::vector<std::string> equipmentIds = BuildContentIds("equipment");
        const std::vector<std::string> equipmentSlotIds = BuildContentIds("equipmentSlot");
        const std::vector<std::string> dungeonIds = BuildContentIds("dungeon");
        const std::vector<std::string> materialIds = BuildContentIds("material");

        if (m_SelectedKey == "defaults")
        {
            YAML::Node defaults = root["defaults"];
            validateId("Main Dungeon", ScalarText(defaults["mainDungeon"]), dungeonIds, false);
            validateId("Material Dungeon", ScalarText(defaults["materialDungeon"]), dungeonIds, false);
            validateId("Initial Selected Equipment", ScalarText(defaults["initialSelectedEquipment"]), equipmentIds, false);
            validateId("Traveler Armor Upgrade Equipment", ScalarText(defaults["travelerArmorUpgradeEquipment"]), equipmentIds, false);

            const auto validateList = [&](const char* label, YAML::Node list, const std::vector<std::string>& ids)
            {
                if (!list || !list.IsSequence())
                    return;
                for (size_t i = 0; i < list.size(); ++i)
                    validateId(std::string(label) + "[" + std::to_string(i) + "]", ScalarText(list[i]), ids, false);
            };

            validateList("Initial Unlocked Skill", defaults["initialUnlockedSkills"], skillIds);
            validateList("Initial Owned Equipment", defaults["initialOwnedEquipment"], equipmentIds);
            validateList("Initial Unlocked Dungeon", defaults["initialUnlockedDungeons"], dungeonIds);

            YAML::Node equipped = defaults["initialEquippedItems"];
            if (equipped && equipped.IsMap())
            {
                for (auto it = equipped.begin(); it != equipped.end(); ++it)
                {
                    const std::string slot = it->first.as<std::string>("");
                    const std::string equipment = ScalarText(it->second);
                    validateId("Initial Equipped Slot", slot, equipmentSlotIds, true);
                    validateId("Initial Equipped Item for " + slot, equipment, equipmentIds, false);
                }
            }
        }
        else if (m_SelectedKey == "upgrades")
        {
            for (const std::string& key : MapKeysInOrder(root))
            {
                YAML::Node upgrade = root[key];
                if (!upgrade || !upgrade.IsMap())
                    continue;

                YAML::Node costs = upgrade["costs"];
                if (costs && costs.IsSequence())
                {
                    for (size_t i = 0; i < costs.size(); ++i)
                    {
                        validateId(key + " cost[" + std::to_string(i) + "] item",
                            ScalarText(costs[i]["item"]),
                            materialIds,
                            true);
                    }
                }

                YAML::Node unlockSkills = upgrade["unlockSkills"];
                if (unlockSkills && unlockSkills.IsSequence())
                {
                    for (size_t i = 0; i < unlockSkills.size(); ++i)
                    {
                        validateId(key + " unlockSkills[" + std::to_string(i) + "]",
                            ScalarText(unlockSkills[i]),
                            skillIds,
                            true);
                    }
                }
            }
        }
        else if (m_SelectedKey == "equipment" || m_SelectedKey == "equipmentSlots")
        {
            const char* rootKey = m_SelectedKey == "equipment" ? "equipment" : "equipmentSlots";
            YAML::Node records = root[rootKey];
            if (records && records.IsSequence())
            {
                std::unordered_set<std::string> seen;
                for (size_t i = 0; i < records.size(); ++i)
                {
                    YAML::Node record = records[i];
                    const std::string id = ScalarText(record["id"]);
                    if (id.empty())
                        errors.push_back(m_SelectedKey + "[" + std::to_string(i) + "] has no id.");
                    else if (!seen.insert(id).second)
                        errors.push_back("Duplicate " + m_SelectedKey + " id: " + id);

                    if (m_SelectedKey != "equipment")
                        continue;

                    validateId("Equipment " + id + " slotId", ScalarText(record["slotId"]), equipmentSlotIds, false);
                    const std::string icon = ScalarText(record["icon"]);
                    if (!icon.empty() && !EditorWidgets::ProjectAssetExists(icon) && !AssetAliasRegistry::Has(icon))
                        warnings.push_back("Equipment " + id + " icon path is missing: " + icon);
                }
            }
        }
        else if (m_SelectedKey == "dungeons")
        {
            YAML::Node records = root["dungeons"];
            if (records && records.IsSequence())
            {
                std::unordered_set<std::string> seen;
                for (size_t i = 0; i < records.size(); ++i)
                {
                    YAML::Node record = records[i];
                    const std::string id = ScalarText(record["id"]);
                    if (id.empty())
                        errors.push_back("Dungeon[" + std::to_string(i) + "] has no id.");
                    else if (!seen.insert(id).second)
                        errors.push_back("Duplicate dungeon id: " + id);

                    YAML::Node unlocks = record["unlocksOnFirstClear"];
                    if (unlocks && unlocks.IsSequence())
                    {
                        for (size_t j = 0; j < unlocks.size(); ++j)
                            validateId("Dungeon " + id + " unlocksOnFirstClear[" + std::to_string(j) + "]", ScalarText(unlocks[j]), dungeonIds, false);
                    }
                }
            }
        }
        else if (m_SelectedKey == "dungeonRewardSummary")
        {
            YAML::Node summaries = root["dungeonRewardSummary"];
            if (!summaries || !summaries.IsSequence())
            {
                errors.push_back("dungeonRewardSummary must be a list.");
            }
            else if (summaries.size() == 0)
            {
                warnings.push_back("dungeonRewardSummary is empty.");
            }
            else
            {
                for (size_t i = 0; i < summaries.size(); ++i)
                {
                    if (ScalarText(summaries[i]).empty())
                        warnings.push_back("dungeonRewardSummary[" + std::to_string(i) + "] is empty.");
                }
            }
        }
        else if (m_SelectedKey == "skillNodes")
        {
            YAML::Node records = root["skillNodes"];
            if (records && records.IsSequence())
            {
                std::unordered_set<std::string> seen;
                for (size_t i = 0; i < records.size(); ++i)
                {
                    YAML::Node record = records[i];
                    const std::string id = ScalarText(record["id"]);
                    if (id.empty())
                        errors.push_back("Skill node[" + std::to_string(i) + "] has no id.");
                    else if (!seen.insert(id).second)
                        errors.push_back("Duplicate skill node id: " + id);
                }

                for (size_t i = 0; i < records.size(); ++i)
                {
                    YAML::Node record = records[i];
                    const std::string id = ScalarText(record["id"]);
                    const std::string parentId = ScalarText(record["parentId"]);
                    if (!parentId.empty() && seen.find(parentId) == seen.end())
                        errors.push_back("Skill " + id + " parentId is missing: " + parentId);
                }
            }
        }
        else if (m_SelectedKey == "materials" || m_SelectedKey == "relationships")
        {
            const char* rootKey = RootKeyForSelectedAsset(m_SelectedKey);
            YAML::Node records = rootKey ? root[rootKey] : YAML::Node{};
            if (records && records.IsSequence())
            {
                std::unordered_set<std::string> seen;
                for (size_t i = 0; i < records.size(); ++i)
                {
                    const std::string id = ScalarText(records[i]["id"]);
                    if (id.empty())
                        errors.push_back(m_SelectedKey + "[" + std::to_string(i) + "] has no id.");
                    else if (!seen.insert(id).second)
                        errors.push_back("Duplicate " + m_SelectedKey + " id: " + id);
                }
            }
        }

        EditorWidgets::SectionHeader("Reference Validation", "Checks ids and asset references used by the selected progression file.");
        if (errors.empty() && warnings.empty())
        {
            EditorWidgets::InlineStatus("Validation passed.", EditorWidgets::StatusKind::Success);
            return;
        }

        for (const std::string& error : errors)
            EditorWidgets::InlineStatus(error.c_str(), EditorWidgets::StatusKind::Error);
        for (const std::string& warning : warnings)
            EditorWidgets::InlineStatus(warning.c_str(), EditorWidgets::StatusKind::Warning);
    }
    void ProgressionContentEditorPanel::DrawManifestTab()
    {
        if (!m_ManifestDocument.IsParseValid())
        {
            EditorWidgets::InlineStatus("Manifest YAML parse failed.", EditorWidgets::StatusKind::Error);
            DrawRawPreview(m_ManifestDocument.GetRawPreview(), "##ProgressionManifestRawPreview",
                m_ManifestPath.c_str());
            return;
        }

        if (YamlTreeEditor::DrawYamlNode(m_ManifestDocument.Root(), "manifest", 0,
            m_NewScalarValues, m_NewMapKeys))
        {
            m_ManifestDocument.MarkDirty();
            RefreshEntriesFromManifest();
        }
    }
    void ProgressionContentEditorPanel::DrawRawPreview(const std::string& text, const char* id,
        const char* sourcePath)
    {
        EditorGameplayShell::DrawRawPreview(text, id, sourcePath);
    }

} // namespace Wheatear
