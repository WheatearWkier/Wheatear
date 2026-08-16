#pragma once
#include "Wheatear/Scene/Entity.h"

#include <string>

namespace Wheatear {
    // Shared SpriteSheet reference fields (sheet path + cell index + named-rect
    // SubRect dropdown). Used by the Sprite Renderer and UI Image drawers so a
    // designer can point a sprite at any sheet cell or named rect without
    // hand-editing scene YAML.
    void DrawSpriteSheetReference(std::string& sheetPath,
        int& cellIndex,
        std::string& subRect);

    void DrawUICanvasComponent(Entity entity);
    void DrawUIWidgetComponent(Entity entity);
    void DrawUIAnimatorComponent(Entity entity);
    void DrawUIImageComponent(Entity entity);
    void DrawUIRadialCooldownComponent(Entity entity);
    void DrawUIPanelComponent(Entity entity);
    void DrawUITextComponent(Entity entity);
    void DrawUIButtonComponent(Entity entity);
    void DrawUIProgressBarComponent(Entity entity);
    void DrawUISliderComponent(Entity entity);
    void DrawUIPagerComponent(Entity entity);
    void DrawUIScrollViewComponent(Entity entity);
    void DrawUIPathComponent(Entity entity);
    void DrawUISkillTreeViewComponent(Entity entity);
    void DrawUIPageItemComponent(Entity entity);
    void DrawUICheckboxComponent(Entity entity);
} // namespace Wheatear
