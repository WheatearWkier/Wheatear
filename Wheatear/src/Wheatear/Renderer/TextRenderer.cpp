#include "wtpch.h"
#include "TextRenderer.h"

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Renderer/Renderer2D.h"

#include <cstdlib>
#include <filesystem>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace Wheatear {

    Ref<Font> TextRenderer::s_DefaultFont = nullptr;

    static std::filesystem::path ResolveFontPath(const std::string& filepath)
    {
        return AssetPath::Resolve(filepath);
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

    static uint32_t DecodeNextUTF8(const std::string& text, size_t& index)
    {
        const unsigned char c0 = static_cast<unsigned char>(text[index++]);
        if (c0 < 0x80)
            return c0;

        auto continuation = [&](unsigned char c) { return (c & 0xC0) == 0x80; };
        if ((c0 & 0xE0) == 0xC0 && index < text.size())
        {
            const unsigned char c1 = static_cast<unsigned char>(text[index]);
            if (continuation(c1))
            {
                index++;
                return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
            }
        }
        else if ((c0 & 0xF0) == 0xE0 && index + 1 < text.size())
        {
            const unsigned char c1 = static_cast<unsigned char>(text[index]);
            const unsigned char c2 = static_cast<unsigned char>(text[index + 1]);
            if (continuation(c1) && continuation(c2))
            {
                index += 2;
                return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            }
        }
        else if ((c0 & 0xF8) == 0xF0 && index + 2 < text.size())
        {
            const unsigned char c1 = static_cast<unsigned char>(text[index]);
            const unsigned char c2 = static_cast<unsigned char>(text[index + 1]);
            const unsigned char c3 = static_cast<unsigned char>(text[index + 2]);
            if (continuation(c1) && continuation(c2) && continuation(c3))
            {
                index += 3;
                return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) |
                    ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            }
        }

        return '?';
    }

    void TextRenderer::SetDefaultFont(const Ref<Font>& font)
    {
        s_DefaultFont = font;
    }

    Ref<Font> TextRenderer::GetDefaultFont()
    {
        if (s_DefaultFont && s_DefaultFont->IsLoaded())
            return s_DefaultFont;

        const std::vector<std::string> candidates = {
            "assets/fonts/NotoSansSC-VF.ttf",
            "assets/fonts/Open-Sans-2.ttf"
        };

        std::vector<std::string> fontCandidates = candidates;
        AppendSystemFontCandidates(fontCandidates);

        for (const auto& candidate : fontCandidates)
        {
            std::filesystem::path resolved = ResolveFontPath(candidate);
            if (!std::filesystem::exists(resolved))
                continue;

            s_DefaultFont = Font::Create(resolved.string(), 48.0f, 2048, 2048);
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

        const glm::vec3 center = {
            topLeft.x + (x0 + x1) * 0.5f * params.Scale,
            topLeft.y - (y0 + y1) * 0.5f * params.Scale,
            topLeft.z
        };

        const glm::vec3 size = {
            (x1 - x0) * params.Scale,
            (y1 - y0) * params.Scale,
            1.0f
        };

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), center)
            * glm::scale(glm::mat4(1.0f), size);

        Renderer2D::DrawAnimationFrame(transform, font->GetAtlasTexture(),
            glyph.UVMin, glyph.UVMax, false, color, params.EntityID);
    }

    void TextRenderer::DrawText(const Ref<Font>& font,
        const std::string& text,
        const glm::vec3& topLeft,
        const glm::vec4& color,
        const TextRenderParams& params)
    {
        if (!font || !font->IsLoaded() || text.empty())
            return;

        const FontGlyph* spaceGlyph = font->GetGlyph(' ');
        const float lineAdvance = font->GetLineHeight() * params.LineSpacing;

        float cursorX = 0.0f;
        float cursorY = 0.0f;

        for (size_t i = 0; i < text.size();)
        {
            const uint32_t codepoint = DecodeNextUTF8(text, i);

            if (codepoint == '\r')
                continue;

            if (codepoint == '\n')
            {
                cursorX = 0.0f;
                cursorY += lineAdvance;
                continue;
            }

            if (codepoint == '\t')
            {
                const float tabAdvance = (spaceGlyph ? spaceGlyph->Advance : 12.0f) * 4.0f;
                if (params.WrapWidth > 0.0f && cursorX > 0.0f &&
                    (cursorX + tabAdvance) * params.Scale > params.WrapWidth)
                {
                    cursorX = 0.0f;
                    cursorY += lineAdvance;
                }
                cursorX += tabAdvance;
                continue;
            }

            const FontGlyph* glyph = font->GetGlyph(codepoint);
            if (!glyph)
                continue;

            if (params.WrapWidth > 0.0f && cursorX > 0.0f &&
                (cursorX + glyph->Advance) * params.Scale > params.WrapWidth)
            {
                cursorX = 0.0f;
                cursorY += lineAdvance;
                if (codepoint == ' ')
                    continue;
            }

            DrawGlyphQuad(font, *glyph, topLeft, color, params, cursorX, cursorY);
            cursorX += glyph->Advance + params.LetterSpacing;
        }
    }

    glm::vec2 TextRenderer::MeasureText(const Ref<Font>& font,
        const std::string& text,
        const TextRenderParams& params)
    {
        if (!font || !font->IsLoaded() || text.empty())
            return { 0.0f, 0.0f };

        const FontGlyph* spaceGlyph = font->GetGlyph(' ');
        const float lineAdvance = font->GetLineHeight() * params.LineSpacing;

        float cursorX = 0.0f;
        float cursorY = 0.0f;
        float maxWidth = 0.0f;

        for (size_t i = 0; i < text.size();)
        {
            const uint32_t codepoint = DecodeNextUTF8(text, i);
            if (codepoint == '\r')
                continue;

            if (codepoint == '\n')
            {
                maxWidth = std::max(maxWidth, cursorX);
                cursorX = 0.0f;
                cursorY += lineAdvance;
                continue;
            }

            if (codepoint == '\t')
            {
                const float tabAdvance = (spaceGlyph ? spaceGlyph->Advance : 12.0f) * 4.0f;
                cursorX += tabAdvance + params.LetterSpacing;
                maxWidth = std::max(maxWidth, cursorX);
                continue;
            }

            const FontGlyph* glyph = font->GetGlyph(codepoint);
            if (!glyph)
                continue;

            if (params.WrapWidth > 0.0f && cursorX > 0.0f &&
                (cursorX + glyph->Advance) * params.Scale > params.WrapWidth)
            {
                maxWidth = std::max(maxWidth, cursorX);
                cursorX = 0.0f;
                cursorY += lineAdvance;
            }

            cursorX += glyph->Advance + params.LetterSpacing;
            maxWidth = std::max(maxWidth, cursorX);
        }

        maxWidth *= params.Scale;
        return { maxWidth, cursorY * params.Scale + font->GetLineHeight() * params.Scale };
    }

} // namespace Wheatear
