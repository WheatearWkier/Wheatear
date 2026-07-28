#pragma once

#include <string>
#include <glm/glm.hpp>

namespace Wheatear::UI {

    // 带颜色重置按钮的三维向量拖拽控件
    // resetValue : 点击轴按钮时恢复的默认值
    // columnWidth: 左侧标签列宽度
    void DrawVec3Control(
        const std::string& label,
        glm::vec3& values,
        float              resetValue = 0.0f,
        float              columnWidth = 130.0f);

} // namespace Wheatear::UI