#pragma once

#include "Wheatear/Core/Core.h"

#include <string>

namespace Wheatear {

    class Scene;
    struct SpriteRendererComponent;
    class Texture2D;

} // namespace Wheatear

namespace Wheatear::GameplayVisualService {

    WHEATEAR_API Ref<Texture2D> LoadTextureCached(const std::string& texturePath);
    WHEATEAR_API int ResolveFrameIndex(float timer, float frameRate, int frameCount, bool loop);
    WHEATEAR_API bool ApplySpriteFrame(SpriteRendererComponent& sprite,
        const std::string& framePattern,
        int oneBasedFrame);
    WHEATEAR_API bool ApplyUIImageFrame(Scene* scene,
        const std::string& entityName,
        const std::string& framePattern,
        int oneBasedFrame,
        bool clearWhenEmpty = false);

} // namespace Wheatear::GameplayVisualService
