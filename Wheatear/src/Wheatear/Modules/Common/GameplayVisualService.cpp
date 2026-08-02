#include "wtpch.h"
#include "GameplayVisualService.h"

#include "Wheatear/Modules/Common/GameplayTextService.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Wheatear::GameplayVisualService {

    Ref<Texture2D> LoadTextureCached(const std::string& texturePath)
    {
        if (texturePath.empty())
            return nullptr;

        static std::unordered_map<std::string, Ref<Texture2D>> textureCache;
        if (auto it = textureCache.find(texturePath); it != textureCache.end())
            return it->second;

        Ref<Texture2D> texture = Texture2D::Create(texturePath);
        if (!texture || !texture->IsLoaded())
            return nullptr;

        textureCache[texturePath] = texture;
        return texture;
    }

    int ResolveFrameIndex(float timer, float frameRate, int frameCount, bool loop)
    {
        const int safeFrameCount = std::max(1, frameCount);
        const float safeFrameRate = std::max(1.0f, frameRate);
        int frame = 1 + static_cast<int>(std::floor(std::max(0.0f, timer) * safeFrameRate));

        if (loop)
            return ((frame - 1) % safeFrameCount) + 1;

        return std::clamp(frame, 1, safeFrameCount);
    }

    bool ApplySpriteFrame(SpriteRendererComponent& sprite,
        const std::string& framePattern,
        int oneBasedFrame)
    {
        const std::string path = GameplayTextService::FormatFramePath(framePattern, oneBasedFrame);
        if (path.empty())
            return false;

        if (Ref<Texture2D> texture = LoadTextureCached(path))
        {
            sprite.Texture = texture;
            return true;
        }
        return false;
    }

    bool ApplyUIImageFrame(Scene* scene,
        const std::string& entityName,
        const std::string& framePattern,
        int oneBasedFrame,
        bool clearWhenEmpty)
    {
        const std::string path = GameplayTextService::FormatFramePath(framePattern, oneBasedFrame);
        if (path.empty())
            return false;

        UIRuntimeTools::SetImageTexture(scene, entityName, path, clearWhenEmpty);
        return true;
    }

} // namespace Wheatear::GameplayVisualService
