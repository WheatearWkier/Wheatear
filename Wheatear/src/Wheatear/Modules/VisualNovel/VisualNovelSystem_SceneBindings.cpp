#include "wtpch.h"
#include "VisualNovelSystem.h"
#include "VisualNovelSystemInternal.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRenderer.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    using namespace VisualNovelSystemInternal;

    void VisualNovelSystem::UpdateBGM(Scene* scene,
        const VisualNovelComponent& component,
        RuntimeState& state)
    {
        if (!state.Loaded)
            return;

        const std::string desiredPath = NormalizeAssetPath(state.Runtime.GetCurrentMusic());
        const std::string desiredTitle = ResolveMusicTitle(
            desiredPath,
            state.Runtime.GetCurrentMusicTitle());

        if (desiredPath == state.CurrentBGMPath)
        {
            if (state.BGMHandle != 0)
                GameplayAudioService::SetBGMVolume(state.BGMHandle, 0.82f);
            return;
        }

        if (state.BGMHandle != 0)
        {
            AudioEngine::StopSound(state.BGMHandle);
            state.BGMHandle = 0;
        }

        state.CurrentBGMPath = desiredPath;
        state.CurrentBGMTitle = desiredTitle;

        if (state.CurrentBGMPath.empty())
        {
            state.CurrentBGMTitle.clear();
            state.BGMNoticeTimer = 0.0f;
            return;
        }

        state.BGMHandle = GameplayAudioService::PlayBGM(
            state.CurrentBGMPath,
            0.82f,
            true);

        if (state.BGMHandle != 0)
            state.BGMNoticeTimer = state.BGMNoticeDuration;
        else
            state.BGMNoticeTimer = 0.0f;
    }
    void VisualNovelSystem::UpdateMusicNotice(Scene* scene,
        const VisualNovelComponent& component,
        RuntimeState& state,
        float deltaSeconds)
    {
        const bool visible = state.BGMNoticeTimer > 0.0f && !state.CurrentBGMTitle.empty();
        if (!visible)
        {
            SetWidgetVisible(scene, component.MusicNoticePanelEntityName, false);
            if (component.MusicNoticeTextEntityName != component.MusicNoticePanelEntityName)
                SetWidgetVisible(scene, component.MusicNoticeTextEntityName, false);
            return;
        }

        const float duration = std::max(0.01f, state.BGMNoticeDuration);
        const float elapsed = std::clamp(duration - state.BGMNoticeTimer, 0.0f, duration);
        const float normalized = elapsed / duration;
        float alpha = 1.0f;
        if (normalized < 0.14f)
            alpha = normalized / 0.14f;
        else if (normalized > 0.76f)
            alpha = std::max(0.0f, (1.0f - normalized) / 0.24f);

        const glm::vec2 panelSize = { 0.300f, 0.058f };
        const float slide = (1.0f - alpha) * (panelSize.x * 0.72f);
        const glm::vec2 panelPosition = { 0.030f - slide, 0.047f };
        const glm::vec2 textPosition = { panelPosition.x + 0.014f, panelPosition.y + 0.012f };
        const glm::vec2 textSize = { panelSize.x - 0.026f, panelSize.y - 0.016f };
        const std::string parentTag = FindFirstCanvasTag(scene);

        Entity panelEntity = EnsureNoticePanel(
            scene,
            component.MusicNoticePanelEntityName,
            parentTag,
            panelPosition,
            panelSize);
        const bool textOnPanel = component.MusicNoticeTextEntityName.empty()
            || component.MusicNoticeTextEntityName == component.MusicNoticePanelEntityName;
        Entity textEntity = textOnPanel
            ? EnsureNoticeText(
                scene,
                component.MusicNoticePanelEntityName,
                parentTag,
                textPosition,
                textSize)
            : EnsureNoticeText(
                scene,
                component.MusicNoticeTextEntityName,
                parentTag,
                textPosition,
                textSize);

        if (panelEntity && panelEntity.HasComponent<UIWidgetComponent>())
        {
            auto& widget = panelEntity.GetComponent<UIWidgetComponent>();
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = panelPosition;
            widget.Size = panelSize;
        }

        if (!textOnPanel && textEntity && textEntity.HasComponent<UIWidgetComponent>())
        {
            auto& widget = textEntity.GetComponent<UIWidgetComponent>();
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = textPosition;
            widget.Size = textSize;
        }

        if (panelEntity && panelEntity.HasComponent<UIPanelComponent>())
        {
            auto& panel = panelEntity.GetComponent<UIPanelComponent>();
            panel.BackgroundColor = { 0.025f, 0.052f, 0.070f, 0.0f };
            panel.BorderColor = { 0.45f, 0.88f, 0.94f, 0.0f };
        }

        if (panelEntity && panelEntity.HasComponent<UIImageComponent>())
        {
            auto& image = panelEntity.GetComponent<UIImageComponent>();
            image.Color = { 1.0f, 1.0f, 1.0f, 0.92f * alpha };
        }

        if (textEntity && textEntity.HasComponent<UITextComponent>())
        {
            auto& text = textEntity.GetComponent<UITextComponent>();
            const std::string notice = "音乐 " + state.CurrentBGMTitle;
            if (text.Text != notice)
            {
                text.Text = notice;
                UIRenderer::PreloadUIText(text);
            }
            text.Color = { 0.84f, 0.96f, 1.0f, alpha };
            text.OutlineColor = { 0.01f, 0.02f, 0.025f, 0.88f * alpha };
            text.ShadowColor = { 0.0f, 0.0f, 0.0f, 0.62f * alpha };
        }

        SetWidgetVisible(scene, component.MusicNoticePanelEntityName, alpha > 0.02f);
        if (!textOnPanel)
            SetWidgetVisible(scene, component.MusicNoticeTextEntityName, alpha > 0.02f);
    }
    void VisualNovelSystem::UpdateSceneBindings(Scene* scene,
        const VisualNovelComponent& component,
        RuntimeState& state)
    {
        const VisualNovelLine* line = state.Runtime.GetCurrentLine();
        const bool showStoryUi = !state.DialogueHidden && !state.ShowHistory && !state.ShowSettings && !state.ShowSaveLoad;
        const bool waitingForChoice = showStoryUi && state.Runtime.IsWaitingForChoice();

        UpdateVNSettingsAudioControls(scene, component, state.ShowSettings);

        SetWidgetsWithPrefixVisible(scene, "VN_Command", showStoryUi);
        SetWidgetsWithPrefixVisible(scene, "VN_History", state.ShowHistory);
        SetWidgetsWithPrefixVisible(scene, "VN_Settings", state.ShowSettings);
        SetWidgetsWithPrefixVisible(scene, "VN_SaveLoad", state.ShowSaveLoad);

        SetWidgetVisible(scene, component.CommandBarEntityName, showStoryUi);
        SetWidgetVisible(scene, component.HistoryPanelEntityName, state.ShowHistory);
        SetWidgetVisible(scene, component.SettingsPanelEntityName, state.ShowSettings);
        SetWidgetVisible(scene, component.SaveLoadPanelEntityName, state.ShowSaveLoad);
        SetText(scene, "VN_Command_Auto", state.Runtime.IsAutoPlay() ? "自动开" : "自动");

        ApplyVNCommandBar(scene,
            component,
            showStoryUi,
            state.Runtime.IsAutoPlay(),
            state.SkipMode,
            state.ShowSettings,
            state.ShowHistory);

        const std::string historyText = BuildHistoryText(state.Runtime);
        UpdateHistoryScroll(scene, component, state.Runtime, historyText, state.ShowHistory);
        SetTextVisible(scene, component.HistoryTextEntityName,
            historyText,
            state.ShowHistory);
        SetTextVisible(scene, component.SettingsTextEntityName,
            BuildSettingsText(component, state.Runtime),
            state.ShowSettings);
        EnsureVNSaveLoadLayout(scene,
            component,
            state.Runtime,
            state.ShowSaveLoad,
            state.SaveLoadSaveMode,
            state.PendingOverwriteSlot);

        SetTextVisible(scene,
            component.SystemMessageEntityName,
            state.SystemMessage,
            state.SystemMessageTimer > 0.0f && !state.DialogueHidden);
        UpdateVNCommandTooltip(scene, component, showStoryUi);

        if (!line)
        {
            SetWidgetVisible(scene, "VN_DialoguePanel", showStoryUi);
            SetText(scene, component.SpeakerTextEntityName, "");
            SetText(scene, component.BodyTextEntityName, "");
            SetText(scene, component.AdvanceHintEntityName, "");
            for (uint32_t i = 0; i < component.MaxVisibleChoices; ++i)
                SetWidgetVisible(scene, component.ChoiceEntityPrefix + std::to_string(i + 1), false);
            SetTextVisible(scene, component.AutoPlayIndicatorEntityName, "", false);
            return;
        }

        SetWidgetVisible(scene, "VN_DialoguePanel", showStoryUi);
        SetWidgetVisible(scene, component.SpeakerTextEntityName, showStoryUi && !waitingForChoice);
        SetWidgetVisible(scene, component.BodyTextEntityName, showStoryUi);
        SetWidgetVisible(scene, component.AdvanceHintEntityName, showStoryUi);

        const std::string speakerText = waitingForChoice ? "" : ResolveSpeakerDisplayName(state.Runtime, line->Speaker);
        PreloadTextForEntity(scene, component.SpeakerTextEntityName, speakerText);
        PreloadTextForEntity(scene, component.BodyTextEntityName, line->Text);
        SetText(scene, component.SpeakerTextEntityName, speakerText);
        SetText(scene, component.BodyTextEntityName, state.Runtime.GetVisibleText());

        std::string hint;
        if (state.SkipMode)
            hint = "快进中";
        else if (waitingForChoice)
            hint = "选项";
        else if (state.Runtime.IsLineComplete())
            hint = "继续";
        SetText(scene, component.AdvanceHintEntityName, hint);

        const auto& choices = state.Runtime.GetCurrentChoices();
        // Gated choices (RequiredFlag set but flag not in StoryFlags) are filtered
        // out here and in UpdateRuntimeInputs, so the Nth visible button stays
        // stable across hidden options instead of indexing choices by raw N.
        const std::vector<size_t> visibleIndices = CollectVisibleChoiceIndices(choices, state.Runtime);
        const uint32_t maxVisibleChoices = std::min<uint32_t>(
            component.MaxVisibleChoices,
            static_cast<uint32_t>(visibleIndices.size()));

        for (uint32_t i = 0; i < component.MaxVisibleChoices; ++i)
        {
            const std::string entityName = component.ChoiceEntityPrefix + std::to_string(i + 1);
            const bool visible = waitingForChoice && i < maxVisibleChoices;
            SetWidgetVisible(scene, entityName, visible);
            if (visible)
            {
                const size_t scriptIndex = visibleIndices[i];
                const std::string choiceText = std::to_string(i + 1) + ". " + choices[scriptIndex].Text;
                PreloadTextForEntity(scene, entityName, choiceText);
                SetText(scene, entityName, choiceText);
                SetButtonCommand(scene, entityName,
                    IsExternalChoiceCommand(choices[scriptIndex].TargetLabel) ? choices[scriptIndex].TargetLabel : "");
            }
            else
            {
                SetButtonCommand(scene, entityName, "");
            }
        }

        const std::string playModeIndicator = state.SkipMode
            ? "快进中"
            : (state.Runtime.IsAutoPlay() ? "自动" : "");
        SetTextVisible(scene,
            component.AutoPlayIndicatorEntityName,
            playModeIndicator,
            !playModeIndicator.empty() && showStoryUi);

        const std::string& background = state.Runtime.GetCurrentBackground();
        const bool backgroundHasTexture = TrySetSpriteTexture(scene, component.BackgroundEntityName, background);
        if (backgroundHasTexture)
        {
            SetSpriteColor(scene, component.BackgroundEntityName, { 1.0f, 1.0f, 1.0f, 1.0f });
            SetSpriteColor(scene, component.FloorEntityName, { 1.0f, 1.0f, 1.0f, 0.0f });
        }
        else
        {
            ClearSpriteTexture(scene, component.BackgroundEntityName);
            SetSpriteColor(scene, component.BackgroundEntityName, ResolveBackgroundColor(background));
            SetSpriteColor(scene, component.FloorEntityName, ResolveFloorColor(background));
        }

        std::unordered_set<std::string> visible;
        for (const std::string& name : state.Runtime.GetCurrentVisibleCharacters())
            visible.insert(name);

        const auto& expressions = state.Runtime.GetCurrentCharacterExpressions();
        for (const auto& character : state.Runtime.GetScript().GetCharacters())
        {
            const bool isVisible = visible.count(character.Name) > 0;
            const bool isSpeaker = line->Speaker == character.Name;
            const float alpha = isVisible ? (isSpeaker ? 1.0f : 0.58f) : 0.0f;

            auto expressionIt = expressions.find(character.Name);
            const std::string expression = expressionIt == expressions.end()
                ? "neutral"
                : expressionIt->second;

            const std::string entityName = component.CharacterEntityPrefix + character.Name;
            const std::string texturePath = ResolveCharacterTexturePath(character, expression);
            if (TryPlaySpriteAnimation(scene, entityName, expression))
            {
                SetSpriteColor(scene, entityName, { 1.0f, 1.0f, 1.0f, alpha });
            }
            else if (!texturePath.empty() && TrySetSpriteTexture(scene, entityName, texturePath))
            {
                SetSpriteColor(scene, entityName, { 1.0f, 1.0f, 1.0f, alpha });
            }
            else
            {
                ClearSpriteTexture(scene, entityName);
                SetSpriteColor(scene, entityName,
                    ResolveCharacterColor(character.Name, expression, alpha));
            }
        }
    }
} // namespace Wheatear
