#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

struct ma_engine;
struct ma_sound;

namespace Wheatear {

    struct AudioClip
    {
        std::string FilePath;
        bool        Loaded = false;
    };

    class AudioEngine
    {
    public:
        static void Init();
        static void Shutdown();

        static void PlaySound(const std::string& filepath, float volume = 1.0f);

        static uint32_t PlaySoundWithHandle(const std::string& filepath,
            float volume = 1.0f,
            bool  loop = false);

        static void StopSound(uint32_t handle);
        static void PauseSound(uint32_t handle);
        static void ResumeSound(uint32_t handle);
        static void SetVolume(uint32_t handle, float volume);
        static bool IsPlaying(uint32_t handle);

        // Converts a 0-100 UI volume value into a perceptual gain. A midpoint
        // slider should feel comfortable, not like half of a raw waveform.
        static float PercentToGain(float percent);

        static void OnSceneStop();

    private:
        static void CollectFinishedSounds();

        static ma_engine* s_Engine;
        static std::unordered_map<uint32_t, ma_sound*> s_Sounds;
        static uint32_t                                 s_NextHandle;
    };

}
