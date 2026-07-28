#pragma once
#include <cstdint>

#include <glm/glm.hpp>

namespace Wheatear {

    struct UIButtonComponent;
    struct UICheckboxComponent;
    struct UIImageComponent;
    struct UIPanelComponent;
    struct UIProgressBarComponent;
    struct UISliderComponent;
    struct UITextComponent;
    struct UIWidgetComponent;

    // 把归一化UI坐标转换为世界空间的mat4
    // viewportWidth/Height 是当前帧缓冲的像素尺寸
    class UIRenderer
    {
    public:
        // 在Scene::OnUpdateRuntime末尾调用
        static void BeginUIPass(uint32_t viewportWidth, uint32_t viewportHeight);
        static void EndUIPass(const glm::mat4& restoreViewProjection = glm::mat4(1.0f));

        // 渲染单个UI实体（由Scene遍历组件后调用）
        static void DrawUIImage(
            const UIWidgetComponent& widget,
            const UIImageComponent& image,
            int entityID = -1);


        static void DrawUIPanel(
            const UIWidgetComponent& widget,
            const UIPanelComponent& panel,
            int entityID = -1);
        static void DrawUIButton(
            const UIWidgetComponent& widget,
            const UIButtonComponent& button,
            int entityID = -1);

        static void DrawUIProgressBar(
            const UIWidgetComponent& widget,
            const UIProgressBarComponent& bar,
            int entityID = -1);


        static void DrawUISlider(
            const UIWidgetComponent& widget,
            const UISliderComponent& slider,
            int entityID = -1);

        static void DrawUICheckbox(
            const UIWidgetComponent& widget,
            const UICheckboxComponent& checkbox,
            int entityID = -1);
        static void DrawUIText(
            const UIWidgetComponent& widget,
            const UITextComponent& text,
            int entityID = -1);

        // 工具：归一化坐标 → Renderer2D用的mat4
        static glm::mat4 WidgetToTransform(
            const UIWidgetComponent& widget,
            uint32_t viewportWidth,
            uint32_t viewportHeight);

    private:
        static uint32_t s_ViewportWidth;
        static uint32_t s_ViewportHeight;
        static glm::mat4 s_SavedViewProjection;

    };

} // namespace Wheatear
