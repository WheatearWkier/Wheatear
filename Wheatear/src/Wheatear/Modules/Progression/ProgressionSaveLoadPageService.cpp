#include "wtpch.h"
#include "ProgressionSaveLoadPageService.h"

#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
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
            SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadConfirmPanel, visible);
            SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadConfirmText, visible);
            SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadConfirmYes, visible);
            SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadConfirmNo, visible);

            if (!visible)
                return;

            SetText(scene, SystemBindings::Progression::SaveLoadConfirmText,
                pendingOverwriteSlot > 0
                    ? "该槽位已有存档。\n是否覆盖 " + std::to_string(pendingOverwriteSlot) + " 号槽？"
                    : "");
            SetButtonCommand(scene, SystemBindings::Progression::SaveLoadConfirmYes, "gamesave:confirm_overwrite");
            SetButtonCommand(scene, SystemBindings::Progression::SaveLoadConfirmNo, "gamesave:cancel_overwrite");
        }

    } // namespace

    void EnsureLayout(Scene* scene, bool saveMode, int pendingOverwriteSlot, const std::string& saveDirectory)
    {
        if (!HasEntity(scene, SystemBindings::Progression::SaveLoadSlotScroll))
            return;

        const std::string resolvedSaveDirectory = saveDirectory.empty() ? "assets/saves" : saveDirectory;
        SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadMainPanel, true);
        SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadIcon, true);
        SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadTitle, true);
        SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadClose, true);
        SetWidgetVisible(scene, SystemBindings::Progression::SaveLoadSlotScroll, true);
        FindAuthoredScrollView(scene, SystemBindings::Progression::SaveLoadSlotScroll);
        FindAuthoredButton(scene, SystemBindings::Progression::SaveLoadClose);

        SetText(scene, SystemBindings::Progression::SaveLoadTitle, saveMode ? "保存" : "读取");
        SetText(scene, SystemBindings::Progression::SaveLoadClose, "关闭");
        SetButtonCommand(scene, SystemBindings::Progression::SaveLoadClose, "gamesave:close");

        for (int slot = 1; slot <= GameProgress::GetMaxSaveSlots(); ++slot)
        {
            const std::string entityName = SystemBindings::IndexedName(SystemBindings::Progression::SaveLoadSlotPrefix, slot);
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
