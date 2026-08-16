#include "wtpch.h"
#include "AudioEngine.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Assets/AssetPath.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <system_error>
#include <vector>

namespace Wheatear {

    ma_engine* AudioEngine::s_Engine = nullptr;
    std::unordered_map<uint32_t, AudioEngine::ManagedSound> AudioEngine::s_Sounds = {};
    uint32_t AudioEngine::s_NextHandle = 1;
    static std::string ResolvePath(const std::string& filepath)
    {
        return AssetPath::ResolveAsset(filepath).string();
    }

    namespace {

        // Decoded-once PCM cache. ma_sound_init_from_file(MA_SOUND_FLAG_DECODE)
        // re-reads and re-decodes the whole file from disk on every one-shot
        // (hits/landings/attacks at 60fps), stalling the main thread. Instead
        // decode each path once into f32 PCM and let every play own a fresh
        // ma_audio_buffer over that immutable memory: concurrent plays never
        // share (or seek) a data-source cursor.
        struct CachedSfxPcm
        {
            std::vector<float> Pcm;
            ma_uint32 SampleRate = 0;
            ma_uint32 Channels = 0;
        };

        std::unordered_map<std::string, CachedSfxPcm>& SfxPcmCache()
        {
            static std::unordered_map<std::string, CachedSfxPcm> cache;
            return cache;
        }

        bool IsReadableAudioFile(const std::string& resolvedPath)
        {
            std::error_code error;
            return !resolvedPath.empty()
                && std::filesystem::is_regular_file(std::filesystem::path(resolvedPath), error);
        }

        const CachedSfxPcm* GetOrLoadSfxPcm(const std::string& resolvedPath)
        {
            CachedSfxPcm& cached = SfxPcmCache()[resolvedPath];
            if (!cached.Pcm.empty())
                return &cached;

            ma_decoder decoder;
            if (ma_decoder_init_file(resolvedPath.c_str(), nullptr, &decoder) != MA_SUCCESS)
                return nullptr;

            // Snapshot the format before decoding; the decoder is uninitialized
            // below and its fields must not be read afterwards.
            const ma_uint32 outputSampleRate = decoder.outputSampleRate;
            const ma_uint32 outputChannels = decoder.outputChannels;
            if (outputSampleRate == 0 || outputChannels == 0
                || outputSampleRate > 384000 || outputChannels > 32)
            {
                // Reject corrupt/absurd headers so a bogus sample rate cannot
                // drive a multi-GB allocation below.
                ma_decoder_uninit(&decoder);
                return nullptr;
            }

            // Decode the whole file to f32 PCM in memory (one-time cost).
            // Cap the preallocation; a lying header must not trigger bad_alloc.
            std::vector<float> pcm;
            pcm.reserve(std::min(
                static_cast<size_t>(outputSampleRate) * outputChannels,
                static_cast<size_t>(64 * 1024 * 1024)));
            std::array<float, 8192> chunk{};
            for (;;)
            {
                ma_uint64 framesRead = 0;
                const ma_result readResult = ma_decoder_read_pcm_frames(
                    &decoder, chunk.data(), chunk.size() / outputChannels, &framesRead);
                pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + static_cast<ptrdiff_t>(framesRead * outputChannels));
                if (readResult != MA_SUCCESS || framesRead == 0)
                    break;
            }
            ma_decoder_uninit(&decoder);
            if (pcm.empty())
                return nullptr;

            cached.Pcm = std::move(pcm);
            cached.SampleRate = outputSampleRate;
            cached.Channels = outputChannels;
            return &cached;
        }

        // Creates a fresh ma_audio_buffer over the cached PCM. The buffer is
        // owned by the caller (the ManagedSound) and must be uninitialized and
        // freed when the sound is torn down.
        ma_audio_buffer* CreateSfxBuffer(const CachedSfxPcm& pcm)
        {
            if (pcm.Channels == 0 || pcm.Pcm.size() < pcm.Channels)
                return nullptr;

            auto* buffer = new ma_audio_buffer();
            const ma_audio_buffer_config config = ma_audio_buffer_config_init(
                ma_format_f32,
                pcm.Channels,
                static_cast<ma_uint64>(pcm.Pcm.size() / pcm.Channels),
                pcm.Pcm.data(),
                nullptr);
            if (ma_audio_buffer_init(&config, buffer) != MA_SUCCESS)
            {
                delete buffer;
                return nullptr;
            }
            return buffer;
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
        const float normalized = std::clamp(percent, 0.0f, 100.0f) / 100.0f;
        return std::pow(normalized, 1.5f);
    }
    void AudioEngine::Shutdown()
    {
        // Stop and release all sounds first
        for (auto& [handle, managed] : s_Sounds)
        {
            ma_sound_stop(managed.Sound);
            ma_sound_uninit(managed.Sound);
            if (managed.Buffer)
            {
                auto* buffer = static_cast<ma_audio_buffer*>(managed.Buffer);
                ma_audio_buffer_uninit(buffer);
                delete buffer;
            }
            delete managed.Sound;
        }
        s_Sounds.clear();

        // Release decoded PCM (owned before the engine is destroyed).
        SfxPcmCache().clear();

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
        ma_audio_buffer* sfxBuffer = nullptr;
        if (loop)
        {
            // Long-running music should stream from the resolved file. Sharing
            // the short-SFX buffer cache for looped BGM can leave playback tied
            // to reused data-source cursors.
            result = ma_sound_init_from_file(
                s_Engine, resolvedPath.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, sound);
        }
        else
        {
            const CachedSfxPcm* pcm = GetOrLoadSfxPcm(resolvedPath);
            if (!pcm)
            {
                WT_CORE_WARN("AudioEngine: failed to load [{0}] resolved to [{1}]", filepath, resolvedPath);
                delete sound;
                return 0;
            }

            // Play from the in-memory PCM; each play gets its own buffer so
            // concurrent plays of the same SFX never share (or seek) the same
            // data source. The buffer is freed together with the sound.
            sfxBuffer = CreateSfxBuffer(*pcm);
            if (!sfxBuffer)
            {
                WT_CORE_WARN("AudioEngine: failed to init buffer for [{0}] resolved to [{1}]", filepath, resolvedPath);
                delete sound;
                return 0;
            }

            result = ma_sound_init_from_data_source(
                s_Engine, sfxBuffer, 0, nullptr, sound);
        }
        if (result != MA_SUCCESS)
        {
            WT_CORE_WARN("AudioEngine: failed to init sound [{0}] resolved to [{1}], error code = {2}",
                filepath, resolvedPath, (int)result);
            if (sfxBuffer)
            {
                ma_audio_buffer_uninit(sfxBuffer);
                delete sfxBuffer;
            }
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
            if (sfxBuffer)
            {
                ma_audio_buffer_uninit(sfxBuffer);
                delete sfxBuffer;
            }
            delete sound;
            return 0;
        }

        uint32_t handle = s_NextHandle++;
        s_Sounds[handle] = ManagedSound{ sound, sfxBuffer };
        return handle;
    }

    void AudioEngine::StopSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it == s_Sounds.end()) return;

        ma_sound_stop(it->second.Sound);
        ma_sound_uninit(it->second.Sound);
        if (it->second.Buffer)
        {
            auto* buffer = static_cast<ma_audio_buffer*>(it->second.Buffer);
            ma_audio_buffer_uninit(buffer);
            delete buffer;
        }
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
            if (managed.Buffer)
            {
                auto* buffer = static_cast<ma_audio_buffer*>(managed.Buffer);
                ma_audio_buffer_uninit(buffer);
                delete buffer;
            }
            delete managed.Sound;
        }
        s_Sounds.clear();
        s_NextHandle = 1;

        // Drop decoded PCM on scene change so BGM/SFX of the previous scene
        // do not pin memory; they re-decode once on next use.
        SfxPcmCache().clear();
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
                if (managed.Buffer)
                {
                    auto* buffer = static_cast<ma_audio_buffer*>(managed.Buffer);
                    ma_audio_buffer_uninit(buffer);
                    delete buffer;
                }
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
