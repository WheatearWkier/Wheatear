#pragma once

#include "Wheatear/Core/Core.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear::GameProgress {

    struct PlayerAttributes
    {
        int HP = 420;
        int ATK = 24;
        int DEF = 10;
        int MATK = 18;
        int MDEF = 8;
    };

    struct RelationshipRecord
    {
        std::string CharacterId;
        std::string DisplayName;
        int Affinity = 0;
        int SupportLevel = 0;
        bool Unlocked = false;
        std::string Role;
        std::string NextMilestone;
    };

    struct PlayerSettings
    {
        int TextSpeed = 48;
        int MasterVolume = 50;
        int BGMVolume = 50;
        int SFXVolume = 50;
        bool Fullscreen = false;
        bool ScreenShake = true;
    };

    struct MaterialCost
    {
        std::string ItemId;
        std::string DisplayName;
        int Amount = 0;
    };

    struct CommandResult
    {
        bool Handled = false;
        bool Success = false;
        bool Changed = false;
        std::string Message;
    };

    struct DungeonResult
    {
        bool Valid = false;
        std::string DungeonId;
        std::string DungeonName;
        std::string Grade = "C";
        bool FirstClear = false;
        int BestCombo = 0;
        int HitsTaken = 0;
        int Experience = 0;
        float ClearTimeSeconds = 0.0f;
        std::string RewardSummary;
    };

    struct State
    {
        int CurrentChapter = 2;
        std::string Objective;

        int PlayerLevel = 1;
        int Experience = 0;
        int ExperienceToNext = 100;
        int MagicSwordLevel = 1;
        int TravelerArmorLevel = 0;
        int Gold = 0;
        PlayerAttributes Attributes;

        std::unordered_map<std::string, int> Materials;
        std::unordered_map<std::string, std::string> MaterialNames;
        std::unordered_map<std::string, int> BestCombosByDungeon;
        std::unordered_set<std::string> CompletedDungeons;
        std::unordered_set<std::string> UnlockedDungeons;
        std::unordered_set<std::string> UnlockedSkills;
        std::unordered_set<std::string> OwnedEquipment;
        std::unordered_map<std::string, std::string> EquippedItemsBySlot;
        std::unordered_set<std::string> StoryFlags;
        std::vector<std::string> Notifications;
        std::vector<RelationshipRecord> Relationships;
        PlayerSettings Settings;
        std::string ActiveSupportCharacterId = "mentor";
        std::string SelectedSkillNodeId = "magic_sword_core";
        std::string SelectedEquipmentId = "traveler_armor";
        int EquipmentPage = 1;
        float SkillTreePanX = 0.0f;
        float SkillTreePanY = 0.0f;

        DungeonResult LastDungeonResult;
        std::string LastResultMessage;
    };

    WHEATEAR_API State& GetState();
    WHEATEAR_API void ResetForNewGame();
    WHEATEAR_API bool SaveSlot(int slot);
    WHEATEAR_API bool LoadSlot(int slot);

    WHEATEAR_API void AddMaterial(const std::string& itemId, const std::string& displayName, int amount);
    WHEATEAR_API int GetMaterialAmount(const std::string& itemId);
    WHEATEAR_API bool HasMaterials(const std::vector<MaterialCost>& costs);
    WHEATEAR_API bool SpendMaterials(const std::vector<MaterialCost>& costs);

    WHEATEAR_API void AddExperience(int amount);
    WHEATEAR_API bool RecordDungeonClear(const std::string& dungeonId, int bestCombo, int firstClearExperience, int repeatExperience);
    WHEATEAR_API void RecordLastDungeonResult(const std::string& dungeonId,
        const std::string& grade,
        bool firstClear,
        int bestCombo,
        int hitsTaken,
        float clearTimeSeconds,
        int experience,
        const std::string& rewardSummary);
    WHEATEAR_API bool IsDungeonUnlocked(const std::string& dungeonId);
    WHEATEAR_API bool IsSkillUnlocked(const std::string& skillId);

    WHEATEAR_API bool CanUpgradeMagicSwordToLv2();
    WHEATEAR_API bool TryUpgradeMagicSwordToLv2();
    WHEATEAR_API bool CanUpgradeTravelerArmorToLv1();
    WHEATEAR_API bool TryUpgradeTravelerArmorToLv1();

    WHEATEAR_API CommandResult ExecuteCommand(const std::string& command);

    WHEATEAR_API std::string BuildHubSubtitle();
    WHEATEAR_API std::string BuildHubStatus();
    WHEATEAR_API std::string GetDungeonButtonText();
    WHEATEAR_API std::string GetSkillButtonText();
    WHEATEAR_API std::string GetEquipmentButtonText();
    WHEATEAR_API std::string BuildResultTitle();
    WHEATEAR_API std::string BuildResultStats();
    WHEATEAR_API std::string BuildResultRewards();
    WHEATEAR_API std::string BuildSkillTreeStatus();
    WHEATEAR_API std::string BuildSkillTreeDetails();
    WHEATEAR_API std::string BuildSkillTreeMaterials();
    WHEATEAR_API std::string GetMagicSwordUpgradeButtonText();
    WHEATEAR_API std::string BuildSkillTreeStatusV2();
    WHEATEAR_API std::string BuildSkillTreeDetailsV2();
    WHEATEAR_API std::string BuildSkillTreeMaterialsV2();
    WHEATEAR_API std::string GetMagicSwordUpgradeButtonTextV2();
    WHEATEAR_API std::string BuildEquipmentStatus();
    WHEATEAR_API std::string BuildEquipmentDetails();
    WHEATEAR_API std::string BuildEquipmentTooltip(const std::string& equipmentId);
    WHEATEAR_API std::string BuildEquipmentPageText();
    WHEATEAR_API std::string BuildEquipmentMaterials();
    WHEATEAR_API std::string GetTravelerArmorUpgradeButtonText();
    WHEATEAR_API std::string GetEquipmentToggleButtonText();
    WHEATEAR_API bool IsEquipmentOwned(const std::string& equipmentId);
    WHEATEAR_API bool IsEquipmentEquipped(const std::string& equipmentId);
    WHEATEAR_API std::string GetEquipmentSlotId(const std::string& equipmentId);
    WHEATEAR_API std::string GetEquipmentSlotDisplayName(const std::string& slotId);
    WHEATEAR_API std::string GetEquipmentIconPath(const std::string& equipmentId);
    WHEATEAR_API std::string GetEquippedEquipmentForSlot(const std::string& slotId);
    WHEATEAR_API std::string BuildDungeonSelectStatus();
    WHEATEAR_API std::string BuildDungeonSelectRewards();
    WHEATEAR_API std::string BuildRelationshipStatus();
    WHEATEAR_API std::string BuildSupportStatus();
    WHEATEAR_API std::string BuildSettingsStatus();
    WHEATEAR_API std::string BuildSaveLoadStatus();
    WHEATEAR_API std::string GetSaveButtonText(int slot);
    WHEATEAR_API std::string GetLoadButtonText(int slot);

} // namespace Wheatear::GameProgress
