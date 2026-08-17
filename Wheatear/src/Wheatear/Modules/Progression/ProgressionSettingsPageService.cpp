#include "wtpch.h"
#include "ProgressionSettingsPageService.h"

#include "GameProgress.h"
#include "Wheatear/Config/UserSettings.h"
#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
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
            if (!HasEntity(scene, SystemBindings::Progression::SettingsControlPanel))
                return;

            for (const char* label : {
                SystemBindings::Progression::SettingsMasterVolumeLabel,
                SystemBindings::Progression::SettingsBGMVolumeLabel,
                SystemBindings::Progression::SettingsSFXVolumeLabel
            })
            {
                FindAuthoredText(scene, label);
            }

            constexpr std::array<ControlBinding, 3> sliders = {{
                { SystemBindings::Progression::SettingsMasterVolumeSlider, "progression:set_master_volume" },
                { SystemBindings::Progression::SettingsBGMVolumeSlider, "progression:set_bgm_volume" },
                { SystemBindings::Progression::SettingsSFXVolumeSlider, "progression:set_sfx_volume" },
            }};
            for (const auto& binding : sliders)
            {
                if (FindAuthoredSlider(scene, binding.EntityName))
                    SetSliderCommand(scene, binding.EntityName, binding.Command);
            }

            constexpr std::array<ControlBinding, 6> buttons = {{
                { SystemBindings::Progression::SettingsVolumeDownButton, "progression:master_volume_down" },
                { SystemBindings::Progression::SettingsVolumeUpButton, "progression:master_volume_up" },
                { SystemBindings::Progression::SettingsBGMDownButton, "progression:bgm_volume_down" },
                { SystemBindings::Progression::SettingsBGMUpButton, "progression:bgm_volume_up" },
                { SystemBindings::Progression::SettingsSFXDownButton, "progression:sfx_volume_down" },
                { SystemBindings::Progression::SettingsSFXUpButton, "progression:sfx_volume_up" },
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
        SetSliderValue(scene, SystemBindings::Progression::SettingsTextSpeedSlider, static_cast<float>(settings.TextSpeed));
        SetSliderValue(scene, SystemBindings::Progression::SettingsMasterVolumeSlider, static_cast<float>(settings.MasterVolume));
        SetSliderValue(scene, SystemBindings::Progression::SettingsBGMVolumeSlider, static_cast<float>(settings.BGMVolume));
        SetSliderValue(scene, SystemBindings::Progression::SettingsSFXVolumeSlider, static_cast<float>(settings.SFXVolume));
    }

} // namespace Wheatear::ProgressionSettingsPageService
