#pragma once

#include "Wheatear/Core/Core.h"

#include <glm/glm.hpp>

namespace Wheatear {

    // Tiny batched 3D line renderer for navigation/physics debug overlays.
    // Lines are accumulated by gameplay systems and flushed once by the
    // render system after the main 3D pass.
    class DebugDraw3D
    {
    public:
        static void Begin();
        static void Line(const glm::vec3& from, const glm::vec3& to,
            const glm::vec4& color);
        static void Box(const glm::vec3& minimum, const glm::vec3& maximum,
            const glm::vec4& color);
        static void Flush();
        static bool HasLines();

    private:
        struct Vertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
        };
    };

} // namespace Wheatear
