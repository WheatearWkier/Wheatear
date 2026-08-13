#include "wtpch.h"
#include "GameplayRewardService.h"

namespace Wheatear::GameplayRewardService {

    std::string ExtractRewardAmount(const std::string& rewardSummary, const char* displayName)
    {
        if (rewardSummary.empty() || !displayName)
            return "0";

        const size_t namePos = rewardSummary.find(displayName);
        if (namePos == std::string::npos)
            return "0";

        const size_t marker = rewardSummary.find('x', namePos);
        if (marker == std::string::npos)
            return "?";

        size_t cursor = marker + 1;
        while (cursor < rewardSummary.size() && rewardSummary[cursor] == ' ')
            ++cursor;

        std::string amount;
        while (cursor < rewardSummary.size())
        {
            const char ch = rewardSummary[cursor];
            if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '~')
            {
                amount.push_back(ch);
                ++cursor;
                continue;
            }
            break;
        }

        return amount.empty() ? "?" : amount;
    }

    bool IsZeroAmount(const std::string& amount)
    {
        return amount.empty() || amount == "0";
    }

} // namespace Wheatear::GameplayRewardService
