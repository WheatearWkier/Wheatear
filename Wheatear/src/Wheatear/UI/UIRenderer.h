#pragma once
#include <cstdint>

#include <glm/glm.hpp>

namespace Wheatear {

    struct UIButtonComponent;
    struct UICheckboxComponent;
    struct UIImageComponent;
    struct UIRadialCooldownComponent;
    struct UIPanelComponent;
    struct UIPathComponent;
    struct UIProgressBarComponent;
    struct UISliderComponent;
    struct UIScrollViewComponent;
    struct UISkillTreeViewComponent;
    struct UITextComponent;
    struct UIWidgetComponent;

    class UIRenderer
    {
    public:
        static void BeginUIPass(uint32_t viewportWidth, uint32_t viewportHeight);
        static void EndUIPass(const glm::mat4& restoreViewProjection = glm::mat4(1.0f));

        static void DrawUIImage(
            const UIWidgetComponent& widget,
            const UIImageComponent& image,
            int entityID = -1);

        static void DrawUIRadialCooldown(
            const UIWidgetComponent& widget,
            const UIRadialCooldownComponent& cooldown,
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

        static void DrawUIScrollView(
            const UIWidgetComponent& widget,
            const UIScrollViewComponent& scrollView,
            int entityID = -1);

        static void DrawUIPath(
            const UIWidgetComponent& widget,
            const UIPathComponent& path,
            int entityID = -1);

        static void DrawUIBezier(
            const UIWidgetComponent& widget,
            const glm::vec2& start,
            const glm::vec2& control,
            const glm::vec2& end,
            const glm::vec4& color,
            float lineWidth = 1.0f,
            int segments = 16,
            int entityID = -1);

        static void DrawUISkillTreeView(
            const UIWidgetComponent& widget,
            const UISkillTreeViewComponent& skillTreeView,
            int entityID = -1);

        static void DrawUICheckbox(
            const UIWidgetComponent& widget,
            const UICheckboxComponent& checkbox,
            int entityID = -1);
        static void DrawUIText(
            const UIWidgetComponent& widget,
            const UITextComponent& text,
            int entityID = -1);
        static void PreloadUIText(const UITextComponent& text);

        static glm::mat4 WidgetToTransform(
            const UIWidgetComponent& widget,
            uint32_t viewportWidth,
            uint32_t viewportHeight);

    private:
        static uint32_t s_ViewportWidth;
        static uint32_t s_ViewportHeight;

    };

} // namespace Wheatear
