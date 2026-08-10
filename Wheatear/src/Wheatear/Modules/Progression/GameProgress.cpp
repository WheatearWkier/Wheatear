#include "wtpch.h"
#include "GameProgress.h"

#include "ProgressionContent.h"
#include "ProgressionSettingsCommandService.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/UserSettings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <vector>

namespace Wheatear::GameProgress {

    namespace {

        static constexpr int kMaxSaveSlots = 20;
        static constexpr int kCurrentSaveVersion = 3;
        static constexpr const char* kDefaultLoadScenePath = "assets/scenes/VerticalSliceIntro.wt";
        static constexpr const char* kSaveLoadScenePath = "assets/scenes/VerticalSliceSaveLoad.wt";

        static int ClampSaveSlot(int slot)
        {
            return std::clamp(slot, 1, kMaxSaveSlots);
        }

        static std::filesystem::path SavePathForSlot(int slot)
        {
            const int safeSlot = ClampSaveSlot(slot);
            return AssetPath::Resolve("assets/saves/progression_slot" + std::to_string(safeSlot) + ".wtsave");
        }

        static std::filesystem::path GameRuntimeSavePathForSlot(int slot, const std::string& saveDirectory)
        {
            const int safeSlot = ClampSaveSlot(slot);
            const std::string directory = saveDirectory.empty() ? "assets/saves" : saveDirectory;
            return AssetPath::Resolve(directory) / ("slot" + std::to_string(safeSlot) + ".vnstate");
        }

        static std::string FormatSaveSlotNumber(int slot)
        {
            const int safeSlot = ClampSaveSlot(slot);
            return safeSlot < 10
                ? "0" + std::to_string(safeSlot)
                : std::to_string(safeSlot);
        }

        static std::string PayloadAfter(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0 ? value.substr(prefix.size()) : std::string{};
        }

        static std::string ToLowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        static std::string NormalizeAssetLikePath(std::string value)
        {
            std::replace(value.begin(), value.end(), '\\', '/');
            value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
            value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());

            while (!value.empty() && (value.front() == '/' || value.front() == '.'))
            {
                if (value.rfind("./", 0) == 0)
                {
                    value.erase(0, 2);
                    continue;
                }
                if (value.front() == '/')
                {
                    value.erase(value.begin());
                    continue;
                }
                break;
            }

            const std::string marker = "assets/";
            const size_t markerPos = ToLowerCopy(value).find(marker);
            if (markerPos != std::string::npos)
                value = value.substr(markerPos);

            return value;
        }

        static std::string NormalizeScenePathString(const std::filesystem::path& path)
        {
            return NormalizeAssetLikePath(path.generic_string());
        }

        static bool IsSaveLoadScenePath(const std::string& scenePath)
        {
            return ToLowerCopy(NormalizeAssetLikePath(scenePath)) == ToLowerCopy(kSaveLoadScenePath);
        }

        static std::string UnescapeSavedField(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());
            bool escaping = false;
            for (char c : value)
            {
                if (escaping)
                {
                    switch (c)
                    {
                    case 'n': result += '\n'; break;
                    case '|': result += '|'; break;
                    case '\\': result += '\\'; break;
                    default: result += c; break;
                    }
                    escaping = false;
                    continue;
                }

                if (c == '\\')
                {
                    escaping = true;
                    continue;
                }

                result += c;
            }
            if (escaping)
                result += '\\';
            return result;
        }

        static std::string ReadVNScriptSourceForSlot(int slot, const std::string& saveDirectory = "assets/saves")
        {
            std::ifstream input(GameRuntimeSavePathForSlot(slot, saveDirectory), std::ios::binary);
            if (!input.is_open())
                return {};

            std::string line;
            while (std::getline(input, line))
            {
                if (line.rfind("SOURCE ", 0) == 0)
                    return UnescapeSavedField(line.substr(7));
            }
            return {};
        }

        static std::string ScenePathFromVNScriptPath(const std::string& scriptPath)
        {
            const std::string normalized = ToLowerCopy(NormalizeAssetLikePath(scriptPath));
            if (normalized.find("vertical_slice_chapter3_preview.vn") != std::string::npos)
                return "assets/scenes/VerticalSliceChapter3Preview.wt";
            if (normalized.find("vertical_slice_post_fake.vn") != std::string::npos)
                return "assets/scenes/VerticalSlicePostFake.wt";
            if (normalized.find("vertical_slice_intro.vn") != std::string::npos)
                return "assets/scenes/VerticalSliceIntro.wt";
            return {};
        }

        static std::string ResolveScenePathForSave()
        {
            const State& state = GetState();
            const std::string current = NormalizeAssetLikePath(state.CurrentScenePath);
            const std::string previous = NormalizeAssetLikePath(state.PreviousScenePath);

            if (IsSaveLoadScenePath(current) && !previous.empty())
                return previous;
            if (!current.empty())
                return current;
            if (!previous.empty() && !IsSaveLoadScenePath(previous))
                return previous;
            return kDefaultLoadScenePath;
        }

        static std::string ResolveLoadScenePathForSlot(int slot, const std::string& requestedScenePath)
        {
            const std::string requested = NormalizeAssetLikePath(requestedScenePath);
            if (!requested.empty())
                return requested;

            const int safeSlot = ClampSaveSlot(slot);
            const SaveSlotInfo info = GetSaveSlotInfo(safeSlot);
            if (!info.ScenePath.empty())
                return info.ScenePath;

            const std::string vnScene = ScenePathFromVNScriptPath(ReadVNScriptSourceForSlot(safeSlot));
            if (!vnScene.empty())
                return vnScene;

            const State& state = GetState();
            const std::string current = NormalizeAssetLikePath(state.CurrentScenePath);
            const std::string previous = NormalizeAssetLikePath(state.PreviousScenePath);
            if (IsSaveLoadScenePath(current) && !previous.empty())
                return previous;
            if (!current.empty() && !IsSaveLoadScenePath(current))
                return current;
            return kDefaultLoadScenePath;
        }
        static std::string JoinSet(const std::unordered_set<std::string>& values)
        {
            std::vector<std::string> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end());

            std::ostringstream stream;
            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i > 0)
                    stream << "|";
                stream << sorted[i];
            }
            return stream.str();
        }

        static void LoadSet(std::unordered_set<std::string>& values, const std::string& line)
        {
            values.clear();

            std::stringstream stream(line);
            std::string item;
            while (std::getline(stream, item, '|'))
            {
                if (!item.empty())
                    values.insert(item);
            }
        }

        static std::string JoinMap(const std::unordered_map<std::string, std::string>& values)
        {
            std::vector<std::pair<std::string, std::string>> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            std::ostringstream stream;
            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i > 0)
                    stream << "|";
                stream << sorted[i].first << ":" << sorted[i].second;
            }
            return stream.str();
        }

        static void LoadMap(std::unordered_map<std::string, std::string>& values, const std::string& line)
        {
            values.clear();

            std::stringstream stream(line);
            std::string item;
            while (std::getline(stream, item, '|'))
            {
                const size_t split = item.find(':');
                if (split == std::string::npos)
                    continue;

                const std::string key = item.substr(0, split);
                const std::string value = item.substr(split + 1);
                if (!key.empty() && !value.empty())
                    values[key] = value;
            }
        }

        static int ParseInt(const std::string& value, int fallback)
        {
            try
            {
                return std::stoi(value);
            }
            catch (...)
            {
                return fallback;
            }
        }

        static float ParseFloat(const std::string& value, float fallback)
        {
            try
            {
                return std::stof(value);
            }
            catch (...)
            {
                return fallback;
            }
        }

        static bool ParseBool(const std::string& value, bool fallback)
        {
            if (value == "1" || value == "true" || value == "True")
                return true;
            if (value == "0" || value == "false" || value == "False")
                return false;
            return fallback;
        }

        static std::optional<int> ParseTrailingSlot(const std::string& value, const std::string& prefix)
        {
            if (value.rfind(prefix, 0) != 0)
                return std::nullopt;

            const std::string payload = value.substr(prefix.size());
            if (payload.empty())
                return std::nullopt;

            for (char c : payload)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                    return std::nullopt;
            }

            return ClampSaveSlot(ParseInt(payload, 1));
        }

        static const std::string& MainDungeonId()
        {
            return ProgressionContent::Get().MainDungeonId;
        }

        static const std::string& BeastPathDungeonId()
        {
            return ProgressionContent::Get().MaterialDungeonId;
        }

        static const std::string& TravelerArmorUpgradeEquipmentId()
        {
            return ProgressionContent::Get().TravelerArmorUpgradeEquipmentId;
        }

        static std::vector<RelationshipRecord> DefaultRelationships()
        {
            return ProgressionContent::Get().Relationships;
        }

        static int ExperienceForNextLevel(int level)
        {
            return 100 + std::max(0, level - 1) * 55;
        }

        static std::string DungeonDisplayName(const std::string& dungeonId)
        {
            if (const auto* dungeon = ProgressionContent::FindDungeon(dungeonId))
                return dungeon->Name;
            return dungeonId;
        }

        static std::string FormatSeconds(float seconds)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1) << std::max(seconds, 0.0f) << "s";
            return stream.str();
        }

        static std::string DefaultMaterialName(const std::string& itemId)
        {
            return ProgressionContent::MaterialName(itemId);
        }

        static std::vector<MaterialCost> MagicSwordLv2Cost()
        {
            return ProgressionContent::Get().MagicSwordLv2.Costs;
        }

        static std::vector<MaterialCost> TravelerArmorLv1Cost()
        {
            return ProgressionContent::Get().TravelerArmorLv1.Costs;
        }

        static void ApplyAttributeBonus(State& state, const ProgressionContent::AttributeBonus& bonus)
        {
            state.Attributes.HP += bonus.HP;
            state.Attributes.ATK += bonus.ATK;
            state.Attributes.DEF += bonus.DEF;
            state.Attributes.MATK += bonus.MATK;
            state.Attributes.MDEF += bonus.MDEF;
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
            const auto& content = ProgressionContent::Get();
            State state;
            state.Objective = content.DefaultObjective;
            state.ExperienceToNext = ExperienceForNextLevel(state.PlayerLevel);
            for (const std::string& skillId : content.InitialUnlockedSkills)
                state.UnlockedSkills.insert(skillId);
            for (const std::string& equipmentId : content.InitialOwnedEquipment)
                state.OwnedEquipment.insert(equipmentId);
            state.EquippedItemsBySlot = content.InitialEquippedItemsBySlot;
            for (const std::string& dungeonId : content.InitialUnlockedDungeons)
                state.UnlockedDungeons.insert(dungeonId);
            for (const std::string& flag : content.InitialStoryFlags)
                state.StoryFlags.insert(flag);
            state.SelectedEquipmentId = content.InitialSelectedEquipmentId;
            state.Relationships = DefaultRelationships();
            state.LastResultMessage = content.DefaultLastResultMessage;
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

        static std::string BuildMaterialInventoryText()
        {
            const auto& content = ProgressionContent::Get();
            std::ostringstream stream;
            bool wroteAny = false;
            for (const MaterialCost& material : content.Materials)
            {
                if (material.ItemId.empty())
                    continue;
                if (wroteAny)
                    stream << " / ";
                stream << material.DisplayName << " x" << GetMaterialAmount(material.ItemId);
                wroteAny = true;
            }

            if (!wroteAny)
            {
                const State& state = GetState();
                std::vector<std::pair<std::string, int>> sorted(state.Materials.begin(), state.Materials.end());
                std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                for (const auto& [itemId, amount] : sorted)
                {
                    if (wroteAny)
                        stream << " / ";
                    stream << DefaultMaterialName(itemId) << " x" << amount;
                    wroteAny = true;
                }
            }
            return stream.str();
        }

        using SkillNodeDisplayInfo = ProgressionContent::SkillNodeDefinition;
        using SkillNodeInfo = ProgressionContent::SkillNodeDefinition;

        static const std::vector<SkillNodeDisplayInfo>& GetSkillNodeDisplayInfos()
        {
            return ProgressionContent::Get().SkillNodes;
        }

        static const SkillNodeDisplayInfo* LookupSkillNodeDisplayInfo(const std::string& nodeId)
        {
            const auto& nodes = GetSkillNodeDisplayInfos();
            for (const auto& node : nodes)
            {
                if (node.Id == nodeId)
                    return &node;
            }
            return nodes.empty() ? nullptr : &nodes.front();
        }

        static const SkillNodeInfo& FindSkillNode(const std::string& nodeId)
        {
            const auto& nodes = ProgressionContent::Get().SkillNodes;
            if (const auto* node = ProgressionContent::FindSkillNode(nodeId))
                return *node;
            return nodes.front();
        }

        static bool RequiresMagicSwordLv2(const SkillNodeDisplayInfo& node)
        {
            return node.Requirement.find("Lv2") != std::string::npos;
        }

        static std::string SkillNodeDisplayState(const State& state, const std::string& nodeId)
        {
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(nodeId);
            if (!node)
                return {};
            if (state.UnlockedSkills.find(nodeId) != state.UnlockedSkills.end())
                return "已习得";
            if (RequiresMagicSwordLv2(*node) && state.MagicSwordLevel < 2)
                return "需要魔剑 Lv2";
            if (node->UnlockChapter <= state.CurrentChapter)
                return "可学习";
            if (node->UnlockChapter == state.CurrentChapter + 1)
                return "下一章节开放";
            return "后续第 " + std::to_string(node->UnlockChapter) + " 章开放";
        }

        static std::string LegacySkillActionToNodeId(const std::string& action)
        {
            return ProgressionContent::ResolveLegacySkillSelection(action);
        }

        static std::string SkillNodeState(const State& state, const std::string& nodeId)
        {
            return SkillNodeDisplayState(state, nodeId);
        }

        static std::string SkillActionToNodeId(const std::string& action)
        {
            return ProgressionContent::ResolveLegacySkillSelection(action);
        }

        using EquipmentInfo = ProgressionContent::EquipmentDefinition;

        static const std::vector<EquipmentInfo>& EquipmentCatalog()
        {
            return ProgressionContent::Get().Equipment;
        }

        static const EquipmentInfo& FindEquipment(const std::string& equipmentId)
        {
            const std::vector<EquipmentInfo>& equipment = EquipmentCatalog();
            if (const auto* item = ProgressionContent::FindEquipment(equipmentId))
                return *item;
            return equipment.front();
        }

        static std::string SlotDisplayName(const std::string& slotId)
        {
            return ProgressionContent::SlotDisplayName(slotId);
        }

        static bool IsEquipmentEquippedInState(const State& state, const std::string& equipmentId)
        {
            for (const auto& [slotId, itemId] : state.EquippedItemsBySlot)
            {
                if (itemId == equipmentId)
                    return true;
            }
            return false;
        }

        static std::string FindEquippedSlotForItem(const State& state, const std::string& equipmentId)
        {
            for (const auto& [slotId, itemId] : state.EquippedItemsBySlot)
            {
                if (itemId == equipmentId)
                    return slotId;
            }
            return {};
        }

        static bool IsEquipmentOwnedInState(const State& state, const std::string& equipmentId)
        {
            return state.OwnedEquipment.find(equipmentId) != state.OwnedEquipment.end();
        }

        static std::vector<std::string> BuildVisibleBagEquipment(const State& state)
        {
            std::vector<std::string> result;
            const std::vector<EquipmentInfo>& equipment = EquipmentCatalog();
            result.reserve(equipment.size());
            for (const EquipmentInfo& item : equipment)
            {
                if (IsEquipmentOwnedInState(state, item.Id)
                    && !IsEquipmentEquippedInState(state, item.Id))
                {
                    result.emplace_back(item.Id);
                }
            }
            return result;
        }

        static void SelectFirstVisibleEquipmentOnPage(State& state)
        {
            const std::vector<std::string> bagEquipment = BuildVisibleBagEquipment(state);
            const size_t start = static_cast<size_t>(std::max(0, state.EquipmentPage - 1)) * 4;
            if (start < bagEquipment.size())
                state.SelectedEquipmentId = bagEquipment[start];
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
        ApplySettingsToRuntime();
    }

    void SetSceneTransitionContext(const std::filesystem::path& previousScenePath, const std::filesystem::path& currentScenePath)
    {
        State& state = GetState();
        const std::string previous = NormalizeScenePathString(previousScenePath);
        std::string current = NormalizeScenePathString(currentScenePath);
        if (current.empty())
            current = kDefaultLoadScenePath;

        if (!previous.empty() && previous != current)
            state.PreviousScenePath = previous;
        else if (!state.CurrentScenePath.empty() && state.CurrentScenePath != current)
            state.PreviousScenePath = state.CurrentScenePath;

        state.CurrentScenePath = current;
    }

    std::string GetCurrentScenePath()
    {
        return NormalizeAssetLikePath(GetState().CurrentScenePath);
    }

    std::string GetScenePathForSave()
    {
        return ResolveScenePathForSave();
    }
    void ApplySettingsToRuntime()
    {
        ProgressionSettingsCommandService::ApplyToRuntime();
    }

    int GetMaxSaveSlots()
    {
        return kMaxSaveSlots;
    }

    std::filesystem::path GetProgressSavePath(int slot)
    {
        return SavePathForSlot(slot);
    }

    std::filesystem::path GetGameRuntimeSavePath(int slot, const std::string& saveDirectory)
    {
        return GameRuntimeSavePathForSlot(slot, saveDirectory);
    }

    bool ClearGameRuntimeSaveSlot(int slot, const std::string& saveDirectory)
    {
        std::error_code error;
        const std::filesystem::path path = GameRuntimeSavePathForSlot(slot, saveDirectory);
        const bool exists = std::filesystem::exists(path, error);
        if (error)
            return false;
        if (!exists)
            return true;

        std::filesystem::remove(path, error);
        return !error;
    }

    bool IsSaveSlotOccupied(int slot)
    {
        return std::filesystem::exists(SavePathForSlot(slot));
    }

    bool IsGameRuntimeSaveSlotOccupied(int slot, const std::string& saveDirectory)
    {
        return std::filesystem::exists(GameRuntimeSavePathForSlot(slot, saveDirectory));
    }

    bool IsGameSaveSlotOccupied(int slot, const std::string& saveDirectory)
    {
        return IsSaveSlotOccupied(slot) || IsGameRuntimeSaveSlotOccupied(slot, saveDirectory);
    }

    SaveSlotInfo GetSaveSlotInfo(int slot)
    {
        SaveSlotInfo info;
        info.Slot = ClampSaveSlot(slot);
        const std::filesystem::path path = SavePathForSlot(info.Slot);
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
            return info;

        info.Exists = true;
        std::string line;
        while (std::getline(input, line))
        {
            const size_t split = line.find('=');
            if (split == std::string::npos)
                continue;

            const std::string key = line.substr(0, split);
            const std::string value = line.substr(split + 1);
            if (key == "saveVersion")
                info.SaveVersion = ParseInt(value, info.SaveVersion);
            else if (key == "chapter")
                info.Chapter = ParseInt(value, info.Chapter);
            else if (key == "objective")
                info.Objective = value;
            else if (key == "playerLevel")
                info.PlayerLevel = ParseInt(value, info.PlayerLevel);
            else if (key == "gold")
                info.Gold = ParseInt(value, info.Gold);
            else if (key == "scenePath")
                info.ScenePath = NormalizeAssetLikePath(value);
        }

        return info;
    }

    bool SaveSlot(int slot)
    {
        State& state = GetState();
        const std::filesystem::path path = SavePathForSlot(slot);
        const int safeSlot = ClampSaveSlot(slot);
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);

        std::ofstream output(path, std::ios::binary);
        if (!output.is_open())
        {
            state.LastResultMessage = "存档失败：无法写入 " + path.generic_string();
            return false;
        }

        output << "schema=wheatear.progress.v3\n";
        output << "saveVersion=" << kCurrentSaveVersion << "\n";
        output << "scenePath=" << ResolveScenePathForSave() << "\n";
        output << "chapter=" << state.CurrentChapter << "\n";
        output << "objective=" << state.Objective << "\n";
        output << "playerLevel=" << state.PlayerLevel << "\n";
        output << "experience=" << state.Experience << "\n";
        output << "experienceToNext=" << state.ExperienceToNext << "\n";
        output << "magicSwordLevel=" << state.MagicSwordLevel << "\n";
        output << "travelerArmorLevel=" << state.TravelerArmorLevel << "\n";
        output << "gold=" << state.Gold << "\n";
        output << "hp=" << state.Attributes.HP << "\n";
        output << "atk=" << state.Attributes.ATK << "\n";
        output << "def=" << state.Attributes.DEF << "\n";
        output << "matk=" << state.Attributes.MATK << "\n";
        output << "mdef=" << state.Attributes.MDEF << "\n";
        output << "completedDungeons=" << JoinSet(state.CompletedDungeons) << "\n";
        output << "unlockedDungeons=" << JoinSet(state.UnlockedDungeons) << "\n";
        output << "unlockedSkills=" << JoinSet(state.UnlockedSkills) << "\n";
        output << "ownedEquipment=" << JoinSet(state.OwnedEquipment) << "\n";
        output << "equippedItems=" << JoinMap(state.EquippedItemsBySlot) << "\n";
        output << "storyFlags=" << JoinSet(state.StoryFlags) << "\n";
        output << "activeSupport=" << state.ActiveSupportCharacterId << "\n";

        for (const auto& [itemId, amount] : state.Materials)
            output << "material." << itemId << "=" << amount << "\n";
        for (const auto& [dungeonId, combo] : state.BestCombosByDungeon)
            output << "bestCombo." << dungeonId << "=" << combo << "\n";
        for (const RelationshipRecord& relationship : state.Relationships)
        {
            output << "relationship." << relationship.CharacterId << ".affinity=" << relationship.Affinity << "\n";
            output << "relationship." << relationship.CharacterId << ".supportLevel=" << relationship.SupportLevel << "\n";
            output << "relationship." << relationship.CharacterId << ".unlocked=" << (relationship.Unlocked ? 1 : 0) << "\n";
        }

        state.LastResultMessage = "已保存到 " + std::to_string(safeSlot) + " 号槽。";
        PushNotification(state, state.LastResultMessage);
        return true;
    }

    bool LoadSlot(int slot)
    {
        const std::filesystem::path path = SavePathForSlot(slot);
        const int safeSlot = ClampSaveSlot(slot);
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
        {
            GetState().LastResultMessage = "没有找到 " + std::to_string(safeSlot) + " 号槽存档。";
            return false;
        }

        State loaded = MakeDefaultState();

        std::string line;
        while (std::getline(input, line))
        {
            const size_t split = line.find('=');
            if (split == std::string::npos)
                continue;

            const std::string key = line.substr(0, split);
            const std::string value = line.substr(split + 1);

            if (key == "chapter") loaded.CurrentChapter = ParseInt(value, loaded.CurrentChapter);
            else if (key == "objective") loaded.Objective = value;
            else if (key == "scenePath") loaded.CurrentScenePath = NormalizeAssetLikePath(value);
            else if (key == "playerLevel") loaded.PlayerLevel = ParseInt(value, loaded.PlayerLevel);
            else if (key == "experience") loaded.Experience = ParseInt(value, loaded.Experience);
            else if (key == "experienceToNext") loaded.ExperienceToNext = ParseInt(value, loaded.ExperienceToNext);
            else if (key == "magicSwordLevel") loaded.MagicSwordLevel = ParseInt(value, loaded.MagicSwordLevel);
            else if (key == "travelerArmorLevel") loaded.TravelerArmorLevel = ParseInt(value, loaded.TravelerArmorLevel);
            else if (key == "gold") loaded.Gold = ParseInt(value, loaded.Gold);
            else if (key == "hp") loaded.Attributes.HP = ParseInt(value, loaded.Attributes.HP);
            else if (key == "atk") loaded.Attributes.ATK = ParseInt(value, loaded.Attributes.ATK);
            else if (key == "def") loaded.Attributes.DEF = ParseInt(value, loaded.Attributes.DEF);
            else if (key == "matk") loaded.Attributes.MATK = ParseInt(value, loaded.Attributes.MATK);
            else if (key == "mdef") loaded.Attributes.MDEF = ParseInt(value, loaded.Attributes.MDEF);
            else if (key == "completedDungeons") LoadSet(loaded.CompletedDungeons, value);
            else if (key == "unlockedDungeons") LoadSet(loaded.UnlockedDungeons, value);
            else if (key == "unlockedSkills") LoadSet(loaded.UnlockedSkills, value);
            else if (key == "ownedEquipment") LoadSet(loaded.OwnedEquipment, value);
            else if (key == "equippedItems") LoadMap(loaded.EquippedItemsBySlot, value);
            else if (key == "storyFlags") LoadSet(loaded.StoryFlags, value);
            else if (key == "activeSupport") loaded.ActiveSupportCharacterId = value;
            else if (key.rfind("material.", 0) == 0)
            {
                const std::string itemId = key.substr(9);
                loaded.Materials[itemId] = ParseInt(value, 0);
                loaded.MaterialNames[itemId] = DefaultMaterialName(itemId);
            }
            else if (key.rfind("bestCombo.", 0) == 0)
            {
                loaded.BestCombosByDungeon[key.substr(10)] = ParseInt(value, 0);
            }
            else if (key.rfind("relationship.", 0) == 0)
            {
                const std::string payload = key.substr(13);
                const size_t propertySplit = payload.rfind('.');
                if (propertySplit == std::string::npos)
                    continue;

                const std::string characterId = payload.substr(0, propertySplit);
                const std::string property = payload.substr(propertySplit + 1);
                for (RelationshipRecord& relationship : loaded.Relationships)
                {
                    if (relationship.CharacterId != characterId)
                        continue;

                    if (property == "affinity") relationship.Affinity = ParseInt(value, relationship.Affinity);
                    else if (property == "supportLevel") relationship.SupportLevel = ParseInt(value, relationship.SupportLevel);
                    else if (property == "unlocked") relationship.Unlocked = ParseBool(value, relationship.Unlocked);
                    break;
                }
            }
        }

        loaded.LastResultMessage = "已读取 " + std::to_string(safeSlot) + " 号槽。";
        PushNotification(loaded, loaded.LastResultMessage);
        GetState() = loaded;
        ApplySettingsToRuntime();
        return true;
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

    void SetActiveSideCombatDungeon(const std::string& dungeonId)
    {
        State& state = GetState();
        state.ActiveSideCombatDungeonId = dungeonId;
    }

    const std::string& GetActiveSideCombatDungeonId()
    {
        return GetState().ActiveSideCombatDungeonId;
    }

    bool RecordDungeonClear(const std::string& dungeonId, int bestCombo, int firstClearExperience, int repeatExperience)
    {
        if (dungeonId.empty())
            return false;

        State& state = GetState();
        const bool firstClear = state.CompletedDungeons.insert(dungeonId).second;
        state.BestCombosByDungeon[dungeonId] = std::max(state.BestCombosByDungeon[dungeonId], bestCombo);

        if (const auto* dungeon = ProgressionContent::FindDungeon(dungeonId))
        {
            if (firstClear)
            {
                for (const std::string& unlockedDungeon : dungeon->UnlocksOnFirstClear)
                    state.UnlockedDungeons.insert(unlockedDungeon);
                if (!dungeon->FirstClearNotification.empty())
                    PushNotification(state, dungeon->FirstClearNotification);
            }
            for (const std::string& flag : dungeon->FlagsOnClear)
                state.StoryFlags.insert(flag);
            if (!dungeon->ObjectiveOnClear.empty())
                state.Objective = dungeon->ObjectiveOnClear;
        }

        std::ostringstream stream;
        stream << (firstClear ? "首通 " : "再战 ") << DungeonDisplayName(dungeonId)
               << "，最高连击 x" << bestCombo;
        PushNotification(state, stream.str());
        state.LastResultMessage = stream.str();

        AddExperience(firstClear ? firstClearExperience : repeatExperience);
        return firstClear;
    }

    void RecordLastDungeonResult(const std::string& dungeonId,
        const std::string& grade,
        bool firstClear,
        int bestCombo,
        int hitsTaken,
        float clearTimeSeconds,
        int experience,
        const std::string& rewardSummary,
        const std::unordered_map<std::string, int>& rewardAmounts)
    {
        if (dungeonId.empty())
            return;

        State& state = GetState();
        state.LastDungeonResult.Valid = true;
        state.LastDungeonResult.DungeonId = dungeonId;
        state.LastDungeonResult.DungeonName = DungeonDisplayName(dungeonId);
        state.LastDungeonResult.Grade = grade.empty() ? "C" : grade;
        state.LastDungeonResult.FirstClear = firstClear;
        state.LastDungeonResult.BestCombo = bestCombo;
        state.LastDungeonResult.HitsTaken = hitsTaken;
        state.LastDungeonResult.ClearTimeSeconds = clearTimeSeconds;
        state.LastDungeonResult.Experience = experience;
        state.LastDungeonResult.RewardSummary = rewardSummary;
        state.LastDungeonResult.RewardAmounts = rewardAmounts;

        std::ostringstream stream;
        stream << state.LastDungeonResult.DungeonName
               << "完成，评价 " << state.LastDungeonResult.Grade
               << "，经验 +" << experience
               << "，最高连击 x" << bestCombo;
        state.LastResultMessage = stream.str();
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

    bool IsEquipmentOwned(const std::string& equipmentId)
    {
        const State& state = GetState();
        return state.OwnedEquipment.find(equipmentId) != state.OwnedEquipment.end();
    }

    bool IsEquipmentEquipped(const std::string& equipmentId)
    {
        return IsEquipmentEquippedInState(GetState(), equipmentId);
    }

    std::string GetEquipmentSlotId(const std::string& equipmentId)
    {
        return FindEquipment(equipmentId).SlotId;
    }

    std::string GetEquipmentSlotDisplayName(const std::string& slotId)
    {
        return SlotDisplayName(slotId);
    }

    std::string GetEquipmentIconPath(const std::string& equipmentId)
    {
        return FindEquipment(equipmentId).IconPath;
    }

    std::string GetEquippedEquipmentForSlot(const std::string& slotId)
    {
        const State& state = GetState();
        if (auto it = state.EquippedItemsBySlot.find(slotId); it != state.EquippedItemsBySlot.end())
            return it->second;
        return {};
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
        const auto& upgrade = ProgressionContent::Get().MagicSwordLv2;
        ApplyAttributeBonus(state, upgrade.Bonus);
        for (const std::string& skillId : upgrade.UnlockSkills)
            state.UnlockedSkills.insert(skillId);
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
        ApplyAttributeBonus(state, ProgressionContent::Get().TravelerArmorLv1.Bonus);
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
            if (GetState().SelectedEquipmentId != TravelerArmorUpgradeEquipmentId())
            {
                GetState().LastResultMessage = "当前选中装备暂未开放强化。请选择旅人护衣查看竖切强化流程。";
                result.Success = true;
            }
            else
            {
                result.Changed = TryUpgradeTravelerArmorToLv1();
                result.Success = result.Changed || GetState().TravelerArmorLevel >= 1;
            }
        }
        else if (action == "learn_selected_skill_v2")
        {
            State& state = GetState();
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
            if (!node)
            {
                state.LastResultMessage = "技能节点无效。";
                result.Success = true;
            }
            else if (node->UnlockChapter > state.CurrentChapter)
            {
                state.LastResultMessage = std::string(node->Name) + " 会在后续第 " + std::to_string(node->UnlockChapter) + " 章开放。";
                result.Success = true;
            }
            else if (RequiresMagicSwordLv2(*node) && state.MagicSwordLevel < 2)
            {
                state.LastResultMessage = std::string(node->Name) + " 需要先把魔剑觉醒到 Lv2。";
                result.Success = true;
            }
            else if (state.UnlockedSkills.insert(state.SelectedSkillNodeId).second)
            {
                state.LastResultMessage = std::string("已习得技能节点: ") + node->Name;
                PushNotification(state, state.LastResultMessage);
                result.Changed = true;
                result.Success = true;
            }
            else
            {
                state.LastResultMessage = std::string(node->Name) + " 已经习得。";
                result.Success = true;
            }
        }
        else if (action == "learn_selected_skill")
        {
            const std::string selected = GetState().SelectedSkillNodeId;
            if (selected == "vfx_magic_bolt" || selected == "wind_step")
            {
                result.Changed = TryUpgradeMagicSwordToLv2();
                result.Success = result.Changed || GetState().MagicSwordLevel >= 2;
            }
            else if (selected == "break_limit")
            {
                GetState().LastResultMessage = "断限追击是第七章后正式教学的高手机制，当前竖切只展示节点和规则。";
                result.Success = true;
            }
            else
            {
                GetState().LastResultMessage = "该节点已经习得。请尝试选择魔法弹或疾风步查看 Lv2 解锁条件。";
                result.Success = true;
            }
        }
        else if (action.rfind("select_skill_node:", 0) == 0)
        {
            const std::string selectedNode = action.substr(18);
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(selectedNode);
            if (node)
            {
                GetState().SelectedSkillNodeId = node->Id;
                GetState().LastResultMessage = std::string("已选中技能节点: ") + node->Name;
                result.Changed = true;
                result.Success = true;
            }
            else
            {
                GetState().LastResultMessage = "未找到技能节点: " + selectedNode;
                result.Success = true;
            }
        }
        else if (const std::string selectedNode = LegacySkillActionToNodeId(action); !selectedNode.empty())
        {
            const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(selectedNode);
            GetState().SelectedSkillNodeId = selectedNode;
            GetState().LastResultMessage = std::string("已选中技能节点: ") + (node ? node->Name : selectedNode);
            result.Changed = true;
            result.Success = true;
        }
        else if (const std::string selectedNode = SkillActionToNodeId(action); !selectedNode.empty())
        {
            GetState().SelectedSkillNodeId = selectedNode;
            const SkillNodeInfo& node = FindSkillNode(selectedNode);
            GetState().LastResultMessage = std::string("已选中技能节点：") + node.Name;
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("equipment_page_slider:", 0) == 0)
        {
            State& state = GetState();
            const float pageValue = ParseFloat(action.substr(22), static_cast<float>(state.EquipmentPage));
            state.EquipmentPage = pageValue >= 1.5f ? 2 : 1;
            SelectFirstVisibleEquipmentOnPage(state);
            state.LastResultMessage = "装备背包切换到第 " + std::to_string(state.EquipmentPage) + " 页。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "equipment_page_1" || action == "equipment_page_2")
        {
            State& state = GetState();
            state.EquipmentPage = action == "equipment_page_2" ? 2 : 1;
            SelectFirstVisibleEquipmentOnPage(state);
            state.LastResultMessage = "装备背包切换到第 " + std::to_string(state.EquipmentPage) + " 页。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("select_equipment_slot:", 0) == 0)
        {
            const std::string slotId = action.substr(22);
            const std::string equipmentId = GetEquippedEquipmentForSlot(slotId);
            if (equipmentId.empty())
            {
                GetState().LastResultMessage = std::string(SlotDisplayName(slotId)) + " 当前没有装备。";
                result.Success = true;
            }
            else
            {
                GetState().SelectedEquipmentId = equipmentId;
                const EquipmentInfo& item = FindEquipment(equipmentId);
                GetState().LastResultMessage = std::string("已查看已装备：") + item.Name;
                result.Changed = true;
                result.Success = true;
            }
        }
        else if (action == "toggle_selected_equipment")
        {
            State& state = GetState();
            const EquipmentInfo& item = FindEquipment(state.SelectedEquipmentId);
            if (!IsEquipmentOwned(state.SelectedEquipmentId))
            {
                state.LastResultMessage = std::string(item.Name) + " 尚未获得。";
                result.Success = true;
            }
            else if (const std::string equippedSlot = FindEquippedSlotForItem(state, state.SelectedEquipmentId); !equippedSlot.empty())
            {
                state.EquippedItemsBySlot.erase(equippedSlot);
                state.LastResultMessage = std::string("已脱下：") + item.Name;
                result.Changed = true;
                result.Success = true;
            }
            else
            {
                state.EquippedItemsBySlot[item.SlotId] = state.SelectedEquipmentId;
                state.LastResultMessage = std::string("已装备：") + item.Name;
                result.Changed = true;
                result.Success = true;
            }
        }
        else if (action.rfind("select_equipment_", 0) == 0)
        {
            GetState().SelectedEquipmentId = action.substr(17);
            const EquipmentInfo& item = FindEquipment(GetState().SelectedEquipmentId);
            GetState().LastResultMessage = std::string("已选中装备：") + item.Name;
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "reset")
        {
            ResetForNewGame();
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_flag:", 0) == 0)
        {
            State& state = GetState();
            const std::string flag = action.substr(9);
            if (!flag.empty())
            {
                result.Changed = state.StoryFlags.insert(flag).second;
                state.LastResultMessage = "剧情标记已设置: " + flag;
                result.Success = true;
            }
        }
        else if (action.rfind("clear_flag:", 0) == 0)
        {
            State& state = GetState();
            const std::string flag = action.substr(11);
            if (!flag.empty())
            {
                result.Changed = state.StoryFlags.erase(flag) > 0;
                state.LastResultMessage = "剧情标记已清除: " + flag;
                result.Success = true;
            }
        }
        else if (action.rfind("set_active_dungeon:", 0) == 0)
        {
            const std::string dungeonId = action.substr(19);
            State& state = GetState();
            result.Changed = state.ActiveSideCombatDungeonId != dungeonId;
            SetActiveSideCombatDungeon(dungeonId);
            result.Success = true;
        }
        else if (action == "clear_active_dungeon")
        {
            State& state = GetState();
            result.Changed = !state.ActiveSideCombatDungeonId.empty();
            SetActiveSideCombatDungeon({});
            result.Success = true;
        }
        else if (action.rfind("set_chapter:", 0) == 0)
        {
            State& state = GetState();
            const int chapter = std::clamp(ParseInt(action.substr(12), state.CurrentChapter), 1, 99);
            result.Changed = state.CurrentChapter != chapter;
            state.CurrentChapter = chapter;
            state.LastResultMessage = "当前章节切换到第 " + std::to_string(chapter) + " 章。";
            result.Success = true;
        }
        else if (action == "select_support_mentor")
        {
            GetState().ActiveSupportCharacterId = "mentor";
            GetState().LastResultMessage = "已配置支援：魔剑士导师。当前竖切中提供空连指导和短冷却支援。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "select_support_white_mage"
            || action == "select_support_guard"
            || action == "select_support_black_mage")
        {
            GetState().LastResultMessage = "该队友将在后续章节加入；当前竖切先保留支援槽入口。";
            result.Success = true;
        }
        else if (ProgressionSettingsCommandService::IsSettingsCommand(action))
        {
            result = ProgressionSettingsCommandService::Execute(action, GetState());
        }
        else if (action.rfind("set_text_speed:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.TextSpeed)) + 0.5f), 12, 180);
            GetState().LastResultMessage = "文字速度设置为 " + std::to_string(settings.TextSpeed) + " 字/秒。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_master_volume:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.MasterVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(18), static_cast<float>(settings.MasterVolume)) + 0.5f), 0, 100);
            GetState().LastResultMessage = "主音量设置为 " + std::to_string(settings.MasterVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_bgm_volume:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.BGMVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.BGMVolume)) + 0.5f), 0, 100);
            GetState().LastResultMessage = "BGM 音量设置为 " + std::to_string(settings.BGMVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action.rfind("set_sfx_volume:", 0) == 0)
        {
            auto& settings = UserSettings::Get();
            settings.SFXVolume = std::clamp(static_cast<int>(ParseFloat(action.substr(15), static_cast<float>(settings.SFXVolume)) + 0.5f), 0, 100);
            GetState().LastResultMessage = "音效音量设置为 " + std::to_string(settings.SFXVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "text_speed_up")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::min(180, settings.TextSpeed + 6);
            GetState().LastResultMessage = "文字速度提高到 " + std::to_string(settings.TextSpeed) + " 字/秒。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "text_speed_down")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::max(12, settings.TextSpeed - 6);
            GetState().LastResultMessage = "文字速度降低到 " + std::to_string(settings.TextSpeed) + " 字/秒。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "master_volume_up")
        {
            auto& settings = UserSettings::Get();
            settings.MasterVolume = std::min(100, settings.MasterVolume + 5);
            GetState().LastResultMessage = "主音量 " + std::to_string(settings.MasterVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "master_volume_down")
        {
            auto& settings = UserSettings::Get();
            settings.MasterVolume = std::max(0, settings.MasterVolume - 5);
            GetState().LastResultMessage = "主音量 " + std::to_string(settings.MasterVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "bgm_volume_up")
        {
            auto& settings = UserSettings::Get();
            settings.BGMVolume = std::min(100, settings.BGMVolume + 5);
            GetState().LastResultMessage = "BGM 音量 " + std::to_string(settings.BGMVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "bgm_volume_down")
        {
            auto& settings = UserSettings::Get();
            settings.BGMVolume = std::max(0, settings.BGMVolume - 5);
            GetState().LastResultMessage = "BGM 音量 " + std::to_string(settings.BGMVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "sfx_volume_up")
        {
            auto& settings = UserSettings::Get();
            settings.SFXVolume = std::min(100, settings.SFXVolume + 5);
            GetState().LastResultMessage = "音效音量 " + std::to_string(settings.SFXVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "sfx_volume_down")
        {
            auto& settings = UserSettings::Get();
            settings.SFXVolume = std::max(0, settings.SFXVolume - 5);
            GetState().LastResultMessage = "音效音量 " + std::to_string(settings.SFXVolume) + "%。";
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "toggle_screen_shake")
        {
            auto& settings = UserSettings::Get();
            settings.ScreenShake = !settings.ScreenShake;
            GetState().LastResultMessage = std::string("屏幕震动已") + (settings.ScreenShake ? "开启。" : "关闭。");
            result.Changed = true;
            result.Success = true;
        }
        else if (action == "toggle_fullscreen")
        {
            auto& settings = UserSettings::Get();
            settings.Fullscreen = !settings.Fullscreen;
            ApplySettingsToRuntime();
            GetState().LastResultMessage = std::string("全屏偏好已") + (settings.Fullscreen ? "开启。" : "关闭。") + "已应用到当前窗口。";
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
               << "  经验 " << state.Experience << "/" << state.ExperienceToNext
               << "  /  " << (IsDungeonUnlocked(BeastPathDungeonId())
                   ? DungeonDisplayName(BeastPathDungeonId()) + "已解锁"
                   : DungeonDisplayName(BeastPathDungeonId()) + "未解锁");
        return stream.str();
    }

    std::string BuildHubStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "目标: " << state.Objective << "\n";
        stream << "材料: " << BuildMaterialInventoryText() << "\n";
        stream << "能力: HP " << state.Attributes.HP
               << " / 攻击 " << state.Attributes.ATK
               << " / 防御 " << state.Attributes.DEF
               << " / 魔攻 " << state.Attributes.MATK << "\n";

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
        return IsDungeonUnlocked(BeastPathDungeonId())
            ? "重刷" + DungeonDisplayName(BeastPathDungeonId())
            : DungeonDisplayName(BeastPathDungeonId()) + "未解锁";
    }

    std::string GetSkillButtonText()
    {
        const State& state = GetState();
        if (state.MagicSwordLevel >= 2)
            return "魔剑技能树";
        return CanUpgradeMagicSwordToLv2() ? "技能树：可觉醒" : "魔剑技能树";
    }

    std::string GetEquipmentButtonText()
    {
        const State& state = GetState();
        if (state.TravelerArmorLevel >= 1)
            return "装备与强化";
        return CanUpgradeTravelerArmorToLv1() ? "装备：可强化" : "装备与强化";
    }

    std::string BuildResultTitle()
    {
        const State& state = GetState();
        if (!state.LastDungeonResult.Valid)
            return "战斗结算";

        std::ostringstream stream;
        stream << (state.LastDungeonResult.FirstClear ? "首通 " : "重刷 ")
               << state.LastDungeonResult.DungeonName;
        return stream.str();
    }

    std::string BuildResultStats()
    {
        const State& state = GetState();
        std::ostringstream stream;

        if (!state.LastDungeonResult.Valid)
        {
            stream << "还没有可展示的战斗记录。\n";
            stream << "从据点进入黑林兽道，完成战斗后会在这里显示结算。";
            return stream.str();
        }

        const DungeonResult& result = state.LastDungeonResult;
        stream << "评价: " << result.Grade << "\n";
        stream << "最佳连击: x" << result.BestCombo << "\n";
        stream << "受击次数: " << result.HitsTaken << "\n";
        stream << "通关时间: " << FormatSeconds(result.ClearTimeSeconds) << "\n";
        stream << "获得经验: +" << result.Experience << "\n";
        stream << "当前等级: Lv" << state.PlayerLevel << "  经验 "
               << state.Experience << "/" << state.ExperienceToNext;
        return stream.str();
    }

    std::string BuildResultRewards()
    {
        const State& state = GetState();
        std::ostringstream stream;
        if (state.LastDungeonResult.Valid && !state.LastDungeonResult.RewardSummary.empty())
            stream << state.LastDungeonResult.RewardSummary << "\n";
        stream << "背包材料: " << BuildMaterialInventoryText() << "\n";
        stream << "下一步: 回据点升级魔剑/装备，或重刷副本继续调连招手感。";
        return stream.str();
    }

    std::string BuildSkillTreeStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "魔剑 Lv" << state.MagicSwordLevel << " / 技能网络\n";
        stream << "中心向四个方向展开：近战、魔法、机动、支援。\n";
        stream << "当前选中: " << FindSkillNode(state.SelectedSkillNodeId).Name
               << " [" << SkillNodeState(state, state.SelectedSkillNodeId) << "]\n";
        stream << "节点颜色: 金色=当前 / 青色=已学或可用 / 灰色=后续章节。";
        return stream.str();
    }

    std::string BuildSkillTreeDetails()
    {
        const State& state = GetState();
        const SkillNodeInfo& node = FindSkillNode(state.SelectedSkillNodeId);
        std::ostringstream stream;
        stream << node.Name << "\n";
        stream << "分支: " << node.Branch << "\n";
        stream << "输入: " << node.Input << "\n";
        stream << "状态: " << SkillNodeState(state, state.SelectedSkillNodeId) << "\n";
        stream << "连招职责: " << node.ComboRole << "\n";
        stream << "条件: " << node.Requirement << "\n";
        stream << node.Description;
        return stream.str();
    }

    std::string BuildSkillTreeMaterials()
    {
        std::ostringstream stream;
        const State& state = GetState();
        stream << "材料栏: " << BuildMaterialInventoryText() << "\n";
        if (state.SelectedSkillNodeId == "vfx_magic_bolt" || state.SelectedSkillNodeId == "wind_step")
            stream << "选中节点需求: 魔剑 Lv2 / " << BuildCostText(MagicSwordLv2Cost());
        else if (state.SelectedSkillNodeId == "break_limit")
            stream << "选中节点需求: 第七章王宫战后正式开放。";
        else
            stream << "选中节点无需额外材料，已经属于当前基础战斗动作。";
        return stream.str();
    }

    std::string GetMagicSwordUpgradeButtonText()
    {
        const State& state = GetState();
        if (state.SelectedSkillNodeId == "break_limit")
            return "后期节点";
        if (state.SelectedSkillNodeId != "vfx_magic_bolt" && state.SelectedSkillNodeId != "wind_step")
            return "节点已学 / 查看详情";
        if (state.MagicSwordLevel >= 2)
            return "Lv2 节点已解锁";
        return CanUpgradeMagicSwordToLv2() ? "学习选中节点" : "材料不足";
    }

    std::string BuildSkillTreeStatusV2()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "魔剑 Lv" << state.MagicSwordLevel << " / 完整技能网\n";
        stream << "五大分支: 近战 / 魔法 / 魔剑融合 / 机动 / 断限\n";
        if (const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId))
            stream << "当前选中: " << node->Name << " [" << SkillNodeDisplayState(state, state.SelectedSkillNodeId) << "]\n";
        stream << "拖动画布浏览整棵树，灰暗节点代表未学或后续章节开放。";
        return stream.str();
    }

    std::string BuildSkillTreeDetailsV2()
    {
        const State& state = GetState();
        const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
        if (!node)
            return "暂无节点信息";

        std::ostringstream stream;
        stream << node->Name << "\n";
        stream << "分支: " << node->Branch << "\n";
        stream << "输入: " << node->Input << "\n";
        stream << "状态: " << SkillNodeDisplayState(state, state.SelectedSkillNodeId) << "\n";
        stream << "职责: " << node->ComboRole << "\n";
        stream << "条件: " << node->Requirement << "\n";
        stream << node->Description;
        return stream.str();
    }

    std::string BuildSkillTreeMaterialsV2()
    {
        const State& state = GetState();
        const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
        std::ostringstream stream;
        stream << "材料: " << BuildMaterialInventoryText() << "\n";
        if (!node)
            return stream.str();

        if (node->UnlockChapter > state.CurrentChapter)
            stream << "后续章节开放: 第 " << node->UnlockChapter << " 章";
        else if (RequiresMagicSwordLv2(*node) && state.MagicSwordLevel < 2)
            stream << "当前节点需要魔剑 Lv2。先在据点刷材料并完成魔剑觉醒。";
        else if (state.UnlockedSkills.find(state.SelectedSkillNodeId) != state.UnlockedSkills.end())
            stream << "当前节点已学，可继续查看相邻分支。";
        else
            stream << "当前节点可学习，学习后会点亮并保留在技能树中。";
        return stream.str();
    }

    std::string GetMagicSwordUpgradeButtonTextV2()
    {
        const State& state = GetState();
        const SkillNodeDisplayInfo* node = LookupSkillNodeDisplayInfo(state.SelectedSkillNodeId);
        if (!node)
            return "节点无效";
        if (node->UnlockChapter > state.CurrentChapter)
            return "后续章节开放";
        if (RequiresMagicSwordLv2(*node) && state.MagicSwordLevel < 2)
            return "需要魔剑 Lv2";
        if (state.UnlockedSkills.find(state.SelectedSkillNodeId) != state.UnlockedSkills.end())
            return "节点已学";
        return "学习选中节点";
    }

    std::string BuildEquipmentStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "背包 " << state.EquipmentPage << " / 2\n";
        stream << "能力  HP " << state.Attributes.HP
               << " / 攻击 " << state.Attributes.ATK
               << " / 防御 " << state.Attributes.DEF
               << " / 魔攻 " << state.Attributes.MATK << "\n";
        stream << "选中  " << FindEquipment(state.SelectedEquipmentId).Name;
        return stream.str();
    }

    std::string BuildEquipmentDetails()
    {
        const State& state = GetState();
        const EquipmentInfo& item = FindEquipment(state.SelectedEquipmentId);
        const bool owned = IsEquipmentOwned(state.SelectedEquipmentId);
        const bool equipped = IsEquipmentEquippedInState(state, state.SelectedEquipmentId);
        std::ostringstream stream;
        stream << item.Name << "\n";
        stream << "槽位  " << item.Slot << "\n";
        stream << "状态  " << (equipped ? "已装备" : (owned ? "背包中" : item.Status));
        if (state.SelectedEquipmentId == TravelerArmorUpgradeEquipmentId())
            stream << " +" << state.TravelerArmorLevel;
        stream << "\n";
        stream << "属性  " << item.Stats << "\n";
        stream << "来源  " << item.Source;
        return stream.str();
    }

    std::string BuildEquipmentTooltip(const std::string& equipmentId)
    {
        const State& state = GetState();
        const EquipmentInfo& item = FindEquipment(equipmentId);
        const bool equipped = IsEquipmentEquippedInState(state, equipmentId);
        std::ostringstream stream;
        stream << item.Name << "\n";
        stream << item.Stats << "\n";
        stream << (equipped ? "已装备" : (IsEquipmentOwned(equipmentId) ? "背包中" : item.Status));
        return stream.str();
    }

    std::string BuildEquipmentPageText()
    {
        const State& state = GetState();
        std::ostringstream stream;
        if (state.EquipmentPage == 1)
            stream << "第 1 页: 防具 / 饰品 / 初期刷本装备";
        else
            stream << "第 2 页: 后续章节装备 / 特殊道具";
        return stream.str();
    }

    std::string BuildEquipmentMaterials()
    {
        std::ostringstream stream;
        stream << "材料  " << BuildMaterialInventoryText() << "\n";
        if (GetState().SelectedEquipmentId == TravelerArmorUpgradeEquipmentId())
            stream << "+1 需求  " << BuildCostText(TravelerArmorLv1Cost());
        else
            stream << "当前装备不可强化。";
        return stream.str();
    }

    std::string GetTravelerArmorUpgradeButtonText()
    {
        const State& state = GetState();
        if (state.SelectedEquipmentId != TravelerArmorUpgradeEquipmentId())
            return "选择旅人护衣强化";
        if (state.TravelerArmorLevel >= 1)
            return "旅人护衣 +1 已完成";
        return CanUpgradeTravelerArmorToLv1() ? "强化旅人护衣 +1" : "材料不足";
    }

    std::string GetEquipmentToggleButtonText()
    {
        const State& state = GetState();
        const EquipmentInfo& item = FindEquipment(state.SelectedEquipmentId);
        if (!IsEquipmentOwned(state.SelectedEquipmentId))
            return "未获得";
        if (IsEquipmentEquippedInState(state, state.SelectedEquipmentId))
            return std::string("脱下 ") + item.Name;
        return std::string("装备 ") + item.Name;
    }

    std::string BuildDungeonSelectStatus()
    {
        const State& state = GetState();
        const auto& content = ProgressionContent::Get();
        std::ostringstream stream;

        std::string currentCategory;
        bool wroteAny = false;
        for (const auto& dungeon : content.Dungeons)
        {
            if (dungeon.Id.empty())
                continue;

            if (dungeon.Category != currentCategory)
            {
                if (wroteAny)
                    stream << "\n";
                currentCategory = dungeon.Category;
                stream << (currentCategory.empty() ? "副本" : currentCategory) << "\n";
            }

            const bool completed = state.CompletedDungeons.count(dungeon.Id) > 0;
            const bool unlocked = IsDungeonUnlocked(dungeon.Id);
            std::string status = completed
                ? "已通关"
                : (unlocked ? dungeon.StatusWhenUnlocked : dungeon.StatusWhenLocked);
            if (status.empty())
                status = unlocked ? "可挑战" : "未解锁";

            stream << dungeon.Name << "  推荐 Lv" << dungeon.RecommendedLevel
                   << "  状态: " << status
                   << "  最佳连击 x";
            if (auto it = state.BestCombosByDungeon.find(dungeon.Id); it != state.BestCombosByDungeon.end())
                stream << it->second;
            else
                stream << 0;
            stream << "\n";
            wroteAny = true;
        }

        stream << "\n\n当前目标: " << state.Objective;
        return stream.str();
    }

    std::string BuildDungeonSelectRewards()
    {
        const auto& rewards = ProgressionContent::Get().DungeonRewardSummary;
        std::ostringstream stream;
        for (size_t i = 0; i < rewards.size(); ++i)
        {
            if (i > 0)
                stream << "\n";
            stream << rewards[i];
        }
        return stream.str();
    }

    std::string BuildRelationshipStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;

        for (const RelationshipRecord& relationship : state.Relationships)
        {
            stream << relationship.DisplayName << "  ";
            stream << (relationship.Unlocked ? "已相遇" : "未加入") << "  ";
            stream << "好感 " << relationship.Affinity << "/100  ";
            stream << "支援 Lv" << relationship.SupportLevel << "\n";
            stream << "定位: " << relationship.Role << "\n";
            stream << "下一步: " << relationship.NextMilestone << "\n\n";
        }

        stream << "规则: 好感不消耗材料，主要通过 VN 选择、角色事件和章节推进提升。";
        return stream.str();
    }

    std::string BuildSupportStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "当前支援槽 1: ";

        const RelationshipRecord* active = nullptr;
        for (const RelationshipRecord& relationship : state.Relationships)
        {
            if (relationship.CharacterId == state.ActiveSupportCharacterId)
            {
                active = &relationship;
                break;
            }
        }

        if (active)
        {
            stream << active->DisplayName << "  Lv" << active->SupportLevel << "\n";
            stream << "效果: 空中连击时提供支援斩击，帮助玩家补 hit、维持浮空和练习断限节奏。\n";
            stream << "好感收益: 当前好感 " << active->Affinity << "，冷却和伤害获得竖切加成。";
        }
        else
        {
            stream << "未配置\n";
            stream << "请先选择一个已加入队友。";
        }

        stream << "\n\n预留槽位: 第二支援槽在第三章白魔法队友加入后开放。";
        return stream.str();
    }

    std::string BuildSettingsStatus()
    {
        return ProgressionSettingsCommandService::BuildStatusText();
    }

    std::string BuildSaveLoadStatus()
    {
        const State& state = GetState();
        std::ostringstream stream;
        stream << "游戏公共存档\n";
        stream << "保存和读取在整个 Sandbox 内共用同一套槽位。VN 槽位会记录剧情位置，其他场景会记录当前场景和成长进度。\n";
        stream << "当前：第 " << state.CurrentChapter << " 章 / 主角 Lv" << state.PlayerLevel
               << " / 魔剑 Lv" << state.MagicSwordLevel << "\n";
        stream << (state.LastResultMessage.empty() ? "请选择槽位继续。" : state.LastResultMessage);
        return stream.str();
    }

    std::string BuildSaveSlotSummary(int slot)
    {
        const SaveSlotInfo info = GetSaveSlotInfo(slot);
        const int safeSlot = ClampSaveSlot(slot);
        if (!info.Exists)
            return "槽位 " + std::to_string(safeSlot) + "  空槽";

        std::ostringstream stream;
        stream << "槽位 " << safeSlot
               << "  第 " << info.Chapter << " 章"
               << "  Lv" << info.PlayerLevel
               << "  金币 " << info.Gold;
        return stream.str();
    }

    std::string BuildSaveSlotDetails(int slot)
    {
        const SaveSlotInfo info = GetSaveSlotInfo(slot);
        if (!info.Exists)
            return "点击即可保存当前进度。";

        if (!info.Objective.empty())
            return info.Objective;
        return "已有游戏进度，可读取或覆盖。";
    }

    std::string BuildGameSaveSlotButtonText(int slot, bool saveMode, const std::string& saveDirectory)
    {
        const int safeSlot = ClampSaveSlot(slot);
        const bool hasRuntimeState = IsGameRuntimeSaveSlotOccupied(safeSlot, saveDirectory);
        const bool hasProgress = IsSaveSlotOccupied(safeSlot);
        const bool occupied = hasRuntimeState || hasProgress;

        std::ostringstream stream;
        stream << "槽位 " << FormatSaveSlotNumber(safeSlot)
               << "  " << (occupied ? "已有存档" : "空槽") << "\n";

        if (!occupied)
            stream << (saveMode ? "点击保存当前进度" : "没有可读取的存档");
        else if (saveMode)
            stream << "点击后确认是否覆盖";
        else if (hasProgress)
            stream << BuildSaveSlotSummary(safeSlot);
        else
            stream << "已有剧情位置存档";

        return stream.str();
    }

    std::string BuildGameSaveSlotDetails(int slot, const std::string& saveDirectory)
    {
        const int safeSlot = ClampSaveSlot(slot);
        const bool hasRuntimeState = IsGameRuntimeSaveSlotOccupied(safeSlot, saveDirectory);
        const bool hasProgress = IsSaveSlotOccupied(safeSlot);
        if (!hasRuntimeState && !hasProgress)
            return "空槽。";

        const SaveSlotInfo info = GetSaveSlotInfo(safeSlot);
        if (info.Exists && !info.Objective.empty())
            return info.Objective;
        if (hasRuntimeState && !hasProgress)
            return "已有剧情位置状态，可从同槽读取。";
        if (hasRuntimeState)
            return "已有完整存档，可读取或覆盖。";
        return "已有成长进度存档，可读取或覆盖。";
    }

    std::string BuildLoadGameCommand(int slot, const std::string& scenePath)
    {
        const std::string targetScene = ResolveLoadScenePathForSlot(slot, scenePath);
        return "loadgame:" + targetScene + ":" + std::to_string(ClampSaveSlot(slot));
    }

    std::string GetSaveButtonText(int slot)
    {
        return "保存到 " + std::to_string(ClampSaveSlot(slot)) + " 号槽";
    }

    std::string GetLoadButtonText(int slot)
    {
        const int safeSlot = ClampSaveSlot(slot);
        return IsGameSaveSlotOccupied(safeSlot)
            ? "读取 " + std::to_string(safeSlot) + " 号槽"
            : std::to_string(safeSlot) + " 号槽为空";
    }
} // namespace Wheatear::GameProgress
