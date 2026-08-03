#include "wtpch.h"
#include "ProgressionSaveLoadPageService.h"

#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <string>

namespace Wheatear::ProgressionSaveLoadPageService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetParent;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        using GameplayUILayoutService::EnsureButton;
        using GameplayUILayoutService::EnsureScrollView;
        using GameplayUILayoutService::EnsureText;
        using GameplayUILayoutService::SetButtonPalette;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

    } // namespace

    void EnsureLayout(Scene* scene)
    {
        if (!HasEntity(scene, "SaveLoad_Status"))
            return;

        EnsureScrollView(scene, "SaveLoad_StatusScroll", "WT_UI_Canvas",
            { 0.205f, 0.292f }, { 0.50f, 0.124f }, 36, 1.35f);
        SetWidgetParent(scene, "SaveLoad_Status", "SaveLoad_StatusScroll");
        SetWidgetTopLeft(scene, "SaveLoad_Status", { 0.025f, 0.025f }, { 0.90f, 1.04f });

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

        constexpr float slotStep = 0.178f;
        EnsureScrollView(scene, "SaveLoad_SlotScroll", "WT_UI_Canvas",
            { 0.125f, 0.455f }, { 0.60f, 0.265f }, 42,
            0.060f + static_cast<float>(GameProgress::GetMaxSaveSlots()) * slotStep);

        for (int slot = 1; slot <= GameProgress::GetMaxSaveSlots(); ++slot)
        {
            const std::string prefix = "SaveLoad_Slot_" + std::to_string(slot);
            const float y = 0.030f + static_cast<float>(slot - 1) * slotStep;
            EnsureText(scene,
                prefix + "_Summary",
                "SaveLoad_SlotScroll",
                { 0.030f, y },
                { 0.54f, 0.130f },
                50 + slot,
                GameProgress::BuildSaveSlotSummary(slot) + "\n" + GameProgress::BuildSaveSlotDetails(slot),
                15.0f,
                GameProgress::IsSaveSlotOccupied(slot)
                    ? glm::vec4{ 0.95f, 0.93f, 0.78f, 1.0f }
                    : glm::vec4{ 0.68f, 0.74f, 0.72f, 1.0f });
            EnsureButton(scene,
                prefix + "_Save",
                "SaveLoad_SlotScroll",
                { 0.610f, y + 0.018f },
                { 0.130f, 0.082f },
                70 + slot,
                "保存",
                "progression:save_slot" + std::to_string(slot));
            EnsureButton(scene,
                prefix + "_Load",
                "SaveLoad_SlotScroll",
                { 0.755f, y + 0.018f },
                { 0.130f, 0.082f },
                70 + slot,
                "读取",
                "progression:load_slot" + std::to_string(slot));

            SetButtonPalette(scene, prefix + "_Save",
                glm::vec4(0.26f, 0.18f, 0.10f, 0.90f),
                glm::vec4(0.50f, 0.34f, 0.16f, 0.98f),
                glm::vec4(0.18f, 0.11f, 0.06f, 1.0f));
            SetButtonPalette(scene, prefix + "_Load",
                GameProgress::IsSaveSlotOccupied(slot)
                    ? glm::vec4(0.10f, 0.25f, 0.24f, 0.90f)
                    : glm::vec4(0.08f, 0.09f, 0.10f, 0.58f),
                glm::vec4(0.18f, 0.46f, 0.42f, 0.98f),
                glm::vec4(0.06f, 0.16f, 0.16f, 1.0f));
        }
    }

} // namespace Wheatear::ProgressionSaveLoadPageService
