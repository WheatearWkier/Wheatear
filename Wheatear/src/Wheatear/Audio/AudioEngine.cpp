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
    std::unordered_map<uint32_t, ma_sound*> AudioEngine::s_Sounds = {};
    uint32_t AudioEngine::s_NextHandle = 1;
    static std::string ResolvePath(const std::string& filepath)
    {
        return AssetPath::ResolveAsset(filepath).string();
    }

    namespace {

        // Decoded-once SFX cache. ma_sound_init_from_file(MA_SOUND_FLAG_DECODE)
        // re-reads and re-decodes the whole file from disk on every one-shot
        // (hits/landings/attacks at 60fps), stalling the main thread. Instead
        // decode each path once into a pair of ma_audio_buffers (two copies so
        // rapid overlapping plays do not fight over one data source) and create
        // sounds from memory via ma_sound_init_from_data_source.
        struct CachedSfx
        {
            std::array<std::unique_ptr<ma_audio_buffer>, 2> Buffers;
            // Decoded PCM owned by the cache. ma_audio_buffer only references
            // caller memory (no copy flag in this miniaudio version), so the
            // PCM must outlive every buffer that plays it.
            std::array<std::vector<float>, 2> OwnedPcm;
            int Next = 0;
        };

        std::unordered_map<std::string, CachedSfx>& SfxBufferCache()
        {
            static std::unordered_map<std::string, CachedSfx> cache;
            return cache;
        }

        bool IsReadableAudioFile(const std::string& resolvedPath)
        {
            std::error_code error;
            return !resolvedPath.empty()
                && std::filesystem::is_regular_file(std::filesystem::path(resolvedPath), error);
        }

        ma_audio_buffer* GetOrLoadSfxBuffer(const std::string& resolvedPath)
        {
            CachedSfx& cached = SfxBufferCache()[resolvedPath];
            const int slot = cached.Next;

            // Round-robin between the two buffer slots so consecutive plays of
            // the same SFX do not start from a half-consumed data source.
            cached.Next = (cached.Next + 1) % static_cast<int>(cached.Buffers.size());

            ma_audio_buffer* buffer = cached.Buffers[slot].get();
            if (buffer)
            {
                ma_audio_buffer_seek_to_pcm_frame(buffer, 0);
                return buffer;
            }

            ma_decoder decoder;
            if (ma_decoder_init_file(resolvedPath.c_str(), nullptr, &decoder) != MA_SUCCESS)
                return nullptr;

            // Snapshot the format before decoding; the decoder is uninitialized
            // below and its fields must not be read afterwards.
            const ma_uint32 outputSampleRate = decoder.outputSampleRate;
            const ma_uint32 outputChannels = decoder.outputChannels;
            if (outputSampleRate == 0 || outputChannels == 0)
            {
                ma_decoder_uninit(&decoder);
                return nullptr;
            }

            // Decode the whole file to f32 PCM in memory (one-time cost).
            std::vector<float> pcm;
            pcm.reserve(static_cast<size_t>(outputSampleRate) * outputChannels);
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

            // Keep the decoded PCM alive inside the cache entry: ma_audio_buffer
            // references the caller's memory and would dangle once the local
            // vector goes out of scope.
            std::vector<float>& ownedPcm = cached.OwnedPcm[slot];
            ownedPcm = std::move(pcm);

            cached.Buffers[slot] = std::make_unique<ma_audio_buffer>();
            const ma_audio_buffer_config config = ma_audio_buffer_config_init(
                ma_format_f32,
                outputChannels,
                static_cast<ma_uint64>(ownedPcm.size() / outputChannels),
                ownedPcm.data(),
                nullptr);
            if (ma_audio_buffer_init(&config, cached.Buffers[slot].get()) != MA_SUCCESS)
            {
                cached.Buffers[slot].reset();
                cached.OwnedPcm[slot].clear();
                return nullptr;
            }
            return cached.Buffers[slot].get();
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
        for (auto& [handle, sound] : s_Sounds)
        {
            ma_sound_stop(sound);
            ma_sound_uninit(sound);
            delete sound;
        }
        s_Sounds.clear();

        // Release decoded SFX buffers (owned before the engine is destroyed).
        SfxBufferCache().clear();

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
            // the short-SFX buffer cache for looped BGM can leave playback tied
            // to reused data-source cursors.
            result = ma_sound_init_from_file(
                s_Engine, resolvedPath.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, sound);
        }
        else
        {
            ma_audio_buffer* buffer = GetOrLoadSfxBuffer(resolvedPath);
            if (!buffer)
            {
                WT_CORE_WARN("AudioEngine: failed to load [{0}] resolved to [{1}]", filepath, resolvedPath);
                delete sound;
                return 0;
            }

            // Play from the in-memory buffer; the data source is owned by the cache
            // and outlives the sound, so no per-play disk I/O or decode.
            result = ma_sound_init_from_data_source(
                s_Engine, buffer, 0, nullptr, sound);
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
            ma_sound_set_volume(it->second, std::clamp(volume, 0.0f, 2.0f));
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

        // Drop decoded buffers on scene change so BGM/SFX of the previous scene
        // do not pin memory; they re-decode once on next use.
        SfxBufferCache().clear();
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
