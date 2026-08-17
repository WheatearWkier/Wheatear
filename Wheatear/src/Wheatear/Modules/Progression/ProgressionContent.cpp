#include "wtpch.h"
#include "ProgressionContent.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <filesystem>

namespace Wheatear::ProgressionContent {

    namespace {

        static constexpr const char* kContentPath = "assets/gameplay/progression/progression_content.yaml";

        static std::string ReadString(const YAML::Node& node, const std::string& fallback = {})
        {
            return node ? node.as<std::string>(fallback) : fallback;
        }

        static bool IsProjectAssetReference(std::string value)
        {
            std::replace(value.begin(), value.end(), '\\', '/');
            return value == "assets" || value.rfind("assets/", 0) == 0;
        }

        static std::vector<std::string> ReadStringList(const YAML::Node& node)
        {
            std::vector<std::string> values;
            if (!node || !node.IsSequence())
                return values;

            values.reserve(node.size());
            for (const auto& item : node)
            {
                const std::string value = ReadString(item);
                if (!value.empty())
                    values.push_back(value);
            }
            return values;
        }

        static std::unordered_map<std::string, std::string> ReadStringMap(const YAML::Node& node)
        {
            std::unordered_map<std::string, std::string> values;
            if (!node || !node.IsMap())
                return values;

            for (const auto& entry : node)
            {
                if (!entry.first.IsScalar())
                    continue;

                const std::string key = entry.first.as<std::string>("");
                const std::string value = ReadString(entry.second);
                if (!key.empty() && !value.empty())
                    values[key] = value;
            }
            return values;
        }

        static AttributeBonus ReadAttributeBonus(const YAML::Node& node)
        {
            AttributeBonus bonus;
            if (!node)
                return bonus;

            bonus.HP = node["HP"].as<int>(bonus.HP);
            bonus.ATK = node["ATK"].as<int>(bonus.ATK);
            bonus.DEF = node["DEF"].as<int>(bonus.DEF);
            bonus.MATK = node["MATK"].as<int>(bonus.MATK);
            bonus.MDEF = node["MDEF"].as<int>(bonus.MDEF);
            return bonus;
        }

        static std::vector<GameProgress::MaterialCost> ReadCosts(const YAML::Node& node)
        {
            std::vector<GameProgress::MaterialCost> costs;
            if (!node || !node.IsSequence())
                return costs;

            costs.reserve(node.size());
            for (const auto& item : node)
            {
                GameProgress::MaterialCost cost;
                cost.ItemId = ReadString(item["item"], ReadString(item["id"]));
                cost.DisplayName = ReadString(item["name"]);
                cost.Amount = item["amount"].as<int>(0);
                if (!cost.ItemId.empty() && cost.Amount > 0)
                    costs.push_back(std::move(cost));
            }
            return costs;
        }

        static UpgradeDefinition ReadUpgrade(const YAML::Node& node)
        {
            UpgradeDefinition upgrade;
            if (!node)
                return upgrade;

            upgrade.EquipmentId = node["equipmentId"]
                ? node["equipmentId"].as<std::string>() : std::string{};
            upgrade.DisplayName = node["name"]
                ? node["name"].as<std::string>() : std::string{};
            upgrade.TargetLevel = node["targetLevel"]
                ? node["targetLevel"].as<int>(1) : 1;
            upgrade.GoldCost = node["goldCost"]
                ? node["goldCost"].as<int>(0) : 0;
            upgrade.Costs = ReadCosts(node["costs"]);
            upgrade.Bonus = ReadAttributeBonus(node["attributeBonus"]);
            upgrade.UnlockSkills = ReadStringList(node["unlockSkills"]);
            upgrade.ResultMessage = node["resultMessage"]
                ? node["resultMessage"].as<std::string>() : std::string{};
            upgrade.NotificationTitle = node["notification"]
                ? node["notification"].as<std::string>() : std::string{};
            return upgrade;
        }

        static const UpgradeDefinition* FindUpgradeIn(const std::vector<UpgradeDefinition>& upgrades,
            const std::string& id)
        {
            for (const auto& upgrade : upgrades)
            {
                if (upgrade.Id == id)
                    return &upgrade;
            }
            return nullptr;
        }

        static Content BuildFallbackContent()
        {
            Content content;
            content.DefaultObjective = "整理黑熊掉落的材料，确认魔剑和装备的强化方向。";
            content.DefaultLastResultMessage = "据点已开启。完成黑熊战后，掉落会写入这里。";
            content.InitialUnlockedSkills = {
                "basic_attack",
                "air_basic",
                "launcher",
                "air_chase",
                "magic_sword_core"
            };
            content.InitialOwnedEquipment = {
                "traveler_armor",
                "black_forest_armor",
                "beast_tooth_pendant",
                "novice_magic_ring",
                "wind_boots",
                "old_ward_charm",
                "training_blade",
                "angel_feather"
            };
            content.InitialEquippedItemsBySlot["armor"] = "traveler_armor";
            content.InitialSelectedEquipmentId = "traveler_armor";
            content.TravelerArmorUpgradeEquipmentId = "traveler_armor";
            content.InitialUnlockedDungeons = { content.MainDungeonId };
            content.InitialStoryFlags = { "FLAG_CH02_SIDE_COMBAT_STARTED" };
            content.Materials = {
                { "MAT-MAGIC-CORE-T0", "魔核碎片", 0 },
                { "MAT-BEAST-SINEW", "兽筋", 0 },
                { "MAT-BEAST-CLAW", "熊爪", 0 }
            };
            content.MagicSwordLv2.Id = "magicSwordLv2";
            content.MagicSwordLv2.EquipmentId = ""; // special upgrade: unlocked via the skill tree
            content.MagicSwordLv2.DisplayName = "魔剑";
            content.MagicSwordLv2.TargetLevel = 2;
            content.MagicSwordLv2.GoldCost = 0;
            content.MagicSwordLv2.Costs = {
                { "MAT-MAGIC-CORE-T0", "魔核碎片", 1 },
                { "MAT-BEAST-SINEW", "兽筋", 2 },
                { "MAT-BEAST-CLAW", "熊爪", 1 }
            };
            content.MagicSwordLv2.Bonus.ATK = 3;
            content.MagicSwordLv2.Bonus.MATK = 3;
            content.MagicSwordLv2.UnlockSkills = {
                "magic_sword_lv2"
            };
            content.MagicSwordLv2.ResultMessage = "魔剑 Lv2 觉醒：基础斩击、跳斩和火球衔接更稳定。";
            content.MagicSwordLv2.NotificationTitle = "魔剑 Lv2 已觉醒";
            content.TravelerArmorLv1.Id = "travelerArmorLv1";
            content.TravelerArmorLv1.EquipmentId = "traveler_armor";
            content.TravelerArmorLv1.DisplayName = "旅人护衣";
            content.TravelerArmorLv1.TargetLevel = 1;
            content.TravelerArmorLv1.GoldCost = 0;
            content.TravelerArmorLv1.Costs = {
                { "MAT-BEAST-SINEW", "兽筋", 1 },
                { "MAT-BEAST-CLAW", "熊爪", 1 }
            };
            content.TravelerArmorLv1.Bonus.HP = 30;
            content.TravelerArmorLv1.Bonus.DEF = 2;
            content.TravelerArmorLv1.ResultMessage = "旅人护衣 +1：生命和防御提高，低空连击失误更不容易暴毙。";
            content.TravelerArmorLv1.NotificationTitle = "旅人护衣 +1 完成";
            content.Upgrades = { content.MagicSwordLv2, content.TravelerArmorLv1 };
            content.Dungeons = {
                {
                    content.MainDungeonId,
                    "黑熊丈夫讨伐",
                    "主线副本",
                    1,
                    "可挑战",
                    "可挑战",
                    "魔核碎片 x1 / 兽筋 x2 / 熊爪 x1 / 经验 90",
                    "兽筋、熊爪、少量魔核碎片、连击评分额外材料",
                    { content.MaterialDungeonId },
                    { "FLAG_CH02_BOSS_DEFEATED", "FLAG_HUB_UNLOCKED" },
                    "在据点强化魔剑，重刷黑林兽道练习空连，或继续前往边境村。",
                    "新副本解锁：黑林兽道"
                },
                {
                    content.MaterialDungeonId,
                    "黑林兽道",
                    "材料副本",
                    2,
                    "击败黑熊丈夫后解锁",
                    "已解锁，可重刷",
                    "兽筋、熊爪、少量魔核碎片、连击评分额外材料",
                    "兽筋、熊爪、少量魔核碎片、连击评分额外材料"
                }
            };
            content.DungeonRewardSummary = {
                "黑熊丈夫讨伐首通: 魔核碎片 x1 / 兽筋 x2 / 熊爪 x1 / 经验 90",
                "黑林兽道重刷: 兽筋、熊爪、少量魔核碎片、连击评分额外材料",
                "用途: 魔剑 Lv2、旅人护衣 +1、后续空连训练节点。"
            };
            content.Relationships = {
                {
                    "mentor",
                    "魔剑士导师",
                    100,
                    2,
                    true,
                    "青梅伪装 / 空连指导",
                    "当前已满好感。后续揭露真青梅身份后解锁正宫支援。"
                }
            };
            content.SkillNodes = {
                {
                    "magic_sword_core",
                    "",
                    false,
                    "魔剑核心",
                    "核心",
                    "剧情获得",
                    "技能树中心",
                    "序章后由真青梅赠与",
                    "魔剑会自动吸收靠近的材料，是主角后续成长和双修技能的承载物。",
                    0.50f,
                    0.50f,
                    false,
                    0
                }
            };
            content.EquipmentSlots = {
                { "weapon", "副武器" },
                { "armor", "防具" },
                { "ring", "戒指" },
                { "charm", "护符" },
                { "boots", "足部" },
                { "special", "特殊" }
            };
            content.Equipment = {
                {
                    "traveler_armor",
                    "旅人护衣",
                    "防具",
                    1,
                    "已装备",
                    "生命 +0 / 防御 +0；+1 后 生命 +30 / 防御 +2",
                    "第二章剧情装备",
                    "前期容错装。",
                    "armor",
                    AssetAliasRegistry::Path("progression.equipment.traveler_armor")
                }
            };
            return content;
        }

        static void LoadDefaults(const YAML::Node& defaults, Content* content)
        {
            if (!defaults || !content)
                return;

            content->MainDungeonId = ReadString(defaults["mainDungeon"], content->MainDungeonId);
            content->MaterialDungeonId = ReadString(defaults["materialDungeon"], content->MaterialDungeonId);
            content->DefaultObjective = ReadString(defaults["objective"], content->DefaultObjective);
            content->DefaultLastResultMessage = ReadString(defaults["lastResultMessage"], content->DefaultLastResultMessage);
            content->InitialUnlockedSkills = ReadStringList(defaults["initialUnlockedSkills"]);
            content->InitialOwnedEquipment = ReadStringList(defaults["initialOwnedEquipment"]);
            content->InitialEquippedItemsBySlot = ReadStringMap(defaults["initialEquippedItems"]);
            content->InitialSelectedEquipmentId = ReadString(defaults["initialSelectedEquipment"], content->InitialSelectedEquipmentId);
            content->TravelerArmorUpgradeEquipmentId = ReadString(defaults["travelerArmorUpgradeEquipment"], content->TravelerArmorUpgradeEquipmentId);
            content->InitialUnlockedDungeons = ReadStringList(defaults["initialUnlockedDungeons"]);
            content->InitialStoryFlags = ReadStringList(defaults["initialStoryFlags"]);
        }

        static void LoadMaterials(const YAML::Node& node, Content* content)
        {
            if (!node || !node.IsSequence() || !content)
                return;

            content->Materials.clear();
            content->Materials.reserve(node.size());
            for (const auto& item : node)
            {
                GameProgress::MaterialCost material;
                material.ItemId = ReadString(item["id"], ReadString(item["item"]));
                material.DisplayName = ReadString(item["name"]);
                if (!material.ItemId.empty())
                    content->Materials.push_back(std::move(material));
            }
        }

        static void LoadDungeons(const YAML::Node& node, Content* content)
        {
            if (!node || !node.IsSequence() || !content)
                return;

            content->Dungeons.clear();
            content->Dungeons.reserve(node.size());
            for (const auto& item : node)
            {
                DungeonDefinition dungeon;
                dungeon.Id = ReadString(item["id"]);
                dungeon.Name = ReadString(item["name"], dungeon.Id);
                dungeon.Category = ReadString(item["category"]);
                dungeon.RecommendedLevel = item["recommendedLevel"].as<int>(dungeon.RecommendedLevel);
                dungeon.StatusWhenLocked = ReadString(item["statusWhenLocked"]);
                dungeon.StatusWhenUnlocked = ReadString(item["statusWhenUnlocked"]);
                dungeon.FirstClearRewardText = ReadString(item["firstClearRewardText"]);
                dungeon.RepeatRewardText = ReadString(item["repeatRewardText"]);
                dungeon.UnlocksOnFirstClear = ReadStringList(item["unlocksOnFirstClear"]);
                dungeon.FlagsOnClear = ReadStringList(item["flagsOnClear"]);
                dungeon.ObjectiveOnClear = ReadString(item["objectiveOnClear"]);
                dungeon.FirstClearNotification = ReadString(item["firstClearNotification"]);
                if (!dungeon.Id.empty())
                    content->Dungeons.push_back(std::move(dungeon));
            }
        }

        static void LoadRelationships(const YAML::Node& node, Content* content)
        {
            if (!node || !node.IsSequence() || !content)
                return;

            content->Relationships.clear();
            content->Relationships.reserve(node.size());
            for (const auto& item : node)
            {
                GameProgress::RelationshipRecord relationship;
                relationship.CharacterId = ReadString(item["id"]);
                relationship.DisplayName = ReadString(item["name"]);
                relationship.Affinity = item["affinity"].as<int>(relationship.Affinity);
                relationship.SupportLevel = item["supportLevel"].as<int>(relationship.SupportLevel);
                relationship.Unlocked = item["unlocked"].as<bool>(relationship.Unlocked);
                relationship.Role = ReadString(item["role"]);
                relationship.NextMilestone = ReadString(item["nextMilestone"]);
                if (!relationship.CharacterId.empty())
                    content->Relationships.push_back(std::move(relationship));
            }
        }

        static bool ReadSkillNodePosition(const YAML::Node& item, float& x, float& y)
        {
            const YAML::Node position = item["position"];
            if (!position)
                return false;

            try
            {
                if (position.IsSequence() && position.size() >= 2)
                {
                    x = std::clamp(position[0].as<float>(x), 0.0f, 1.0f);
                    y = std::clamp(position[1].as<float>(y), 0.0f, 1.0f);
                    return true;
                }

                if (position.IsMap())
                {
                    x = std::clamp(position["x"].as<float>(x), 0.0f, 1.0f);
                    y = std::clamp(position["y"].as<float>(y), 0.0f, 1.0f);
                    return true;
                }
            }
            catch (...)
            {
                return false;
            }

            return false;
        }

        static void LoadSkillNodes(const YAML::Node& node, Content* content)
        {
            if (!node || !node.IsSequence() || !content)
                return;

            content->SkillNodes.clear();
            content->SkillNodes.reserve(node.size());
            for (const auto& item : node)
            {
                SkillNodeDefinition skill;
                skill.Id = ReadString(item["id"]);
                const YAML::Node parentId = item["parentId"];
                skill.HasParentId = parentId.IsDefined();
                skill.ParentId = ReadString(parentId);
                skill.Name = ReadString(item["name"], skill.Id);
                skill.Branch = ReadString(item["branch"]);
                skill.Input = ReadString(item["input"]);
                skill.ComboRole = ReadString(item["comboRole"]);
                skill.Requirement = ReadString(item["requirement"]);
                skill.Description = ReadString(item["description"]);
                skill.HasPosition = ReadSkillNodePosition(item, skill.PositionX, skill.PositionY);
                skill.UnlockChapter = item["unlockChapter"].as<int>(skill.UnlockChapter);
                if (!skill.Id.empty())
                    content->SkillNodes.push_back(std::move(skill));
            }
        }

        static void LoadEquipmentSlots(const YAML::Node& node, Content* content)
        {
            if (!node || !node.IsSequence() || !content)
                return;

            content->EquipmentSlots.clear();
            content->EquipmentSlots.reserve(node.size());
            for (const auto& item : node)
            {
                EquipmentSlotDefinition slot;
                slot.Id = ReadString(item["id"]);
                slot.Name = ReadString(item["name"], slot.Id);
                if (!slot.Id.empty())
                    content->EquipmentSlots.push_back(std::move(slot));
            }
        }

        static void LoadEquipment(const YAML::Node& node, Content* content)
        {
            if (!node || !node.IsSequence() || !content)
                return;

            content->Equipment.clear();
            content->Equipment.reserve(node.size());
            for (const auto& item : node)
            {
                EquipmentDefinition equipment;
                equipment.Id = ReadString(item["id"]);
                equipment.Name = ReadString(item["name"], equipment.Id);
                equipment.Slot = ReadString(item["slot"]);
                equipment.Page = item["page"].as<int>(equipment.Page);
                equipment.Status = ReadString(item["status"]);
                equipment.Stats = ReadString(item["stats"]);
                equipment.Source = ReadString(item["source"]);
                equipment.Description = ReadString(item["description"]);
                equipment.SlotId = ReadString(item["slotId"]);
                equipment.IconPath = AssetAliasRegistry::Resolve(ReadString(item["icon"]));
                if (!equipment.Id.empty())
                    content->Equipment.push_back(std::move(equipment));
            }
        }

        static void RestoreEmptySections(Content* content, const Content& fallback)
        {
            if (!content)
                return;

            if (content->MainDungeonId.empty())
                content->MainDungeonId = fallback.MainDungeonId;
            if (content->MaterialDungeonId.empty())
                content->MaterialDungeonId = fallback.MaterialDungeonId;
            if (content->DefaultObjective.empty())
                content->DefaultObjective = fallback.DefaultObjective;
            if (content->DefaultLastResultMessage.empty())
                content->DefaultLastResultMessage = fallback.DefaultLastResultMessage;
            if (content->InitialUnlockedSkills.empty())
                content->InitialUnlockedSkills = fallback.InitialUnlockedSkills;
            if (content->InitialOwnedEquipment.empty())
                content->InitialOwnedEquipment = fallback.InitialOwnedEquipment;
            if (content->InitialEquippedItemsBySlot.empty())
                content->InitialEquippedItemsBySlot = fallback.InitialEquippedItemsBySlot;
            if (content->InitialSelectedEquipmentId.empty())
                content->InitialSelectedEquipmentId = fallback.InitialSelectedEquipmentId;
            if (content->TravelerArmorUpgradeEquipmentId.empty())
                content->TravelerArmorUpgradeEquipmentId = fallback.TravelerArmorUpgradeEquipmentId;
            if (content->InitialUnlockedDungeons.empty())
                content->InitialUnlockedDungeons = fallback.InitialUnlockedDungeons;
            if (content->InitialStoryFlags.empty())
                content->InitialStoryFlags = fallback.InitialStoryFlags;
            if (content->Materials.empty())
                content->Materials = fallback.Materials;
            if (content->MagicSwordLv2.Costs.empty())
                content->MagicSwordLv2 = fallback.MagicSwordLv2;
            if (content->TravelerArmorLv1.Costs.empty())
                content->TravelerArmorLv1 = fallback.TravelerArmorLv1;
            if (content->Upgrades.empty())
                content->Upgrades = fallback.Upgrades;
            if (content->Dungeons.empty())
                content->Dungeons = fallback.Dungeons;
            if (content->DungeonRewardSummary.empty())
                content->DungeonRewardSummary = fallback.DungeonRewardSummary;
            if (content->Relationships.empty())
                content->Relationships = fallback.Relationships;
            if (content->SkillNodes.empty())
                content->SkillNodes = fallback.SkillNodes;
            if (content->EquipmentSlots.empty())
                content->EquipmentSlots = fallback.EquipmentSlots;
            if (content->Equipment.empty())
                content->Equipment = fallback.Equipment;
        }

        static void LoadContentSections(const YAML::Node& root, Content* content)
        {
            if (!content)
                return;

            LoadDefaults(root["defaults"], content);
            LoadMaterials(root["materials"], content);
            if (const YAML::Node upgrades = root["upgrades"])
            {
                // The upgrades: table is the single source of truth; iterate
                // the whole map so new recipes are picked up without code.
                content->Upgrades.clear();
                if (upgrades.IsMap())
                {
                    for (const auto& entry : upgrades)
                    {
                        if (!entry.first.IsScalar())
                            continue;

                        UpgradeDefinition upgrade = ReadUpgrade(entry.second);
                        upgrade.Id = entry.first.as<std::string>();
                        content->Upgrades.push_back(std::move(upgrade));
                    }
                }

                // Keep the two legacy named fields in sync so the existing
                // skill-tree / armor flows keep working unchanged.
                if (const UpgradeDefinition* magicSword = FindUpgradeIn(content->Upgrades, "magicSwordLv2"))
                    content->MagicSwordLv2 = *magicSword;
                if (const UpgradeDefinition* armor = FindUpgradeIn(content->Upgrades, "travelerArmorLv1"))
                    content->TravelerArmorLv1 = *armor;
            }
            LoadDungeons(root["dungeons"], content);
            if (const YAML::Node rewardSummary = root["dungeonRewardSummary"])
                content->DungeonRewardSummary = ReadStringList(rewardSummary);
            LoadRelationships(root["relationships"], content);
            LoadSkillNodes(root["skillNodes"], content);
            LoadEquipmentSlots(root["equipmentSlots"], content);
            LoadEquipment(root["equipment"], content);
        }

        static Content LoadContent()
        {
            const Content fallback = BuildFallbackContent();
            Content content = fallback;
            const std::filesystem::path path = AssetPath::ResolveRuntimeData(kContentPath);
            if (!std::filesystem::is_regular_file(path))
            {
                WT_CORE_WARN("ProgressionContent: content file not found '{}'", path.string());
                return content;
            }

            try
            {
                const YAML::Node root = YAML::LoadFile(path.string());
                const YAML::Node files = root["files"];
                if (files && files.IsMap())
                {
                    for (const auto& entry : files)
                    {
                        if (!entry.first.IsScalar() || !entry.second.IsScalar())
                            continue;

                        const std::string sectionReference = entry.second.as<std::string>();
                        const std::filesystem::path sectionPath = IsProjectAssetReference(sectionReference)
                            ? AssetPath::ResolveRuntimeData(sectionReference)
                            : path.parent_path() / sectionReference;
                        if (!std::filesystem::is_regular_file(sectionPath))
                        {
                            WT_CORE_WARN("ProgressionContent: section file not found '{}'", sectionPath.string());
                            continue;
                        }

                        LoadContentSections(YAML::LoadFile(sectionPath.string()), &content);
                    }
                }
                else
                {
                    LoadContentSections(root, &content);
                }
            }
            catch (const std::exception& exception)
            {
                WT_CORE_WARN("ProgressionContent: failed to load '{}': {}", path.string(), exception.what());
                return fallback;
            }

            RestoreEmptySections(&content, fallback);
            return content;
        }

        static Content& Storage()
        {
            static Content content = LoadContent();
            return content;
        }

    } // namespace

    const Content& Get()
    {
        return Storage();
    }

    void Reload()
    {
        Storage() = LoadContent();
    }

    std::string MaterialName(const std::string& itemId)
    {
        const auto& content = Get();
        for (const auto& material : content.Materials)
        {
            if (material.ItemId == itemId)
                return material.DisplayName;
        }
        return "未知材料";
    }

    std::string SlotDisplayName(const std::string& slotId)
    {
        const auto& content = Get();
        for (const auto& slot : content.EquipmentSlots)
        {
            if (slot.Id == slotId)
                return slot.Name;
        }
        return "空槽";
    }

    const DungeonDefinition* FindDungeon(const std::string& dungeonId)
    {
        const auto& content = Get();
        for (const auto& dungeon : content.Dungeons)
        {
            if (dungeon.Id == dungeonId)
                return &dungeon;
        }
        return nullptr;
    }

    const SkillNodeDefinition* FindSkillNode(const std::string& nodeId)
    {
        const auto& content = Get();
        for (const auto& node : content.SkillNodes)
        {
            if (node.Id == nodeId)
                return &node;
        }
        return nullptr;
    }

    const EquipmentDefinition* FindEquipment(const std::string& equipmentId)
    {
        const auto& content = Get();
        for (const auto& item : content.Equipment)
        {
            if (item.Id == equipmentId)
                return &item;
        }
        return nullptr;
    }

    const GameProgress::RelationshipRecord* FindRelationship(const std::string& characterId)
    {
        const auto& content = Get();
        for (const auto& record : content.Relationships)
        {
            if (record.CharacterId == characterId)
                return &record;
        }
        return nullptr;
    }

    const UpgradeDefinition* FindUpgrade(const std::string& upgradeId)
    {
        const auto& content = Get();
        for (const auto& upgrade : content.Upgrades)
        {
            if (upgrade.Id == upgradeId)
                return &upgrade;
        }
        return nullptr;
    }

    const UpgradeDefinition* FindUpgradeForEquipment(const std::string& equipmentId)
    {
        if (equipmentId.empty())
            return nullptr;
        const auto& content = Get();
        for (const auto& upgrade : content.Upgrades)
        {
            if (!upgrade.EquipmentId.empty() && upgrade.EquipmentId == equipmentId)
                return &upgrade;
        }
        return nullptr;
    }

} // namespace Wheatear::ProgressionContent
