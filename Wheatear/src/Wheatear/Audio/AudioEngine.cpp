#include "wtpch.h"
#include "AudioEngine.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/AssetPath.h"

// miniaudio implementation ¡ª must be defined in only one .cpp
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <filesystem>

namespace Wheatear {

    ma_engine* AudioEngine::s_Engine = nullptr;
    std::unordered_map<uint32_t, ma_sound*> AudioEngine::s_Sounds = {};
    uint32_t AudioEngine::s_NextHandle = 1;
    static std::string ResolvePath(const std::string& filepath)
    {
        return AssetPath::ResolveAsset(filepath).string();
    }

    void AudioEngine::Init()
    {
        s_Engine = new ma_engine();

        ma_result result = ma_engine_init(nullptr, s_Engine);
        if (result != MA_SUCCESS)
        {
            WT_CORE_ERROR("AudioEngine: initialization failed! error code = {0}", (int)result);
            delete s_Engine;
            s_Engine = nullptr;
            return;
        }

        WT_CORE_INFO("AudioEngine: initialization successful");
    }

    void AudioEngine::Shutdown()
    {
        // Stop and release all sounds first
        for (auto& [handle, sound] : s_Sounds)
        {
            ma_sound_stop(sound);
            ma_sound_uninit(sound);
            delete sound;
        }
        s_Sounds.clear();

        if (s_Engine)
        {
            ma_engine_uninit(s_Engine);
            delete s_Engine;
            s_Engine = nullptr;
        }

        WT_CORE_INFO("AudioEngine: shutdown complete");
    }

    void AudioEngine::PlaySound(const std::string& filepath, float volume)
    {
        if (!s_Engine) return;

        // ma_engine_play_sound() cannot set per-call volume, so reuse
        // the managed sound path and let CollectFinishedSounds() recycle it.
        PlaySoundWithHandle(filepath, volume, false);
    }

    uint32_t AudioEngine::PlaySoundWithHandle(const std::string& filepath,
        float volume, bool loop)
    {
        if (!s_Engine) return 0;

        CollectFinishedSounds();

        std::string resolvedPath = ResolvePath(filepath);

        ma_sound* sound = new ma_sound();
        ma_result result = ma_sound_init_from_file(
            s_Engine,
            resolvedPath.c_str(),
            MA_SOUND_FLAG_DECODE,
            nullptr, nullptr,
            sound
        );

        if (result != MA_SUCCESS)
        {
            WT_CORE_WARN("AudioEngine: failed to load [{0}], error code = {1}", filepath, (int)result);
            delete sound;
            return 0;
        }

        ma_sound_set_volume(sound, volume);
        ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_start(sound);

        uint32_t handle = s_NextHandle++;
        s_Sounds[handle] = sound;
        return handle;
    }

    void AudioEngine::StopSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it == s_Sounds.end()) return;

        ma_sound_stop(it->second);
        ma_sound_uninit(it->second);
        delete it->second;
        s_Sounds.erase(it);
    }

    void AudioEngine::PauseSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_stop(it->second);
    }

    void AudioEngine::ResumeSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_start(it->second);
    }

    void AudioEngine::SetVolume(uint32_t handle, float volume)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_set_volume(it->second, volume);
    }

    bool AudioEngine::IsPlaying(uint32_t handle)
    {
        CollectFinishedSounds();

        auto it = s_Sounds.find(handle);
        if (it == s_Sounds.end()) return false;
        return ma_sound_is_playing(it->second) == MA_TRUE;
    }

    void AudioEngine::OnSceneStop()
    {
        for (auto& [handle, sound] : s_Sounds)
        {
            ma_sound_stop(sound);
            ma_sound_uninit(sound);
            delete sound;
        }
        s_Sounds.clear();
        s_NextHandle = 1;
    }

    void AudioEngine::CollectFinishedSounds()
    {
        for (auto it = s_Sounds.begin(); it != s_Sounds.end(); )
        {
            ma_sound* sound = it->second;
            if (sound && ma_sound_at_end(sound) == MA_TRUE && ma_sound_is_playing(sound) == MA_FALSE)
            {
                ma_sound_uninit(sound);
                delete sound;
                it = s_Sounds.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

}
