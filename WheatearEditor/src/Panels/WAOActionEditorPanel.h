#pragma once

#include "Editor/EditorToolRegistry.h"
#include "Wheatear/Gameplay/Action/ActionRunner.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
        bool DrawParamsEditor();
        void DrawEffectsTable();
        void DrawEffectEditor();
        void DrawValidationPanel();
        void DrawPreviewPanel();
        void DrawDebugLedger();
        void DrawActionSetsPanel();
        void LoadActionSetEditor();
        bool SaveActionSetEditor();
        void BeginEdit(const WAO::ActionRecipe& recipe);
        void CreateRecipeInSet(const std::string& setKey, const WAO::ActionRecipe* sourceRecipe = nullptr);
        void DuplicateSelectedRecipe();
        bool DeleteSelectedRecipe();
        bool SaveEditedRecipe();
        bool ReloadActionSources();

        // Runs the selected recipe in isolation against a synthetic ActionRuntime
        // (no Scene/ECS), surfacing the effect ledger + post-run attribute deltas
        // so designers can tune numbers without entering Play mode.
        void RunSandbox(const WAO::ActionRecipe& recipe);
        void DrawSandboxResult();

        // Id rename modal + project-wide dotted reference rewrite.
        void DrawRenameDialog(const WAO::ActionRecipe& recipe);
        bool PerformRename(const std::string& oldId, const std::string& newId);

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
        std::string m_NewActionSetKey;
        std::string m_SaveStatus;
        int m_SelectedEffectIndex = -1;
        bool m_ActionSetsLoaded = false;
        bool m_ActionSetsDirty = false;
        std::string m_ActionSetsStatus;
        std::string m_SelectedActionSetKey;

        // Sandbox run state (see RunSandbox).
        bool m_SandboxRan = false;
        std::string m_SandboxStatus;
        WAO::ActionExecutionResult m_SandboxResult;
        std::unordered_map<std::string, float> m_SandboxBefore;
        std::unordered_map<std::string, float> m_SandboxAfter;

        // Id rename state (see RenameActionId).
        bool m_RenameOpen = false;
        std::string m_RenameNewId;
        std::string m_RenameStatus;
    };

} // namespace Wheatear
