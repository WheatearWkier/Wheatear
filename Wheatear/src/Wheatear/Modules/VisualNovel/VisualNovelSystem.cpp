#include "wtpch.h"
#include "VisualNovelSystem.h"

#include "VisualNovelInputService.h"
#include "VisualNovelSystemInternal.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Config/UserSettings.h"
#include "Wheatear/Gameplay/Services/GameplayAudioService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRenderer.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    using namespace VisualNovelSystemInternal;

    namespace {

        static const SavePolicy& GetSceneSavePolicy(Scene* scene)
        {
            static const SavePolicy s_DefaultPolicy{};
            return scene ? scene->GetSavePolicy() : s_DefaultPolicy;
        }

        static bool CanSceneSave(Scene* scene)
        {
            return GetSceneSavePolicy(scene).CanSave;
        }

        static bool CanSceneLoad(Scene* scene)
        {
            return GetSceneSavePolicy(scene).CanLoad;
        }

        static std::string GetSceneSaveDirectory(Scene* scene)
        {
            return GetSceneSavePolicy(scene).SaveDirectory;
        }

    } // namespace


    void VisualNovelSystem::OnRuntimeStart(Scene* scene)
    {
        for (auto e : scene->GetRegistry().view<VisualNovelComponent>())
        {
            Entity entity{ e, scene };
            auto& component = entity.GetComponent<VisualNovelComponent>();
            RuntimeState& state = GetState(entity.GetUUID());
            if (component.PlayOnStart)
                LoadRuntime(scene, state, component);
        }
    }

    void VisualNovelSystem::OnRuntimeStop(Scene* scene)
    {
        for (auto& [id, state] : m_RuntimeStates)
            StopBGM(state);
        m_RuntimeStates.clear();
    }

    void VisualNovelSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene || scene->GetRegistry().view<VisualNovelComponent>().empty())
            return;

        // Drain only VN commands here. Shared save/load commands are owned by
        // ProgressionSystem and can delegate back through HandleGameSaveCommand.
        const std::vector<std::string> vnCommands = CommandBus::DrainGameplayCommands("vn:");

        for (auto e : scene->GetRegistry().view<VisualNovelComponent>())
        {
            Entity entity{ e, scene };
            auto& component = entity.GetComponent<VisualNovelComponent>();
            RuntimeState& state = GetState(entity.GetUUID());

            const std::filesystem::path resolvedPath = AssetPath::ResolveRuntimeData(component.ScriptPath);
            const SavePolicy& policy = GetSceneSavePolicy(scene);
            if (!state.Loaded
                || state.LoadedPath != resolvedPath
                || state.LoadedAutoLoadSlot != policy.AutoLoadSlot
                || state.LoadedSaveDirectory != policy.SaveDirectory)
            {
                LoadRuntime(scene, state, component);
            }
            else
            {
                // Hot reload: if the .vn file changed on disk (edited in the VN
                // Script Editor), reload the script in place, preserving the
                // variable table so the designer sees edits immediately without
                // restarting Play mode. Throttled to 500 ms and guarded against
                // mid-save truncation (editor writes via truncate): a parse that
                // yields an empty script while the previous one was non-empty is
                // treated as an in-progress write and skipped.
                state.LastScriptCheckTime += ts.GetSeconds();
                if (state.LastScriptCheckTime >= 0.5f)
                {
                    state.LastScriptCheckTime = 0.0f;
                    std::error_code error;
                    const auto writeTime = std::filesystem::exists(resolvedPath, error)
                        ? std::filesystem::last_write_time(resolvedPath, error)
                        : std::filesystem::file_time_type{};
                    if (writeTime != state.LastScriptWriteTime)
                    {
                        const bool previousNonEmpty = !state.Runtime.GetScript().IsEmpty();
                        const auto variables = state.Runtime.GetVariables();
                        VisualNovelScript reloaded = VisualNovelScript::FromFile(resolvedPath);
                        if (reloaded.IsEmpty() && previousNonEmpty)
                        {
                            // Editor is mid-write (file truncated but not closed);
                            // keep playing the old script and re-check next tick.
                            state.LastScriptWriteTime = writeTime;
                            state.SystemMessage = "VN script saving... (hot reload deferred)";
                            state.SystemMessageTimer = 1.0f;
                        }
                        else
                        {
                            state.Runtime.SetScript(std::move(reloaded));
                            for (const auto& [name, value] : variables)
                                state.Runtime.SetVariable(name, value);
                            state.LastScriptWriteTime = writeTime;
                            state.SystemMessage = "VN script reloaded (hot).";
                            state.SystemMessageTimer = 2.0f;
                        }
                    }
                }
            }

            component.CharactersPerSecond = static_cast<float>(UserSettings::Get().TextSpeed);
            state.Runtime.SetCharactersPerSecond(component.CharactersPerSecond);
            state.Runtime.SetAutoPlayDelay(component.AutoPlayDelay);
            const float deltaSeconds = ts.GetSeconds();
            state.SystemMessageTimer = std::max(0.0f, state.SystemMessageTimer - deltaSeconds);
            state.BGMNoticeTimer = std::max(0.0f, state.BGMNoticeTimer - deltaSeconds);

            for (const std::string& command : vnCommands)
                ExecuteCommand(scene, component, state, command);

            UpdateInput(scene, component, state);

            const bool uiBlocksStory = state.ShowHistory || state.ShowSettings || state.ShowSaveLoad || state.DialogueHidden;
            if (uiBlocksStory)
            {
                StopSkip(state);
            }
            else if (state.SkipMode)
            {
                UpdateSkip(state, deltaSeconds);
            }
            else
            {
                state.Runtime.Update(deltaSeconds);
            }

            UpdateBGM(scene, component, state);
            UpdateSceneBindings(scene, component, state);
            UpdateMusicNotice(scene, component, state, deltaSeconds);
        }
    }

    VisualNovelSystem::RuntimeState& VisualNovelSystem::GetState(UUID id)
    {
        return m_RuntimeStates[id];
    }

    void VisualNovelSystem::StopSkip(RuntimeState& state)
    {
        state.SkipMode = false;
        state.SkipTimer = 0.0f;
    }

    void VisualNovelSystem::UpdateSkip(RuntimeState& state, float deltaSeconds)
    {
        if (!state.SkipMode || !state.Loaded)
            return;

        if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
        {
            StopSkip(state);
            return;
        }

        state.SkipTimer += std::max(0.0f, deltaSeconds);
        int steps = 0;
        while (state.SkipTimer >= kVNSkipStepInterval && steps < kVNMaxSkipStepsPerFrame)
        {
            state.SkipTimer -= kVNSkipStepInterval;

            if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
            {
                StopSkip(state);
                break;
            }

            state.Runtime.Advance();
            ++steps;

            if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
            {
                StopSkip(state);
                break;
            }
        }

        if (steps >= kVNMaxSkipStepsPerFrame)
            state.SkipTimer = std::min(state.SkipTimer, kVNSkipStepInterval);
    }

    void VisualNovelSystem::StopBGM(RuntimeState& state)
    {
        if (state.BGMHandle != 0)
            AudioEngine::StopSound(state.BGMHandle);

        state.BGMHandle = 0;
        state.CurrentBGMPath.clear();
        state.CurrentBGMTitle.clear();
        state.BGMNoticeTimer = 0.0f;
    }



    bool VisualNovelSystem::LoadRuntime(Scene* scene, RuntimeState& state, const VisualNovelComponent& component)
    {
        const SavePolicy& policy = GetSceneSavePolicy(scene);
        StopBGM(state);
        state.LoadedPath = AssetPath::ResolveRuntimeData(component.ScriptPath);
        state.Runtime.SetCharactersPerSecond(component.CharactersPerSecond);
        state.Runtime.SetAutoPlayDelay(component.AutoPlayDelay);
        state.Loaded = state.Runtime.LoadScript(state.LoadedPath);
        state.LoadedAutoLoadSlot = policy.AutoLoadSlot;
        state.LoadedSaveDirectory = policy.SaveDirectory;
        state.ShowHistory = false;
        state.ShowSettings = false;
        state.ShowSaveLoad = false;
        state.SaveLoadSaveMode = true;
        state.PendingOverwriteSlot = 0;
        state.DialogueHidden = false;
        StopSkip(state);
        state.PreviousChoicePressed.assign(9, false);

        if (!state.Loaded)
        {
            WT_CORE_WARN("VisualNovelSystem: failed to load script '{}'", state.LoadedPath.string());
            return false;
        }

        state.Runtime.SetAutoPlay(component.AutoPlayOnStart);

        std::error_code error;
        state.LastScriptWriteTime = std::filesystem::exists(state.LoadedPath, error)
            ? std::filesystem::last_write_time(state.LoadedPath, error)
            : std::filesystem::file_time_type{};

        if (policy.CanLoad && policy.AutoLoadSlot > 0)
        {
            const std::filesystem::path savePath = BuildSavePath(scene, policy.AutoLoadSlot);
            const bool runtimeLoaded = state.Runtime.LoadState(savePath);
            if (runtimeLoaded || GameProgress::IsGameSaveSlotOccupied(policy.AutoLoadSlot, policy.SaveDirectory))
                GameProgress::LoadSlot(policy.AutoLoadSlot);
        }

        return true;
    }

    bool VisualNovelSystem::HandleGameSaveCommand(Scene* scene, const std::string& command)
    {
        if (!scene || !StartsWith(command, "gamesave:"))
            return false;

        bool handled = false;
        for (auto e : scene->GetRegistry().view<VisualNovelComponent>())
        {
            Entity entity{ e, scene };
            RuntimeState& state = GetState(entity.GetUUID());
            handled = ExecuteGameSaveCommand(scene, state, command) || handled;
        }
        return handled;
    }

    void VisualNovelSystem::PushSystemMessage(RuntimeState& state, const std::string& message)
    {
        state.SystemMessage = message;
        state.SystemMessageTimer = 2.0f;
    }

    void VisualNovelSystem::SaveToSlot(Scene* scene,
        RuntimeState& state,
        int slot,
        bool allowOverwrite)
    {
        StopSkip(state);
        if (!CanSceneSave(scene))
        {
            PushSystemMessage(state, "当前场景禁止保存。");
            return;
        }

        const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
        if (!allowOverwrite && HasAnySaveSlotData(scene, safeSlot))
        {
            state.PendingOverwriteSlot = safeSlot;
            PushSystemMessage(state, "该槽位已有存档，是否覆盖 " + std::to_string(safeSlot) + " 号槽？");
            return;
        }

        const std::filesystem::path savePath = BuildSavePath(scene, safeSlot);
        const bool vnSaved = state.Runtime.SaveState(savePath);
        const bool progressSaved = GameProgress::SaveSlot(safeSlot);
        state.PendingOverwriteSlot = 0;

        if (vnSaved && progressSaved)
        {
            state.ShowSaveLoad = false;
            PushSystemMessage(state, "已保存到 " + std::to_string(safeSlot) + " 号槽。");
        }
        else if (vnSaved)
        {
            PushSystemMessage(state, "已保存到 " + std::to_string(safeSlot) + " 号槽。");
        }
        else
        {
            PushSystemMessage(state, "保存失败。");
        }
    }

    void VisualNovelSystem::LoadFromSlot(Scene* scene,
        RuntimeState& state,
        int slot)
    {
        StopSkip(state);
        if (!CanSceneLoad(scene))
        {
            PushSystemMessage(state, "当前场景禁止读取。");
            return;
        }

        const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
        if (!GameProgress::IsGameSaveSlotOccupied(safeSlot, GetSceneSaveDirectory(scene)))
        {
            PushSystemMessage(state, "槽位 " + std::to_string(safeSlot) + " 没有存档。");
            return;
        }

        state.ShowSaveLoad = false;
        state.PendingOverwriteSlot = 0;
        const CommandResult result = CommandBus::Execute(scene, GameProgress::BuildLoadGameCommand(safeSlot));
        if (result.Success)
            PushSystemMessage(state, "正在读取 " + std::to_string(safeSlot) + " 号槽。");
        else
            PushSystemMessage(state, result.Message.empty() ? "读取失败。" : result.Message);
    }

    bool VisualNovelSystem::ExecuteGameSaveCommand(Scene* scene,
        RuntimeState& state,
        const std::string& command)
    {
        if (!StartsWith(command, "gamesave:"))
            return false;

        const std::string action = ToLower(command.substr(9));
        if (action.rfind("slot_save_", 0) == 0)
        {
            state.SaveLoadSaveMode = true;
            SaveToSlot(scene, state, ParseVNSaveSlot(action.substr(10)), false);
            return true;
        }

        if (action.rfind("load_", 0) == 0)
        {
            state.SaveLoadSaveMode = false;
            state.PendingOverwriteSlot = 0;
            LoadFromSlot(scene, state, ParseVNSaveSlot(action.substr(5)));
            return true;
        }

        return false;
    }

    bool VisualNovelSystem::ExecuteCommand(Scene* scene,
        VisualNovelComponent& component,
        RuntimeState& state,
        const std::string& command)
    {
        if (!StartsWith(command, "vn:"))
            return false;

        const std::string action = ToLower(command.substr(3));

        if (action == "auto")
        {
            StopSkip(state);
            state.Runtime.ToggleAutoPlay();
            state.DialogueHidden = false;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            PushSystemMessage(state, state.Runtime.IsAutoPlay() ? "自动播放已开启。" : "自动播放已关闭。");
            return true;
        }

        if (action == "history")
        {
            StopSkip(state);
            state.ShowHistory = !state.ShowHistory;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "settings")
        {
            StopSkip(state);
            state.ShowSettings = !state.ShowSettings;
            state.ShowHistory = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "close")
        {
            StopSkip(state);
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            return true;
        }

        if (action == "hide")
        {
            StopSkip(state);
            state.DialogueHidden = true;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            return true;
        }

        if (action == "savemenu" || action == "loadmenu")
        {
            StopSkip(state);
            const bool saveMode = action == "savemenu";
            if (saveMode && !CanSceneSave(scene))
            {
                state.ShowSaveLoad = false;
                state.PendingOverwriteSlot = 0;
                PushSystemMessage(state, "当前场景禁止保存。");
                return true;
            }
            if (!saveMode && !CanSceneLoad(scene))
            {
                state.ShowSaveLoad = false;
                state.PendingOverwriteSlot = 0;
                PushSystemMessage(state, "当前场景禁止读取。");
                return true;
            }

            state.ShowSaveLoad = true;
            state.SaveLoadSaveMode = saveMode;
            state.PendingOverwriteSlot = 0;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "confirm_overwrite")
        {
            if (state.PendingOverwriteSlot > 0)
                SaveToSlot(scene, state, state.PendingOverwriteSlot, true);
            return true;
        }

        if (action == "cancel_overwrite")
        {
            StopSkip(state);
            state.PendingOverwriteSlot = 0;
            PushSystemMessage(state, "已取消覆盖。");
            return true;
        }

        if (action == "textspeed+" || action == "speed+")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::min(180, settings.TextSpeed + 12);
            UserSettings::Save();
            UserSettings::ApplyToRuntime();
            component.CharactersPerSecond = static_cast<float>(settings.TextSpeed);
            PushSystemMessage(state, "文字速度已提高。");
            return true;
        }

        if (action == "textspeed-" || action == "speed-")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::max(12, settings.TextSpeed - 12);
            UserSettings::Save();
            UserSettings::ApplyToRuntime();
            component.CharactersPerSecond = static_cast<float>(settings.TextSpeed);
            PushSystemMessage(state, "文字速度已降低。");
            return true;
        }

        if (action == "autodelay+")
        {
            component.AutoPlayDelay = std::min(6.0f, component.AutoPlayDelay + 0.25f);
            PushSystemMessage(state, "自动播放延迟已增加。");
            return true;
        }

        if (action == "autodelay-")
        {
            component.AutoPlayDelay = std::max(0.4f, component.AutoPlayDelay - 0.25f);
            PushSystemMessage(state, "自动播放延迟已减少。");
            return true;
        }

        if (action == "advance")
        {
            StopSkip(state);
            if (component.RestartOnFinish || !state.Runtime.IsFinished())
                state.Runtime.Advance();
            return true;
        }

        if (action == "skip")
        {
            if (state.SkipMode)
            {
                StopSkip(state);
                PushSystemMessage(state, "快进已关闭。");
                return true;
            }

            if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
            {
                StopSkip(state);
                return true;
            }

            state.Runtime.SetAutoPlay(false);
            state.DialogueHidden = false;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.SkipMode = true;
            state.SkipTimer = kVNSkipStepInterval;
            PushSystemMessage(state, "快进已开启，遇到选项会自动停止。");
            return true;
        }

        return false;
    }

    void VisualNovelSystem::UpdateInput(Scene* scene,
        VisualNovelComponent& component,
        RuntimeState& state)
    {
        if (!state.Loaded)
            return;

        if (state.PreviousChoicePressed.size() < 9)
            state.PreviousChoicePressed.assign(9, false);

        const VisualNovelInputService::InputSnapshot input = VisualNovelInputService::Sample();
        const bool commandButtonMousePressed = input.PrimaryMousePressed && IsAnyVNCommandButtonHovered(scene);
        const bool advancePressed = input.AdvanceActionPressed || (input.PrimaryMousePressed && !commandButtonMousePressed);
        const bool commandPressed = input.PrimaryMousePressed;
        state.PreviousCommandPressed = commandPressed;

        if (state.DialogueHidden)
        {
            StopSkip(state);
            const bool pressed = advancePressed;
            if (pressed && !state.PreviousAdvancePressed)
                state.DialogueHidden = false;
            state.PreviousAdvancePressed = pressed;
            return;
        }

        const bool autoPressed = input.AutoPressed;
        if (autoPressed && !state.PreviousAutoPressed)
            ExecuteCommand(scene, component, state, "vn:auto");
        state.PreviousAutoPressed = autoPressed;

        const bool historyPressed = input.HistoryPressed;
        if (historyPressed && !state.PreviousHistoryPressed)
            ExecuteCommand(scene, component, state, "vn:history");
        state.PreviousHistoryPressed = historyPressed;

        const bool savePressed = input.SavePressed;
        if (savePressed && !state.PreviousSavePressed)
            ExecuteCommand(scene, component, state, "vn:save");
        state.PreviousSavePressed = savePressed;

        const bool loadPressed = input.LoadPressed;
        if (loadPressed && !state.PreviousLoadPressed)
            ExecuteCommand(scene, component, state, "vn:load");
        state.PreviousLoadPressed = loadPressed;

        if (state.ShowHistory || state.ShowSettings || state.ShowSaveLoad)
        {
            StopSkip(state);
            state.PreviousAdvancePressed = advancePressed;
            return;
        }

        if (state.Runtime.IsWaitingForChoice())
        {
            StopSkip(state);
            const auto& choices = state.Runtime.GetCurrentChoices();
            // Visible indices already exclude gated-out options; input maps a
            // displayed N to the underlying script choice so Choose advances to
            // the correct TargetLabel even when earlier options are hidden.
            const std::vector<size_t> visibleIndices = CollectVisibleChoiceIndices(choices, state.Runtime);
            const size_t maxChoices = std::min<size_t>(visibleIndices.size(), 9);

            for (size_t i = 0; i < maxChoices; ++i)
            {
                const bool pressed = input.ChoicePressed[i];
                if (pressed && !state.PreviousChoicePressed[i])
                {
                    const size_t scriptIndex = visibleIndices[i];
                    if (IsExternalChoiceCommand(choices[scriptIndex].TargetLabel))
                        component.RuntimeRequestedCommand = choices[scriptIndex].TargetLabel;
                    else
                        state.Runtime.Choose(scriptIndex);
                    break;
                }
                state.PreviousChoicePressed[i] = pressed;
            }

            const bool mousePressed = input.PrimaryMousePressed && !commandButtonMousePressed;
            if (mousePressed && !state.PreviousAdvancePressed)
            {
                for (size_t i = 0; i < maxChoices; ++i)
                {
                    if (IsEntityHoveredButton(scene, component.ChoiceEntityPrefix + std::to_string(i + 1)))
                    {
                        const size_t scriptIndex = visibleIndices[i];
                        if (IsExternalChoiceCommand(choices[scriptIndex].TargetLabel))
                            component.RuntimeRequestedCommand = choices[scriptIndex].TargetLabel;
                        else
                            state.Runtime.Choose(scriptIndex);
                        break;
                    }
                }
            }

            state.PreviousAdvancePressed = advancePressed;
            return;
        }

        if (state.SkipMode)
        {
            const bool pressed = advancePressed;
            if (pressed && !state.PreviousAdvancePressed)
                StopSkip(state);
            state.PreviousAdvancePressed = pressed;
            return;
        }

        const bool pressed = advancePressed;
        if (pressed && !state.PreviousAdvancePressed)
        {
            if (component.RestartOnFinish || !state.Runtime.IsFinished())
                state.Runtime.Advance();
        }
        state.PreviousAdvancePressed = pressed;
    }

    std::vector<size_t> VisualNovelSystem::CollectVisibleChoiceIndices(
        const std::vector<VisualNovelChoice>& choices,
        const VisualNovelRuntime& runtime)
    {
        std::vector<size_t> indices;
        indices.reserve(choices.size());
        const auto& flags = GameProgress::GetState().StoryFlags;
        for (size_t i = 0; i < choices.size(); ++i)
        {
            const VisualNovelChoice& choice = choices[i];
            bool visible = true;
            if (!choice.RequiredFlag.empty())
                visible = flags.count(choice.RequiredFlag) > 0;
            else if (!choice.RequiredCondition.empty())
                visible = EvaluateVNExpression(choice.RequiredCondition, runtime.GetVariables());
            if (visible)
                indices.push_back(i);
        }
        return indices;
    }


} // namespace Wheatear
