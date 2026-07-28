#pragma once

#include "Wheatear/Core/UUID.h"
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

    private:
        struct RuntimeState
        {
            VisualNovelRuntime Runtime;
            std::filesystem::path LoadedPath;
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
            bool DialogueHidden = false;
            float SystemMessageTimer = 0.0f;
            std::string SystemMessage;
            int LoadedAutoLoadSlot = 0;
        };

        RuntimeState& GetState(UUID id);
        bool LoadRuntime(RuntimeState& state, const VisualNovelComponent& component);
        void UpdateInput(Scene* scene, VisualNovelComponent& component, RuntimeState& state);
        bool ExecuteHoveredCommand(Scene* scene, VisualNovelComponent& component, RuntimeState& state);
        bool ExecuteCommand(Scene* scene, VisualNovelComponent& component, RuntimeState& state, const std::string& command);
        void UpdateSceneBindings(Scene* scene, const VisualNovelComponent& component, RuntimeState& state);

    private:
        std::unordered_map<UUID, RuntimeState> m_RuntimeStates;
    };

} // namespace Wheatear
