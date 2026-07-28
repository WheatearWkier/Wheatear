#pragma once

#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Renderer/Texture.h"

#include <string>
#include <unordered_map>

namespace Wheatear {

    // Atlas 生成器的配置，每个 AnimationClip 独立一份
    // key = clip 名字，由 SceneHierarchyPanel 持有并传入
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