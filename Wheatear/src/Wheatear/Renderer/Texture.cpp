#include "wtpch.h"
#include "Texture.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Wheatear {

    namespace {

        struct TextureCacheEntry
        {
            Ref<Texture2D> Texture;
            uint64_t LastUsedTick = 0;
            uint64_t Bytes = 0;
        };

        static std::unordered_map<std::string, TextureCacheEntry>& TextureCache()
        {
            static std::unordered_map<std::string, TextureCacheEntry> cache;
            return cache;
        }

        // VRAM budget for cached textures. When exceeded, entries that nobody
        // references anymore (use_count == 1: only the cache holds them) are
        // evicted least-recently-used first, so long editor sessions do not
        // accumulate every texture ever loaded.
        static constexpr uint64_t kTextureCacheBudgetBytes = 512ull * 1024 * 1024;

        static uint64_t s_TextureAccessTick = 0;
        static uint64_t s_TotalCachedBytes = 0;

        static std::string MakeTextureCacheKey(const std::string& path)
        {
            return AssetPath::ToProjectRelative(path).generic_string();
        }

        static uint64_t TextureBytes(const Ref<Texture2D>& texture)
        {
            return static_cast<uint64_t>(texture->GetWidth()) * texture->GetHeight() * 4ull;
        }

        static void EvictOverBudget()
        {
            if (s_TotalCachedBytes <= kTextureCacheBudgetBytes)
                return;

            auto& cache = TextureCache();

            std::vector<std::pair<uint64_t, const std::string*>> evictable;
            evictable.reserve(cache.size());
            for (const auto& [key, entry] : cache)
            {
                if (entry.Texture.use_count() == 1)
                    evictable.emplace_back(entry.LastUsedTick, &key);
            }

            std::sort(evictable.begin(), evictable.end(),
                [](const auto& left, const auto& right) { return left.first < right.first; });

            for (const auto& [tick, key] : evictable)
            {
                if (s_TotalCachedBytes <= kTextureCacheBudgetBytes)
                    break;

                auto it = cache.find(*key);
                if (it == cache.end())
                    continue;
                s_TotalCachedBytes -= it->second.Bytes;
                cache.erase(it);
            }
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
        const uint64_t tick = ++s_TextureAccessTick;
        if (auto it = cache.find(cacheKey); it != cache.end())
        {
            it->second.LastUsedTick = tick;
            return it->second.Texture;
        }

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
        {
            TextureCacheEntry entry;
            entry.Texture = texture;
            entry.LastUsedTick = tick;
            entry.Bytes = TextureBytes(texture);
            s_TotalCachedBytes += entry.Bytes;
            cache[cacheKey] = std::move(entry);
            EvictOverBudget();
        }

        return texture;
    }

} // namespace Wheatear
