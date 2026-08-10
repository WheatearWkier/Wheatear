#include "wtpch.h"
#include "Texture.h"

#include "Wheatear/Core/AssetPath.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Wheatear {

    namespace {

        static std::unordered_map<std::string, Ref<Texture2D>>& TextureCache()
        {
            static std::unordered_map<std::string, Ref<Texture2D>> cache;
            return cache;
        }

        static std::string MakeTextureCacheKey(const std::string& path)
        {
            return AssetPath::ToProjectRelative(path).generic_string();
        }

    } // namespace

    Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLTexture2D>(width, height);
        case RendererAPI::API::None:
            WT_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        WT_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    Ref<Texture2D> Texture2D::Create(const std::string& path)
    {
        const std::string cacheKey = MakeTextureCacheKey(path);
        auto& cache = TextureCache();
        if (auto it = cache.find(cacheKey); it != cache.end())
            return it->second;

        Ref<Texture2D> texture;
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            texture = CreateRef<OpenGLTexture2D>(path);
            break;
        case RendererAPI::API::None:
            WT_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        default:
            WT_CORE_ASSERT(false, "Unknown RendererAPI");
            return nullptr;
        }

        if (texture && texture->IsLoaded())
            cache[cacheKey] = texture;

        return texture;
    }

} // namespace Wheatear
