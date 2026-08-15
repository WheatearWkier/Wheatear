#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
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

    struct SaveSlotInfo
    {
        int Slot = 1;
        int SaveVersion = 1;
        bool Exists = false;
        int Chapter = 0;
        int PlayerLevel = 1;
        int Gold = 0;
        std::string Objective;
        std::string ScenePath;
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
        std::unordered_map<std::string, int> RewardAmounts;
    };

    struct State
    {
        int CurrentChapter = 2;
        std::string Objective;
        std::string CurrentScenePath;
        std::string PreviousScenePath;

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
        std::string ActiveSupportCharacterId = "mentor";
        std::string SelectedSkillNodeId = "magic_sword_core";
        std::string SelectedEquipmentId;
        int EquipmentPage = 1;
        float SkillTreePanX = 0.0f;
        float SkillTreePanY = 0.0f;

        std::string ActiveSideCombatDungeonId;
        DungeonResult LastDungeonResult;
        std::string LastResultMessage;
    };

    WHEATEAR_API State& GetState();
    WHEATEAR_API void ResetForNewGame();
    WHEATEAR_API void SetSceneTransitionContext(const std::filesystem::path& previousScenePath, const std::filesystem::path& currentScenePath);
    WHEATEAR_API std::string GetCurrentScenePath();
    WHEATEAR_API std::string GetScenePathForSave();
    WHEATEAR_API void ApplySettingsToRuntime();
    WHEATEAR_API int GetMaxSaveSlots();
    WHEATEAR_API std::filesystem::path GetProgressSavePath(int slot);
    WHEATEAR_API std::filesystem::path GetGameRuntimeSavePath(int slot, const std::string& saveDirectory = "assets/saves");
    WHEATEAR_API bool ClearGameRuntimeSaveSlot(int slot, const std::string& saveDirectory = "assets/saves");
    WHEATEAR_API bool IsSaveSlotOccupied(int slot);
    WHEATEAR_API bool IsGameRuntimeSaveSlotOccupied(int slot, const std::string& saveDirectory = "assets/saves");
    WHEATEAR_API bool IsGameSaveSlotOccupied(int slot, const std::string& saveDirectory = "assets/saves");
    WHEATEAR_API SaveSlotInfo GetSaveSlotInfo(int slot);
    WHEATEAR_API bool SaveSlot(int slot);
    WHEATEAR_API bool LoadSlot(int slot);

    WHEATEAR_API void AddMaterial(const std::string& itemId, const std::string& displayName, int amount);
    WHEATEAR_API int GetMaterialAmount(const std::string& itemId);
    WHEATEAR_API bool HasMaterials(const std::vector<MaterialCost>& costs);
    WHEATEAR_API bool SpendMaterials(const std::vector<MaterialCost>& costs);

    WHEATEAR_API void AddExperience(int amount);
    WHEATEAR_API void SetActiveSideCombatDungeon(const std::string& dungeonId);
    WHEATEAR_API const std::string& GetActiveSideCombatDungeonId();
    WHEATEAR_API bool RecordDungeonClear(const std::string& dungeonId, int bestCombo, int firstClearExperience, int repeatExperience);
    WHEATEAR_API void RecordLastDungeonResult(const std::string& dungeonId,
        const std::string& grade,
        bool firstClear,
        int bestCombo,
        int hitsTaken,
        float clearTimeSeconds,
        int experience,
        const std::string& rewardSummary,
        const std::unordered_map<std::string, int>& rewardAmounts);
    WHEATEAR_API bool IsDungeonUnlocked(const std::string& dungeonId);
    WHEATEAR_API bool IsSkillUnlocked(const std::string& skillId);

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
    WHEATEAR_API std::string GetSkillTreeLearnButtonText();
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
    WHEATEAR_API std::string BuildGameSaveSlotButtonText(int slot, bool saveMode, const std::string& saveDirectory = "assets/saves");
    WHEATEAR_API std::string BuildLoadGameCommand(int slot, const std::string& scenePath = "");

} // namespace Wheatear::GameProgress
