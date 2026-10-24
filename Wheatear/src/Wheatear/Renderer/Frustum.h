#pragma once

#include <glm/glm.hpp>

#include <limits>

namespace Wheatear {

    // Camera frustum extracted from a view-projection matrix. Used by the
    // render system to reject meshes before they reach the draw call.
    class Frustum
    {
    public:
        Frustum() = default;
        explicit Frustum(const glm::mat4& viewProjection) { Set(viewProjection); }

        void Set(const glm::mat4& viewProjection)
        {
            const glm::vec4 row0(viewProjection[0][0], viewProjection[1][0],
                viewProjection[2][0], viewProjection[3][0]);
            const glm::vec4 row1(viewProjection[0][1], viewProjection[1][1],
                viewProjection[2][1], viewProjection[3][1]);
            const glm::vec4 row2(viewProjection[0][2], viewProjection[1][2],
                viewProjection[2][2], viewProjection[3][2]);
            const glm::vec4 row3(viewProjection[0][3], viewProjection[1][3],
                viewProjection[2][3], viewProjection[3][3]);

            m_Planes[0] = row3 + row0; // left
            m_Planes[1] = row3 - row0; // right
            m_Planes[2] = row3 + row1; // bottom
            m_Planes[3] = row3 - row1; // top
            m_Planes[4] = row3 + row2; // near
            m_Planes[5] = row3 - row2; // far

            for (glm::vec4& plane : m_Planes)
            {
                const float length = glm::length(glm::vec3(plane));
                if (length > 1e-8f)
                    plane /= length;
            }
        }

        bool IsAABBVisible(const glm::vec3& minimum, const glm::vec3& maximum) const
        {
            for (const glm::vec4& plane : m_Planes)
            {
                const glm::vec3 positive(
                    plane.x >= 0.0f ? maximum.x : minimum.x,
                    plane.y >= 0.0f ? maximum.y : minimum.y,
                    plane.z >= 0.0f ? maximum.z : minimum.z);
                if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.0f)
                    return false;
            }
            return true;
        }

        bool IsTransformedAABBVisible(const glm::vec3& minimum,
            const glm::vec3& maximum,
            const glm::mat4& transform) const
        {
            glm::vec3 worldMin(std::numeric_limits<float>::max());
            glm::vec3 worldMax(-std::numeric_limits<float>::max());
            for (int corner = 0; corner < 8; ++corner)
            {
                const glm::vec3 local(
                    (corner & 1) ? maximum.x : minimum.x,
                    (corner & 2) ? maximum.y : minimum.y,
                    (corner & 4) ? maximum.z : minimum.z);
                const glm::vec3 world = glm::vec3(transform * glm::vec4(local, 1.0f));
                worldMin = glm::min(worldMin, world);
                worldMax = glm::max(worldMax, world);
            }
            return IsAABBVisible(worldMin, worldMax);
        }

    private:
        glm::vec4 m_Planes[6] = {};
    };

} // namespace Wheatear
