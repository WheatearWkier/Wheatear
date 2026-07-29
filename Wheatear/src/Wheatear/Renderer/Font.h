#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Texture.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

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

        std::vector<unsigned char> m_FontData;
        int m_FontOffset = 0;
        float m_Scale = 1.0f;

        uint32_t m_NextX = 1;
        uint32_t m_NextY = 1;
        uint32_t m_RowHeight = 0;
        std::vector<uint32_t> m_AtlasPixels;

        Ref<Texture2D> m_AtlasTexture;
        std::unordered_map<uint32_t, FontGlyph> m_Glyphs;
    };

} // namespace Wheatear
