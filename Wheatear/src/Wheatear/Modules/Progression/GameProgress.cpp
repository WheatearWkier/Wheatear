#include "wtpch.h"
#include "GameProgress.h"

#include <algorithm>
#include <sstream>

namespace Wheatear::GameProgress {

    namespace {

        static constexpr const char* MainDungeonId = "CH02_MAIN_BearAwakening";
        static constexpr const char* BeastPathDungeonId = "CH02_MAT_BeastPath";

        static std::string PayloadAfter(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0 ? value.substr(prefix.size()) : std::string{};
        }

        static int ExperienceForNextLevel(int level)
        {
            return 100 + std::max(0, level - 1) * 55;
        }

        static const char* DefaultMaterialName(const std::string& itemId)
        {
            if (itemId == "MAT-MAGIC-CORE-T0")
                return "魔核碎片";
            if (itemId == "MAT-BEAST-SINEW")
                return "兽筋";
            if (itemId == "MAT-BEAST-CLAW")
                return "熊爪";
            return "未知材料";
        }

        static std::vector<MaterialCost> MagicSwordLv2Cost()
        {
            return {
                { "MAT-MAGIC-CORE-T0", "魔核碎片", 1 },
                { "MAT-BEAST-SINEW", "兽筋", 2 },
                { "MAT-BEAST-CLAW", "熊爪", 1 }
            };
        }

        static std::vector<MaterialCost> TravelerArmorLv1Cost()
        {
            return {
                { "MAT-BEAST-SINEW", "兽筋", 1 },
                { "MAT-BEAST-CLAW", "熊爪", 1 }
            };
        }

        static void PushNotification(State& state, const std::string& message)
        {
            if (message.empty())
                return;

            state.Notifications.push_back(message);
            if (state.Notifications.size() > 5)
                state.Notifications.erase(state.Notifications.begin());
        }

        static State MakeDefaultState()
        {
            State state;
            state.Objective = "整理黑熊掉落的材料，确认魔剑和装备的强化方向。";
            state.ExperienceToNext = ExperienceForNextLevel(state.PlayerLevel);
            state.UnlockedSkills.insert("basic_attack");
            state.UnlockedSkills.insert("air_basic");
            state.UnlockedSkills.insert("launcher");
            state.UnlockedSkills.insert("air_chase");
            state.UnlockedSkills.insert("magic_bolt");
            state.UnlockedSkills.insert("ally_support");
            state.StoryFlags.insert("FLAG_CH02_SIDE_COMBAT_STARTED");
            state.LastResultMessage = "据点已开启。完成黑熊战后，掉落会写入这里。";
            return state;
        }

        static std::string BuildCostText(const std::vector<MaterialCost>& costs)
        {
            std::ostringstream stream;
            for (size_t i = 0; i < costs.size(); ++i)
            {
                if (i > 0)
                    stream << " / ";
                stream << costs[i].DisplayName << " " << GetMaterialAmount(costs[i].ItemId) << "/" << costs[i].Amount;
            }
            return stream.str();
        }

    } // namespace

    State& GetState()
    {
        static State state = MakeDefaultState();
        return state;
    }

    void ResetForNewGame()
    {
        GetState() = MakeDefaultState();
    }

    void AddMaterial(const std::string& itemId, const std::string& displayName, int amount)
    {
        if (itemId.empty() || amount <= 0)
            return;

        State& state = GetState();
        state.Materials[itemId] += amount;
        state.MaterialNames[itemId] = displayName.empty() ? DefaultMaterialName(itemId) : displayName;

        std::ostringstream stream;
        stream << "获得 " << DefaultMaterialName(itemId) << " x" << amount;
        PushNotification(state, stream.str());
        state.LastResultMessage = stream.str();
    }

    int GetMaterialAmount(const std::string& itemId)
    {
        const State& state = GetState();
        if (auto it = state.Materials.find(itemId); it != state.Materials.end())
            return it->second;
        return 0;
    }

    bool HasMaterials(const std::vector<MaterialCost>& costs)
    {
        for (const MaterialCost& cost : costs)
        {
            if (cost.Amount > 0 && GetMaterialAmount(cost.ItemId) < cost.Amount)
                return false;
        }
        return true;
    }

    bool SpendMaterials(const std::vector<MaterialCost>& costs)
    {
        if (!HasMaterials(costs))
            return false;

        State& state = GetState();
        for (const MaterialCost& cost : costs)
        {
            if (cost.Amount > 0)
                state.Materials[cost.ItemId] -= cost.Amount;
        }
        return true;
    }

    void AddExperience(int amount)
    {
        if (amount <= 0)
            return;

        State& state = GetState();
        state.Experience += amount;

        int levelUps = 0;
        while (state.Experience >= state.ExperienceToNext)
        {
            state.Experience -= state.ExperienceToNext;
            ++state.PlayerLevel;
            ++levelUps;
            state.ExperienceToNext = ExperienceForNextLevel(state.PlayerLevel);
            state.Attributes.HP += 18;
            state.Attributes.ATK += 2;
            state.Attributes.DEF += 1;
            state.Attributes.MATK += 2;
            state.Attributes.MDEF += 1;
        }

        std::ostringstream stream;
        stream << "获得经验 " << amount;
        if (levelUps > 0)
            stream << "，主角升到 Lv" << state.PlayerLevel;
        PushNotification(state, stream.str());
        state.LastResultMessage = stream.str();
    }

    bool RecordDungeonClear(const std::string& dungeonId, int bestCombo, int firstClearExperience, int repeatExperience)
    {
        if (dungeonId.empty())
            return false;

        State& state = GetState();
        const bool firstClear = state.CompletedDungeons.insert(dungeonId).second;
        state.BestCombosByDungeon[dungeonId] = std::max(state.BestCombosByDungeon[dungeonId], bestCombo);

        if (dungeonId == MainDungeonId)
        {
            state.UnlockedDungeons.insert(BeastPathDungeonId);
            state.StoryFlags.insert("FLAG_CH02_BOSS_DEFEATED");
            state.StoryFlags.insert("FLAG_HUB_UNLOCKED");
            state.Objective = "在据点强化魔剑，重刷黑林兽道练习空连，或继续前往边境村。";
            if (firstClear)
                PushNotification(state, "新副本解锁：黑林兽道");
        }

        std::ostringstream stream;
        stream << (firstClear ? "首通 " : "再战 ") << dungeonId
               << "，最佳连击 x" << bestCombo;
        PushNotification(state, stream.str());
        state.LastResultMessage = stream.str();

        AddExperience(firstClear ? firstClearExperience : repeatExperience);
        return firstClear;
    }

    bool IsDungeonUnlocked(const std::string& dungeonId)
    {
        const State& state = GetState();
        return state.UnlockedDungeons.find(dungeonId) != state.UnlockedDungeons.end();
    }

    bool IsSkillUnlocked(const std::string& skillId)
    {
        const State& state = GetState();
        return state.UnlockedSkills.find(skillId) != state.UnlockedSkills.end();
    }

    bool CanUpgradeMagicSwordToLv2()
    {
        return GetState().MagicSwordLevel < 2 && HasMaterials(MagicSwordLv2Cost());
    }

    bool TryUpgradeMagicSwordToLv2()
    {
        State& state = GetState();
        if (state.MagicSwordLevel >= 2)
        {
            state.LastResultMessage = "魔剑 Lv2 已经觉醒。";
            return false;
        }

        const std::vector<MaterialCost> costs = MagicSwordLv2Cost();
        if (!SpendMaterials(costs))
        {
            state.LastResultMessage = "魔剑 Lv2 材料不足：" + BuildCostText(costs);
            return false;
        }

        state.MagicSwordLevel = 2;
        state.Attributes.ATK += 3;
        state.Attributes.MATK += 3;
        state.UnlockedSkills.insert("magic_sword_lv2");
        state.UnlockedSkills.insert("basic_slash_boost");
        state.UnlockedSkills.insert("air_chain_training");
        state.Objective = "魔剑已经回应你。可以重刷练习空连，也可以继续追查假青梅的去向。";
        state.LastResultMessage = "魔剑 Lv2 觉醒：基础斩击、跳斩和火球衔接更稳定。";
        PushNotification(state, "魔剑 Lv2 已觉醒");
        return true;
    }

    bool CanUpgradeTravelerArmorToLv1()
    {
        return GetState().TravelerArmorLevel < 1 && HasMaterials(TravelerArmorLv1Cost());
    }

    bool TryUpgradeTravelerArmorToLv1()
    {
        State& state = GetState();
        if (state.TravelerArmorLevel >= 1)
        {
            state.LastResultMessage = "旅人护衣已经强化到 +1。";
            return false;
        }

        const std::vector<MaterialCost> costs = TravelerArmorLv1Cost();
        if (!SpendMaterials(costs))
        {
            state.LastResultMessage = "旅人护衣 +1 材料不足：" + BuildCostText(costs);
            return false;
        }

        state.TravelerArmorLevel = 1;
        state.Attributes.HP += 30;
        state.Attributes.DEF += 2;
        state.LastResultMessage = "旅人护衣 +1：生命和防御提高，低空连击失误更不容易暴毙。";
        PushNotification(state, "旅人护衣 +1 完成");
        return true;
    }

    CommandResult ExecuteCommand(const std::string& command)
    {
        CommandResult result;
        const std::string action = PayloadAfter(command, "progression:");
        if (action.empty())
            return result;

        result.Handled = true;
        if (action == "upgrade_magic_sword")
        {
            result.Changed = TryUpgradeMagicSwordToLv2();
            result.Success = result.Changed || GetState().MagicSwordLevel >= 2;
        }
        else if (action == "upgrade_traveler_armor")
        {
            result.Changed = TryUpgradeTravelerArmorToLv1();
            result.Success = result.Changed || GetState().TravelerArmorLevel >= 1;
        }
        else if (action == "reset")
        {
            ResetForNewGame();
            result.Changed = true;
            result.Success = true;
        }

        result.Message = GetState().LastResultMessage;
        return result;
    }

    std::string BuildHubSubtitle()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "第" << state.CurrentChapter << "章  /  魔剑 Lv" << state.MagicSwordLevel
               << "  /  主角 Lv" << state.PlayerLevel
               << "  EXP " << state.Experience << "/" << state.ExperienceToNext
               << "  /  " << (IsDungeonUnlocked(BeastPathDungeonId) ? "黑林兽道已解锁" : "黑林兽道未解锁");
        return stream.str();
    }

    std::string BuildHubStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "目标: " << state.Objective << "\n";
        stream << "材料: 魔核碎片 x" << GetMaterialAmount("MAT-MAGIC-CORE-T0")
               << " / 兽筋 x" << GetMaterialAmount("MAT-BEAST-SINEW")
               << " / 熊爪 x" << GetMaterialAmount("MAT-BEAST-CLAW") << "\n";
        stream << "能力: HP " << state.Attributes.HP
               << " / ATK " << state.Attributes.ATK
               << " / DEF " << state.Attributes.DEF
               << " / MATK " << state.Attributes.MATK << "\n";

        if (state.MagicSwordLevel < 2)
            stream << "魔剑 Lv2: " << (CanUpgradeMagicSwordToLv2() ? "可升级" : BuildCostText(MagicSwordLv2Cost()));
        else
            stream << "已解锁: 魔剑 Lv2 / 基础斩击强化 / 空中连击训练";

        if (!state.LastResultMessage.empty())
            stream << "\n" << state.LastResultMessage;

        return stream.str();
    }

    std::string GetDungeonButtonText()
    {
        return IsDungeonUnlocked(BeastPathDungeonId) ? "重刷黑林兽道" : "黑林兽道未解锁";
    }

    std::string GetSkillButtonText()
    {
        const State& state = GetState();
        if (state.MagicSwordLevel >= 2)
            return "魔剑 Lv2 已觉醒";
        return CanUpgradeMagicSwordToLv2() ? "升级魔剑 Lv2" : "魔剑材料不足";
    }

    std::string GetEquipmentButtonText()
    {
        const State& state = GetState();
        if (state.TravelerArmorLevel >= 1)
            return "旅人护衣 +1";
        return CanUpgradeTravelerArmorToLv1() ? "强化护衣 +1" : "装备材料不足";
    }

} // namespace Wheatear::GameProgress
