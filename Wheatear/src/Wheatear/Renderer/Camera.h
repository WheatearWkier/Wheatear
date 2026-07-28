#pragma once

#include <glm/glm.hpp>

namespace Wheatear {

    // 基类：仅持有投影矩阵
    // SceneCamera 和 EditorCamera 都继承此类
    class Camera
    {
    public:
        Camera() = default;
        explicit Camera(const glm::mat4& projection) : m_Projection(projection) {}
        virtual ~Camera() = default;

        const glm::mat4& GetProjection() const { return m_Projection; }

    protected:
        glm::mat4 m_Projection = glm::mat4(1.0f);
    };

} // namespace Wheatear