#pragma once

#include "Editor/EditorToolRegistry.h"
#include "Wheatear/Gameplay/Action/ActionTypes.h"

#include <cstdint>
#include <string>

namespace Wheatear {

    class WAOActionEditorPanel
    {
    public:
        void Open(const EditorToolContext& context);
        void OnImGuiRender();

    private:
        void DrawActionList();
        void DrawActionDetails();
        void DrawRecipeOverview();
        void DrawRecipeEditor();
        void DrawEffectsTable();
        void DrawEffectEditor();
        void DrawValidationPanel();
        void DrawPreviewPanel();
        void DrawDebugLedger();
        void BeginEdit(const WAO::ActionRecipe& recipe);
        bool SaveEditedRecipe();
        bool ReloadActionSources();

    private:
        bool m_Open = false;
        bool m_GroupByModule = true;
        char m_Filter[128] = {};
        std::string m_SelectedActionId;
        uint64_t m_SelectedRecordSequence = 0;
        bool m_EditMode = false;
        bool m_EditDirty = false;
        WAO::ActionRecipe m_EditRecipe;
        std::string m_EditingActionId;
        std::string m_SaveStatus;
        int m_SelectedEffectIndex = -1;
    };

} // namespace Wheatear
