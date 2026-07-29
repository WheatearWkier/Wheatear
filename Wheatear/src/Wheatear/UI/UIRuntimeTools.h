#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Wheatear {

    class Scene;
} // namespace Wheatear

namespace Wheatear::UIRuntimeTools {

    void SetWidgetVisible(Scene* scene, const std::string& entityName, bool visible);
    void SetWidgetTopLeft(Scene* scene, const std::string& entityName, const glm::vec2& position, const glm::vec2& size);
    void SetWidgetCenter(Scene* scene,
        const std::string& entityName,
        const glm::vec2& position,
        const glm::vec2& size,
        float rotation = 0.0f);
    void SetWidgetParent(Scene* scene, const std::string& entityName, const std::string& parentTag);

    void SetText(Scene* scene, const std::string& entityName, const std::string& text);
    void SetProgress(Scene* scene, const std::string& entityName, float value, float maxValue);
    void SetImageAlpha(Scene* scene, const std::string& entityName, float alpha);
    void SetImageColor(Scene* scene, const std::string& entityName, const glm::vec4& color);
    void SetImageTexture(Scene* scene, const std::string& entityName, const std::string& texturePath, bool clearWhenEmpty = false);
    bool IsButtonHovered(Scene* scene, const std::string& entityName);

} // namespace Wheatear::UIRuntimeTools
