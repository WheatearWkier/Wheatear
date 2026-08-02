#include "wtpch.h"
#include "GameplayUIService.h"

#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>

namespace Wheatear::GameplayUIService {

    void SetVisible(Scene* scene, const std::string& entityName, bool visible)
    {
        UIRuntimeTools::SetWidgetVisible(scene, entityName, visible);
    }

    void SetHealth(Scene* scene,
        const std::string& barEntityName,
        const std::string& textEntityName,
        const std::string& label,
        float health,
        float maxHealth)
    {
        SetResource(scene, barEntityName, textEntityName, label, health, maxHealth);
    }

    void SetResource(Scene* scene,
        const std::string& barEntityName,
        const std::string& textEntityName,
        const std::string& label,
        float value,
        float maxValue)
    {
        UIRuntimeTools::SetProgress(scene, barEntityName, value, maxValue <= 0.0f ? 1.0f : maxValue);
        UIRuntimeTools::SetText(scene,
            textEntityName,
            label + " " + std::to_string(static_cast<int>(std::max(0.0f, value))) + "/" +
                std::to_string(static_cast<int>(std::max(0.0f, maxValue))));
    }

    void SetTooltip(Scene* scene,
        const std::string& panelEntityName,
        const std::string& textEntityName,
        bool visible,
        const glm::vec2& topLeft,
        const glm::vec2& size,
        const std::string& text)
    {
        UIRuntimeTools::SetWidgetVisible(scene, panelEntityName, visible);
        UIRuntimeTools::SetWidgetVisible(scene, textEntityName, visible);
        if (!visible)
            return;

        UIRuntimeTools::SetWidgetTopLeft(scene, panelEntityName, topLeft, size);
        UIRuntimeTools::SetWidgetTopLeft(scene,
            textEntityName,
            topLeft + glm::vec2(0.012f, 0.010f),
            size - glm::vec2(0.024f, 0.020f));
        UIRuntimeTools::SetText(scene, textEntityName, text);
    }

} // namespace Wheatear::GameplayUIService
