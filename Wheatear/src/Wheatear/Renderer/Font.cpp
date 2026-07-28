#include "wtpch.h"
#include "Font.h"

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

namespace Wheatear {

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

        m_AtlasPixels.assign(static_cast<size_t>(m_AtlasWidth) * m_AtlasHeight, 0x00ffffffu);
        m_AtlasTexture = Texture2D::Create(m_AtlasWidth, m_AtlasHeight);
        if (!m_AtlasTexture)
        {
            WT_CORE_WARN("Font: failed to allocate atlas texture for {0}", m_Path);
            return false;
        }

        m_Loaded = true;

        // Preload ASCII so editor/UI text is stable before the first frame.
        for (uint32_t codepoint = 32; codepoint <= 126; ++codepoint)
            LoadGlyph(codepoint);

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

        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&info, static_cast<int>(codepoint), m_Scale, m_Scale, &x0, &y0, &x1, &y1);

        FontGlyph glyph;
        glyph.Advance = static_cast<float>(advance) * m_Scale;
        glyph.OffsetMin = { static_cast<float>(x0), static_cast<float>(y0) };
        glyph.OffsetMax = { static_cast<float>(x1), static_cast<float>(y1) };
        glyph.Size = { static_cast<float>(std::max(0, x1 - x0)), static_cast<float>(std::max(0, y1 - y0)) };

        if (glyph.Size.x <= 0.0f || glyph.Size.y <= 0.0f)
        {
            auto [it, inserted] = m_Glyphs.emplace(codepoint, glyph);
            return &it->second;
        }

        uint32_t atlasX = 0;
        uint32_t atlasY = 0;
        if (!AllocateGlyphRect(static_cast<uint32_t>(glyph.Size.x), static_cast<uint32_t>(glyph.Size.y), atlasX, atlasY))
        {
            WT_CORE_WARN("Font: atlas full while loading codepoint U+{0:X} from {1}", codepoint, m_Path);
            return nullptr;
        }

        std::vector<unsigned char> bitmap(static_cast<size_t>(glyph.Size.x) * static_cast<size_t>(glyph.Size.y), 0);
        stbtt_MakeCodepointBitmap(&info, bitmap.data(),
            static_cast<int>(glyph.Size.x), static_cast<int>(glyph.Size.y),
            static_cast<int>(glyph.Size.x), m_Scale, m_Scale,
            static_cast<int>(codepoint));

        for (uint32_t y = 0; y < static_cast<uint32_t>(glyph.Size.y); ++y)
        {
            for (uint32_t x = 0; x < static_cast<uint32_t>(glyph.Size.x); ++x)
            {
                const uint8_t alpha = bitmap[static_cast<size_t>(y) * static_cast<size_t>(glyph.Size.x) + x];
                m_AtlasPixels[static_cast<size_t>(atlasY + y) * m_AtlasWidth + (atlasX + x)] =
                    (static_cast<uint32_t>(alpha) << 24) | 0x00ffffffu;
            }
        }

        const float invWidth = 1.0f / static_cast<float>(m_AtlasWidth);
        const float invHeight = 1.0f / static_cast<float>(m_AtlasHeight);
        glyph.UVMin = { atlasX * invWidth, (atlasY + glyph.Size.y) * invHeight };
        glyph.UVMax = { (atlasX + glyph.Size.x) * invWidth, atlasY * invHeight };

        auto [it, inserted] = m_Glyphs.emplace(codepoint, glyph);
        UploadAtlas();
        return &it->second;
    }

    void Font::UploadAtlas()
    {
        if (!m_AtlasTexture || m_AtlasPixels.empty())
            return;

        m_AtlasTexture->SetData(m_AtlasPixels.data(),
            static_cast<uint32_t>(m_AtlasPixels.size() * sizeof(uint32_t)));
    }

} // namespace Wheatear
