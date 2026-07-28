#include "wtpch.h"
#include "UIInputSystem.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scripting/ScriptEngine.h"

namespace Wheatear {

    bool UIInputSystem::s_MouseWasPressed = false;

    struct UINormalizedRect
    {
        float Left = 0.0f;
        float Right = 0.0f;
        float Top = 0.0f;
        float Bottom = 0.0f;
    };

    static bool StartsWith(const std::string& value, const std::string& prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    static bool IsNativeButtonCommand(const std::string& command)
    {
        return command == "quit"
            || StartsWith(command, "scene:")
            || StartsWith(command, "newgame:")
            || StartsWith(command, "loadgame:")
            || StartsWith(command, "progression:")
            || StartsWith(command, "vn:");
    }

    static UINormalizedRect WidgetToNormalizedRect(const UIWidgetComponent& widget)
    {
        const float halfW = widget.Size.x * 0.5f;
        const float halfH = widget.Size.y * 0.5f;
        float centerX = widget.Position.x;
        float centerY = widget.Position.y;

        switch (widget.Anchor)
        {
        case UIAnchor::TopLeft:
            centerX += halfW;
            centerY += halfH;
            break;
        case UIAnchor::TopCenter:
            centerY += halfH;
            break;
        case UIAnchor::TopRight:
            centerX -= halfW;
            centerY += halfH;
            break;
        case UIAnchor::MiddleLeft:
            centerX += halfW;
            break;
        case UIAnchor::MiddleCenter:
            break;
        case UIAnchor::MiddleRight:
            centerX -= halfW;
            break;
        case UIAnchor::BottomLeft:
            centerX += halfW;
            centerY -= halfH;
            break;
        case UIAnchor::BottomCenter:
            centerY -= halfH;
            break;
        case UIAnchor::BottomRight:
            centerX -= halfW;
            centerY -= halfH;
            break;
        }

        return {
            centerX - halfW,
            centerX + halfW,
            centerY - halfH,
            centerY + halfH
        };
    }

    static bool PointInRect(const UINormalizedRect& rect, float normMouseX, float normMouseY)
    {
        return normMouseX >= rect.Left && normMouseX <= rect.Right
            && normMouseY >= rect.Top && normMouseY <= rect.Bottom;
    }

    static void FireScriptCallback(Scene* scene, entt::entity e, const std::string& function)
    {
        if (function.empty()) return;
        if (IsNativeButtonCommand(function)) return;
        if (!ScriptEngine::IsInitialized()) return;

        std::string methodName = function;
        if (StartsWith(methodName, "script:"))
            methodName = methodName.substr(7);
        if (methodName.empty()) return;

        ScriptEngine::InvokeMethod(Entity{ e, scene }, methodName);
    }

    bool UIInputSystem::HitTest(const UIWidgetComponent& widget,
        float normMouseX, float normMouseY)
    {
        return PointInRect(WidgetToNormalizedRect(widget), normMouseX, normMouseY);
    }

    void UIInputSystem::FireOnClick(Scene* scene, entt::entity e)
    {
        Entity entity{ e, scene };
        if (!entity.HasComponent<UIButtonComponent>()) return;

        auto& button = entity.GetComponent<UIButtonComponent>();
        FireScriptCallback(scene, e, button.OnClickFunction);
    }

    void UIInputSystem::OnUpdate(Scene* scene,
        float mouseX, float mouseY,
        uint32_t viewportWidth,
        uint32_t viewportHeight)
    {
        if (viewportWidth == 0 || viewportHeight == 0) return;

        const float normX = mouseX / static_cast<float>(viewportWidth);
        const float normY = mouseY / static_cast<float>(viewportHeight);
        const bool mouseInViewport = (normX >= 0.0f && normX <= 1.0f &&
            normY >= 0.0f && normY <= 1.0f);

        auto buttonView = scene->GetRegistry().view<UIWidgetComponent, UIButtonComponent>();
        for (auto e : buttonView)
        {
            auto& widget = buttonView.get<UIWidgetComponent>(e);
            auto& button = buttonView.get<UIButtonComponent>(e);

            if (!widget.Visible)
            {
                button.IsHovered = false;
                button.IsPressed = false;
                continue;
            }

            const bool hit = mouseInViewport && HitTest(widget, normX, normY);
            button.IsHovered = hit;
            button.IsPressed = hit && s_MouseWasPressed;
        }

        auto checkboxView = scene->GetRegistry().view<UIWidgetComponent, UICheckboxComponent>();
        for (auto e : checkboxView)
        {
            auto& widget = checkboxView.get<UIWidgetComponent>(e);
            auto& checkbox = checkboxView.get<UICheckboxComponent>(e);

            if (!widget.Visible)
            {
                checkbox.IsHovered = false;
                checkbox.IsPressed = false;
                continue;
            }

            const bool hit = mouseInViewport && HitTest(widget, normX, normY);
            checkbox.IsHovered = hit;
            checkbox.IsPressed = hit && s_MouseWasPressed;
        }

        auto sliderView = scene->GetRegistry().view<UIWidgetComponent, UISliderComponent>();
        for (auto e : sliderView)
        {
            auto& widget = sliderView.get<UIWidgetComponent>(e);
            auto& slider = sliderView.get<UISliderComponent>(e);

            if (!widget.Visible)
            {
                slider.IsHovered = false;
                slider.IsDragging = false;
                continue;
            }

            const UINormalizedRect rect = WidgetToNormalizedRect(widget);
            const bool hit = mouseInViewport && PointInRect(rect, normX, normY);
            slider.IsHovered = hit;

            if (s_MouseWasPressed && (hit || slider.IsDragging))
            {
                const float width = rect.Right - rect.Left;
                if (width > 0.0f)
                    slider.SetNormalized((normX - rect.Left) / width);
                slider.IsDragging = true;
            }
            else if (!s_MouseWasPressed)
            {
                slider.IsDragging = false;
            }
        }
    }

    void UIInputSystem::OnMousePressed(Scene* scene)
    {
        s_MouseWasPressed = true;
    }

    void UIInputSystem::OnMouseReleased(Scene* scene)
    {
        if (!s_MouseWasPressed) return;
        s_MouseWasPressed = false;

        auto buttonView = scene->GetRegistry().view<UIWidgetComponent, UIButtonComponent>();
        for (auto e : buttonView)
        {
            auto& widget = buttonView.get<UIWidgetComponent>(e);
            auto& button = buttonView.get<UIButtonComponent>(e);
            if (widget.Visible && button.IsHovered)
                FireOnClick(scene, e);
            button.IsPressed = false;
        }

        auto checkboxView = scene->GetRegistry().view<UIWidgetComponent, UICheckboxComponent>();
        for (auto e : checkboxView)
        {
            auto& widget = checkboxView.get<UIWidgetComponent>(e);
            auto& checkbox = checkboxView.get<UICheckboxComponent>(e);
            if (widget.Visible && checkbox.IsHovered)
            {
                checkbox.Checked = !checkbox.Checked;
                FireScriptCallback(scene, e, checkbox.OnValueChangedFunction);
            }
            checkbox.IsPressed = false;
        }

        auto sliderView = scene->GetRegistry().view<UIWidgetComponent, UISliderComponent>();
        for (auto e : sliderView)
        {
            auto& slider = sliderView.get<UISliderComponent>(e);
            if (slider.IsDragging)
                FireScriptCallback(scene, e, slider.OnValueChangedFunction);
            slider.IsDragging = false;
        }
    }

} // namespace Wheatear
