#include "wtpch.h"
#include "ProgressionSettingsPageService.h"

#include "GameProgress.h"
#include "Wheatear/Config/UserSettings.h"
#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <array>
#include <string>

namespace Wheatear::ProgressionSettingsPageService {

    namespace {

        using SceneQueries::FindEntityByName;

        using GameplayUILayoutService::FindAuthoredButton;
        using GameplayUILayoutService::FindAuthoredSlider;
        using GameplayUILayoutService::FindAuthoredText;
        using GameplayUILayoutService::SetButtonCommand;
        using GameplayUILayoutService::SetSliderCommand;
        using GameplayUILayoutService::SetSliderValue;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        struct ControlBinding
        {
            const char* EntityName;
            const char* Command;
        };

        static void BindAuthoredAudioControls(Scene* scene)
        {
            if (!HasEntity(scene, "Settings_ControlPanel"))
                return;

            for (const char* label : {
                "Settings_MasterVolumeLabel",
                "Settings_BGMVolumeLabel",
                "Settings_SFXVolumeLabel"
            })
            {
                FindAuthoredText(scene, label);
            }

            constexpr std::array<ControlBinding, 3> sliders = {{
                { "Settings_MasterVolumeSlider", "progression:set_master_volume" },
                { "Settings_BGMVolumeSlider", "progression:set_bgm_volume" },
                { "Settings_SFXVolumeSlider", "progression:set_sfx_volume" },
            }};
            for (const auto& binding : sliders)
            {
                if (FindAuthoredSlider(scene, binding.EntityName))
                    SetSliderCommand(scene, binding.EntityName, binding.Command);
            }

            constexpr std::array<ControlBinding, 6> buttons = {{
                { "Settings_Button_VolumeDown", "progression:master_volume_down" },
                { "Settings_Button_VolumeUp", "progression:master_volume_up" },
                { "Settings_Button_BGMDown", "progression:bgm_volume_down" },
                { "Settings_Button_BGMUp", "progression:bgm_volume_up" },
                { "Settings_Button_SFXDown", "progression:sfx_volume_down" },
                { "Settings_Button_SFXUp", "progression:sfx_volume_up" },
            }};
            for (const auto& binding : buttons)
            {
                if (FindAuthoredButton(scene, binding.EntityName))
                    SetButtonCommand(scene, binding.EntityName, binding.Command);
            }
        }

    } // namespace

    void UpdateAudioControls(Scene* scene)
    {
        BindAuthoredAudioControls(scene);

        const auto& settings = UserSettings::Get();
        SetSliderValue(scene, "Settings_TextSpeedSlider", static_cast<float>(settings.TextSpeed));
        SetSliderValue(scene, "Settings_MasterVolumeSlider", static_cast<float>(settings.MasterVolume));
        SetSliderValue(scene, "Settings_BGMVolumeSlider", static_cast<float>(settings.BGMVolume));
        SetSliderValue(scene, "Settings_SFXVolumeSlider", static_cast<float>(settings.SFXVolume));
    }

} // namespace Wheatear::ProgressionSettingsPageService