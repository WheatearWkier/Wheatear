#include "wtpch.h"
#include "ProgressionSystem.h"

#include "GameProgress.h"
#include "ProgressionEquipmentPageService.h"
#include "ProgressionResultPageService.h"
#include "ProgressionSaveLoadPageService.h"
#include "ProgressionSkillTreePageService.h"
#include "ProgressionSettingsPageService.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <string>

namespace Wheatear {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
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
                ? "掉落奖励\n悬停图标查看用途与背包数量。\n下一步：回据点升级，或重刷练习连击。"
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
            SetText(scene, "SkillTree_Status", GameProgress::BuildSkillTreeStatusV2());
            SetText(scene, "SkillTree_Details", GameProgress::BuildSkillTreeDetailsV2());
            SetText(scene, "SkillTree_Materials", GameProgress::BuildSkillTreeMaterialsV2());
            SetText(scene, "SkillTree_Button_UpgradeMagicSword", GameProgress::GetMagicSwordUpgradeButtonTextV2());
            SetProgress(scene, "SkillTree_MagicSwordBar",
                static_cast<float>(state.MagicSwordLevel),
                2.0f);
            if (!ProgressionSkillTreePageService::SyncView(scene))
            {
                ProgressionSkillTreePageService::UpdateDrag(scene);
                ProgressionSkillTreePageService::UpdateLegacyCanvas(scene);
            }
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
            if (!HasEntity(scene, "SaveLoad_Status"))
                return;

            ProgressionSaveLoadPageService::EnsureLayout(scene);
            SetText(scene, "SaveLoad_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "SaveLoad_Status", GameProgress::BuildSaveLoadStatus());
            SetText(scene, "SaveLoad_Button_1", GameProgress::GetSaveButtonText(1));
            SetText(scene, "SaveLoad_Button_2", GameProgress::GetLoadButtonText(1));
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
        ProgressionSkillTreePageService::ResetCache();
        UpdateProgressionPages(scene);
    }

    void ProgressionSystem::OnUpdateRuntime(Scene* scene, Timestep)
    {
        UpdateProgressionPages(scene);
    }

} // namespace Wheatear
