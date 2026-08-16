#include "wtpch.h"
#include "GameplayUILayoutService.h"

#include "Wheatear/Core/Log.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRenderer.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace Wheatear::GameplayUILayoutService {

    namespace {

        using SceneQueries::FindEntityByName;

        void WarnMissingAuthoredUI(Scene* scene,
            const std::string& entityName,
            const char* missing)
        {
            if (!scene || entityName.empty() || !missing)
                return;

            static std::unordered_set<std::string> warned;
            std::ostringstream key;
            key << scene << ':' << entityName << ':' << missing;
            if (warned.insert(key.str()).second)
            {
                WT_CORE_WARN("GameplayUILayoutService: '{}' is missing {}. Add it to the scene asset; runtime UI creation is disabled.",
                    entityName,
                    missing);
            }
        }

    } // namespace

    bool HasEntity(Scene* scene, const std::string& name)
    {
        return static_cast<bool>(FindEntityByName(scene, name));
    }

    Entity EnsureUIWidget(Scene* scene,
        const std::string& entityName,
        bool visible)
    {

        Entity entity = FindAuthoredUIWidget(scene, entityName);
        if (entity && entity.HasComponent<UIWidgetComponent>())
            entity.GetComponent<UIWidgetComponent>().Visible = visible;
        return entity;
    }

    Entity EnsurePager(Scene* scene, const std::string& pagerName, int pageCount)
    {
        (void)pageCount;
        return FindAuthoredPager(scene, pagerName);
    }

    Entity EnsurePanel(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        glm::vec4 background,
        glm::vec4 border,
        float borderThickness,
        bool clipChildren)
    {
        (void)background;
        (void)border;
        (void)borderThickness;
        (void)clipChildren;
        return FindAuthoredPanel(scene, entityName);
    }

    Entity EnsureScrollView(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        float contentHeight)
    {
        (void)contentHeight;
        return FindAuthoredScrollView(scene, entityName);
    }

    Entity EnsureText(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        const std::string& value,
        float fontSize,
        glm::vec4 color)
    {
        (void)fontSize;
        (void)color;

        Entity entity = FindAuthoredText(scene, entityName);
        if (!entity)
            return {};

        auto& text = entity.GetComponent<UITextComponent>();
        text.Text = value;
        UIRenderer::PreloadUIText(text);
        return entity;
    }

    Entity EnsureButton(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        const std::string& label,
        const std::string& command)
    {
        Entity entity = EnsureText(scene,
            entityName,
            parentName,
            position,
            size,
            sortOrder,
            label,
            18.0f,
            { 0.95f, 0.93f, 0.82f, 1.0f });
        if (!entity)
            return {};

        if (entity.HasComponent<UIButtonComponent>())
            entity.GetComponent<UIButtonComponent>().OnClickFunction = command;
        return entity;
    }

    Entity EnsureSlider(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        float minValue,
        float maxValue,
        const std::string& command)
    {
        (void)minValue;
        (void)maxValue;

        Entity entity = FindAuthoredSlider(scene, entityName);
        if (entity && entity.HasComponent<UISliderComponent>())
            entity.GetComponent<UISliderComponent>().OnValueChangedFunction = command;
        return entity;
    }

    Entity FindAuthoredUIWidget(Scene* scene, const std::string& entityName)
    {
        if (!scene || entityName.empty())
            return {};

        Entity entity = FindEntityByName(scene, entityName);
        if (!entity)
        {
            WarnMissingAuthoredUI(scene, entityName, "entity");
            return {};
        }
        if (!entity.HasComponent<UIWidgetComponent>())
        {
            WarnMissingAuthoredUI(scene, entityName, "UIWidgetComponent");
            return {};
        }

        return entity;
    }

    Entity FindAuthoredPager(Scene* scene, const std::string& pagerName)
    {
        Entity pager = FindEntityByName(scene, pagerName);
        if (!pager)
        {
            WarnMissingAuthoredUI(scene, pagerName, "entity");
            return {};
        }
        if (!pager.HasComponent<UIWidgetComponent>())
        {
            WarnMissingAuthoredUI(scene, pagerName, "UIWidgetComponent");
            return {};
        }
        if (!pager.HasComponent<UIPagerComponent>())
        {
            WarnMissingAuthoredUI(scene, pagerName, "UIPagerComponent");
            return {};
        }
        return pager;
    }

    Entity FindAuthoredPanel(Scene* scene, const std::string& entityName)
    {
        Entity entity = FindAuthoredUIWidget(scene, entityName);
        if (!entity)
            return {};

        if (!entity.HasComponent<UIPanelComponent>())
        {
            WarnMissingAuthoredUI(scene, entityName, "UIPanelComponent");
            return {};
        }
        return entity;
    }

    Entity FindAuthoredScrollView(Scene* scene, const std::string& entityName)
    {
        Entity entity = FindAuthoredPanel(scene, entityName);
        if (!entity)
            return {};

        if (!entity.HasComponent<UIScrollViewComponent>())
        {
            WarnMissingAuthoredUI(scene, entityName, "UIScrollViewComponent");
            return {};
        }
        entity.GetComponent<UIScrollViewComponent>().ClampOffset();
        return entity;
    }

    Entity FindAuthoredText(Scene* scene, const std::string& entityName)
    {
        Entity entity = FindAuthoredUIWidget(scene, entityName);
        if (!entity)
            return {};

        if (!entity.HasComponent<UITextComponent>())
        {
            WarnMissingAuthoredUI(scene, entityName, "UITextComponent");
            return {};
        }

        return entity;
    }

    Entity FindAuthoredButton(Scene* scene, const std::string& entityName)
    {
        Entity entity = FindAuthoredText(scene, entityName);
        if (!entity)
            return {};

        if (!entity.HasComponent<UIButtonComponent>())
        {
            WarnMissingAuthoredUI(scene, entityName, "UIButtonComponent");
            return {};
        }

        return entity;
    }

    Entity FindAuthoredSlider(Scene* scene, const std::string& entityName)
    {
        Entity entity = FindAuthoredUIWidget(scene, entityName);
        if (!entity)
            return {};

        if (!entity.HasComponent<UISliderComponent>())
        {
            WarnMissingAuthoredUI(scene, entityName, "UISliderComponent");
            return {};
        }

        return entity;
    }

    void SetPageItem(Scene* scene, const std::string& entityName, Entity pager, int page)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (!entity || !pager)
            return;

        if (!entity.HasComponent<UIPageItemComponent>())
        {
            WarnMissingAuthoredUI(scene, entityName, "UIPageItemComponent");
            return;
        }

        auto& pageItem = entity.GetComponent<UIPageItemComponent>();
        pageItem.PagerEntity = pager.GetUUID();
        pageItem.Page = std::max(page, 1);
    }

    void SetButtonCommand(Scene* scene, const std::string& entityName, const std::string& command)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (entity && entity.HasComponent<UIButtonComponent>())
            entity.GetComponent<UIButtonComponent>().OnClickFunction = command;
    }

    void SetSliderCommand(Scene* scene, const std::string& entityName, const std::string& command)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (entity && entity.HasComponent<UISliderComponent>())
            entity.GetComponent<UISliderComponent>().OnValueChangedFunction = command;
    }

    void SetButtonPalette(Scene* scene,
        const std::string& entityName,
        glm::vec4 normal,
        glm::vec4 hover,
        glm::vec4 pressed)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIButtonComponent>())
            return;

        auto& button = entity.GetComponent<UIButtonComponent>();
        button.NormalColor = normal;
        button.HoverColor = hover;
        button.PressedColor = pressed;
    }

    void SetPanelColors(Scene* scene,
        const std::string& entityName,
        glm::vec4 background,
        glm::vec4 border)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UIPanelComponent>())
            return;

        auto& panel = entity.GetComponent<UIPanelComponent>();
        panel.BackgroundColor = background;
        panel.BorderColor = border;
    }

    void SetPanelClipChildren(Scene* scene, const std::string& entityName, bool clipChildren)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (entity && entity.HasComponent<UIPanelComponent>())
            entity.GetComponent<UIPanelComponent>().ClipChildren = clipChildren;
    }

    void SetSliderValue(Scene* scene, const std::string& entityName, float value)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UISliderComponent>())
            return;

        auto& slider = entity.GetComponent<UISliderComponent>();
        if (slider.IsDragging)
            return;
        slider.Value = std::clamp(value, slider.MinValue, slider.MaxValue);
    }

    void SetSlider(Scene* scene,
        const std::string& entityName,
        float value,
        float minValue,
        float maxValue)
    {
        (void)minValue;
        (void)maxValue;
        SetSliderValue(scene, entityName, value);
    }

} // namespace Wheatear::GameplayUILayoutService
