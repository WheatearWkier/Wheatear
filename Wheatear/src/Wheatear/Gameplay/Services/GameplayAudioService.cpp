#include "wtpch.h"
#include "GameplayAudioService.h"

#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Config/UserSettings.h"

#include <algorithm>

namespace Wheatear::GameplayAudioService {

    float MasterGain()
    {
        const auto& settings = UserSettings::Get();
        return AudioEngine::PercentToGain(static_cast<float>(settings.MasterVolume));
    }

    float SFXGain(float volume)
    {
        const auto& settings = UserSettings::Get();
        const float sfx = AudioEngine::PercentToGain(static_cast<float>(settings.SFXVolume));
        return std::clamp(volume, 0.0f, 2.0f) * MasterGain() * sfx;
    }

    float BGMGain(float volume)
    {
        const auto& settings = UserSettings::Get();
        const float bgm = AudioEngine::PercentToGain(static_cast<float>(settings.BGMVolume));
        return std::clamp(volume, 0.0f, 2.0f) * MasterGain() * bgm;
    }

    void PlaySFX(const std::string& path, float volume)
    {
        if (path.empty())
            return;

        AudioEngine::PlaySound(path, SFXGain(volume));
    }

    uint32_t PlayBGM(const std::string& path, float volume, bool loop)
    {
        if (path.empty())
            return 0;

        return AudioEngine::PlaySoundWithHandle(path, BGMGain(volume), loop);
    }

    void SetBGMVolume(uint32_t handle, float volume)
    {
        if (handle == 0)
            return;

        AudioEngine::SetVolume(handle, BGMGain(volume));
    }

} // namespace Wheatear::GameplayAudioService
