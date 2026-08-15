#include "wtpch.h"
#include "UIInputSystem.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scripting/ScriptEngine.h"
#include "Wheatear/UI/UIWidgetLayout.h"
#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Wheatear {

    bool UIInputSystem::s_MouseWasPressed = false;

    namespace {

        static entt::entity s_DraggingPanel = entt::null;
        static entt::entity s_DraggingScrollView = entt::null;
        static entt::entity s_DraggingSkillTreeView = entt::null;
        static entt::entity s_DraggingSlider = entt::null;
        static bool s_DragStartResolved = false;

        enum class PointerTargetKind
        {
            None,
            Button,
            Checkbox,
            Slider
        };

    } // namespace

    struct UINormalizedRect
    {
        float Left = 0.0f;
        float Right = 0.0f;
        float Top = 0.0f;
        float Bottom = 0.0f;
    };

    using Wheatear::StringUtils::StartsWith;

    static bool IsNativeButtonCommand(const std::string& command)
    {
        return CommandBus::IsNativeCommand(command);
    }

    static bool ExecuteSliderNativeCommand(Scene* scene, const std::string& command, float value)
    {
        if (!CommandBus::IsNativeCommand(command))
            return false;

        std::string resolvedCommand = command;
        if (command == "progression:set_text_speed"
            || command == "progression:set_master_volume"
            || command == "progression:set_bgm_volume"
            || command == "progression:set_sfx_volume"
            || command == "progression:equipment_page_slider")
        {
            resolvedCommand += ":" + std::to_string(value);
        }

        return CommandBus::Execute(scene, resolvedCommand).Handled;
    }

    static UINormalizedRect WidgetToNormalizedRect(const UIWidgetComponent& widget)
    {
        const float halfW = widget.Size.x * 0.5f;
        const float halfH = widget.Size.y * 0.5f;
        float centerX = widget.Position.x;
        float centerY = widget.Position.y;

        switch (widget.Anchor)
        {
        case UIAnchor::TopLeft:
            centerX += halfW;
            centerY += halfH;
            break;
        case UIAnchor::TopCenter:
            centerY += halfH;
            break;
        case UIAnchor::TopRight:
            centerX -= halfW;
            centerY += halfH;
            break;
        case UIAnchor::MiddleLeft:
            centerX += halfW;
            break;
        case UIAnchor::MiddleCenter:
            break;
        case UIAnchor::MiddleRight:
            centerX -= halfW;
            break;
        case UIAnchor::BottomLeft:
            centerX += halfW;
            centerY -= halfH;
            break;
        case UIAnchor::BottomCenter:
            centerY -= halfH;
            break;
        case UIAnchor::BottomRight:
            centerX -= halfW;
            centerY -= halfH;
            break;
        }

        return {
            centerX - halfW,
            centerX + halfW,
            centerY - halfH,
            centerY + halfH
        };
    }

    static bool PointInRect(const UINormalizedRect& rect, float normMouseX, float normMouseY)
    {
        return normMouseX >= rect.Left && normMouseX <= rect.Right
            && normMouseY >= rect.Top && normMouseY <= rect.Bottom;
    }

    static bool PointInLayoutRect(const UIWidgetLayout::Rect& rect, float normMouseX, float normMouseY)
    {
        return normMouseX >= rect.Left && normMouseX <= rect.Right
            && normMouseY >= rect.Top && normMouseY <= rect.Bottom;
    }

    void UIInputSystem::Reset()
    {
        s_MouseWasPressed = false;
        s_DraggingPanel = entt::null;
        s_DraggingScrollView = entt::null;
        s_DraggingSkillTreeView = entt::null;
        s_DraggingSlider = entt::null;
        s_DragStartResolved = false;
    }

    static bool PointInPanelDragHandle(const UIWidgetLayout::Rect& rect,
        const UIPanelComponent& panel,
        float normMouseX,
        float normMouseY)
    {
        if (!PointInLayoutRect(rect, normMouseX, normMouseY))
            return false;

        const float height = rect.Bottom - rect.Top;
        if (height <= 0.0f)
            return false;

        if (panel.DragHandleHeight <= 0.0f)
            return true;

        const float handleHeight = height * std::clamp(panel.DragHandleHeight, 0.0f, 1.0f);
        return normMouseY <= rect.Top + handleHeight;
    }

    static UIWidgetLayout::Rect GetScrollThumbRect(const UIWidgetLayout::Rect& rect,
        const UIScrollViewComponent& scrollView)
    {
        const float width = std::max(rect.Right - rect.Left, 0.0f);
        const float height = std::max(rect.Bottom - rect.Top, 0.0f);
        const float barWidth = std::clamp(scrollView.ScrollbarWidth, 0.004f, std::max(width * 0.25f, 0.004f));
        const float thumbHeight = std::clamp(height / std::max(scrollView.ContentHeight, 1.0f), std::min(height, 0.05f), height);
        const float travel = std::max(height - thumbHeight, 0.0f);
        const float thumbTop = rect.Top + travel * scrollView.GetNormalized();

        return {
            rect.Right - barWidth,
            rect.Right,
            thumbTop,
            thumbTop + thumbHeight
        };
    }

    static std::string HitTestSkillTreeNode(const UISkillTreeViewComponent& tree,
        const UIWidgetLayout::Rect& rect,
        float normMouseX,
        float normMouseY)
    {
        const float width = rect.Right - rect.Left;
        const float height = rect.Bottom - rect.Top;
        if (width <= 0.0f || height <= 0.0f || !PointInLayoutRect(rect, normMouseX, normMouseY))
            return {};

        const glm::vec2 local = {
            (normMouseX - rect.Left) / width,
            (normMouseY - rect.Top) / height
        };

        std::string bestId;
        float bestDistance = std::numeric_limits<float>::max();
        const float halfWidth = std::max(tree.NodeSize.x * 0.5f, 0.001f);
        const float halfHeight = std::max(tree.NodeSize.y * 0.5f, 0.001f);
        for (const auto& node : tree.Nodes)
        {
            const glm::vec2 nodeLocal = node.Position + tree.Pan;
            if (nodeLocal.x + halfWidth < 0.0f || nodeLocal.x - halfWidth > 1.0f
                || nodeLocal.y + halfHeight < 0.0f || nodeLocal.y - halfHeight > 1.0f)
                continue;

            const float dx = (local.x - nodeLocal.x) / halfWidth;
            const float dy = (local.y - nodeLocal.y) / halfHeight;
            const float distance = dx * dx + dy * dy;
            if (distance <= 1.0f && distance < bestDistance)
            {
                bestDistance = distance;
                bestId = node.Id;
            }
        }
        return bestId;
    }

    static UIWidgetLayout::Rect GetParentRect(UIWidgetLayout::Context& layout,
        Scene* scene,
        const UIWidgetComponent& widget)
    {
        if (!scene)
            return { 0.0f, 1.0f, 0.0f, 1.0f };

        auto& registry = scene->GetRegistry();
        const entt::entity parent = layout.ResolveReference(widget.ParentEntity);
        if (parent == entt::null || !registry.valid(parent) || !registry.all_of<UIWidgetComponent>(parent))
            return { 0.0f, 1.0f, 0.0f, 1.0f };

        return UIWidgetLayout::ResolveRect(layout, parent);
    }

    static void SetWidgetLocalTopLeft(UIWidgetComponent& widget, float left, float top)
    {
        const UIWidgetLayout::Rect localRect = UIWidgetLayout::WidgetToLocalRect(widget);
        const float width = localRect.Right - localRect.Left;
        const float height = localRect.Bottom - localRect.Top;
        const float halfW = width * 0.5f;
        const float halfH = height * 0.5f;

        switch (widget.Anchor)
        {
        case UIAnchor::TopLeft:
            widget.Position = { left, top };
            break;
        case UIAnchor::TopCenter:
            widget.Position = { left + halfW, top };
            break;
        case UIAnchor::TopRight:
            widget.Position = { left + width, top };
            break;
        case UIAnchor::MiddleLeft:
            widget.Position = { left, top + halfH };
            break;
        case UIAnchor::MiddleCenter:
            widget.Position = { left + halfW, top + halfH };
            break;
        case UIAnchor::MiddleRight:
            widget.Position = { left + width, top + halfH };
            break;
        case UIAnchor::BottomLeft:
            widget.Position = { left, top + height };
            break;
        case UIAnchor::BottomCenter:
            widget.Position = { left + halfW, top + height };
            break;
        case UIAnchor::BottomRight:
            widget.Position = { left + width, top + height };
            break;
        }
    }

    static bool PointBlockedByParentClip(UIWidgetLayout::Context& layout,
        entt::entity entity,
        float normMouseX,
        float normMouseY)
    {
        const auto clip = UIWidgetLayout::ResolveParentClipRect(layout, entity);
        return clip && !UIWidgetLayout::PointInRect(*clip, normMouseX, normMouseY);
    }

    static bool WidgetCanReceivePointer(UIWidgetLayout::Context& layout, entt::entity entity)
    {
        return UIWidgetLayout::EntityVisibleInClipAndViewport(layout, entity, 0.004f);
    }

    static void FireScriptCallback(Scene* scene, entt::entity e, const std::string& function)
    {
        if (function.empty()) return;
        if (IsNativeButtonCommand(function)) return;
        if (!ScriptEngine::IsInitialized()) return;

        std::string methodName = function;
        if (StartsWith(methodName, "script:"))
            methodName = methodName.substr(7);
        if (methodName.empty()) return;

        ScriptEngine::InvokeMethod(Entity{ e, scene }, methodName);
    }

    bool UIInputSystem::HitTest(const UIWidgetComponent& widget,
        float normMouseX, float normMouseY)
    {
        return PointInRect(WidgetToNormalizedRect(widget), normMouseX, normMouseY);
    }

    void UIInputSystem::FireOnClick(Scene* scene, entt::entity e)
    {
        Entity entity{ e, scene };
        if (!entity.HasComponent<UIButtonComponent>()) return;

        auto& button = entity.GetComponent<UIButtonComponent>();
        if (button.OnClickFunction.empty())
            return;

        if (CommandBus::Execute(scene, button.OnClickFunction).Handled)
            return;

        FireScriptCallback(scene, e, button.OnClickFunction);
    }

    void UIInputSystem::OnUpdate(Scene* scene,
        float mouseX, float mouseY,
        uint32_t viewportWidth,
        uint32_t viewportHeight)
    {
        if (!scene || viewportWidth == 0 || viewportHeight == 0) return;

        const float normX = mouseX / static_cast<float>(viewportWidth);
        const float normY = mouseY / static_cast<float>(viewportHeight);
        const bool mouseInViewport = (normX >= 0.0f && normX <= 1.0f &&
            normY >= 0.0f && normY <= 1.0f);
        UIWidgetLayout::Context layout(scene);

        auto panelView = scene->GetRegistry().view<UIWidgetComponent, UIPanelComponent>();
        auto scrollView = scene->GetRegistry().view<UIWidgetComponent, UIScrollViewComponent>();
        auto skillTreeView = scene->GetRegistry().view<UIWidgetComponent, UISkillTreeViewComponent>();

        if (s_MouseWasPressed && !s_DragStartResolved && s_DraggingScrollView == entt::null)
        {
            entt::entity bestScrollView = entt::null;
            int bestSortOrder = std::numeric_limits<int>::min();
            for (auto e : scrollView)
            {
                auto& scroll = scrollView.get<UIScrollViewComponent>(e);
                scroll.ClampOffset();
                if (!scroll.ShowScrollbar || !scroll.DragScrollbar || scroll.ContentHeight <= 1.0f || !WidgetCanReceivePointer(layout, e))
                    continue;

                const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, e);
                const UIWidgetLayout::Rect thumbRect = GetScrollThumbRect(rect, scroll);
                const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
                const bool hit = mouseInViewport
                    && !PointBlockedByParentClip(layout, e, normX, normY)
                    && PointInLayoutRect(thumbRect, normX, normY);
                if (hit && (bestScrollView == entt::null || resolvedWidget.SortOrder >= bestSortOrder))
                {
                    bestScrollView = e;
                    bestSortOrder = resolvedWidget.SortOrder;
                }
            }

            if (bestScrollView != entt::null)
            {
                auto& scroll = scene->GetRegistry().get<UIScrollViewComponent>(bestScrollView);
                scroll.RuntimeThumbDragging = true;
                scroll.RuntimeDragStartMouse = { normX, normY };
                scroll.RuntimeDragStartOffsetY = scroll.OffsetY;
                s_DraggingScrollView = bestScrollView;
                s_DragStartResolved = true;
            }
        }

        const bool scrollViewIsDragging = s_DraggingScrollView != entt::null
            && scene->GetRegistry().valid(s_DraggingScrollView)
            && scene->GetRegistry().all_of<UIWidgetComponent>(s_DraggingScrollView)
            && scene->GetRegistry().all_of<UIScrollViewComponent>(s_DraggingScrollView);
        if (scrollViewIsDragging)
        {
            auto& registry = scene->GetRegistry();
            auto& scroll = registry.get<UIScrollViewComponent>(s_DraggingScrollView);
            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, s_DraggingScrollView);
            const float height = std::max(rect.Bottom - rect.Top, 0.0001f);
            const float thumbHeight = std::clamp(height / std::max(scroll.ContentHeight, 1.0f), std::min(height, 0.05f), height);
            const float travel = std::max(height - thumbHeight, 0.0001f);
            const float delta = (normY - scroll.RuntimeDragStartMouse.y) / travel;
            scroll.OffsetY = scroll.RuntimeDragStartOffsetY + delta * scroll.GetMaxOffset();
            scroll.ClampOffset();
            layout.RectCache.clear();
            layout.VisibilityCache.clear();
        }

        if (s_MouseWasPressed && !s_DragStartResolved
            && s_DraggingSkillTreeView == entt::null
            && !scrollViewIsDragging)
        {
            entt::entity bestSkillTree = entt::null;
            int bestSortOrder = std::numeric_limits<int>::min();
            for (auto e : skillTreeView)
            {
                if (!WidgetCanReceivePointer(layout, e))
                    continue;

                const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, e);
                const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
                const bool hit = mouseInViewport
                    && !PointBlockedByParentClip(layout, e, normX, normY)
                    && PointInLayoutRect(rect, normX, normY);
                if (hit && (bestSkillTree == entt::null || resolvedWidget.SortOrder >= bestSortOrder))
                {
                    bestSkillTree = e;
                    bestSortOrder = resolvedWidget.SortOrder;
                }
            }

            if (bestSkillTree != entt::null)
            {
                auto& tree = scene->GetRegistry().get<UISkillTreeViewComponent>(bestSkillTree);
                tree.RuntimeDragging = true;
                tree.RuntimeDragStartMouse = { normX, normY };
                tree.RuntimeDragStartPan = tree.Pan;
                tree.RuntimeDragDistance = 0.0f;
                s_DraggingSkillTreeView = bestSkillTree;
                s_DragStartResolved = true;
            }
        }

        const bool skillTreeIsDragging = s_DraggingSkillTreeView != entt::null
            && scene->GetRegistry().valid(s_DraggingSkillTreeView)
            && scene->GetRegistry().all_of<UIWidgetComponent>(s_DraggingSkillTreeView)
            && scene->GetRegistry().all_of<UISkillTreeViewComponent>(s_DraggingSkillTreeView);
        if (skillTreeIsDragging)
        {
            auto& registry = scene->GetRegistry();
            auto& tree = registry.get<UISkillTreeViewComponent>(s_DraggingSkillTreeView);
            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, s_DraggingSkillTreeView);
            const float width = std::max(rect.Right - rect.Left, 0.0001f);
            const float height = std::max(rect.Bottom - rect.Top, 0.0001f);
            const glm::vec2 delta = {
                (normX - tree.RuntimeDragStartMouse.x) / width,
                (normY - tree.RuntimeDragStartMouse.y) / height
            };
            tree.Pan = tree.RuntimeDragStartPan + delta;
            tree.ClampPan();
            GameProgress::GetState().SkillTreePanX = tree.Pan.x;
            GameProgress::GetState().SkillTreePanY = tree.Pan.y;
            tree.RuntimeDragDistance = std::max(tree.RuntimeDragDistance, std::sqrt(delta.x * delta.x + delta.y * delta.y));
            layout.RectCache.clear();
            layout.VisibilityCache.clear();
        }

        if (s_MouseWasPressed && !s_DragStartResolved && s_DraggingPanel == entt::null && !skillTreeIsDragging)
        {
            entt::entity bestPanel = entt::null;
            int bestSortOrder = std::numeric_limits<int>::min();
            for (auto e : panelView)
            {
                auto& panel = panelView.get<UIPanelComponent>(e);
                if (!panel.Draggable || !WidgetCanReceivePointer(layout, e))
                    continue;

                const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, e);
                if (mouseInViewport
                    && !scrollViewIsDragging
                    && !skillTreeIsDragging
                    && !PointBlockedByParentClip(layout, e, normX, normY)
                    && PointInPanelDragHandle(rect, panel, normX, normY))
                {
                    const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
                    if (bestPanel == entt::null || resolvedWidget.SortOrder >= bestSortOrder)
                    {
                        bestPanel = e;
                        bestSortOrder = resolvedWidget.SortOrder;
                    }
                }
            }

            if (bestPanel != entt::null)
            {
                auto& panel = scene->GetRegistry().get<UIPanelComponent>(bestPanel);
                const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, bestPanel);
                panel.RuntimeIsDragging = true;
                panel.RuntimeDragPointerOffset = { normX - rect.Left, normY - rect.Top };
                s_DraggingPanel = bestPanel;
            }

            s_DragStartResolved = true;
        }

        const bool panelIsDragging = s_DraggingPanel != entt::null
            && scene->GetRegistry().valid(s_DraggingPanel)
            && scene->GetRegistry().all_of<UIWidgetComponent>(s_DraggingPanel)
            && scene->GetRegistry().all_of<UIPanelComponent>(s_DraggingPanel);
        if (panelIsDragging)
        {
            auto& registry = scene->GetRegistry();
            auto& widget = registry.get<UIWidgetComponent>(s_DraggingPanel);
            auto& panel = registry.get<UIPanelComponent>(s_DraggingPanel);

            const UIWidgetLayout::Rect parentRect = GetParentRect(layout, scene, widget);
            const float parentWidth = std::max(parentRect.Right - parentRect.Left, 0.0001f);
            const float parentHeight = std::max(parentRect.Bottom - parentRect.Top, 0.0001f);
            const UIWidgetLayout::Rect localRect = UIWidgetLayout::WidgetToLocalRect(widget);
            const float localWidth = localRect.Right - localRect.Left;
            const float localHeight = localRect.Bottom - localRect.Top;

            float localLeft = (normX - panel.RuntimeDragPointerOffset.x - parentRect.Left) / parentWidth;
            float localTop = (normY - panel.RuntimeDragPointerOffset.y - parentRect.Top) / parentHeight;
            if (panel.ConstrainDragToParent)
            {
                localLeft = std::clamp(localLeft, 0.0f, std::max(0.0f, 1.0f - localWidth));
                localTop = std::clamp(localTop, 0.0f, std::max(0.0f, 1.0f - localHeight));
            }

            SetWidgetLocalTopLeft(widget, localLeft, localTop);
            panel.RuntimeIsDragging = true;
            layout.RectCache.clear();
        }

        for (auto e : panelView)
        {
            auto& panel = panelView.get<UIPanelComponent>(e);
            if (e != s_DraggingPanel)
                panel.RuntimeIsDragging = false;
        }

        for (auto e : scrollView)
        {
            auto& scroll = scrollView.get<UIScrollViewComponent>(e);
            scroll.ClampOffset();
            if (!scroll.ShowScrollbar || scroll.ContentHeight <= 1.0f || !WidgetCanReceivePointer(layout, e))
            {
                scroll.RuntimeThumbHovered = false;
                if (e != s_DraggingScrollView)
                    scroll.RuntimeThumbDragging = false;
                continue;
            }

            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, e);
            const UIWidgetLayout::Rect thumbRect = GetScrollThumbRect(rect, scroll);
            scroll.RuntimeThumbHovered = mouseInViewport
                && !PointBlockedByParentClip(layout, e, normX, normY)
                && PointInLayoutRect(thumbRect, normX, normY);
            if (e != s_DraggingScrollView)
                scroll.RuntimeThumbDragging = false;
        }

        for (auto e : skillTreeView)
        {
            auto& tree = skillTreeView.get<UISkillTreeViewComponent>(e);
            if (!WidgetCanReceivePointer(layout, e))
            {
                tree.RuntimeHoveredNodeId.clear();
                if (e != s_DraggingSkillTreeView)
                    tree.RuntimeDragging = false;
                continue;
            }

            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, e);
            const bool hit = mouseInViewport
                && !panelIsDragging
                && !scrollViewIsDragging
                && !PointBlockedByParentClip(layout, e, normX, normY)
                && PointInLayoutRect(rect, normX, normY);
            tree.RuntimeHoveredNodeId = hit ? HitTestSkillTreeNode(tree, rect, normX, normY) : std::string{};
            if (e != s_DraggingSkillTreeView)
                tree.RuntimeDragging = false;
        }

        auto buttonView = scene->GetRegistry().view<UIWidgetComponent, UIButtonComponent>();
        entt::entity topHoveredButton = entt::null;
        int topHoveredButtonSortOrder = std::numeric_limits<int>::min();
        for (auto e : buttonView)
        {
            auto& button = buttonView.get<UIButtonComponent>(e);
            if (!WidgetCanReceivePointer(layout, e))
            {
                button.IsHovered = false;
                button.IsPressed = false;
                continue;
            }

            const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
            const bool hit = mouseInViewport
                && !panelIsDragging
                && !scrollViewIsDragging
                && !skillTreeIsDragging
                && !PointBlockedByParentClip(layout, e, normX, normY)
                && HitTest(resolvedWidget, normX, normY);
            button.IsHovered = false;
            button.IsPressed = false;
            if (hit)
            {
                const int sortOrder = resolvedWidget.SortOrder;
                if (topHoveredButton == entt::null || sortOrder >= topHoveredButtonSortOrder)
                {
                    topHoveredButton = e;
                    topHoveredButtonSortOrder = sortOrder;
                }
            }
        }

        auto checkboxView = scene->GetRegistry().view<UIWidgetComponent, UICheckboxComponent>();
        entt::entity topHoveredCheckbox = entt::null;
        int topHoveredCheckboxSortOrder = std::numeric_limits<int>::min();
        for (auto e : checkboxView)
        {
            auto& checkbox = checkboxView.get<UICheckboxComponent>(e);
            if (!WidgetCanReceivePointer(layout, e))
            {
                checkbox.IsHovered = false;
                checkbox.IsPressed = false;
                continue;
            }

            const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
            const bool hit = mouseInViewport
                && !panelIsDragging
                && !scrollViewIsDragging
                && !skillTreeIsDragging
                && !PointBlockedByParentClip(layout, e, normX, normY)
                && HitTest(resolvedWidget, normX, normY);
            checkbox.IsHovered = false;
            checkbox.IsPressed = false;
            if (hit)
            {
                const int sortOrder = resolvedWidget.SortOrder;
                if (topHoveredCheckbox == entt::null || sortOrder >= topHoveredCheckboxSortOrder)
                {
                    topHoveredCheckbox = e;
                    topHoveredCheckboxSortOrder = sortOrder;
                }
            }
        }

        auto sliderView = scene->GetRegistry().view<UIWidgetComponent, UISliderComponent>();
        entt::entity topHoveredSlider = entt::null;
        int topHoveredSliderSortOrder = std::numeric_limits<int>::min();
        for (auto e : sliderView)
        {
            auto& slider = sliderView.get<UISliderComponent>(e);
            if (!WidgetCanReceivePointer(layout, e))
            {
                slider.IsHovered = false;
                if (e != s_DraggingSlider)
                    slider.IsDragging = false;
                continue;
            }

            const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
            const UINormalizedRect rect = WidgetToNormalizedRect(resolvedWidget);
            const bool hit = mouseInViewport
                && !panelIsDragging
                && !scrollViewIsDragging
                && !skillTreeIsDragging
                && !PointBlockedByParentClip(layout, e, normX, normY)
                && PointInRect(rect, normX, normY);
            slider.IsHovered = false;
            if (hit)
            {
                const int sortOrder = resolvedWidget.SortOrder;
                if (topHoveredSlider == entt::null || sortOrder >= topHoveredSliderSortOrder)
                {
                    topHoveredSlider = e;
                    topHoveredSliderSortOrder = sortOrder;
                }
            }
        }

        PointerTargetKind pointerTargetKind = PointerTargetKind::None;
        entt::entity pointerTarget = entt::null;
        int pointerTargetSortOrder = std::numeric_limits<int>::min();
        auto selectPointerTarget = [&](PointerTargetKind kind, entt::entity entity, int sortOrder)
        {
            if (entity == entt::null)
                return;
            if (pointerTarget == entt::null || sortOrder >= pointerTargetSortOrder)
            {
                pointerTargetKind = kind;
                pointerTarget = entity;
                pointerTargetSortOrder = sortOrder;
            }
        };
        selectPointerTarget(PointerTargetKind::Button, topHoveredButton, topHoveredButtonSortOrder);
        selectPointerTarget(PointerTargetKind::Checkbox, topHoveredCheckbox, topHoveredCheckboxSortOrder);
        selectPointerTarget(PointerTargetKind::Slider, topHoveredSlider, topHoveredSliderSortOrder);

        if (topHoveredButton != entt::null)
        {
            auto& button = buttonView.get<UIButtonComponent>(topHoveredButton);
            button.IsHovered = pointerTargetKind == PointerTargetKind::Button;
            button.IsPressed = button.IsHovered && s_MouseWasPressed;
        }
        if (topHoveredCheckbox != entt::null)
        {
            auto& checkbox = checkboxView.get<UICheckboxComponent>(topHoveredCheckbox);
            checkbox.IsHovered = pointerTargetKind == PointerTargetKind::Checkbox;
            checkbox.IsPressed = checkbox.IsHovered && s_MouseWasPressed;
        }
        if (topHoveredSlider != entt::null)
            sliderView.get<UISliderComponent>(topHoveredSlider).IsHovered =
                pointerTargetKind == PointerTargetKind::Slider;

        if (s_MouseWasPressed
            && s_DraggingSlider == entt::null
            && pointerTargetKind == PointerTargetKind::Slider
            && topHoveredSlider != entt::null)
        {
            s_DraggingSlider = topHoveredSlider;
        }

        const bool sliderIsDragging = s_DraggingSlider != entt::null
            && scene->GetRegistry().valid(s_DraggingSlider)
            && scene->GetRegistry().all_of<UIWidgetComponent>(s_DraggingSlider)
            && scene->GetRegistry().all_of<UISliderComponent>(s_DraggingSlider);

        for (auto e : sliderView)
        {
            auto& slider = sliderView.get<UISliderComponent>(e);
            if (!s_MouseWasPressed || !sliderIsDragging || e != s_DraggingSlider)
            {
                slider.IsDragging = false;
                continue;
            }

            const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
            const UINormalizedRect rect = WidgetToNormalizedRect(resolvedWidget);
            const float width = rect.Right - rect.Left;
            if (width > 0.0f)
                slider.SetNormalized((normX - rect.Left) / width);
            slider.IsDragging = true;
        }
    }

    void UIInputSystem::OnMousePressed(Scene* scene)
    {
        s_MouseWasPressed = true;
        s_DragStartResolved = false;
    }

    void UIInputSystem::OnMouseReleased(Scene* scene)
    {
        if (!s_MouseWasPressed) return;
        s_MouseWasPressed = false;
        s_DragStartResolved = false;
        if (!scene)
        {
            s_DraggingPanel = entt::null;
            s_DraggingScrollView = entt::null;
            s_DraggingSkillTreeView = entt::null;
            s_DraggingSlider = entt::null;
            return;
        }
        UIWidgetLayout::Context layout(scene);

        if (s_DraggingPanel != entt::null && scene->GetRegistry().valid(s_DraggingPanel)
            && scene->GetRegistry().all_of<UIPanelComponent>(s_DraggingPanel))
        {
            scene->GetRegistry().get<UIPanelComponent>(s_DraggingPanel).RuntimeIsDragging = false;
        }
        s_DraggingPanel = entt::null;
        if (s_DraggingScrollView != entt::null && scene->GetRegistry().valid(s_DraggingScrollView)
            && scene->GetRegistry().all_of<UIScrollViewComponent>(s_DraggingScrollView))
        {
            scene->GetRegistry().get<UIScrollViewComponent>(s_DraggingScrollView).RuntimeThumbDragging = false;
        }
        s_DraggingScrollView = entt::null;

        bool releaseConsumedBySkillTree = false;
        if (s_DraggingSkillTreeView != entt::null && scene->GetRegistry().valid(s_DraggingSkillTreeView)
            && scene->GetRegistry().all_of<UISkillTreeViewComponent>(s_DraggingSkillTreeView))
        {
            auto& tree = scene->GetRegistry().get<UISkillTreeViewComponent>(s_DraggingSkillTreeView);
            releaseConsumedBySkillTree = true;
            if (tree.RuntimeDragDistance <= 0.006f && !tree.RuntimeHoveredNodeId.empty())
            {
                tree.SelectedNodeId = tree.RuntimeHoveredNodeId;
                for (auto& node : tree.Nodes)
                    node.Selected = node.Id == tree.SelectedNodeId;

                if (!tree.CommandPrefix.empty())
                    CommandBus::Execute(scene, tree.CommandPrefix + tree.SelectedNodeId);
            }

            tree.RuntimeDragging = false;
            tree.RuntimeDragDistance = 0.0f;
        }
        s_DraggingSkillTreeView = entt::null;

        auto buttonView = scene->GetRegistry().view<UIWidgetComponent, UIButtonComponent>();
        entt::entity clickedButton = entt::null;
        int clickedButtonSortOrder = std::numeric_limits<int>::min();
        for (auto e : buttonView)
        {
            auto& button = buttonView.get<UIButtonComponent>(e);
            if (!releaseConsumedBySkillTree && WidgetCanReceivePointer(layout, e) && button.IsHovered)
            {
                const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
                if (clickedButton == entt::null || resolvedWidget.SortOrder >= clickedButtonSortOrder)
                {
                    clickedButton = e;
                    clickedButtonSortOrder = resolvedWidget.SortOrder;
                }
            }
            button.IsPressed = false;
        }
        if (clickedButton != entt::null)
            FireOnClick(scene, clickedButton);

        auto checkboxView = scene->GetRegistry().view<UIWidgetComponent, UICheckboxComponent>();
        entt::entity clickedCheckbox = entt::null;
        int clickedCheckboxSortOrder = std::numeric_limits<int>::min();
        for (auto e : checkboxView)
        {
            auto& checkbox = checkboxView.get<UICheckboxComponent>(e);
            if (!releaseConsumedBySkillTree && WidgetCanReceivePointer(layout, e) && checkbox.IsHovered)
            {
                const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
                if (clickedCheckbox == entt::null || resolvedWidget.SortOrder >= clickedCheckboxSortOrder)
                {
                    clickedCheckbox = e;
                    clickedCheckboxSortOrder = resolvedWidget.SortOrder;
                }
            }
            checkbox.IsPressed = false;
        }
        if (clickedCheckbox != entt::null)
        {
            auto& checkbox = scene->GetRegistry().get<UICheckboxComponent>(clickedCheckbox);
            checkbox.Checked = !checkbox.Checked;
            FireScriptCallback(scene, clickedCheckbox, checkbox.OnValueChangedFunction);
        }

        auto sliderView = scene->GetRegistry().view<UIWidgetComponent, UISliderComponent>();
        for (auto e : sliderView)
        {
            auto& slider = sliderView.get<UISliderComponent>(e);
            if (e == s_DraggingSlider && slider.IsDragging)
            {
                if (!ExecuteSliderNativeCommand(scene, slider.OnValueChangedFunction, slider.Value))
                    FireScriptCallback(scene, e, slider.OnValueChangedFunction);
            }
            slider.IsDragging = false;
        }
        s_DraggingSlider = entt::null;
    }

    bool UIInputSystem::OnMouseScrolled(Scene* scene,
        float yOffset,
        float mouseX,
        float mouseY,
        uint32_t viewportWidth,
        uint32_t viewportHeight)
    {
        if (!scene || viewportWidth == 0 || viewportHeight == 0)
            return false;

        const float normX = mouseX / static_cast<float>(viewportWidth);
        const float normY = mouseY / static_cast<float>(viewportHeight);
        const bool mouseInViewport = normX >= 0.0f && normX <= 1.0f && normY >= 0.0f && normY <= 1.0f;
        if (!mouseInViewport)
            return false;

        UIWidgetLayout::Context layout(scene);
        auto view = scene->GetRegistry().view<UIWidgetComponent, UIScrollViewComponent>();

        entt::entity bestScrollView = entt::null;
        int bestSortOrder = std::numeric_limits<int>::min();
        for (auto e : view)
        {
            auto& scroll = view.get<UIScrollViewComponent>(e);
            scroll.ClampOffset();
            if (!scroll.EnableWheel || scroll.ContentHeight <= 1.0f || !WidgetCanReceivePointer(layout, e))
                continue;

            const UIWidgetLayout::Rect rect = UIWidgetLayout::ResolveRect(layout, e);
            const UIWidgetComponent resolvedWidget = UIWidgetLayout::ResolveWidget(layout, e);
            const bool hit = !PointBlockedByParentClip(layout, e, normX, normY)
                && PointInLayoutRect(rect, normX, normY);
            if (hit && (bestScrollView == entt::null || resolvedWidget.SortOrder >= bestSortOrder))
            {
                bestScrollView = e;
                bestSortOrder = resolvedWidget.SortOrder;
            }
        }

        if (bestScrollView == entt::null)
            return false;

        auto& scroll = scene->GetRegistry().get<UIScrollViewComponent>(bestScrollView);
        scroll.OffsetY -= yOffset * scroll.WheelStep;
        scroll.ClampOffset();
        return true;
    }

} // namespace Wheatear
