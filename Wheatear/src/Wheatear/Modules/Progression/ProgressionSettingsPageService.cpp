#include "wtpch.h"
#include "ProgressionSettingsPageService.h"

#include "GameProgress.h"
#include "Wheatear/Core/UserSettings.h"
#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <string>

namespace Wheatear::ProgressionSettingsPageService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetWidgetTopLeft;

        using GameplayUILayoutService::EnsureButton;
        using GameplayUILayoutService::EnsureSlider;
        using GameplayUILayoutService::EnsureText;
        using GameplayUILayoutService::SetSlider;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        static void EnsureAudioControlLayout(Scene* scene)
        {
            if (!HasEntity(scene, "Settings_ControlPanel"))
                return;

            const std::string parentTag = "WT_UI_Canvas";
            const glm::vec4 labelColor = { 0.88f, 1.0f, 0.9f, 1.0f };
            const glm::vec2 labelSize = { 0.18f, 0.035f };
            const glm::vec2 sliderSize = { 0.25f, 0.035f };
            const glm::vec2 buttonSize = { 0.045f, 0.045f };

            EnsureText(scene, "Settings_MasterVolumeLabel", parentTag, { 0.13f, 0.39f }, labelSize, 42, "主音量", 20.0f, labelColor);
            EnsureSlider(scene, "Settings_MasterVolumeSlider", parentTag, { 0.31f, 0.395f }, sliderSize, 44, 0.0f, 100.0f, "progression:set_master_volume");
            EnsureButton(scene, "Settings_Button_VolumeDown", parentTag, { 0.58f, 0.38f }, buttonSize, 55, "-", "progression:master_volume_down");
            EnsureButton(scene, "Settings_Button_VolumeUp", parentTag, { 0.635f, 0.38f }, buttonSize, 55, "+", "progression:master_volume_up");

            EnsureText(scene, "Settings_BGMVolumeLabel", parentTag, { 0.13f, 0.47f }, labelSize, 42, "音乐", 20.0f, labelColor);
            EnsureSlider(scene, "Settings_BGMVolumeSlider", parentTag, { 0.31f, 0.475f }, sliderSize, 44, 0.0f, 100.0f, "progression:set_bgm_volume");
            EnsureButton(scene, "Settings_Button_BGMDown", parentTag, { 0.58f, 0.46f }, buttonSize, 55, "-", "progression:bgm_volume_down");
            EnsureButton(scene, "Settings_Button_BGMUp", parentTag, { 0.635f, 0.46f }, buttonSize, 55, "+", "progression:bgm_volume_up");

            EnsureText(scene, "Settings_SFXVolumeLabel", parentTag, { 0.13f, 0.55f }, labelSize, 42, "音效", 20.0f, labelColor);
            EnsureSlider(scene, "Settings_SFXVolumeSlider", parentTag, { 0.31f, 0.555f }, sliderSize, 44, 0.0f, 100.0f, "progression:set_sfx_volume");
            EnsureButton(scene, "Settings_Button_SFXDown", parentTag, { 0.58f, 0.54f }, buttonSize, 55, "-", "progression:sfx_volume_down");
            EnsureButton(scene, "Settings_Button_SFXUp", parentTag, { 0.635f, 0.54f }, buttonSize, 55, "+", "progression:sfx_volume_up");

            SetWidgetTopLeft(scene, "Settings_Button_Shake", { 0.13f, 0.635f }, { 0.22f, 0.052f });
            SetWidgetTopLeft(scene, "Settings_Button_Fullscreen", { 0.38f, 0.635f }, { 0.22f, 0.052f });
        }

    } // namespace

    void UpdateAudioControls(Scene* scene)
    {
        EnsureAudioControlLayout(scene);

        const auto& settings = UserSettings::Get();
        SetSlider(scene, "Settings_TextSpeedSlider", static_cast<float>(settings.TextSpeed), 12.0f, 180.0f);
        SetSlider(scene, "Settings_MasterVolumeSlider", static_cast<float>(settings.MasterVolume), 0.0f, 100.0f);
        SetSlider(scene, "Settings_BGMVolumeSlider", static_cast<float>(settings.BGMVolume), 0.0f, 100.0f);
        SetSlider(scene, "Settings_SFXVolumeSlider", static_cast<float>(settings.SFXVolume), 0.0f, 100.0f);
    }

} // namespace Wheatear::ProgressionSettingsPageService
