#include "wtpch.h"
#include "ProgressionSettingsPageService.h"

#include "GameProgress.h"
#include "Wheatear/Core/UserSettings.h"
#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <string>

namespace Wheatear::ProgressionSettingsPageService {

    namespace {

        using SceneQueries::FindEntityByName;

        using GameplayUILayoutService::EnsureButton;
        using GameplayUILayoutService::EnsureSlider;
        using GameplayUILayoutService::EnsureText;
        using GameplayUILayoutService::SetButtonCommand;
        using GameplayUILayoutService::SetSlider;
        using GameplayUILayoutService::SetSliderCommand;

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

            auto ensureTextIfMissing = [&](const std::string& name, glm::vec2 position, const std::string& value)
            {
                if (!HasEntity(scene, name))
                    EnsureText(scene, name, parentTag, position, labelSize, 42, value, 20.0f, labelColor);
            };

            auto ensureSliderOrBind = [&](const std::string& name, glm::vec2 position, const std::string& command)
            {
                if (HasEntity(scene, name))
                    SetSliderCommand(scene, name, command);
                else
                    EnsureSlider(scene, name, parentTag, position, sliderSize, 44, 0.0f, 100.0f, command);
            };

            auto ensureButtonOrBind = [&](const std::string& name, glm::vec2 position, const std::string& label, const std::string& command)
            {
                if (HasEntity(scene, name))
                    SetButtonCommand(scene, name, command);
                else
                    EnsureButton(scene, name, parentTag, position, buttonSize, 55, label, command);
            };

            ensureTextIfMissing("Settings_MasterVolumeLabel", { 0.13f, 0.39f }, "主音量");
            ensureSliderOrBind("Settings_MasterVolumeSlider", { 0.31f, 0.395f }, "progression:set_master_volume");
            ensureButtonOrBind("Settings_Button_VolumeDown", { 0.58f, 0.38f }, "-", "progression:master_volume_down");
            ensureButtonOrBind("Settings_Button_VolumeUp", { 0.635f, 0.38f }, "+", "progression:master_volume_up");

            ensureTextIfMissing("Settings_BGMVolumeLabel", { 0.13f, 0.47f }, "音乐");
            ensureSliderOrBind("Settings_BGMVolumeSlider", { 0.31f, 0.475f }, "progression:set_bgm_volume");
            ensureButtonOrBind("Settings_Button_BGMDown", { 0.58f, 0.46f }, "-", "progression:bgm_volume_down");
            ensureButtonOrBind("Settings_Button_BGMUp", { 0.635f, 0.46f }, "+", "progression:bgm_volume_up");

            ensureTextIfMissing("Settings_SFXVolumeLabel", { 0.13f, 0.55f }, "音效");
            ensureSliderOrBind("Settings_SFXVolumeSlider", { 0.31f, 0.555f }, "progression:set_sfx_volume");
            ensureButtonOrBind("Settings_Button_SFXDown", { 0.58f, 0.54f }, "-", "progression:sfx_volume_down");
            ensureButtonOrBind("Settings_Button_SFXUp", { 0.635f, 0.54f }, "+", "progression:sfx_volume_up");
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
