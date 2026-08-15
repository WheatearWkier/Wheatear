#pragma once

#include "Wheatear/Core/Core.h"

#include <cstdint>
#include <string>

namespace Wheatear {

    class Texture
    {
    public:
        virtual ~Texture() = default;

        virtual uint32_t GetWidth()      const = 0;
        virtual uint32_t GetHeight()     const = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual void SetData(void* data, uint32_t size) = 0;
        // Upload a sub-region (x,y,width,height) of the texture. The source
        // pixels must be tightly packed with exactly width pixels per row.
        // Used by the font atlas to re-upload only newly rasterized glyphs
        // instead of the whole 64 MB atlas.
        virtual void SetSubData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data) = 0;
        virtual void Bind(uint32_t slot = 0) const = 0;

        virtual bool IsLoaded() const = 0;

        virtual bool operator==(const Texture& other) const = 0;
    };

    class Texture2D : public Texture
    {
    public:
        virtual const std::string& GetPath() const = 0;

        static Ref<Texture2D> Create(uint32_t width, uint32_t height);

        static Ref<Texture2D> Create(const std::string& path);
    };

} // namespace Wheatear
