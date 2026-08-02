#pragma once

#include <algorithm>

#include <glm/glm.hpp>

namespace Wheatear::SideCombatMath {

    inline float SignNonZero(float value)
    {
        return value < 0.0f ? -1.0f : 1.0f;
    }

    inline float Approach(float value, float target, float delta)
    {
        if (value < target)
            return std::min(value + delta, target);
        return std::max(value - delta, target);
    }

    inline glm::vec2 ToVec2(const glm::vec3& value)
    {
        return { value.x, value.y };
    }

} // namespace Wheatear::SideCombatMath
