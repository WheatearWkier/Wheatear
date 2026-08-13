#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Texture.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct stbtt_fontinfo;

namespace Wheatear {

    // Shared UTF-8 decoder (single definition; Font.cpp and TextRenderer.cpp
    // previously each carried a private copy that could drift).
    inline uint32_t DecodeNextUTF8(const std::string& text, size_t& index)
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

    struct FontGlyph
    {
        glm::vec2 Size = { 0.0f, 0.0f };
        glm::vec2 OffsetMin = { 0.0f, 0.0f };
        glm::vec2 OffsetMax = { 0.0f, 0.0f };
        glm::vec2 UVMin = { 0.0f, 0.0f };
        glm::vec2 UVMax = { 1.0f, 1.0f };
        float     Advance = 0.0f;
    };

    class WHEATEAR_API Font
    {
    public:
        static Ref<Font> Create(const std::string& filepath,
            float pixelSize = 48.0f,
            uint32_t atlasWidth = 2048,
            uint32_t atlasHeight = 2048);

        const FontGlyph* GetGlyph(uint32_t codepoint);
        const FontGlyph* GetGlyph(char character) { return GetGlyph(static_cast<uint32_t>(static_cast<unsigned char>(character))); }
        void PreloadText(const std::string& text);

        // Horizontal kerning between two consecutive codepoints (scaled to the
        // font's pixel size). Cached; returns 0 for unknown pairs.
        float GetKerning(uint32_t previousCodepoint, uint32_t codepoint);

        const Ref<Texture2D>& GetAtlasTexture() const { return m_AtlasTexture; }
        bool IsLoaded() const { return m_Loaded; }

        const std::string& GetPath() const { return m_Path; }
        float GetPixelSize() const { return m_PixelSize; }
        float GetAscent() const { return m_Ascent; }
        float GetDescent() const { return m_Descent; }
        float GetLineGap() const { return m_LineGap; }
        float GetLineHeight() const { return m_LineHeight; }
        uint32_t GetAtlasWidth() const { return m_AtlasWidth; }
        uint32_t GetAtlasHeight() const { return m_AtlasHeight; }
        float GetOutlinePixelScale() const { return 1.0f; }

    private:
        Font(const std::string& filepath, float pixelSize,
            uint32_t atlasWidth, uint32_t atlasHeight);
        bool Load();
        const FontGlyph* LoadGlyph(uint32_t codepoint);
        bool AllocateGlyphRect(uint32_t width, uint32_t height, uint32_t& outX, uint32_t& outY);
        void UploadAtlas();

    private:
        std::string m_Path;
        float m_PixelSize = 48.0f;
        uint32_t m_AtlasWidth = 0;
        uint32_t m_AtlasHeight = 0;
        bool m_Loaded = false;

        float m_Ascent = 0.0f;
        float m_Descent = 0.0f;
        float m_LineGap = 0.0f;
        float m_LineHeight = 0.0f;
        int m_GlyphPadding = 2;
        bool m_AtlasDirty = false;
        bool m_DeferAtlasUpload = false;

        // Dirty-region tracking: only newly rasterized glyphs are re-uploaded to
        // the GPU instead of the whole atlas on every cache miss.
        bool m_AtlasUploaded = false;
        bool m_HasDirtyRegion = false;
        uint32_t m_DirtyMinX = 0;
        uint32_t m_DirtyMinY = 0;
        uint32_t m_DirtyMaxX = 0;
        uint32_t m_DirtyMaxY = 0;

        std::vector<unsigned char> m_FontData;
        int m_FontOffset = 0;
        float m_Scale = 1.0f;

        uint32_t m_NextX = 1;
        uint32_t m_NextY = 1;
        uint32_t m_RowHeight = 0;
        std::vector<uint32_t> m_AtlasPixels;

        Ref<Texture2D> m_AtlasTexture;
        std::unordered_map<uint32_t, FontGlyph> m_Glyphs;
        std::unordered_map<uint64_t, float> m_KerningCache;
        std::unique_ptr<stbtt_fontinfo> m_FontInfo;
    };

} // namespace Wheatear
