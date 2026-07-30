#pragma once

#include <glm/glm.hpp>

namespace Wheatear {


    struct QuadVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        glm::vec2 TexCoord;
        float     TexIndex;
        float     TilingFactor;
        int       EntityID;
    };

    struct CircleVertex
    {
        glm::vec3 WorldPosition;
        glm::vec3 LocalPosition;
        glm::vec4 Color;
        float     Thickness;
        float     Fade;
        int       EntityID;
    };

    struct LineVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        int       EntityID;
    };

    struct TextVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        glm::vec2 TexCoord;
        float     TexIndex;
        glm::vec4 OutlineColor;
        float     OutlineWidth;
        float     EdgeSoftness;
        int       EntityID;
    };

    // struct TriangleVertex { ... };
    // struct MeshVertex     { ... };

} // namespace Wheatear
