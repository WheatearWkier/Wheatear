#pragma once

#include "EditorLayerBase.h"

namespace Wheatear {

    /// @brief 2D 编辑器层
    ///
    /// 在基类的基础上增加：
    ///   - 2D 物理碰撞体叠加可视化
    ///   - 2D 专属 ImGui 扩展入口
    ///
    /// 未来可在此添加：
    ///   - 2D 动画编辑专属工具
    ///   - Tilemap 工具
    class EditorLayer2D : public EditorLayerBase
    {
    public:
        EditorLayer2D();
        ~EditorLayer2D() override = default;

    protected:
        // ── 基类扩展钩子 ─────────────────────────────────────────────────────

        /// 2D模式不需要3D BeginScene，保持空实现
        void OnBeginRender() override {}

        /// 绘制物理碰撞体线框叠加层
        void OnOverlayRender() override;

        /// 2D 专属 ImGui 扩展，当前不绘制独立 Settings 窗口
        void OnImGuiExtra() override;
    };

} // namespace Wheatear
