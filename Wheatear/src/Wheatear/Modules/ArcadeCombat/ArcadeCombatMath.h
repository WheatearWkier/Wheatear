#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace Wheatear::ArcadeCombatMath {

    inline glm::vec2 ToVec2(const glm::vec3& value)
    {
        return { value.x, value.y };
    }

    inline float Distance2D(const glm::vec3& a, const glm::vec3& b)
    {
        return glm::length(ToVec2(a) - ToVec2(b));
    }

    inline glm::vec2 DirectionTo(const glm::vec3& from, const glm::vec3& to)
    {
        glm::vec2 direction = ToVec2(to) - ToVec2(from);
        if (glm::length2(direction) <= 0.0001f)
            return { 1.0f, 0.0f };
        return glm::normalize(direction);
    }

} // namespace Wheatear::ArcadeCombatMath
