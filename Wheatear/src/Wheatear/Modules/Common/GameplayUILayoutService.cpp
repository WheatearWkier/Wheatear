#include "wtpch.h"
#include "GameplayUILayoutService.h"

#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRenderer.h"

#include <algorithm>

namespace Wheatear::GameplayUILayoutService {

    namespace {

        using SceneQueries::FindEntityByName;

        void ConfigureDefaultWidget(UIWidgetComponent& widget,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            bool visible)
        {
            widget.Visible = visible;
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = position;
            widget.Size = size;
            widget.Rotation = 0.0f;
            widget.SortOrder = sortOrder;
        }

    } // namespace

    bool HasEntity(Scene* scene, const std::string& name)
    {
        return static_cast<bool>(FindEntityByName(scene, name));
    }

    Entity EnsureUIWidget(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        bool visible)
    {
        if (!scene || entityName.empty())
            return {};

        Entity entity = FindEntityByName(scene, entityName);
        if (!entity)
            entity = scene->CreateEntity(entityName);

        auto& widget = entity.HasComponent<UIWidgetComponent>()
            ? entity.GetComponent<UIWidgetComponent>()
            : entity.AddComponent<UIWidgetComponent>();
        ConfigureDefaultWidget(widget, position, size, sortOrder, visible);

        Entity parent = FindEntityByName(scene, parentName);
        widget.ParentEntity = parent ? parent.GetUUID() : UUID(0);
        return entity;
    }

    Entity EnsurePager(Scene* scene, const std::string& pagerName, int pageCount)
    {
        Entity pager = FindEntityByName(scene, pagerName);
        if (!pager && scene)
            pager = scene->CreateEntity(pagerName);
        if (!pager)
            return {};

        auto& widget = pager.HasComponent<UIWidgetComponent>()
            ? pager.GetComponent<UIWidgetComponent>()
            : pager.AddComponent<UIWidgetComponent>();
        ConfigureDefaultWidget(widget, { 0.0f, 0.0f }, { 0.001f, 0.001f }, 0, false);

        auto& pagerComponent = pager.HasComponent<UIPagerComponent>()
            ? pager.GetComponent<UIPagerComponent>()
            : pager.AddComponent<UIPagerComponent>();
        pagerComponent.PageCount = std::max(pageCount, 1);
        pagerComponent.CurrentPage = std::clamp(pagerComponent.CurrentPage, 1, pagerComponent.PageCount);
        return pager;
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
        Entity entity = EnsureUIWidget(scene, entityName, parentName, position, size, sortOrder);
        if (!entity)
            return {};

        auto& panel = entity.HasComponent<UIPanelComponent>()
            ? entity.GetComponent<UIPanelComponent>()
            : entity.AddComponent<UIPanelComponent>();
        panel.BackgroundColor = background;
        panel.BorderColor = border;
        panel.BorderThickness = borderThickness;
        panel.ClipChildren = clipChildren;
        return entity;
    }

    Entity EnsureScrollView(Scene* scene,
        const std::string& entityName,
        const std::string& parentName,
        glm::vec2 position,
        glm::vec2 size,
        int sortOrder,
        float contentHeight)
    {
        Entity entity = EnsurePanel(scene, entityName, parentName, position, size, sortOrder,
            { 0.012f, 0.015f, 0.018f, 0.36f },
            { 0.44f, 0.58f, 0.50f, 0.42f },
            1.0f,
            true);
        if (!entity)
            return {};

        auto& scrollView = entity.HasComponent<UIScrollViewComponent>()
            ? entity.GetComponent<UIScrollViewComponent>()
            : entity.AddComponent<UIScrollViewComponent>();
        scrollView.ContentHeight = std::max(contentHeight, 1.0f);
        scrollView.WheelStep = 0.08f;
        scrollView.ScrollbarWidth = 0.016f;
        scrollView.EnableWheel = true;
        scrollView.ShowScrollbar = true;
        scrollView.DragScrollbar = true;
        scrollView.ClampToContent = true;
        scrollView.ClampOffset();
        return entity;
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
        Entity entity = EnsureUIWidget(scene, entityName, parentName, position, size, sortOrder);
        if (!entity)
            return {};

        auto& text = entity.HasComponent<UITextComponent>()
            ? entity.GetComponent<UITextComponent>()
            : entity.AddComponent<UITextComponent>();
        text.Text = value;
        text.FontSize = fontSize;
        text.Color = color;
        text.FontPath = AssetAliasRegistry::Path("font.ui_default", "assets/fonts/wqy-microhei.ttc");
        text.ShadowColor = { 0.01f, 0.015f, 0.018f, 0.80f };
        text.ShadowOffset = { 1.6f, 1.6f };
        text.OutlineColor = { 0.0f, 0.0f, 0.0f, 0.86f };
        text.OutlineThickness = 1.15f;
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
        Entity entity = EnsureText(scene, entityName, parentName, position, size, sortOrder,
            label,
            18.0f,
            { 0.95f, 0.93f, 0.82f, 1.0f });
        if (!entity)
            return {};

        auto& button = entity.HasComponent<UIButtonComponent>()
            ? entity.GetComponent<UIButtonComponent>()
            : entity.AddComponent<UIButtonComponent>();
        button.OnClickFunction = command;
        button.NormalColor = { 0.10f, 0.11f, 0.13f, 0.86f };
        button.HoverColor = { 0.35f, 0.55f, 0.50f, 0.96f };
        button.PressedColor = { 0.06f, 0.08f, 0.09f, 0.98f };
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
        Entity entity = EnsureUIWidget(scene, entityName, parentName, position, size, sortOrder);
        if (!entity)
            return {};

        auto& slider = entity.HasComponent<UISliderComponent>()
            ? entity.GetComponent<UISliderComponent>()
            : entity.AddComponent<UISliderComponent>();
        slider.MinValue = minValue;
        slider.MaxValue = maxValue <= minValue ? minValue + 1.0f : maxValue;
        slider.TrackColor = { 0.08f, 0.10f, 0.12f, 0.92f };
        slider.FillColor = { 0.30f, 0.78f, 0.72f, 0.96f };
        slider.HandleColor = { 0.92f, 0.98f, 0.92f, 1.0f };
        slider.HoverColor = { 1.0f, 0.92f, 0.50f, 1.0f };
        slider.OnValueChangedFunction = command;
        return entity;
    }

    void SetPageItem(Scene* scene, const std::string& entityName, Entity pager, int page)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (!entity || !pager)
            return;

        auto& pageItem = entity.HasComponent<UIPageItemComponent>()
            ? entity.GetComponent<UIPageItemComponent>()
            : entity.AddComponent<UIPageItemComponent>();
        pageItem.PagerEntity = pager.GetUUID();
        pageItem.Page = std::max(page, 1);
    }

    void SetButtonCommand(Scene* scene, const std::string& entityName, const std::string& command)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (entity && entity.HasComponent<UIButtonComponent>())
            entity.GetComponent<UIButtonComponent>().OnClickFunction = command;
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

    void SetSlider(Scene* scene,
        const std::string& entityName,
        float value,
        float minValue,
        float maxValue)
    {
        Entity entity = FindEntityByName(scene, entityName);
        if (!entity || !entity.HasComponent<UISliderComponent>())
            return;

        auto& slider = entity.GetComponent<UISliderComponent>();
        slider.MinValue = minValue;
        slider.MaxValue = maxValue <= minValue ? minValue + 1.0f : maxValue;
        if (slider.IsDragging)
            return;
        slider.Value = std::clamp(value, slider.MinValue, slider.MaxValue);
    }

} // namespace Wheatear::GameplayUILayoutService
