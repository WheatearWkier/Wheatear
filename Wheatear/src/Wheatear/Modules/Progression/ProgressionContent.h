#pragma once

#include "GameProgress.h"
#include "Wheatear/Core/Core.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::ProgressionContent {

    struct AttributeBonus
    {
        int HP = 0;
        int ATK = 0;
        int DEF = 0;
        int MATK = 0;
        int MDEF = 0;
    };

    struct UpgradeDefinition
    {
        std::vector<GameProgress::MaterialCost> Costs;
        AttributeBonus Bonus;
        std::vector<std::string> UnlockSkills;
    };

    struct DungeonDefinition
    {
        std::string Id;
        std::string Name;
        std::string Category;
        int RecommendedLevel = 1;
        std::string StatusWhenLocked;
        std::string StatusWhenUnlocked;
        std::string FirstClearRewardText;
        std::string RepeatRewardText;
        std::vector<std::string> UnlocksOnFirstClear;
        std::vector<std::string> FlagsOnClear;
        std::string ObjectiveOnClear;
        std::string FirstClearNotification;
    };

    struct SkillNodeDefinition
    {
        std::string Id;
        std::string ParentId;
        bool HasParentId = false;
        std::string Name;
        std::string Branch;
        std::string Input;
        std::string ComboRole;
        std::string Requirement;
        std::string Description;
        float PositionX = 0.5f;
        float PositionY = 0.5f;
        bool HasPosition = false;
        int UnlockChapter = 1;
    };

    struct EquipmentDefinition
    {
        std::string Id;
        std::string Name;
        std::string Slot;
        int Page = 1;
        std::string Status;
        std::string Stats;
        std::string Source;
        std::string Description;
        std::string SlotId;
        std::string IconPath;
    };

    struct EquipmentSlotDefinition
    {
        std::string Id;
        std::string Name;
    };

    struct Content
    {
        std::string MainDungeonId = "CH02_MAIN_BearAwakening";
        std::string MaterialDungeonId = "CH02_MAT_BeastPath";
        std::string DefaultObjective;
        std::string DefaultLastResultMessage;
        std::vector<std::string> InitialUnlockedSkills;
        std::vector<std::string> InitialOwnedEquipment;
        std::unordered_map<std::string, std::string> InitialEquippedItemsBySlot;
        std::string InitialSelectedEquipmentId;
        std::string TravelerArmorUpgradeEquipmentId;
        std::vector<std::string> InitialUnlockedDungeons;
        std::vector<std::string> InitialStoryFlags;
        std::vector<GameProgress::MaterialCost> Materials;
        UpgradeDefinition MagicSwordLv2;
        UpgradeDefinition TravelerArmorLv1;
        std::vector<DungeonDefinition> Dungeons;
        std::vector<std::string> DungeonRewardSummary;
        std::vector<GameProgress::RelationshipRecord> Relationships;
        std::vector<SkillNodeDefinition> SkillNodes;
        std::vector<EquipmentSlotDefinition> EquipmentSlots;
        std::vector<EquipmentDefinition> Equipment;
    };

    WHEATEAR_API const Content& Get();
    WHEATEAR_API void Reload();
    WHEATEAR_API std::string MaterialName(const std::string& itemId);
    WHEATEAR_API std::string SlotDisplayName(const std::string& slotId);
    WHEATEAR_API const DungeonDefinition* FindDungeon(const std::string& dungeonId);
    WHEATEAR_API const SkillNodeDefinition* FindSkillNode(const std::string& nodeId);
    WHEATEAR_API const EquipmentDefinition* FindEquipment(const std::string& equipmentId);

} // namespace Wheatear::ProgressionContent
