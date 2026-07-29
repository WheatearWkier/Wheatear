#pragma once

#include <glm/glm.hpp>

namespace Wheatear {

    /// 鎵€鏈?2D 鎵瑰鐞嗛《鐐圭粨鏋勫畾涔?
    /// 鏂板鍥惧厓鏃跺湪杩欓噷鍔犳柊鐨勯《鐐圭粨鏋勪綋鍗冲彲

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

    // 鈹€鈹€ 鏈潵鎵╁睍绀轰緥 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    // struct TriangleVertex { ... };
    // struct MeshVertex     { ... };

} // namespace Wheatear
