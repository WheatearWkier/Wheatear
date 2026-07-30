#pragma once

#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Renderer/Texture.h"

#include <string>
#include <unordered_map>

namespace Wheatear {

    struct AtlasConfig
    {
        Ref<Texture2D> Texture;
        int   Cols = 6;
        int   Rows = 1;
        int   StartCol = 0;
        int   StartRow = 0;
        int   FrameCount = 6;
        float Duration = 0.1f;
    };

    void DrawSpriteAnimatorComponent(Entity entity);

} // namespace Wheatear
