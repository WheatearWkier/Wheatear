#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

#include <glm/glm.hpp>

namespace Wheatear {

    class Scene;
    struct SpriteRendererComponent;
    class Texture2D;

} // namespace Wheatear

namespace Wheatear::GameplayVisualService {

    struct TextureAtlasFrameSpec
    {
        std::string SheetPath;
        int CellWidth = 0;
        int CellHeight = 0;
        int Columns = 0;
        int StartFrame = 0;

        bool IsValid() const
        {
            return !SheetPath.empty() && CellWidth > 0 && CellHeight > 0;
        }
    };

    WHEATEAR_API Ref<Texture2D> LoadTextureCached(const std::string& texturePath);
    WHEATEAR_API int ResolveFrameIndex(float timer, float frameRate, int frameCount, bool loop);
    WHEATEAR_API bool ResolveAtlasFrame(
        const TextureAtlasFrameSpec& atlas,
        int oneBasedFrame,
        Ref<Texture2D>* texture,
        glm::vec2* uvMin,
        glm::vec2* uvMax);
    WHEATEAR_API bool ApplySpriteAtlasFrame(SpriteRendererComponent& sprite,
        const TextureAtlasFrameSpec& atlas,
        int oneBasedFrame);
    WHEATEAR_API bool ApplySpriteFrame(SpriteRendererComponent& sprite,
        const std::string& framePattern,
        int oneBasedFrame);
    WHEATEAR_API bool ApplyUIImageAtlasFrame(Scene* scene,
        const std::string& entityName,
        const TextureAtlasFrameSpec& atlas,
        int oneBasedFrame,
        bool clearWhenEmpty = false);
    WHEATEAR_API bool ApplyUIImageFrame(Scene* scene,
        const std::string& entityName,
        const std::string& framePattern,
        int oneBasedFrame,
        bool clearWhenEmpty = false);

} // namespace Wheatear::GameplayVisualService
