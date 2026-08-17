#include "wtpch.h"
#include "AudioEngine.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <system_error>

namespace Wheatear {

    ma_engine* AudioEngine::s_Engine = nullptr;
    std::unordered_map<uint32_t, AudioEngine::ManagedSound> AudioEngine::s_Sounds = {};
    uint32_t AudioEngine::s_NextHandle = 1;
    static std::string ResolvePath(const std::string& filepath)
    {
        return AssetPath::ResolveRuntimeData(AssetAliasRegistry::Resolve(filepath)).string();
    }

    namespace {

        bool IsReadableAudioFile(const std::string& resolvedPath)
        {
            std::error_code error;
            return !resolvedPath.empty()
                && std::filesystem::is_regular_file(std::filesystem::path(resolvedPath), error);
        }

    } // namespace

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

    float AudioEngine::PercentToGain(float percent)
    {
        return std::clamp(percent, 0.0f, 100.0f) / 100.0f;
    }
    void AudioEngine::Shutdown()
    {
        // Stop and release all sounds first
        for (auto& [handle, managed] : s_Sounds)
        {
            ma_sound_stop(managed.Sound);
            ma_sound_uninit(managed.Sound);
            delete managed.Sound;
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

        const std::string resolvedPath = ResolvePath(filepath);
        if (!IsReadableAudioFile(resolvedPath))
        {
            WT_CORE_WARN("AudioEngine: file [{0}] resolved to [{1}] but does not exist", filepath, resolvedPath);
            return 0;
        }

        ma_sound* sound = new ma_sound();
        ma_result result = MA_SUCCESS;
        if (loop)
        {
            // Long-running music should stream from the resolved file. Sharing
            // it with short decoded sounds can leave playback tied to reused
            // data-source cursors.
            result = ma_sound_init_from_file(
                s_Engine, resolvedPath.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, sound);
        }
        else
        {
            // One-shot SFX are small; decoding from the resolved file keeps
            // sample-rate/channel handling inside miniaudio's tested path.
            result = ma_sound_init_from_file(
                s_Engine, resolvedPath.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, sound);
        }
        if (result != MA_SUCCESS)
        {
            WT_CORE_WARN("AudioEngine: failed to init sound [{0}] resolved to [{1}], error code = {2}",
                filepath, resolvedPath, (int)result);
            delete sound;
            return 0;
        }

        ma_sound_set_volume(sound, std::clamp(volume, 0.0f, 2.0f));
        ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
        const ma_result startResult = ma_sound_start(sound);
        if (startResult != MA_SUCCESS)
        {
            WT_CORE_WARN("AudioEngine: failed to start sound [{0}] resolved to [{1}], error code = {2}",
                filepath, resolvedPath, (int)startResult);
            ma_sound_uninit(sound);
            delete sound;
            return 0;
        }

        uint32_t handle = s_NextHandle++;
        s_Sounds[handle] = ManagedSound{ sound };
        return handle;
    }

    void AudioEngine::StopSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it == s_Sounds.end()) return;

        ma_sound_stop(it->second.Sound);
        ma_sound_uninit(it->second.Sound);
        delete it->second.Sound;
        s_Sounds.erase(it);
    }

    void AudioEngine::PauseSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_stop(it->second.Sound);
    }

    void AudioEngine::ResumeSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_start(it->second.Sound);
    }

    void AudioEngine::SetVolume(uint32_t handle, float volume)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_set_volume(it->second.Sound, std::clamp(volume, 0.0f, 2.0f));
    }

    bool AudioEngine::IsPlaying(uint32_t handle)
    {
        CollectFinishedSounds();

        auto it = s_Sounds.find(handle);
        if (it == s_Sounds.end()) return false;
        return ma_sound_is_playing(it->second.Sound) == MA_TRUE;
    }

    void AudioEngine::OnSceneStop()
    {
        for (auto& [handle, managed] : s_Sounds)
        {
            ma_sound_stop(managed.Sound);
            ma_sound_uninit(managed.Sound);
            delete managed.Sound;
        }
        s_Sounds.clear();
        s_NextHandle = 1;
    }

    void AudioEngine::CollectFinishedSounds()
    {
        for (auto it = s_Sounds.begin(); it != s_Sounds.end(); )
        {
            ManagedSound& managed = it->second;
            if (managed.Sound
                && ma_sound_at_end(managed.Sound) == MA_TRUE
                && ma_sound_is_playing(managed.Sound) == MA_FALSE)
            {
                ma_sound_uninit(managed.Sound);
                delete managed.Sound;
                it = s_Sounds.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

}
