#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear::GameplayRewardService {

    struct RewardIconDefinition
    {
        const char* Key = "";
        const char* ItemId = "";
        const char* DisplayName = "";
        std::string IconPath;
        const char* Usage = "";
    };

    WHEATEAR_API std::string ExtractRewardAmount(const std::string& rewardSummary, const char* displayName);
    WHEATEAR_API bool IsZeroAmount(const std::string& amount);

} // namespace Wheatear::GameplayRewardService
