#include "wtpch.h"
#include "AudioSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Config/UserSettings.h"

namespace Wheatear {

    void AudioSystem::OnRuntimeStart(Scene* scene)
    {
        size_t started = 0;
        size_t skipped = 0;
        size_t failed = 0;
        for (auto e : scene->GetRegistry().view<AudioSourceComponent>())
        {
            auto& asc = scene->GetRegistry().get<AudioSourceComponent>(e);
            asc.RuntimeHandle = 0;
            if (asc.PlayOnStart && !asc.AudioFilePath.empty())
            {
                const auto& settings = UserSettings::Get();
                const float channel = asc.Loop
                    ? AudioEngine::PercentToGain(static_cast<float>(settings.BGMVolume))
                    : AudioEngine::PercentToGain(static_cast<float>(settings.SFXVolume));
                const float master = AudioEngine::PercentToGain(static_cast<float>(settings.MasterVolume));
                const float gain = asc.Volume * master * channel;
                if (gain <= 0.0f)
                {
                    ++skipped;
                    continue;
                }

                asc.RuntimeHandle = AudioEngine::PlaySoundWithHandle(
                    asc.AudioFilePath, gain, asc.Loop);
                if (asc.RuntimeHandle != 0)
                    ++started;
                else
                    ++failed;
            }
        }

        WT_CORE_INFO("AudioSystem: runtime start started={} skipped={} failed={}", started, skipped, failed);
    }

    void AudioSystem::OnRuntimeStop(Scene* scene)
    {
        AudioEngine::OnSceneStop();
    }

} // namespace Wheatear
