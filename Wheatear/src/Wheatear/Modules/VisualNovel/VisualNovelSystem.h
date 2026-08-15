#pragma once

#include "Wheatear/Core/UUID.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelComponents.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelRuntime.h"
#include "Wheatear/Systems/ISystem.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    struct VisualNovelComponent;

    class VisualNovelSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        bool HandleGameSaveCommand(Scene* scene, const std::string& command);

    private:
        struct RuntimeState
        {
            VisualNovelRuntime Runtime;
            std::filesystem::path LoadedPath;
            std::filesystem::file_time_type LastScriptWriteTime{};
            float LastScriptCheckTime = 0.0f;
            bool Loaded = false;
            bool PreviousAdvancePressed = false;
            bool PreviousAutoPressed = false;
            bool PreviousHistoryPressed = false;
            bool PreviousSavePressed = false;
            bool PreviousLoadPressed = false;
            bool PreviousCommandPressed = false;
            std::vector<bool> PreviousChoicePressed;
            bool ShowHistory = false;
            bool ShowSettings = false;
            bool ShowSaveLoad = false;
            bool SaveLoadSaveMode = true;
            int PendingOverwriteSlot = 0;
            bool DialogueHidden = false;
            bool SkipMode = false;
            float SkipTimer = 0.0f;
            float SystemMessageTimer = 0.0f;
            std::string SystemMessage;
            int LoadedAutoLoadSlot = 0;
            uint32_t BGMHandle = 0;
            std::string CurrentBGMPath;
            std::string CurrentBGMTitle;
            float BGMNoticeTimer = 0.0f;
            float BGMNoticeDuration = 3.4f;
        };

        RuntimeState& GetState(UUID id);
        bool LoadRuntime(RuntimeState& state, const VisualNovelComponent& component);
        void UpdateInput(Scene* scene, VisualNovelComponent& component, RuntimeState& state);
        bool ExecuteCommand(Scene* scene, VisualNovelComponent& component, RuntimeState& state, const std::string& command);
        bool ExecuteGameSaveCommand(Scene* scene, VisualNovelComponent& component, RuntimeState& state, const std::string& command);
        void PushSystemMessage(RuntimeState& state, const std::string& message);
        void SaveToSlot(Scene* scene, VisualNovelComponent& component, RuntimeState& state, int slot, bool allowOverwrite);
        void LoadFromSlot(Scene* scene, VisualNovelComponent& component, RuntimeState& state, int slot);
        void UpdateSceneBindings(Scene* scene, const VisualNovelComponent& component, RuntimeState& state);
        void StopSkip(RuntimeState& state);
        void UpdateSkip(RuntimeState& state, float deltaSeconds);
        void StopBGM(RuntimeState& state);
        void UpdateBGM(Scene* scene, const VisualNovelComponent& component, RuntimeState& state);
        void UpdateMusicNotice(Scene* scene, const VisualNovelComponent& component, RuntimeState& state, float deltaSeconds);

        // Returns the script indices of choices whose gate passes: RequiredFlag
        // is set in the progression StoryFlags, or RequiredCondition evaluates
        // true via EvaluateVNExpression against the runtime variable table.
        // A choice with no gate is always visible. Render and input both consume
        // this so the Nth visible button always maps to the same choice
        // regardless of which options are gated out.
        static std::vector<size_t> CollectVisibleChoiceIndices(
            const std::vector<VisualNovelChoice>& choices,
            const VisualNovelRuntime& runtime);

    private:
        std::unordered_map<UUID, RuntimeState> m_RuntimeStates;
    };

} // namespace Wheatear
