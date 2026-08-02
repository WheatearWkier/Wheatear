#pragma once

#include "Wheatear/Core/Core.h"

#include <cstdint>
#include <string>

namespace Wheatear::GameplayAudioService {

    WHEATEAR_API float MasterGain();
    WHEATEAR_API float SFXGain(float volume = 1.0f);
    WHEATEAR_API float BGMGain(float volume = 1.0f);

    WHEATEAR_API void PlaySFX(const std::string& path, float volume = 1.0f);
    WHEATEAR_API uint32_t PlayBGM(const std::string& path, float volume = 1.0f, bool loop = true);
    WHEATEAR_API void SetBGMVolume(uint32_t handle, float volume = 1.0f);

} // namespace Wheatear::GameplayAudioService
