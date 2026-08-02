#pragma once

#include "Wheatear/Core/Core.h"

#include <array>

namespace Wheatear::VisualNovelInputService {

    struct InputSnapshot
    {
        bool PrimaryMousePressed = false;
        bool AdvancePressed = false;
        bool AutoPressed = false;
        bool HistoryPressed = false;
        bool SavePressed = false;
        bool LoadPressed = false;
        std::array<bool, 9> ChoicePressed = {};
    };

    WHEATEAR_API InputSnapshot Sample();

} // namespace Wheatear::VisualNovelInputService
