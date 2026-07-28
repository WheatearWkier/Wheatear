#pragma once

#include "EditorLayerBase.h"
#include "Wheatear/Renderer/IBLPrecompute.h"

namespace Wheatear {

    /// @brief 3D 编辑器层
    ///
    /// 在基类的基础上增加：
    ///   - 3D BeginScene + EditorSkybox 渲染（OnBeginRender）
    ///   - 光源 Gizmo 叠加可视化（OnOverlayRender）
    ///   - IBL / Environment 设置面板（OnImGuiExtra）
    ///   - Editor Grid 渲染
    ///
    /// 预留扩展点（直接继承或在此添加 System 即可）：
    ///   - 3D 物理（Rigidbody3D / Collider3D）—— 在 Scene 注册新 System
    ///   - 3D 动画（Animator3D / Skinning）—— 在 Scene 注册新 System
    ///   - 3D 碰撞体可视化 —— 在 OnOverlayRender() 中添加
    class EditorLayer3D : public EditorLayerBase
    {
    public:
        EditorLayer3D();
        ~EditorLayer3D() override = default;

        // Layer 生命周期（需要在 OnAttach 时追加3D初始化）
        void OnAttach() override;

    protected:
        // ── 基类扩展钩子 ─────────────────────────────────────────────────────

        /// 初始化 3D 渲染：默认 IBL 加载
        void OnAttach3D();

        /// BeginScene（Camera UBO） + EditorSkybox
        void OnBeginRender() override;

        void OnPostSceneUpdate() override;

        /// 光源 Gizmo 线框 + 碰撞体（预留）+ Editor Grid
        void OnOverlayRender() override;

        /// IBL / Environment Settings 面板
        void OnImGuiExtra() override;

    private:
        // ── IBL ──────────────────────────────────────────────────────────────
        Ref<IBLResult> m_IBL;
        std::string    m_IBLPath;   // 记录路径，场景切换时无需重新加载

        // ── 3D 编辑器选项 ────────────────────────────────────────────────────
        float m_IBLIntensity = 1.0f;

        // ── 预留：3D 物理碰撞体可视化开关 ────────────────────────────────────
        // bool m_ShowPhysicsColliders3D = false;  // 继承自 Base 的 m_ShowPhysicsColliders

        // 用于检测 viewport resize，同步 ResizeSSAO
        uint32_t m_LastSSAOWidth = 0;
        uint32_t m_LastSSAOHeight = 0;
    };

} // namespace Wheatear
