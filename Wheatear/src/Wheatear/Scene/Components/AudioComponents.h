#pragma once

// Audio source component.

#include <cstdint>
#include <string>

namespace Wheatear {

    struct AudioSourceComponent
    {
        std::string AudioFilePath = "";
        float       Volume = 1.0f;
        bool        Loop = false;
        bool        PlayOnStart = false;

        uint32_t    RuntimeHandle = 0;

        AudioSourceComponent() = default;
        AudioSourceComponent(const AudioSourceComponent&) = default;
    };

} // namespace Wheatear
