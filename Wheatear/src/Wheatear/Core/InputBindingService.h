#pragma once

#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear {

    class WHEATEAR_API InputBindingService
    {
    public:
        static bool IsActionDown(const std::string& actionId);
        static const std::vector<int>& GetKeys(const std::string& actionId);
        static void SetKeys(const std::string& actionId, const std::vector<int>& keys);
        static void ResetToDefaults();
        static std::string GetBindingLabel(const std::string& actionId);
        static std::string KeyName(int keyCode);
    };

} // namespace Wheatear
