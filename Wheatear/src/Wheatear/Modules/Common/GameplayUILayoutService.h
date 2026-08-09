#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"

#include <glm/glm.hpp>

#include <string>

namespace Wheatear {

    class Scene;

} // namespace Wheatear

namespace Wheatear::GameplayUILayoutService {

    WHEATEAR_API bool HasEntity(Scene* scene, const std::string& name);

    WHEATEAR_API Entity FindAuthoredUIWidget(Scene* scene, const std::string& entityName);
    WHEATEAR_API Entity FindAuthoredPager(Scene* scene, const std::string& pagerName);
    WHEATEAR_API Entity FindAuthoredPanel(Scene* scene, const std::string& entityName);
    WHEATEAR_API Entity FindAuthoredScrollView(Scene* scene, const std::string& entityName);
    WHEATEAR_API Entity FindAuthoredText(Scene* scene, const std::string& entityName);
    WHEATEAR_API Entity FindAuthoredButton(Scene* scene, const std::string& entityName);
    WHEATEAR_API Entity FindAuthoredSlider(Scene* scene, const std::string& entityName);

    WHEATEAR_API Entity EnsureUIWidget(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        bool visible = true);
    WHEATEAR_API Entity EnsurePager(Scene* scene, const std::string& pagerName, int pageCount);
    WHEATEAR_API Entity EnsurePanel(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        glm::vec4 background,
        glm::vec4 border,
        float borderThickness,
        bool clipChildren = false);
    WHEATEAR_API Entity EnsureScrollView(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        float contentHeight);
    WHEATEAR_API Entity EnsureText(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        const std::string& value,
        float fontSize,
        glm::vec4 color);
    WHEATEAR_API Entity EnsureButton(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        const std::string& label,
        const std::string& command);
    WHEATEAR_API Entity EnsureSlider(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        float minValue,
        float maxValue,
        const std::string& command);

    WHEATEAR_API void SetPageItem(Scene* scene, const std::string& entityName, Entity pager, int page);
    WHEATEAR_API void SetButtonCommand(Scene* scene, const std::string& entityName, const std::string& command);
    WHEATEAR_API void SetSliderCommand(Scene* scene, const std::string& entityName, const std::string& command);
    WHEATEAR_API void SetButtonPalette(Scene* scene,
        const std::string& entityName,
        glm::vec4 normal,
        glm::vec4 hover,
        glm::vec4 pressed);
    WHEATEAR_API void SetPanelColors(Scene* scene,
        const std::string& entityName,
        glm::vec4 background,
        glm::vec4 border);
    WHEATEAR_API void SetPanelClipChildren(Scene* scene, const std::string& entityName, bool clipChildren);
    WHEATEAR_API void SetSliderValue(Scene* scene, const std::string& entityName, float value);
    WHEATEAR_API void SetSlider(Scene* scene,
        const std::string& entityName,
        float value,
        float minValue,
        float maxValue);

} // namespace Wheatear::GameplayUILayoutService
