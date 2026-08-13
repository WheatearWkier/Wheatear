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
            DrawRawPreview(m_ManifestDocument.GetRawPreview(), "##ProgressionManifestRawPreview");
            return;
        }

        if (DrawYamlNode(m_ManifestDocument.Root(), "manifest"))
        {
            m_ManifestDocument.MarkDirty();
            RefreshEntriesFromManifest();
        }
    }
    void ProgressionContentEditorPanel::DrawRawPreview(const std::string& text, const char* id)
    {
        std::string preview = text;
        EditorWidgets::InputMultilineString(id,
            preview,
            ImVec2(-1.0f, -1.0f),
            std::max<size_t>(text.size() + 1, 4096),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);
    }
    bool ProgressionContentEditorPanel::DrawYamlNode(YAML::Node node, const std::string& path, int depth)
    {
        if (!node || !node.IsDefined() || node.IsNull())
            return DrawYamlScalar(node, path);
        if (node.IsMap())
            return DrawYamlMap(node, path, depth);
        if (node.IsSequence())
            return DrawYamlSequence(node, path, depth);
        return DrawYamlScalar(node, path);
    }
    bool ProgressionContentEditorPanel::DrawYamlMap(YAML::Node node, const std::string& path, int depth)
    {
        bool changed = false;
        const std::vector<std::string> keys = MapKeysInOrder(node);
        if (keys.empty())
            EditorWidgets::EmptyState("Empty map.");

        std::string removeKey;
        for (const std::string& key : keys)
        {
            YAML::Node child = node[key];
            const std::string childPath = path + "/" + key;
            ImGui::PushID(childPath.c_str());

            if (child.IsMap() || child.IsSequence())
            {
                const ImGuiTreeNodeFlags flags = depth < 1 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
                const bool open = ImGui::TreeNodeEx("##Node", flags, "%s (%s)", key.c_str(), NodeTypeLabel(child));
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeKey = key;

                if (open)
                {
                    changed |= DrawYamlNode(child, childPath, depth + 1);
                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextUnformatted(key.c_str());
                ImGui::SameLine(220.0f);
                changed |= DrawYamlScalar(child, childPath);
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeKey = key;
            }

            ImGui::PopID();
        }

        if (!removeKey.empty())
        {
            node.remove(removeKey);
            changed = true;
        }

        changed |= DrawYamlAddControls(node, path, false);
        return changed;
    }
    bool ProgressionContentEditorPanel::DrawYamlSequence(YAML::Node node, const std::string& path, int depth)
    {
        bool changed = false;
        if (node.size() == 0)
            EditorWidgets::EmptyState("Empty list.");

        int removeIndex = -1;
        for (size_t i = 0; i < node.size(); ++i)
        {
            YAML::Node child = node[i];
            const std::string childPath = path + "/" + std::to_string(i);
            ImGui::PushID(childPath.c_str());

            if (child.IsMap() || child.IsSequence())
            {
                const bool open = ImGui::TreeNodeEx("##Item", 0, "[%d] (%s)", static_cast<int>(i + 1), NodeTypeLabel(child));
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeIndex = static_cast<int>(i);

                if (open)
                {
                    changed |= DrawYamlNode(child, childPath, depth + 1);
                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::Text("[%d]", static_cast<int>(i + 1));
                ImGui::SameLine(80.0f);
                changed |= DrawYamlScalar(child, childPath);
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeIndex = static_cast<int>(i);
            }

            ImGui::PopID();
        }

        if (removeIndex >= 0)
        {
            node.remove(removeIndex);
            changed = true;
        }

        changed |= DrawYamlAddControls(node, path, true);
        return changed;
    }
    bool ProgressionContentEditorPanel::DrawYamlScalar(YAML::Node node, const std::string& path)
    {
        ImGui::PushID(path.c_str());
        std::string value = ScalarText(node);
        const std::string key = LastPathSegment(path);
        bool changed = false;
        if (ContainsInsensitive(key, "icon") || ContainsInsensitive(key, "texture") || ContainsInsensitive(key, "image"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Texture, 512);
        }
        else if (ContainsInsensitive(key, "audio") || ContainsInsensitive(key, "sound") || ContainsInsensitive(key, "sfx") || ContainsInsensitive(key, "bgm"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Audio, 512);
        }
        else if (ContainsInsensitive(key, "scene"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Scene, 512);
        }
        else if (ContainsInsensitive(key, "script") || ContainsInsensitive(key, "event"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Script, 512);
        }
        else if (ContainsInsensitive(key, "slot"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::EquipmentSlot, 256);
        }
        else if (ContainsInsensitive(key, "equipment"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Equipment, 256);
        }
        else if (ContainsInsensitive(key, "dungeon"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Dungeon, 256);
        }
        else if (ContainsInsensitive(key, "skill"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Skill, 256);
        }
        else if (ContainsInsensitive(key, "material") || key == "item" || ContainsInsensitive(key, "itemid"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Material, 256);
        }
        else if (ContainsInsensitive(key, "flag"))
        {
            changed = EditorContentPickers::DrawStoryFlagField(EditorLocale::Text("Value", "值"), value, 256);
        }
        else
        {
            changed = EditorWidgets::InputString(EditorLocale::Text("Value", "值"), value, 512);
        }
        if (changed)
            node = value;
        ImGui::PopID();
        return changed;
    }
    bool ProgressionContentEditorPanel::DrawYamlAddControls(YAML::Node node, const std::string& path, bool sequence)
    {
        bool changed = false;
        ImGui::PushID((path + "/add").c_str());
        ImGui::Separator();

        if (sequence)
        {
            std::string& value = m_NewScalarValues[path];
            EditorWidgets::InputString("New Value", value, 256);
            if (ImGui::SmallButton("Add Value"))
            {
                node.push_back(value);
                value.clear();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add Map"))
            {
                node.push_back(YAML::Node(YAML::NodeType::Map));
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add List"))
            {
                node.push_back(YAML::Node(YAML::NodeType::Sequence));
                changed = true;
            }
            ImGui::SameLine();
            if (node.size() > 0 && ImGui::SmallButton("Duplicate Last"))
            {
                node.push_back(YAML::Clone(node[node.size() - 1]));
                changed = true;
            }
        }
        else
        {
            std::string& key = m_NewMapKeys[path];
            EditorWidgets::InputString("New Key", key, 192);
            const bool canAdd = !key.empty() && !MapContainsKey(node, key);
            ImGui::BeginDisabled(!canAdd);
            if (ImGui::SmallButton("Add Value"))
            {
                node[key] = "";
                key.clear();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add Map"))
            {
                node[key] = YAML::Node(YAML::NodeType::Map);
                key.clear();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add List"))
            {
                node[key] = YAML::Node(YAML::NodeType::Sequence);
                key.clear();
                changed = true;
            }
            ImGui::EndDisabled();
            if (!key.empty() && MapContainsKey(node, key))
                EditorWidgets::InlineStatus("Key already exists.", EditorWidgets::StatusKind::Warning);
        }

        ImGui::PopID();
        return changed;
    }
} // namespace Wheatear
