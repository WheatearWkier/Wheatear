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
        std::unordered_set<std::string> StoryFlags;
        std::vector<std::string> Notifications;

        std::string LastResultMessage;
    };

    WHEATEAR_API State& GetState();
    WHEATEAR_API void ResetForNewGame();

    WHEATEAR_API void AddMaterial(const std::string& itemId, const std::string& displayName, int amount);
    WHEATEAR_API int GetMaterialAmount(const std::string& itemId);
    WHEATEAR_API bool HasMaterials(const std::vector<MaterialCost>& costs);
    WHEATEAR_API bool SpendMaterials(const std::vector<MaterialCost>& costs);

    WHEATEAR_API void AddExperience(int amount);
    WHEATEAR_API bool RecordDungeonClear(const std::string& dungeonId, int bestCombo, int firstClearExperience, int repeatExperience);
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

} // namespace Wheatear::GameProgress
