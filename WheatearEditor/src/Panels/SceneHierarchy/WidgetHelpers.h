#pragma once

#include <string>
#include <glm/glm.hpp>

namespace Wheatear::UI {

    void DrawVec3Control(
        const std::string& label,
        glm::vec3& values,
        float              resetValue = 0.0f,
        float              columnWidth = 130.0f);

} // namespace Wheatear::UI
