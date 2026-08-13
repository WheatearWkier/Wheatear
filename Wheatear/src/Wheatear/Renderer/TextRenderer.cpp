#include "wtpch.h"
#include "TextRenderer.h"

#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Renderer/Renderer2D.h"

#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace Wheatear {

    Ref<Font> TextRenderer::s_DefaultFont = nullptr;

    static std::filesystem::path ResolveFontPath(const std::string& filepath)
    {
        return AssetPath::Resolve(AssetAliasRegistry::Resolve(filepath));
    }

    static void AppendSystemFontCandidates(std::vector<std::string>& candidates)
    {
        const char* windowsDir = std::getenv("WINDIR");
        if (!windowsDir)
            return;

        const std::filesystem::path fonts = std::filesystem::path(windowsDir) / "Fonts";
        candidates.push_back((fonts / "msyh.ttc").string());
        candidates.push_back((fonts / "simhei.ttf").string());
        candidates.push_back((fonts / "arial.ttf").string());
    }

    static float EffectiveScaleX(const TextRenderParams& params)
    {
        return params.ScaleX > 0.0f ? params.ScaleX : params.Scale;
    }

    static float EffectiveScaleY(const TextRenderParams& params)
    {
        return params.ScaleY > 0.0f ? params.ScaleY : params.Scale;
    }

    void TextRenderer::SetDefaultFont(const Ref<Font>& font)
    {
        s_DefaultFont = font;
    }

    Ref<Font> TextRenderer::GetDefaultFont()
    {
        if (s_DefaultFont && s_DefaultFont->IsLoaded())
            return s_DefaultFont;

        std::vector<std::string> fontCandidates;
        fontCandidates.push_back("font.ui_default");
        AppendSystemFontCandidates(fontCandidates);
        fontCandidates.push_back("font.ui_fallback_sc");
        fontCandidates.push_back("font.latin");

        for (const auto& candidate : fontCandidates)
        {
            std::filesystem::path resolved = ResolveFontPath(candidate);
            if (!std::filesystem::exists(resolved))
                continue;

            s_DefaultFont = Font::Create(resolved.string(), 96.0f, 4096, 4096);
            if (s_DefaultFont)
                return s_DefaultFont;
        }

        WT_CORE_ERROR("TextRenderer: failed to load any fallback font");
        return nullptr;
    }

    static void DrawGlyphQuad(const Ref<Font>& font,
        const FontGlyph& glyph,
        const glm::vec3& topLeft,
        const glm::vec4& color,
        const TextRenderParams& params,
        float cursorX,
        float cursorY)
    {
        if (glyph.Size.x <= 0.0f || glyph.Size.y <= 0.0f)
            return;

        const float x0 = cursorX + glyph.OffsetMin.x;
        const float x1 = cursorX + glyph.OffsetMax.x;
        const float baseline = font->GetAscent();
        const float y0 = cursorY + baseline + glyph.OffsetMin.y;
        const float y1 = cursorY + baseline + glyph.OffsetMax.y;
        const float scaleX = EffectiveScaleX(params);
        const float scaleY = EffectiveScaleY(params);

        float left = topLeft.x + x0 * scaleX;
        float right = topLeft.x + x1 * scaleX;
        float top = topLeft.y - y0 * scaleY;
        float bottom = topLeft.y - y1 * scaleY;
        glm::vec2 uvMin = glyph.UVMin;
        glm::vec2 uvMax = glyph.UVMax;

        if (params.Clip)
        {
            const float clipLeft = params.ClipRect.x;
            const float clipBottom = params.ClipRect.y;
            const float clipRight = params.ClipRect.z;
            const float clipTop = params.ClipRect.w;

            if (right <= clipLeft || left >= clipRight || top <= clipBottom || bottom >= clipTop)
                return;

            const float originalLeft = left;
            const float originalRight = right;
            const float originalTop = top;
            const float originalBottom = bottom;
            const float originalWidth = std::max(0.000001f, originalRight - originalLeft);
            const float originalHeight = std::max(0.000001f, originalTop - originalBottom);

            left = std::max(left, clipLeft);
            right = std::min(right, clipRight);
            bottom = std::max(bottom, clipBottom);
            top = std::min(top, clipTop);

            const float u0 = (left - originalLeft) / originalWidth;
            const float u1 = (right - originalLeft) / originalWidth;
            const float topT = (originalTop - top) / originalHeight;
            const float bottomT = (originalTop - bottom) / originalHeight;

            const float originalUVLeft = glyph.UVMin.x;
            const float originalUVRight = glyph.UVMax.x;
            const float originalUVBottom = glyph.UVMin.y;
            const float originalUVTop = glyph.UVMax.y;

            uvMin.x = originalUVLeft + (originalUVRight - originalUVLeft) * u0;
            uvMax.x = originalUVLeft + (originalUVRight - originalUVLeft) * u1;
            uvMax.y = originalUVTop + (originalUVBottom - originalUVTop) * topT;
            uvMin.y = originalUVTop + (originalUVBottom - originalUVTop) * bottomT;
        }

        const glm::vec3 center = {
            (left + right) * 0.5f,
            (top + bottom) * 0.5f,
            topLeft.z
        };

        const glm::vec3 size = {
            std::max(0.0f, right - left),
            std::max(0.0f, top - bottom),
            1.0f
        };

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), center)
            * glm::scale(glm::mat4(1.0f), size);

        Renderer2D::DrawTextGlyph(transform,
            font->GetAtlasTexture(),
            uvMin,
            uvMax,
            color,
            params.OutlineColor,
            params.OutlineWidth,
            params.EdgeSoftness,
            params.EntityID);
    }

    // Shared layout walker used by both DrawText and MeasureText so wrapping,
    // tab handling, MaxHeight clamping, and kerning stay in one place.
    // onGlyph receives (glyph, cursorX, cursorY) at the glyph's draw origin;
    // outCursorY receives the final baseline Y (after the last line break).
    static void LayoutText(const Ref<Font>& font,
        const std::string& text,
        const TextRenderParams& params,
        const std::function<void(const FontGlyph&, float, float)>& onGlyph,
        float* outCursorY)
    {
        if (outCursorY)
            *outCursorY = 0.0f;

        const FontGlyph* spaceGlyph = font->GetGlyph(' ');
        const float lineAdvance = font->GetLineHeight() * params.LineSpacing;
        const float scaleX = EffectiveScaleX(params);
        const float scaleY = EffectiveScaleY(params);

        float cursorX = 0.0f;
        float cursorY = 0.0f;
        uint32_t previousCodepoint = 0;

        auto breakLine = [&]() -> bool
        {
            cursorX = 0.0f;
            cursorY += lineAdvance;
            previousCodepoint = 0;
            if (outCursorY)
                *outCursorY = cursorY;
            if (params.MaxHeight > 0.0f && (cursorY + lineAdvance) * scaleY > params.MaxHeight)
                return true;
            return false;
        };

        for (size_t i = 0; i < text.size();)
        {
            const uint32_t codepoint = DecodeNextUTF8(text, i);

            if (codepoint == '\r')
                continue;

            if (codepoint == '\n')
            {
                if (breakLine())
                    break;
                continue;
            }

            if (codepoint == '\t')
            {
                const float tabAdvance = (spaceGlyph ? spaceGlyph->Advance : 12.0f) * 4.0f;
                if (params.WrapWidth > 0.0f && cursorX > 0.0f &&
                    (cursorX + tabAdvance) * scaleX > params.WrapWidth)
                {
                    if (breakLine())
                        break;
                }
                cursorX += tabAdvance;
                previousCodepoint = 0;
                continue;
            }

            const FontGlyph* glyph = font->GetGlyph(codepoint);
            if (!glyph)
            {
                previousCodepoint = 0;
                continue;
            }

            if (params.WrapWidth > 0.0f && cursorX > 0.0f &&
                (cursorX + glyph->Advance) * scaleX > params.WrapWidth)
            {
                if (breakLine())
                    break;
                if (codepoint == ' ')
                    continue;
            }

            // Kerning applies between consecutive glyphs on the same line.
            cursorX += font->GetKerning(previousCodepoint, codepoint);

            if (onGlyph)
                onGlyph(*glyph, cursorX, cursorY);

            cursorX += glyph->Advance + params.LetterSpacing;
            previousCodepoint = codepoint;
        }
    }

    void TextRenderer::DrawText(const Ref<Font>& font,
        const std::string& text,
        const glm::vec3& topLeft,
        const glm::vec4& color,
        const TextRenderParams& params)
    {
        if (!font || !font->IsLoaded() || text.empty())
            return;

        LayoutText(font, text, params,
            [&](const FontGlyph& glyph, float cursorX, float cursorY)
            {
                DrawGlyphQuad(font, glyph, topLeft, color, params, cursorX, cursorY);
            },
            nullptr);
    }

    glm::vec2 TextRenderer::MeasureText(const Ref<Font>& font,
        const std::string& text,
        const TextRenderParams& params)
    {
        if (!font || !font->IsLoaded() || text.empty())
            return { 0.0f, 0.0f };

        const float scaleX = EffectiveScaleX(params);
        const float scaleY = EffectiveScaleY(params);

        float maxWidth = 0.0f;
        float finalY = 0.0f;

        // Track the widest point reached on any line; LayoutText owns all the
        // newline/tab/wrap/kerning logic shared with DrawText.
        LayoutText(font, text, params,
            [&](const FontGlyph& glyph, float cursorX, float)
            {
                maxWidth = std::max(maxWidth, cursorX + glyph.Advance + params.LetterSpacing);
            },
            &finalY);

        return { maxWidth * scaleX, finalY * scaleY + font->GetLineHeight() * scaleY };
    }

} // namespace Wheatear
