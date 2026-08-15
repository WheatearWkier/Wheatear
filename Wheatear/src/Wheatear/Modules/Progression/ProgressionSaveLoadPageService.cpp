#include "wtpch.h"
#include "ProgressionSaveLoadPageService.h"

#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
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
        using GameplayUILayoutService::SetButtonCommand;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        static void UpdateConfirmPanel(Scene* scene, bool visible, int pendingOverwriteSlot)
        {
            SetWidgetVisible(scene, "SaveLoad_ConfirmPanel", visible);
            SetWidgetVisible(scene, "SaveLoad_ConfirmText", visible);
            SetWidgetVisible(scene, "SaveLoad_ConfirmYes", visible);
            SetWidgetVisible(scene, "SaveLoad_ConfirmNo", visible);

            if (!visible)
                return;

            SetText(scene, "SaveLoad_ConfirmText",
                pendingOverwriteSlot > 0
                    ? "该槽位已有存档。\n是否覆盖 " + std::to_string(pendingOverwriteSlot) + " 号槽？"
                    : "");
            SetButtonCommand(scene, "SaveLoad_ConfirmYes", "gamesave:confirm_overwrite");
            SetButtonCommand(scene, "SaveLoad_ConfirmNo", "gamesave:cancel_overwrite");
        }

    } // namespace

    void EnsureLayout(Scene* scene, bool saveMode, int pendingOverwriteSlot, const std::string& saveDirectory)
    {
        if (!HasEntity(scene, "SaveLoad_SlotScroll"))
            return;

        const std::string resolvedSaveDirectory = saveDirectory.empty() ? "assets/saves" : saveDirectory;
        SetWidgetVisible(scene, "SaveLoad_MainPanel", true);
        SetWidgetVisible(scene, "SaveLoad_Icon", true);
        SetWidgetVisible(scene, "SaveLoad_Title", true);
        SetWidgetVisible(scene, "SaveLoad_Close", true);
        SetWidgetVisible(scene, "SaveLoad_SlotScroll", true);
        FindAuthoredScrollView(scene, "SaveLoad_SlotScroll");
        FindAuthoredButton(scene, "SaveLoad_Close");

        SetText(scene, "SaveLoad_Title", saveMode ? "保存" : "读取");
        SetText(scene, "SaveLoad_Close", "关闭");
        SetButtonCommand(scene, "SaveLoad_Close", "gamesave:close");

        for (int slot = 1; slot <= GameProgress::GetMaxSaveSlots(); ++slot)
        {
            const std::string entityName = "SaveLoad_Slot_" + std::to_string(slot);
            if (!FindAuthoredButton(scene, entityName))
                continue;

            SetWidgetVisible(scene, entityName, true);
            SetText(scene, entityName, GameProgress::BuildGameSaveSlotButtonText(slot, saveMode, resolvedSaveDirectory));
            SetButtonCommand(scene, entityName,
                saveMode
                    ? "gamesave:slot_save_" + std::to_string(slot)
                    : "gamesave:load_" + std::to_string(slot));
        }

        UpdateConfirmPanel(scene, saveMode && pendingOverwriteSlot > 0, pendingOverwriteSlot);
    }

} // namespace Wheatear::ProgressionSaveLoadPageService
