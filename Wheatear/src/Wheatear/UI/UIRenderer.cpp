#include "wtpch.h"
#include "UIRenderer.h"

#include "Wheatear/Renderer/Camera.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/TextRenderer.h"
#include "Wheatear/Scene/Components.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace Wheatear {

    uint32_t  UIRenderer::s_ViewportWidth = 1920;
    uint32_t  UIRenderer::s_ViewportHeight = 1080;

    static std::unordered_map<std::string, Ref<Font>> s_FontCache;
    static std::unordered_map<std::string, Ref<Texture2D>> s_UITextureCache;

    static Ref<Font> GetUIFont(const std::string& fontPath)
    {
        if (fontPath.empty())
            return TextRenderer::GetDefaultFont();

        const std::string resolvedPath = AssetAliasRegistry::Resolve(fontPath);
        auto it = s_FontCache.find(resolvedPath);
        if (it != s_FontCache.end() && it->second && it->second->IsLoaded())
            return it->second;

        Ref<Font> font = Font::Create(resolvedPath, 96.0f, 4096, 4096);
        if (!font)
            return TextRenderer::GetDefaultFont();

        s_FontCache[resolvedPath] = font;
        return font;
    }

    static Ref<Texture2D> GetUITexture(const std::string& texturePath)
    {
        if (texturePath.empty())
            return nullptr;

        const std::string resolvedPath = AssetAliasRegistry::Resolve(texturePath);
        auto it = s_UITextureCache.find(resolvedPath);
        if (it != s_UITextureCache.end())
            return it->second;

        Ref<Texture2D> texture = Texture2D::Create(resolvedPath);
        s_UITextureCache[resolvedPath] = texture;
        return texture;
    }

    void UIRenderer::PreloadUIText(const UITextComponent& text)
    {
        if (text.Text.empty())
            return;

        Ref<Font> font = GetUIFont(text.FontPath);
        if (font)
            font->PreloadText(text.Text);
    }

    static glm::vec3 PixelOffsetToNDC(const glm::vec3& position,
        const glm::vec2& pixelOffset,
        float viewportWidth,
        float viewportHeight)
    {
        return {
            position.x + pixelOffset.x * 2.0f / viewportWidth,
            position.y - pixelOffset.y * 2.0f / viewportHeight,
            position.z
        };
    }

    static glm::vec3 SnapNDCToPixel(const glm::vec3& position,
        float viewportWidth,
        float viewportHeight)
    {
        const float screenX = (position.x * 0.5f + 0.5f) * viewportWidth;
        const float screenY = (1.0f - (position.y * 0.5f + 0.5f)) * viewportHeight;
        const float snappedX = std::round(screenX);
        const float snappedY = std::round(screenY);
        return {
            snappedX * 2.0f / viewportWidth - 1.0f,
            1.0f - snappedY * 2.0f / viewportHeight,
            position.z
        };
    }

    static glm::vec4 EffectColor(glm::vec4 color, float textAlpha)
    {
        color.a *= textAlpha;
        return color;
    }

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

    static glm::mat4 NDCBoxToTransform(const glm::vec2& center, const glm::vec2& size)
    {
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(center.x, center.y, 0.0f));
        transform = glm::scale(transform, glm::vec3(size.x, size.y, 1.0f));
        return transform;
    }

    static glm::vec2 LocalToNDC(const WidgetRect& rect, const glm::vec2& local)
    {
        const float width = rect.Right - rect.Left;
        const float height = rect.Top - rect.Bottom;
        return {
            rect.Left + width * local.x,
            rect.Top - height * local.y
        };
    }

    static bool LocalPointInView(const glm::vec2& local, float margin = 0.0f)
    {
        return local.x >= -margin && local.x <= 1.0f + margin
            && local.y >= -margin && local.y <= 1.0f + margin;
    }

    static bool LocalSegmentIntersectsView(const glm::vec2& a, const glm::vec2& b, float margin = 0.0f)
    {
        const glm::vec2 minPoint = { std::min(a.x, b.x), std::min(a.y, b.y) };
        const glm::vec2 maxPoint = { std::max(a.x, b.x), std::max(a.y, b.y) };
        return maxPoint.x >= -margin && minPoint.x <= 1.0f + margin
            && maxPoint.y >= -margin && minPoint.y <= 1.0f + margin;
    }

    static bool LocalPathIntersectsView(const std::vector<glm::vec2>& points, float margin = 0.0f)
    {
        if (points.empty())
            return false;

        glm::vec2 minPoint = points.front();
        glm::vec2 maxPoint = points.front();
        for (const glm::vec2& point : points)
        {
            minPoint.x = std::min(minPoint.x, point.x);
            minPoint.y = std::min(minPoint.y, point.y);
            maxPoint.x = std::max(maxPoint.x, point.x);
            maxPoint.y = std::max(maxPoint.y, point.y);
        }

        return maxPoint.x >= -margin && minPoint.x <= 1.0f + margin
            && maxPoint.y >= -margin && minPoint.y <= 1.0f + margin;
    }

    static void AppendQuadraticBezier(std::vector<glm::vec2>& out,
        const glm::vec2& start,
        const glm::vec2& control,
        const glm::vec2& end,
        int segments,
        bool skipFirst)
    {
        segments = std::clamp(segments, 2, 96);
        for (int i = skipFirst ? 1 : 0; i <= segments; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float inv = 1.0f - t;
            out.push_back(inv * inv * start + 2.0f * inv * t * control + t * t * end);
        }
    }

    static void AppendCubicBezier(std::vector<glm::vec2>& out,
        const glm::vec2& start,
        const glm::vec2& control0,
        const glm::vec2& control1,
        const glm::vec2& end,
        int segments,
        bool skipFirst)
    {
        segments = std::clamp(segments, 2, 96);
        for (int i = skipFirst ? 1 : 0; i <= segments; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float inv = 1.0f - t;
            out.push_back(inv * inv * inv * start
                + 3.0f * inv * inv * t * control0
                + 3.0f * inv * t * t * control1
                + t * t * t * end);
        }
    }

    static std::vector<glm::vec2> BuildUIPathPoints(const UIPathComponent& path)
    {
        std::vector<glm::vec2> points;
        const int segments = std::clamp(path.Segments, 2, 96);

        if (path.Mode == UIPathMode::Polyline)
        {
            points = path.Points;
        }
        else if (path.Mode == UIPathMode::QuadraticBezier)
        {
            if (path.Points.size() < 3)
                return {};

            AppendQuadraticBezier(points, path.Points[0], path.Points[1], path.Points[2], segments, false);
            for (size_t i = 3; i + 1 < path.Points.size(); i += 2)
                AppendQuadraticBezier(points, points.back(), path.Points[i], path.Points[i + 1], segments, true);
        }
        else if (path.Mode == UIPathMode::CubicBezier)
        {
            if (path.Points.size() < 4)
                return {};

            AppendCubicBezier(points, path.Points[0], path.Points[1], path.Points[2], path.Points[3], segments, false);
            for (size_t i = 4; i + 2 < path.Points.size(); i += 3)
                AppendCubicBezier(points, points.back(), path.Points[i], path.Points[i + 1], path.Points[i + 2], segments, true);
        }

        if (path.Closed && points.size() >= 2)
            points.push_back(points.front());

        return points;
    }

    static void DrawUILocalPolyline(const UIWidgetComponent& widget,
        const std::vector<glm::vec2>& localPoints,
        const glm::vec4& color,
        float lineWidth,
        int entityID)
    {
        if (!widget.Visible || color.a <= 0.0f || localPoints.size() < 2)
            return;

        const WidgetRect rect = WidgetToRect(widget);
        std::vector<glm::vec3> points;
        points.reserve(localPoints.size());
        for (const glm::vec2& local : localPoints)
        {
            const glm::vec2 ndc = LocalToNDC(rect, local);
            points.emplace_back(ndc.x, ndc.y, 0.0f);
        }

        const float previousWidth = Renderer2D::GetLineWidth();
        Renderer2D::SetLineWidth(std::max(lineWidth, 1.0f));
        Renderer2D::DrawPolyline(points, color, entityID);
        Renderer2D::SetLineWidth(previousWidth);
    }

    //  WidgetToTransform

    glm::mat4 UIRenderer::WidgetToTransform(
        const UIWidgetComponent& widget,
        uint32_t /*viewportWidth*/,
        uint32_t /*viewportHeight*/)
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

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(ndcX, ndcY, 0.0f));
        transform = glm::rotate(transform,
            glm::radians(widget.Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(ndcW, ndcH, 1.0f));

        return transform;
    }


    class UIOrthographicCamera : public Camera
    {
    public:
        UIOrthographicCamera()
        {
            m_Projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        }
    };


    void UIRenderer::BeginUIPass(uint32_t viewportWidth, uint32_t viewportHeight)
    {
        s_ViewportWidth = viewportWidth;
        s_ViewportHeight = viewportHeight;

        RenderCommand::SetScissorTest(false);
        RenderCommand::DisableDepthTest();

        static UIOrthographicCamera uiCamera;
        Renderer2D::BeginScene(uiCamera, glm::mat4(1.0f));
    }

    void UIRenderer::EndUIPass(const glm::mat4& restoreViewProjection)
    {
        Renderer2D::EndScene();
        RenderCommand::SetScissorTest(false);

        Renderer2D::SetViewProjection(restoreViewProjection);

        RenderCommand::EnableDepthTest();
    }


    void UIRenderer::DrawUIImage(
        const UIWidgetComponent& widget,
        const UIImageComponent& image,
        int entityID)
    {
        if (!widget.Visible) return;

        glm::mat4 transform = WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight);

        if (image.Texture)
            Renderer2D::DrawAnimationFrame(transform, image.Texture, image.UVMin, image.UVMax, false, image.Color, entityID);
        else
            Renderer2D::DrawQuad(transform, image.Color, entityID);
    }

    void UIRenderer::DrawUIRadialCooldown(
        const UIWidgetComponent& widget,
        const UIRadialCooldownComponent& cooldown,
        int entityID)
    {
        if (!widget.Visible)
            return;

        Renderer2D::DrawRadialCircle(
            WidgetToTransform(widget, s_ViewportWidth, s_ViewportHeight),
            cooldown.Color,
            cooldown.Progress,
            cooldown.StartAngle,
            cooldown.Thickness,
            cooldown.Fade,
            entityID);
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

    void UIRenderer::DrawUIScrollView(
        const UIWidgetComponent& widget,
        const UIScrollViewComponent& scrollView,
        int entityID)
    {
        if (!widget.Visible || !scrollView.ShowScrollbar || scrollView.ContentHeight <= 1.0f)
            return;

        const WidgetRect rect = WidgetToRect(widget);
        const float width = rect.Right - rect.Left;
        const float height = rect.Top - rect.Bottom;
        if (width <= 0.0f || height <= 0.0f)
            return;

        const float barWidth = std::clamp(scrollView.ScrollbarWidth * 2.0f, 0.006f, std::max(width * 0.25f, 0.006f));
        WidgetRect trackRect;
        trackRect.Left = rect.Right - barWidth;
        trackRect.Right = rect.Right;
        trackRect.Top = rect.Top;
        trackRect.Bottom = rect.Bottom;

        Renderer2D::DrawQuad(RectToTransform(trackRect), { 0.03f, 0.04f, 0.045f, 0.52f }, entityID);

        const float thumbHeight = std::clamp(height / std::max(scrollView.ContentHeight, 1.0f), std::min(height, 0.05f), height);
        const float travel = std::max(height - thumbHeight, 0.0f);
        const float normalized = scrollView.GetNormalized();
        const float thumbTop = rect.Top - travel * normalized;

        WidgetRect thumbRect;
        thumbRect.Left = trackRect.Left + barWidth * 0.18f;
        thumbRect.Right = trackRect.Right - barWidth * 0.18f;
        thumbRect.Top = thumbTop;
        thumbRect.Bottom = thumbTop - thumbHeight;

        const glm::vec4 thumbColor = (scrollView.RuntimeThumbHovered || scrollView.RuntimeThumbDragging)
            ? glm::vec4(0.46f, 0.88f, 0.78f, 0.92f)
            : glm::vec4(0.28f, 0.62f, 0.58f, 0.72f);
        Renderer2D::DrawQuad(RectToTransform(thumbRect), thumbColor, entityID);
    }

    void UIRenderer::DrawUIPath(
        const UIWidgetComponent& widget,
        const UIPathComponent& path,
        int entityID)
    {
        if (!widget.Visible || path.Points.size() < 2)
            return;

        std::vector<glm::vec2> localPoints = BuildUIPathPoints(path);
        if (localPoints.size() < 2 || !LocalPathIntersectsView(localPoints, 0.04f))
            return;

        const float minWidgetPixels = std::max(std::min(widget.Size.x * static_cast<float>(s_ViewportWidth),
            widget.Size.y * static_cast<float>(s_ViewportHeight)), 1.0f);
        const float lineWidthPixels = std::max(path.Thickness * minWidgetPixels, 1.0f);
        if (path.DrawGlow && path.GlowColor.a > 0.0f)
            DrawUILocalPolyline(widget, localPoints, path.GlowColor,
                lineWidthPixels * std::max(path.GlowThicknessMultiplier, 1.0f),
                entityID);
        DrawUILocalPolyline(widget, localPoints, path.Color, lineWidthPixels, entityID);
    }

    void UIRenderer::DrawUIBezier(
        const UIWidgetComponent& widget,
        const glm::vec2& start,
        const glm::vec2& control,
        const glm::vec2& end,
        const glm::vec4& color,
        float lineWidth,
        int segments,
        int entityID)
    {
        if (!widget.Visible || color.a <= 0.0f)
            return;

        std::vector<glm::vec2> localPoints;
        localPoints.reserve(static_cast<size_t>(std::clamp(segments, 2, 96)) + 1);
        AppendQuadraticBezier(localPoints, start, control, end, segments, false);
        DrawUILocalPolyline(widget, localPoints, color, lineWidth, entityID);
    }

    void UIRenderer::DrawUISkillTreeView(
        const UIWidgetComponent& widget,
        const UISkillTreeViewComponent& tree,
        int entityID)
    {
        if (!widget.Visible)
            return;

        const WidgetRect rect = WidgetToRect(widget);
        const float width = rect.Right - rect.Left;
        const float height = rect.Top - rect.Bottom;
        if (width <= 0.0f || height <= 0.0f)
            return;

        if (tree.BackgroundColor.a > 0.0f)
            Renderer2D::DrawQuad(RectToTransform(rect), tree.BackgroundColor, entityID);

        const glm::vec2 coreLocal = glm::vec2(0.5f, 0.5f) + tree.Pan;
        if (LocalPointInView(coreLocal, 0.35f))
        {
            const glm::vec2 coreNdc = LocalToNDC(rect, coreLocal);
            const float widgetPixelW = widget.Size.x * static_cast<float>(s_ViewportWidth);
            const float widgetPixelH = widget.Size.y * static_cast<float>(s_ViewportHeight);
            const float ringBasePx = std::max(std::min(widgetPixelW, widgetPixelH), 1.0f);
            const int ringCount = std::clamp(tree.BackgroundRingCount, 0, 8);
            for (int i = 1; i <= ringCount; ++i)
            {
                const float diameterPx = ringBasePx * (0.22f + 0.17f * static_cast<float>(i));
                const glm::vec2 ringSize = {
                    diameterPx * 2.0f / std::max(static_cast<float>(s_ViewportWidth), 1.0f),
                    diameterPx * 2.0f / std::max(static_cast<float>(s_ViewportHeight), 1.0f)
                };
                glm::vec4 ringColor = tree.GridColor;
                ringColor.a *= 1.0f - static_cast<float>(i - 1) * 0.20f;
                Renderer2D::DrawCircle(NDCBoxToTransform(coreNdc, ringSize), ringColor, 0.018f, 0.010f, entityID);
            }
        }

        // string_view keys (no per-frame string copies; the component owns the
        // node Ids, which outlive this call).
        std::unordered_map<std::string_view, const UISkillTreeNodeView*> nodeById;
        nodeById.reserve(tree.Nodes.size());
        for (const auto& node : tree.Nodes)
            nodeById[node.Id] = &node;

        const float minWidgetPixels = std::max(std::min(widget.Size.x * static_cast<float>(s_ViewportWidth),
            widget.Size.y * static_cast<float>(s_ViewportHeight)), 1.0f);
        const float lineWidthPixels = std::max(tree.LineThickness * minWidgetPixels, 1.0f);

        for (const auto& node : tree.Nodes)
        {
            if (node.ParentId.empty())
                continue;

            auto parentIt = nodeById.find(node.ParentId);
            if (parentIt == nodeById.end())
                continue;

            const auto& parent = *parentIt->second;
            const glm::vec2 parentLocal = parent.Position + tree.Pan;
            const glm::vec2 nodeLocal = node.Position + tree.Pan;
            if (!LocalSegmentIntersectsView(parentLocal, nodeLocal, tree.VirtualizationMargin))
                continue;

            glm::vec2 delta = nodeLocal - parentLocal;
            const float length = glm::length(delta);
            const float autoEdgeInset = std::min(tree.NodeSize.x, tree.NodeSize.y) * 0.44f;
            const float edgeInset = std::clamp(tree.NodeEdgeInset, 0.0f, autoEdgeInset);
            if (length <= edgeInset * 2.0f)
                continue;

            const glm::vec2 direction = delta / length;
            const glm::vec2 start = parentLocal + direction * edgeInset;
            const glm::vec2 end = nodeLocal - direction * edgeInset;
            const glm::vec2 perpendicular = { -direction.y, direction.x };
            glm::vec2 radial = (parent.Position + node.Position) * 0.5f - glm::vec2(0.5f, 0.5f);
            if (glm::length(radial) > 0.0001f)
                radial = glm::normalize(radial);
            else
                radial = perpendicular;
            const glm::vec2 control = (start + end) * 0.5f + radial * tree.CurveAmount;

            const bool active = parent.Learned && node.Learned;
            if (tree.DrawLineGlow)
            {
                glm::vec4 glowColor = tree.LineGlowColor;
                glowColor.a *= active ? 1.25f : 0.64f;
                DrawUIBezier(widget, start, control, end,
                    glowColor,
                    lineWidthPixels * 2.35f,
                    tree.LineSegments,
                    entityID);
            }
            DrawUIBezier(widget, start, control, end,
                active ? tree.ActiveLineColor : tree.LineColor,
                lineWidthPixels,
                tree.LineSegments,
                entityID);
        }

        const float nodeDiameterPx = std::max(std::min(tree.NodeSize.x * widget.Size.x * static_cast<float>(s_ViewportWidth),
            tree.NodeSize.y * widget.Size.y * static_cast<float>(s_ViewportHeight)), 14.0f);
        const glm::vec2 nodeNdcSize = {
            nodeDiameterPx * 2.0f / std::max(static_cast<float>(s_ViewportWidth), 1.0f),
            nodeDiameterPx * 2.0f / std::max(static_cast<float>(s_ViewportHeight), 1.0f)
        };
        const glm::vec2 ringNdcSize = nodeNdcSize * 1.18f;
        const glm::vec2 selectedNdcSize = nodeNdcSize * 1.34f;
        const glm::vec2 iconNdcSize = nodeNdcSize * 0.62f;

        for (const auto& node : tree.Nodes)
        {
            const glm::vec2 local = node.Position + tree.Pan;
            if (!LocalPointInView(local, tree.VirtualizationMargin))
                continue;

            const glm::vec2 center = LocalToNDC(rect, local);
            const bool hovered = tree.RuntimeHoveredNodeId == node.Id;
            const bool selected = node.Selected || tree.SelectedNodeId == node.Id;
            const bool core = node.ParentId.empty();

            glm::vec4 fill = core ? tree.CoreNodeColor : tree.NodeColor;
            if (node.Locked && !node.Learned)
                fill = tree.LockedNodeColor;

            if (selected)
                Renderer2D::DrawCircle(NDCBoxToTransform(center, selectedNdcSize), tree.SelectedNodeColor, 0.16f, 0.010f, entityID);
            else if (hovered)
                Renderer2D::DrawCircle(NDCBoxToTransform(center, selectedNdcSize), tree.HoverNodeColor, 0.18f, 0.010f, entityID);

            Renderer2D::DrawCircle(NDCBoxToTransform(center, ringNdcSize),
                node.Learned ? tree.ActiveLineColor : tree.LineColor,
                0.18f,
                0.010f,
                entityID);
            Renderer2D::DrawCircle(NDCBoxToTransform(center, nodeNdcSize), fill, 1.0f, 0.008f, entityID);

            Ref<Texture2D> icon = GetUITexture(node.IconPath);
            if (icon)
            {
                const glm::vec4 iconTint = node.Locked && !node.Learned
                    ? glm::vec4(0.34f, 0.37f, 0.38f, 0.90f)
                    : glm::vec4(1.0f);
                Renderer2D::DrawQuad(NDCBoxToTransform(center, iconNdcSize), icon, 1.0f, iconTint, entityID);
            }

            if (node.Locked && !node.Learned)
                Renderer2D::DrawCircle(NDCBoxToTransform(center, nodeNdcSize * 0.82f), tree.LockColor, 1.0f, 0.010f, entityID);
        }
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
        font->PreloadText(text.Text);

        WidgetRect rect = WidgetToRect(widget);
        const float viewportHeight = s_ViewportHeight > 0 ? static_cast<float>(s_ViewportHeight) : 1.0f;
        const float viewportWidth = s_ViewportWidth > 0 ? static_cast<float>(s_ViewportWidth) : 1.0f;
        const float fontRatio = text.FontSize / font->GetPixelSize();
        const float baseScaleX = fontRatio * (2.0f / viewportWidth);
        const float baseScaleY = fontRatio * (2.0f / viewportHeight);
        const float rectWidth = std::max(0.0f, rect.Right - rect.Left);
        const float rectHeight = std::max(0.0f, rect.Top - rect.Bottom);
        const bool useAutoPadding = text.Padding.x < 0.0f
            && text.Padding.y < 0.0f
            && text.Padding.z < 0.0f
            && text.Padding.w < 0.0f;
        const float paddingLeft = useAutoPadding
            ? std::min(rectWidth * 0.16f, 14.0f * 2.0f / viewportWidth)
            : std::max(0.0f, text.Padding.x) * 2.0f / viewportWidth;
        const float paddingTop = useAutoPadding
            ? std::min(rectHeight * 0.22f, 10.0f * 2.0f / viewportHeight)
            : std::max(0.0f, text.Padding.y) * 2.0f / viewportHeight;
        const float paddingRight = useAutoPadding
            ? paddingLeft
            : std::max(0.0f, text.Padding.z) * 2.0f / viewportWidth;
        const float paddingBottom = useAutoPadding
            ? paddingTop
            : std::max(0.0f, text.Padding.w) * 2.0f / viewportHeight;
        const float contentLeft = rect.Left + paddingLeft;
        const float contentRight = rect.Right - paddingRight;
        const float contentTop = rect.Top - paddingTop;
        const float contentBottom = rect.Bottom + paddingBottom;
        const float contentWidth = std::max(0.0f, contentRight - contentLeft);
        const float contentHeight = std::max(0.0f, contentTop - contentBottom);

        TextRenderParams params;
        params.ScaleX = baseScaleX;
        params.ScaleY = baseScaleY;
        params.LineSpacing = 1.05f;
        params.WrapWidth = contentWidth;
        params.MaxHeight = contentHeight;
        params.Clip = true;
        params.ClipRect = { rect.Left, rect.Bottom, rect.Right, rect.Top };
        params.OutlineColor = EffectColor(text.OutlineColor, text.Color.a);
        params.OutlineWidth = std::clamp(text.OutlineThickness * font->GetOutlinePixelScale(), 0.0f, 3.0f);
        params.EdgeSoftness = 0.42f;
        params.EntityID = entityID;

        for (int i = 0; text.AutoFit && i < 4 && contentHeight > 0.0f; ++i)
        {
            TextRenderParams measureParams = params;
            measureParams.MaxHeight = 0.0f;
            measureParams.Clip = false;
            const glm::vec2 measured = TextRenderer::MeasureText(font, text.Text, measureParams);
            const float widthRatio = measured.x > contentWidth && contentWidth > 0.0f ? contentWidth / measured.x : 1.0f;
            const float heightRatio = measured.y > contentHeight && contentHeight > 0.0f ? contentHeight / measured.y : 1.0f;
            const float fitRatio = std::min(widthRatio, heightRatio);
            if (fitRatio >= 0.995f)
                break;

            const float scale = std::clamp(fitRatio * 0.98f, 0.72f, 1.0f);
            params.ScaleX *= scale;
            params.ScaleY *= scale;
        }

        TextRenderParams finalMeasureParams = params;
        finalMeasureParams.MaxHeight = 0.0f;
        finalMeasureParams.Clip = false;
        const glm::vec2 measured = TextRenderer::MeasureText(font, text.Text, finalMeasureParams);
        const float remainingX = std::max(0.0f, contentWidth - measured.x);
        const float remainingY = std::max(0.0f, contentHeight - measured.y);
        float alignOffsetX = 0.0f;
        float alignOffsetY = 0.0f;
        if (text.HorizontalAlign == UITextHorizontalAlign::Center)
            alignOffsetX = remainingX * 0.5f;
        else if (text.HorizontalAlign == UITextHorizontalAlign::Right)
            alignOffsetX = remainingX;

        if (text.VerticalAlign == UITextVerticalAlign::Middle)
            alignOffsetY = remainingY * 0.5f;
        else if (text.VerticalAlign == UITextVerticalAlign::Bottom)
            alignOffsetY = remainingY;

        const glm::vec3 textTopLeft = SnapNDCToPixel({ contentLeft + alignOffsetX, contentTop - alignOffsetY, 0.0f },
            viewportWidth,
            viewportHeight);

        const glm::vec4 shadowColor = EffectColor(text.ShadowColor, text.Color.a);
        if (shadowColor.a > 0.001f && (std::abs(text.ShadowOffset.x) > 0.001f || std::abs(text.ShadowOffset.y) > 0.001f))
        {
            TextRenderParams shadowParams = params;
            shadowParams.OutlineColor = glm::vec4(0.0f);
            shadowParams.OutlineWidth = 0.0f;
            TextRenderer::DrawText(font, text.Text,
                PixelOffsetToNDC(textTopLeft, text.ShadowOffset, viewportWidth, viewportHeight),
                shadowColor, shadowParams);
        }

        TextRenderer::DrawText(font, text.Text, textTopLeft, text.Color, params);
    }

} // namespace Wheatear
