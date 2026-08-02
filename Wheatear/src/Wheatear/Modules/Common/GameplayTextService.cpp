#include "wtpch.h"
#include "GameplayTextService.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Wheatear::GameplayTextService {

    std::vector<std::string> SplitCommand(const std::string& command, char delimiter)
    {
        std::vector<std::string> tokens;
        std::string current;
        for (char ch : command)
        {
            if (ch == delimiter)
            {
                tokens.push_back(current);
                current.clear();
                continue;
            }
            current.push_back(ch);
        }
        tokens.push_back(current);
        return tokens;
    }

    void ReplaceAll(std::string& value, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return;

        size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos)
        {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    }

    std::string ReplaceAllCopy(std::string value, const std::string& from, const std::string& to)
    {
        ReplaceAll(value, from, to);
        return value;
    }

    std::string FormatFramePath(const std::string& pattern, int oneBasedFrame)
    {
        if (pattern.empty())
            return {};

        std::ostringstream padded;
        padded << std::setw(2) << std::setfill('0') << std::max(1, oneBasedFrame);

        std::string path = pattern;
        ReplaceAll(path, "{frame2}", padded.str());
        ReplaceAll(path, "{frame}", std::to_string(std::max(1, oneBasedFrame)));
        return path;
    }

    std::string FormatFloat(float value, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(std::max(0, precision)) << value;
        return stream.str();
    }

} // namespace Wheatear::GameplayTextService
