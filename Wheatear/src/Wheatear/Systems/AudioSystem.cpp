#include "wtpch.h"
#include "AudioSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Audio/AudioEngine.h"

namespace Wheatear {

    void AudioSystem::OnRuntimeStart(Scene* scene)
    {
        for (auto e : scene->GetRegistry().view<AudioSourceComponent>())
        {
            auto& asc = scene->GetRegistry().get<AudioSourceComponent>(e);
            if (asc.PlayOnStart && !asc.AudioFilePath.empty())
                asc.RuntimeHandle = AudioEngine::PlaySoundWithHandle(
                    asc.AudioFilePath, asc.Volume, asc.Loop);
        }
    }

    void AudioSystem::OnRuntimeStop(Scene* scene)
    {
        AudioEngine::OnSceneStop();
    }

} // namespace Wheatear