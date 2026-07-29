#include "wtpch.h"
#include "AudioSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Modules/Progression/GameProgress.h"

namespace Wheatear {

    void AudioSystem::OnRuntimeStart(Scene* scene)
    {
        for (auto e : scene->GetRegistry().view<AudioSourceComponent>())
        {
            auto& asc = scene->GetRegistry().get<AudioSourceComponent>(e);
            if (asc.PlayOnStart && !asc.AudioFilePath.empty())
            {
                const auto& settings = GameProgress::GetState().Settings;
                const float channel = asc.Loop
                    ? AudioEngine::PercentToGain(static_cast<float>(settings.BGMVolume))
                    : AudioEngine::PercentToGain(static_cast<float>(settings.SFXVolume));
                const float master = AudioEngine::PercentToGain(static_cast<float>(settings.MasterVolume));
                asc.RuntimeHandle = AudioEngine::PlaySoundWithHandle(
                    asc.AudioFilePath, asc.Volume * master * channel, asc.Loop);
            }
        }
    }

    void AudioSystem::OnRuntimeStop(Scene* scene)
    {
        AudioEngine::OnSceneStop();
    }

} // namespace Wheatear
