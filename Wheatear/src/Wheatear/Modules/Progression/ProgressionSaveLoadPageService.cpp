#include "wtpch.h"
#include "ProgressionSaveLoadPageService.h"

#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <string>

namespace Wheatear::ProgressionSaveLoadPageService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetVisible;

        using GameplayUILayoutService::FindAuthoredButton;
        using GameplayUILayoutService::FindAuthoredScrollView;
        using GameplayUILayoutService::FindAuthoredText;
        using GameplayUILayoutService::SetButtonCommand;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        static void HideLegacyPagedSlots(Scene* scene)
        {
            SetWidgetVisible(scene, "SaveLoad_SlotCard_1", false);
            SetWidgetVisible(scene, "SaveLoad_SlotIcon_1", false);
            SetWidgetVisible(scene, "SaveLoad_Button_1", false);
            SetWidgetVisible(scene, "SaveLoad_Button_2", false);
            SetWidgetVisible(scene, "SaveLoad_SlotCard_2", false);
            SetWidgetVisible(scene, "SaveLoad_EmptySlotText", false);
            SetWidgetVisible(scene, "SaveLoad_Button_3", false);
            SetWidgetVisible(scene, "SaveLoad_Button_4", false);
            SetWidgetVisible(scene, "SaveLoad_PageText", false);
            SetWidgetVisible(scene, "SaveLoad_PagePrev", false);
            SetWidgetVisible(scene, "SaveLoad_PageNext", false);
            SetWidgetVisible(scene, "SaveLoad_LockedScroll", false);
        }

    } // namespace

    void EnsureLayout(Scene* scene)
    {
        if (!HasEntity(scene, "SaveLoad_Status"))
            return;

        if (HasEntity(scene, "SaveLoad_StatusScroll"))
            FindAuthoredScrollView(scene, "SaveLoad_StatusScroll");

        if (!HasEntity(scene, "SaveLoad_SlotScroll"))
            return;

        HideLegacyPagedSlots(scene);
        FindAuthoredScrollView(scene, "SaveLoad_SlotScroll");

        for (int slot = 1; slot <= GameProgress::GetMaxSaveSlots(); ++slot)
        {
            const std::string prefix = "SaveLoad_Slot_" + std::to_string(slot);
            if (FindAuthoredText(scene, prefix + "_Summary"))
            {
                SetText(scene,
                    prefix + "_Summary",
                    GameProgress::BuildSaveSlotSummary(slot) + "\n" + GameProgress::BuildSaveSlotDetails(slot));
            }

            if (FindAuthoredButton(scene, prefix + "_Save"))
                SetButtonCommand(scene, prefix + "_Save", "progression:save_slot" + std::to_string(slot));
            if (FindAuthoredButton(scene, prefix + "_Load"))
                SetButtonCommand(scene, prefix + "_Load", "progression:load_slot" + std::to_string(slot));
        }
    }

} // namespace Wheatear::ProgressionSaveLoadPageService