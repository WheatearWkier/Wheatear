#include "wtpch.h"
#include "Font.h"

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

namespace Wheatear {

    namespace {

        static uint32_t RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            return static_cast<uint32_t>(r)
                | (static_cast<uint32_t>(g) << 8)
                | (static_cast<uint32_t>(b) << 16)
                | (static_cast<uint32_t>(a) << 24);
        }

        static uint32_t DecodeNextUTF8(const std::string& text, size_t& index)
        {
            const unsigned char c0 = static_cast<unsigned char>(text[index++]);
            if (c0 < 0x80)
                return c0;

            auto continuation = [](unsigned char c) { return (c & 0xC0) == 0x80; };
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
                    return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12)
                        | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                }
            }

            return '?';
        }

    } // namespace

    Font::Font(const std::string& filepath, float pixelSize,
        uint32_t atlasWidth, uint32_t atlasHeight)
        : m_Path(AssetPath::ToProjectRelative(filepath).generic_string()),
        m_PixelSize(pixelSize), m_AtlasWidth(atlasWidth), m_AtlasHeight(atlasHeight)
    {
    }

    Ref<Font> Font::Create(const std::string& filepath,
        float pixelSize, uint32_t atlasWidth, uint32_t atlasHeight)
    {
        Ref<Font> font = Ref<Font>(new Font(filepath, pixelSize, atlasWidth, atlasHeight));
        if (!font->Load())
            return nullptr;
        return font;
    }

    bool Font::Load()
    {
        std::filesystem::path path = AssetPath::Resolve(m_Path);
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input.is_open())
        {
            WT_CORE_WARN("Font: cannot open file {0}", m_Path);
            return false;
        }

        const std::streamsize size = input.tellg();
        if (size <= 0)
        {
            WT_CORE_WARN("Font: empty file {0}", m_Path);
            return false;
        }

        m_FontData.resize(static_cast<size_t>(size));
        input.seekg(0, std::ios::beg);
        if (!input.read(reinterpret_cast<char*>(m_FontData.data()), size))
        {
            WT_CORE_WARN("Font: failed to read file {0}", m_Path);
            return false;
        }

        stbtt_fontinfo info{};
        m_FontOffset = stbtt_GetFontOffsetForIndex(m_FontData.data(), 0);
        if (m_FontOffset < 0 || !stbtt_InitFont(&info, m_FontData.data(), m_FontOffset))
        {
            WT_CORE_WARN("Font: invalid font data {0}", m_Path);
            return false;
        }

        int ascent = 0, descent = 0, lineGap = 0;
        m_Scale = stbtt_ScaleForPixelHeight(&info, m_PixelSize);
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        m_Ascent = static_cast<float>(ascent) * m_Scale;
        m_Descent = static_cast<float>(descent) * m_Scale;
        m_LineGap = static_cast<float>(lineGap) * m_Scale;
        m_LineHeight = static_cast<float>(ascent - descent + lineGap) * m_Scale;

        m_AtlasPixels.assign(static_cast<size_t>(m_AtlasWidth) * m_AtlasHeight, RGBA(0, 0, 0, 0));
        m_AtlasTexture = Texture2D::Create(m_AtlasWidth, m_AtlasHeight);
        if (!m_AtlasTexture)
        {
            WT_CORE_WARN("Font: failed to allocate atlas texture for {0}", m_Path);
            return false;
        }

        m_Loaded = true;

        m_DeferAtlasUpload = true;
        for (uint32_t codepoint = 32; codepoint <= 126; ++codepoint)
            LoadGlyph(codepoint);
        m_DeferAtlasUpload = false;

        UploadAtlas();
        return true;
    }

    const FontGlyph* Font::GetGlyph(uint32_t codepoint)
    {
        auto it = m_Glyphs.find(codepoint);
        if (it != m_Glyphs.end())
            return &it->second;

        if (const FontGlyph* glyph = LoadGlyph(codepoint))
            return glyph;

        it = m_Glyphs.find('?');
        if (it != m_Glyphs.end())
            return &it->second;

        return nullptr;
    }

    void Font::PreloadText(const std::string& text)
    {
        if (!m_Loaded || text.empty())
            return;

        const bool previousDeferState = m_DeferAtlasUpload;
        m_DeferAtlasUpload = true;

        for (size_t i = 0; i < text.size();)
        {
            const uint32_t codepoint = DecodeNextUTF8(text, i);
            if (codepoint == '\r' || codepoint == '\n' || codepoint == '\t')
                continue;

            if (m_Glyphs.find(codepoint) == m_Glyphs.end())
                LoadGlyph(codepoint);
        }

        m_DeferAtlasUpload = previousDeferState;
        if (!m_DeferAtlasUpload)
            UploadAtlas();
    }

    bool Font::AllocateGlyphRect(uint32_t width, uint32_t height, uint32_t& outX, uint32_t& outY)
    {
        constexpr uint32_t padding = 1;
        const uint32_t paddedWidth = width + padding * 2;
        const uint32_t paddedHeight = height + padding * 2;

        if (paddedWidth >= m_AtlasWidth || paddedHeight >= m_AtlasHeight)
            return false;

        if (m_NextX + paddedWidth >= m_AtlasWidth)
        {
            m_NextX = 1;
            m_NextY += m_RowHeight + padding;
            m_RowHeight = 0;
        }

        if (m_NextY + paddedHeight >= m_AtlasHeight)
            return false;

        outX = m_NextX + padding;
        outY = m_NextY + padding;
        m_NextX += paddedWidth;
        m_RowHeight = std::max(m_RowHeight, paddedHeight);
        return true;
    }

    const FontGlyph* Font::LoadGlyph(uint32_t codepoint)
    {
        if (!m_Loaded || m_FontData.empty())
            return nullptr;

        stbtt_fontinfo info{};
        if (!stbtt_InitFont(&info, m_FontData.data(), m_FontOffset))
            return nullptr;

        int advance = 0;
        int leftBearing = 0;
        stbtt_GetCodepointHMetrics(&info, static_cast<int>(codepoint), &advance, &leftBearing);

        FontGlyph glyph;
        glyph.Advance = static_cast<float>(advance) * m_Scale;

        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetCodepointBitmapBoxSubpixel(&info,
            static_cast<int>(codepoint),
            m_Scale,
            m_Scale,
            0.0f,
            0.0f,
            &x0,
            &y0,
            &x1,
            &y1);

        if (x1 <= x0 || y1 <= y0)
        {
            auto [it, inserted] = m_Glyphs.emplace(codepoint, glyph);
            return &it->second;
        }

        const int bitmapWidth = x1 - x0;
        const int bitmapHeight = y1 - y0;
        const int paddedWidth = bitmapWidth + m_GlyphPadding * 2;
        const int paddedHeight = bitmapHeight + m_GlyphPadding * 2;

        glyph.OffsetMin = {
            static_cast<float>(x0 - m_GlyphPadding),
            static_cast<float>(y0 - m_GlyphPadding)
        };
        glyph.OffsetMax = {
            static_cast<float>(x1 + m_GlyphPadding),
            static_cast<float>(y1 + m_GlyphPadding)
        };
        glyph.Size = {
            static_cast<float>(paddedWidth),
            static_cast<float>(paddedHeight)
        };

        uint32_t atlasX = 0;
        uint32_t atlasY = 0;
        if (!AllocateGlyphRect(static_cast<uint32_t>(paddedWidth), static_cast<uint32_t>(paddedHeight), atlasX, atlasY))
        {
            WT_CORE_WARN("Font: atlas full while loading codepoint U+{0:X} from {1}", codepoint, m_Path);
            return nullptr;
        }

        std::vector<unsigned char> bitmap(static_cast<size_t>(paddedWidth) * paddedHeight, 0);
        unsigned char* bitmapStart = bitmap.data()
            + static_cast<size_t>(m_GlyphPadding) * paddedWidth
            + static_cast<size_t>(m_GlyphPadding);
        stbtt_MakeCodepointBitmapSubpixel(&info,
            bitmapStart,
            bitmapWidth,
            bitmapHeight,
            paddedWidth,
            m_Scale,
            m_Scale,
            0.0f,
            0.0f,
            static_cast<int>(codepoint));

        for (uint32_t y = 0; y < static_cast<uint32_t>(paddedHeight); ++y)
        {
            for (uint32_t x = 0; x < static_cast<uint32_t>(paddedWidth); ++x)
            {
                const uint8_t alpha = bitmap[static_cast<size_t>(y) * paddedWidth + x];
                m_AtlasPixels[static_cast<size_t>(atlasY + y) * m_AtlasWidth + (atlasX + x)] =
                    RGBA(255, 255, 255, alpha);
            }
        }

        const float invWidth = 1.0f / static_cast<float>(m_AtlasWidth);
        const float invHeight = 1.0f / static_cast<float>(m_AtlasHeight);
        glyph.UVMin = { atlasX * invWidth, (atlasY + glyph.Size.y) * invHeight };
        glyph.UVMax = { (atlasX + glyph.Size.x) * invWidth, atlasY * invHeight };

        auto [it, inserted] = m_Glyphs.emplace(codepoint, glyph);
        m_AtlasDirty = true;
        if (!m_DeferAtlasUpload)
            UploadAtlas();

        return &it->second;
    }

    void Font::UploadAtlas()
    {
        if (!m_AtlasTexture || m_AtlasPixels.empty() || !m_AtlasDirty)
            return;

        m_AtlasTexture->SetData(m_AtlasPixels.data(),
            static_cast<uint32_t>(m_AtlasPixels.size() * sizeof(uint32_t)));
        m_AtlasDirty = false;
    }

} // namespace Wheatear
