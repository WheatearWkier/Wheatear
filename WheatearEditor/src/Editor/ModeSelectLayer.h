#pragma once

#include "Wheatear/Core/Layer.h"

namespace Wheatear {

    /// @brief 启动模式选择层
    ///
    /// 应用启动时首先推入此层，显示一个居中的 ImGui 弹窗，
    /// 让用户选择进入 2D 或 3D 编辑器模式。
    ///
    /// 选择完毕后，此层把自身从 LayerStack 移除，
    /// 并把对应的 EditorLayer2D / EditorLayer3D 推入。
    class ModeSelectLayer : public Layer
    {
    public:
        ModeSelectLayer();
        ~ModeSelectLayer() override = default;

        void OnAttach()      override;
        void OnImGuiRender() override;

    private:
        void LaunchEditor2D();
        void LaunchEditor3D();

        bool m_Decided = false; // 防止多帧重复触发
    };

} // namespace Wheatear
