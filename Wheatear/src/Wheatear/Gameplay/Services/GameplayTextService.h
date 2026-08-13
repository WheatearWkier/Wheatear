#pragma once

#include "Wheatear/Core/Core.h"

#include <string>
#include <vector>

namespace Wheatear::GameplayTextService {

    WHEATEAR_API std::vector<std::string> SplitCommand(const std::string& command, char delimiter = ':');
    WHEATEAR_API void ReplaceAll(std::string& value, const std::string& from, const std::string& to);
    WHEATEAR_API std::string ReplaceAllCopy(std::string value, const std::string& from, const std::string& to);
    WHEATEAR_API std::string FormatFramePath(const std::string& pattern, int oneBasedFrame);
    WHEATEAR_API std::string FormatFloat(float value, int precision = 0);

} // namespace Wheatear::GameplayTextService
