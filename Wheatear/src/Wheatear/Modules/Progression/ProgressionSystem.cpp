#include "wtpch.h"
#include "ProgressionSystem.h"

#include "GameProgress.h"
#include "ProgressionEquipmentPageService.h"
#include "ProgressionResultPageService.h"
#include "ProgressionSaveLoadPageService.h"
#include "ProgressionSkillTreePageService.h"
#include "ProgressionSettingsPageService.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelSystem.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Runtime/SceneTransitionService.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace Wheatear {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;
        using Wheatear::StringUtils::ToLower;

        static constexpr const char* kSaveLoadSceneAlias = "progression.scene.save_load";
        static constexpr const char* kFallbackSaveLoadScenePath = "assets/scenes/VerticalSliceSaveLoad.wt";

        static bool s_SaveLoadSaveMode = true;
        static int s_PendingOverwriteSlot = 0;
        static std::optional<bool> s_PendingSaveLoadMode;
        static std::string s_ActiveSaveDirectory = "assets/saves";
        static std::optional<std::string> s_PendingSaveDirectory;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        static bool IsSaveLoadScenePath(const std::string& scenePath)
        {
            const std::string normalized = ToLower(scenePath);
            return normalized == ToLower(AssetAliasRegistry::Path(kSaveLoadSceneAlias, kFallbackSaveLoadScenePath))
                || normalized.find("verticalslicesaveload.wt") != std::string::npos;
        }

        static std::string GetSaveLoadScenePath()
        {
            return AssetAliasRegistry::Path(kSaveLoadSceneAlias, kFallbackSaveLoadScenePath);
        }

        static std::string GetSaveLoadCloseTargetScene()
        {
            const auto& state = GameProgress::GetState();
            if (!state.PreviousScenePath.empty() && !IsSaveLoadScenePath(state.PreviousScenePath))
                return state.PreviousScenePath;
            if (!state.CurrentScenePath.empty() && !IsSaveLoadScenePath(state.CurrentScenePath))
                return state.CurrentScenePath;
            return "assets/scenes/VerticalSliceIntro.wt";
        }

        static std::optional<int> ParseTrailingSlot(const std::string& action, const std::string& prefix)
        {
            if (action.rfind(prefix, 0) != 0)
                return std::nullopt;

            const std::string payload = action.substr(prefix.size());
            if (payload.empty())
                return std::nullopt;

            for (char c : payload)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                    return std::nullopt;
            }

            return std::clamp(std::stoi(payload), 1, GameProgress::GetMaxSaveSlots());
        }

        static std::string ResolveCommandSaveDirectory(Scene* scene)
        {
            if (HasEntity(scene, "SaveLoad_SlotScroll"))
                return s_ActiveSaveDirectory.empty() ? "assets/saves" : s_ActiveSaveDirectory;
            return scene && !scene->GetSavePolicy().SaveDirectory.empty()
                ? scene->GetSavePolicy().SaveDirectory
                : "assets/saves";
        }

        static void SetPolicyDeniedMessage(bool saving)
        {
            GameProgress::GetState().LastResultMessage = saving
                ? "当前场景禁止保存。"
                : "当前场景禁止读取。";
        }

        static bool SaveProgressionOnlySlot(int slot, const std::string& saveDirectory)
        {
            if (!GameProgress::ClearGameRuntimeSaveSlot(slot, saveDirectory))
            {
                GameProgress::GetState().LastResultMessage = "存档失败：无法清理旧剧情状态。";
                return false;
            }

            return GameProgress::SaveSlot(slot);
        }

        static void UpdateGameSaveCommands(Scene* scene)
        {
            if (!scene)
                return;

            VisualNovelSystem* visualNovelSystem = scene->GetSystem<VisualNovelSystem>();
            const SavePolicy& policy = scene->GetSavePolicy();
            const std::string commandSaveDirectory = ResolveCommandSaveDirectory(scene);
            for (const std::string& command : CommandBus::DrainGameplayCommands("gamesave:"))
            {
                const std::string action = ToLower(command.substr(9));
                if (action == "open_save_menu" || action == "open_save_load" || action == "open_menu")
                {
                    if (!policy.CanSave)
                    {
                        SetPolicyDeniedMessage(true);
                        continue;
                    }

                    s_SaveLoadSaveMode = true;
                    s_PendingSaveLoadMode = true;
                    s_PendingSaveDirectory = policy.SaveDirectory;
                    s_PendingOverwriteSlot = 0;
                    SceneTransitionService::RequestLoadScene(GetSaveLoadScenePath(), command);
                    continue;
                }

                if (action == "open_load_menu")
                {
                    if (!policy.CanLoad)
                    {
                        SetPolicyDeniedMessage(false);
                        continue;
                    }

                    s_SaveLoadSaveMode = false;
                    s_PendingSaveLoadMode = false;
                    s_PendingSaveDirectory = policy.SaveDirectory;
                    s_PendingOverwriteSlot = 0;
                    SceneTransitionService::RequestLoadScene(GetSaveLoadScenePath(), command);
                    continue;
                }

                if (action == "close")
                {
                    s_PendingOverwriteSlot = 0;
                    s_PendingSaveLoadMode.reset();
                    SceneTransitionService::RequestLoadScene(GetSaveLoadCloseTargetScene(), command);
                    continue;
                }

                if (action == "confirm_overwrite")
                {
                    if (!policy.CanSave)
                    {
                        s_PendingOverwriteSlot = 0;
                        SetPolicyDeniedMessage(true);
                        continue;
                    }

                    if (s_PendingOverwriteSlot > 0)
                        SaveProgressionOnlySlot(s_PendingOverwriteSlot, commandSaveDirectory);
                    s_PendingOverwriteSlot = 0;
                    continue;
                }

                if (action == "cancel_overwrite")
                {
                    s_PendingOverwriteSlot = 0;
                    GameProgress::GetState().LastResultMessage = "已取消覆盖。";
                    continue;
                }

                if (visualNovelSystem && visualNovelSystem->HandleGameSaveCommand(scene, command))
                    continue;

                if (const auto slot = ParseTrailingSlot(action, "slot_save_"))
                {
                    if (!policy.CanSave)
                    {
                        SetPolicyDeniedMessage(true);
                        continue;
                    }

                    s_SaveLoadSaveMode = true;
                    if (GameProgress::IsGameSaveSlotOccupied(*slot, commandSaveDirectory))
                    {
                        s_PendingOverwriteSlot = *slot;
                        GameProgress::GetState().LastResultMessage =
                            "该槽位已有存档。\n是否覆盖 " + std::to_string(*slot) + " 号槽？";
                    }
                    else
                    {
                        s_PendingOverwriteSlot = 0;
                        SaveProgressionOnlySlot(*slot, commandSaveDirectory);
                    }
                    continue;
                }

                if (const auto slot = ParseTrailingSlot(action, "load_"))
                {
                    if (!policy.CanLoad)
                    {
                        SetPolicyDeniedMessage(false);
                        continue;
                    }

                    s_SaveLoadSaveMode = false;
                    s_PendingOverwriteSlot = 0;
                    if (GameProgress::IsGameSaveSlotOccupied(*slot, commandSaveDirectory))
                    {
                        const CommandResult result = CommandBus::Execute(scene, GameProgress::BuildLoadGameCommand(*slot));
                        GameProgress::GetState().LastResultMessage = result.Success
                            ? "已读取 " + std::to_string(*slot) + " 号槽。"
                            : (result.Message.empty() ? "读取失败。" : result.Message);
                    }
                    else
                    {
                        GameProgress::GetState().LastResultMessage =
                            std::to_string(*slot) + " 号槽没有存档。";
                    }
                    continue;
                }
            }
        }
        static void UpdateHub(Scene* scene)
        {
            if (!HasEntity(scene, "Hub_Status"))
                return;

            SetText(scene, "Hub_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Hub_Status", GameProgress::BuildHubStatus());
            SetText(scene, "Hub_Button_Dungeon", GameProgress::GetDungeonButtonText());
            SetText(scene, "Hub_Button_Skill", GameProgress::GetSkillButtonText());
            SetText(scene, "Hub_Button_Equip", GameProgress::GetEquipmentButtonText());
        }

        static void UpdateResult(Scene* scene)
        {
            if (!HasEntity(scene, "Result_Stats"))
                return;

            const auto& state = GameProgress::GetState();
            SetText(scene, "Result_Title", GameProgress::BuildResultTitle());
            SetText(scene, "Result_Stats", GameProgress::BuildResultStats());
            SetText(scene, "Result_Rewards", state.LastDungeonResult.Valid
                ? "掉落奖励\n悬停图标查看用途与数量。\n下一步：回据点升级，或者重刷练习连击。"
                : "还没有掉落记录。\n完成一个地下城后，这里会显示材料图标。");
            SetProgress(scene, "Result_EXPBar",
                static_cast<float>(state.Experience),
                static_cast<float>(state.ExperienceToNext));
            ProgressionResultPageService::UpdateDrops(scene);
        }

        static void UpdateSkillTree(Scene* scene)
        {
            if (!HasEntity(scene, "SkillTree_Status"))
                return;

            const auto& state = GameProgress::GetState();
            SetText(scene, "SkillTree_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "SkillTree_Status", GameProgress::BuildSkillTreeStatus());
            SetText(scene, "SkillTree_Details", GameProgress::BuildSkillTreeDetails());
            SetText(scene, "SkillTree_Materials", GameProgress::BuildSkillTreeMaterials());
            SetText(scene, "SkillTree_Button_LearnSelectedSkill", GameProgress::GetSkillTreeLearnButtonText());
            SetProgress(scene, "SkillTree_MagicSwordBar",
                static_cast<float>(state.MagicSwordLevel),
                2.0f);
            ProgressionSkillTreePageService::SyncView(scene);
        }

        static void UpdateEquipment(Scene* scene)
        {
            if (!HasEntity(scene, "Equipment_Status"))
                return;

            ProgressionEquipmentPageService::EnsureLayout(scene);
            ProgressionEquipmentPageService::SyncPager(scene);
            const auto& state = GameProgress::GetState();
            SetText(scene, "Equipment_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Equipment_Status", GameProgress::BuildEquipmentStatus());
            SetText(scene, "Equipment_Details", GameProgress::BuildEquipmentDetails());
            SetText(scene, "Equipment_PageText", GameProgress::BuildEquipmentPageText());
            SetText(scene, "Equipment_Materials", GameProgress::BuildEquipmentMaterials());
            SetText(scene, "Equipment_Button_UpgradeArmor", GameProgress::GetTravelerArmorUpgradeButtonText());
            SetProgress(scene, "Equipment_ArmorBar",
                static_cast<float>(state.TravelerArmorLevel),
                1.0f);
            ProgressionEquipmentPageService::UpdateItems(scene);
        }

        static void UpdateDungeonSelect(Scene* scene)
        {
            if (!HasEntity(scene, "Dungeon_Status"))
                return;

            SetText(scene, "Dungeon_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Dungeon_Status", GameProgress::BuildDungeonSelectStatus());
            SetText(scene, "Dungeon_Rewards", GameProgress::BuildDungeonSelectRewards());
        }

        static void UpdateRelationship(Scene* scene)
        {
            if (!HasEntity(scene, "Relationship_Status"))
                return;

            SetText(scene, "Relationship_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Relationship_Status", GameProgress::BuildRelationshipStatus());
        }

        static void UpdateSupport(Scene* scene)
        {
            if (!HasEntity(scene, "Support_Status"))
                return;

            SetText(scene, "Support_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Support_Status", GameProgress::BuildSupportStatus());
        }

        static void UpdateSettings(Scene* scene)
        {
            if (!HasEntity(scene, "Settings_Status"))
                return;

            ProgressionSettingsPageService::UpdateAudioControls(scene);
            SetText(scene, "Settings_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Settings_Status", GameProgress::BuildSettingsStatus());
        }

        static void UpdateSaveLoad(Scene* scene)
        {
            if (!HasEntity(scene, "SaveLoad_SlotScroll"))
                return;

            ProgressionSaveLoadPageService::EnsureLayout(
                scene,
                s_SaveLoadSaveMode,
                s_PendingOverwriteSlot,
                s_ActiveSaveDirectory);
        }

        static void UpdateProgressionPages(Scene* scene)
        {
            UpdateHub(scene);
            UpdateResult(scene);
            UpdateSkillTree(scene);
            UpdateEquipment(scene);
            UpdateDungeonSelect(scene);
            UpdateRelationship(scene);
            UpdateSupport(scene);
            UpdateSettings(scene);
            UpdateSaveLoad(scene);
        }

    } // namespace

    void ProgressionSystem::OnRuntimeStart(Scene* scene)
    {
        if (s_PendingSaveLoadMode)
        {
            s_SaveLoadSaveMode = *s_PendingSaveLoadMode;
            s_PendingSaveLoadMode.reset();
        }
        else
        {
            s_SaveLoadSaveMode = true;
        }

        if (s_PendingSaveDirectory)
        {
            s_ActiveSaveDirectory = s_PendingSaveDirectory->empty()
                ? "assets/saves"
                : *s_PendingSaveDirectory;
            s_PendingSaveDirectory.reset();
        }
        else
        {
            s_ActiveSaveDirectory = scene && !scene->GetSavePolicy().SaveDirectory.empty()
                ? scene->GetSavePolicy().SaveDirectory
                : "assets/saves";
        }

        s_PendingOverwriteSlot = 0;
        UpdateProgressionPages(scene);
    }

    void ProgressionSystem::OnUpdateRuntime(Scene* scene, Timestep)
    {
        UpdateGameSaveCommands(scene);
        UpdateProgressionPages(scene);
    }

} // namespace Wheatear
