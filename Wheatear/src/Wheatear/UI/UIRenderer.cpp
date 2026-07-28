#include "wtpch.h"
#include "UIRenderer.h"

#include "Wheatear/Renderer/Camera.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/TextRenderer.h"
#include "Wheatear/Scene/Components.h"

#include <algorithm>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

namespace Wheatear {

    uint32_t  UIRenderer::s_ViewportWidth = 1920;
    uint32_t  UIRenderer::s_ViewportHeight = 1080;
    glm::mat4 UIRenderer::s_SavedViewProjection = glm::mat4(1.0f);

    static std::unordered_map<std::string, Ref<Font>> s_FontCache;

    static Ref<Font> GetUIFont(const std::string& fontPath)
    {
        if (fontPath.empty())
            return TextRenderer::GetDefaultFont();

        auto it = s_FontCache.find(fontPath);
        if (it != s_FontCache.end() && it->second && it->second->IsLoaded())
            return it->second;

        Ref<Font> font = Font::Create(fontPath, 48.0f, 2048, 2048);
        if (!font)
            return TextRenderer::GetDefaultFont();

        s_FontCache[fontPath] = font;
        return font;
    }
    // ═══════════════════════════════════════════════════════════════════════
    //  坐标转换工具（file-local）
    // ═══════════════════════════════════════════════════════════════════════

    // 归一化坐标(0~1) -> NDC(-1~1)，Y 轴翻转（ImGui/屏幕 Y 向下，NDC Y 向上）
    static glm::vec2 NormToNDC(const glm::vec2& pos)
    {
        return { pos.x * 2.0f - 1.0f, 1.0f - pos.y * 2.0f };
    }

    static glm::vec2 NormSizeToNDC(const glm::vec2& size)
    {
        return { size.x * 2.0f, size.y * 2.0f };
    }

    struct WidgetRect
    {
        float Left = 0.0f;
        float Right = 0.0f;
        float Top = 0.0f;
        float Bottom = 0.0f;
    };

    static WidgetRect WidgetToRect(const UIWidgetComponent& widget)
    {
        float ndcX = widget.Position.x * 2.0f - 1.0f;
        float ndcY = -(widget.Position.y * 2.0f - 1.0f);
        float ndcW = widget.Size.x * 2.0f;
        float ndcH = widget.Size.y * 2.0f;

        switch (widget.Anchor)
        {
        case UIAnchor::TopLeft:
            ndcX += ndcW * 0.5f;
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::TopCenter:
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::TopRight:
            ndcX -= ndcW * 0.5f;
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::MiddleLeft:
            ndcX += ndcW * 0.5f;
            break;
        case UIAnchor::MiddleCenter:
            break;
        case UIAnchor::MiddleRight:
            ndcX -= ndcW * 0.5f;
            break;
        case UIAnchor::BottomLeft:
            ndcX += ndcW * 0.5f;
            ndcY += ndcH * 0.5f;
            break;
        case UIAnchor::BottomCenter:
            ndcY += ndcH * 0.5f;
            break;
        case UIAnchor::BottomRight:
            ndcX -= ndcW * 0.5f;
            ndcY += ndcH * 0.5f;
            break;
        }

        WidgetRect rect;
        rect.Left = ndcX - ndcW * 0.5f;
        rect.Right = ndcX + ndcW * 0.5f;
        rect.Top = ndcY + ndcH * 0.5f;
        rect.Bottom = ndcY - ndcH * 0.5f;
        return rect;
    }
    static glm::mat4 RectToTransform(const WidgetRect& rect)
    {
        const float width = rect.Right - rect.Left;
        const float height = rect.Top - rect.Bottom;
        const float centerX = (rect.Left + rect.Right) * 0.5f;
        const float centerY = (rect.Top + rect.Bottom) * 0.5f;

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(centerX, centerY, 0.0f));
        transform = glm::scale(transform, glm::vec3(width, height, 1.0f));
        return transform;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  WidgetToTransform
    //  将 UIWidgetComponent 的归一化布局信息转换为 Renderer2D 用的 mat4 变换。
    //  NDC 空间：X/Y 均在 [-1, 1]，Quad 本地坐标 [-0.5, 0.5]。
    // ═══════════════════════════════════════════════════════════════════════

    glm::mat4 UIRenderer::WidgetToTransform(
        const UIWidgetComponent& widget,
        uint32_t /*viewportWidth*/,
        uint32_t /*viewportHeight*/)
    {
        // Position 为中心点（归一化 0~1），转换为 NDC
        float ndcX = widget.Position.x * 2.0f - 1.0f;
        float ndcY = -(widget.Position.y * 2.0f - 1.0f); // Y 轴翻转

        // Renderer2D Quad 本地坐标 [-0.5, 0.5]
        // Scale 直接等于 NDC 尺寸，quad 会被缩放到正确大小
        float ndcW = widget.Size.x * 2.0f;
        float ndcH = widget.Size.y * 2.0f;

        // Anchor 偏移（在 NDC 空间中把中心点移到锚点对应位置）
        switch (widget.Anchor)
        {
        case UIAnchor::TopLeft:
            ndcX += ndcW * 0.5f;
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::TopCenter:
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::TopRight:
            ndcX -= ndcW * 0.5f;
            ndcY -= ndcH * 0.5f;
            break;
        case UIAnchor::MiddleLeft:
            ndcX += ndcW * 0.5f;
            break;
        case UIAnchor::MiddleCenter:
            break;
        case UIAnchor::MiddleRight:
            ndcX -= ndcW * 0.5f;
            break;
        case UIAnchor::BottomLeft:
            ndcX += ndcW * 0.5f;
            ndcY += ndcH * 0.5f;
            break;
        case UIAnchor::BottomCenter:
            ndcY += ndcH * 0.5f;
            break;
        case UIAnchor::BottomRight:
            ndcX -= ndcW * 0.5f;
            ndcY += ndcH * 0.5f;
            break;
        }

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(ndcX, ndcY, 0.0f));
        transform = glm::rotate(transform,
            glm::radians(widget.Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(ndcW, ndcH, 1.0f));

        return transform;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  UI 正交相机（NDC 直通投影）
    // ═══════════════════════════════════════════════════════════════════════

    class UIOrthographicCamera : public Camera
    {
    public:
        UIOrthographicCamera()
        {
            // NDC 直通：投影矩阵将 [-1,1]^3 映射到裁剪空间，相当于 identity
            m_Projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        }
    };

    // ═══════════════════════════════════════════════════════════════════════
    //  Pass 管理
    // ═══════════════════════════════════════════════════════════════════════

    void UIRenderer::BeginUIPass(uint32_t viewportWidth, uint32_t viewportHeight)
    {
        s_ViewportWidth = viewportWidth;
        s_ViewportHeight = viewportHeight;

        // UI 不做深度测试，始终绘制在场景上方
        RenderCommand::DisableDepthTest();

        // 使用单例 UI 相机（ortho NDC 直通），view = identity
        static UIOrthographicCamera uiCamera;
        Renderer2D::BeginScene(uiCamera, glm::mat4(1.0f));
    }

    void UIRenderer::EndUIPass(const glm::mat4& restoreViewProjection)
    {
        // 提交 UI 批次
        Renderer2D::EndScene();

        // BUG FIX: EndUIPass 之后调用 SetViewProjection 恢复游戏相机 VP 到 UBO，
        // 确保后续 OnOverlayRender 的 BeginScene 能在正确状态下开始。
        // 注意：SetViewProjection 只更新 UBO，不重置批次指针（批次已在 EndScene 中 flush）。
        Renderer2D::SetViewProjection(restoreViewProjection);

        // 恢复深度测试
        RenderCommand::EnableDepthTest();
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  绘制接口
    // ═══════════════════════════════════════════════════════════════════════

    void UIRenderer::DrawUIImage(
        const UIWidgetComponent& widget,
        const UIImageComponent& image,
        int entityID)
    {
        if (!widget.Visible) return;

        glm::mat4 transform = WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight);

        if (image.Texture)
            Renderer2D::DrawQuad(transform, image.Texture, 1.0f, image.Color, entityID);
        else
            Renderer2D::DrawQuad(transform, image.Color, entityID);
    }
    void UIRenderer::DrawUIPanel(
        const UIWidgetComponent& widget,
        const UIPanelComponent& panel,
        int entityID)
    {
        if (!widget.Visible) return;

        const glm::mat4 transform = WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight);
        Renderer2D::DrawQuad(transform, panel.BackgroundColor, entityID);

        if (panel.BorderThickness > 0.0f)
        {
            Renderer2D::SetLineWidth(panel.BorderThickness);
            Renderer2D::DrawRect(transform, panel.BorderColor, entityID);
            Renderer2D::SetLineWidth(1.0f);
        }
    }

    void UIRenderer::DrawUIButton(
        const UIWidgetComponent& widget,
        const UIButtonComponent& button,
        int entityID)
    {
        if (!widget.Visible) return;

        glm::mat4 transform = WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight);

        // 根据状态选色
        glm::vec4 color = button.NormalColor;
        if (button.IsPressed) color = button.PressedColor;
        else if (button.IsHovered) color = button.HoverColor;

        Renderer2D::DrawQuad(transform, color, entityID);
    }

    void UIRenderer::DrawUIProgressBar(
        const UIWidgetComponent& widget,
        const UIProgressBarComponent& bar,
        int entityID)
    {
        if (!widget.Visible) return;

        WidgetRect rect = WidgetToRect(widget);
        Renderer2D::DrawQuad(RectToTransform(rect), bar.BackgroundColor, entityID);

        const float ratio = bar.GetNormalized();
        if (ratio <= 0.0f) return;

        WidgetRect fillRect = rect;
        fillRect.Right = fillRect.Left + (rect.Right - rect.Left) * ratio;
        Renderer2D::DrawQuad(RectToTransform(fillRect), bar.ForegroundColor, entityID);
    }

    void UIRenderer::DrawUISlider(
        const UIWidgetComponent& widget,
        const UISliderComponent& slider,
        int entityID)
    {
        if (!widget.Visible) return;

        const WidgetRect rect = WidgetToRect(widget);
        const float width = rect.Right - rect.Left;
        const float height = rect.Top - rect.Bottom;
        const float centerY = (rect.Top + rect.Bottom) * 0.5f;
        const float trackHalfHeight = std::max(height * 0.14f, 0.006f);

        WidgetRect trackRect = rect;
        trackRect.Top = centerY + trackHalfHeight;
        trackRect.Bottom = centerY - trackHalfHeight;
        Renderer2D::DrawQuad(RectToTransform(trackRect), slider.TrackColor, entityID);

        const float ratio = slider.GetNormalized();
        WidgetRect fillRect = trackRect;
        fillRect.Right = fillRect.Left + width * ratio;
        Renderer2D::DrawQuad(RectToTransform(fillRect), slider.FillColor, entityID);

        const float handleHalfSize = std::min(height * 0.45f, width * 0.08f);
        const float handleX = rect.Left + width * ratio;
        WidgetRect handleRect;
        handleRect.Left = handleX - handleHalfSize;
        handleRect.Right = handleX + handleHalfSize;
        handleRect.Top = centerY + handleHalfSize;
        handleRect.Bottom = centerY - handleHalfSize;

        const glm::vec4 handleColor = (slider.IsHovered || slider.IsDragging)
            ? slider.HoverColor
            : slider.HandleColor;
        Renderer2D::DrawQuad(RectToTransform(handleRect), handleColor, entityID);
    }

    void UIRenderer::DrawUICheckbox(
        const UIWidgetComponent& widget,
        const UICheckboxComponent& checkbox,
        int entityID)
    {
        if (!widget.Visible) return;

        const WidgetRect rect = WidgetToRect(widget);
        const float width = rect.Right - rect.Left;
        const float height = rect.Top - rect.Bottom;
        const float size = std::min(width, height);
        const float centerY = (rect.Top + rect.Bottom) * 0.5f;

        WidgetRect boxRect;
        boxRect.Left = rect.Left;
        boxRect.Right = rect.Left + size;
        boxRect.Top = centerY + size * 0.5f;
        boxRect.Bottom = centerY - size * 0.5f;

        const glm::vec4 boxColor = checkbox.IsPressed
            ? checkbox.PressedColor
            : (checkbox.IsHovered ? checkbox.HoverColor : checkbox.BoxColor);
        Renderer2D::DrawQuad(RectToTransform(boxRect), boxColor, entityID);
        Renderer2D::DrawRect(RectToTransform(boxRect), checkbox.CheckColor, entityID);

        if (checkbox.Checked)
        {
            const float inset = size * 0.24f;
            WidgetRect checkRect;
            checkRect.Left = boxRect.Left + inset;
            checkRect.Right = boxRect.Right - inset;
            checkRect.Top = boxRect.Top - inset;
            checkRect.Bottom = boxRect.Bottom + inset;
            Renderer2D::DrawQuad(RectToTransform(checkRect), checkbox.CheckColor, entityID);
        }
    }

    void UIRenderer::DrawUIText(
        const UIWidgetComponent& widget,
        const UITextComponent& text,
        int entityID)
    {
        if (!widget.Visible || text.Text.empty())
            return;

        Ref<Font> font = GetUIFont(text.FontPath);
        if (!font)
            return;

        WidgetRect rect = WidgetToRect(widget);
        const float viewportHeight = s_ViewportHeight > 0 ? static_cast<float>(s_ViewportHeight) : 1.0f;
        const float baseScale = (text.FontSize / font->GetPixelSize()) * (2.0f / viewportHeight);
        const float paddingX = 0.02f;
        const float paddingY = 0.02f;

        TextRenderParams params;
        params.Scale = baseScale;
        params.LineSpacing = 1.05f;
        params.WrapWidth = std::max(0.0f, (rect.Right - rect.Left) - paddingX * 2.0f);
        params.EntityID = entityID;

        TextRenderer::DrawText(font, text.Text,
            { rect.Left + paddingX, rect.Top - paddingY, 0.0f },
            text.Color, params);
    }

} // namespace Wheatear
