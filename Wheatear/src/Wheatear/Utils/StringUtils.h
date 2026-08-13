#pragma once

// Shared string helpers. Historically each module duplicated StartsWith/Trim/
// ToLower/PayloadAfter with byte-identical bodies (and occasional whitespace
// drift). Consolidating them here removes the drift hazard; existing local
// copies are migrated incrementally.

#include <algorithm>
#include <cctype>
#include <string>

namespace Wheatear::StringUtils {

    inline bool StartsWith(const std::string& value, const std::string& prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    inline bool StartsWith(const std::string& value, const char* prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    inline std::string Trim(std::string value)
    {
        const size_t start = value.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return {};

        const size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }

    inline std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    inline std::string PayloadAfter(const std::string& value, const std::string& prefix)
    {
        return StartsWith(value, prefix) ? value.substr(prefix.size()) : std::string{};
    }

} // namespace Wheatear::StringUtils
