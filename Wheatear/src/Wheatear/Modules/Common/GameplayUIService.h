#pragma once

#include "Wheatear/Core/Core.h"

#include <glm/glm.hpp>

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::GameplayUIService {

    WHEATEAR_API void SetVisible(Scene* scene, const std::string& entityName, bool visible);
    WHEATEAR_API void SetHealth(Scene* scene,
        const std::string& barEntityName,
        const std::string& textEntityName,
        const std::string& label,
        float health,
        float maxHealth);
    WHEATEAR_API void SetResource(Scene* scene,
        const std::string& barEntityName,
        const std::string& textEntityName,
        const std::string& label,
        float value,
        float maxValue);
    WHEATEAR_API void SetTooltip(Scene* scene,
        const std::string& panelEntityName,
        const std::string& textEntityName,
        bool visible,
        const glm::vec2& topLeft,
        const glm::vec2& size,
        const std::string& text);

} // namespace Wheatear::GameplayUIService
