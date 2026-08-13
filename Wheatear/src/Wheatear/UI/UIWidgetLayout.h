#pragma once

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace Wheatear::UIWidgetLayout {

    struct Rect
    {
        float Left = 0.0f;
        float Right = 0.0f;
        float Top = 0.0f;
        float Bottom = 0.0f;
    };

    struct Context
    {
        Scene* ScenePtr = nullptr;
        std::unordered_map<UUID, entt::entity> Entities;
        mutable std::unordered_map<uint32_t, Rect> RectCache;
        mutable std::unordered_map<uint32_t, bool> VisibilityCache;
        // Reusable recursion guard for the Resolve* entry points; the recursion
        // erases its keys on exit, so one shared set per context (per frame) is
        // safe and avoids allocating a fresh set per widget per frame.
        std::unordered_set<uint32_t> ScratchVisiting;

        explicit Context(Scene* scene)
            : ScenePtr(scene)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<IDComponent>())
            {
                const UUID id = registry.get<IDComponent>(entity).ID;
                if (static_cast<uint64_t>(id) != 0)
                    Entities[id] = entity;
            }
        }

        entt::entity FindByUUID(UUID uuid) const
        {
            if (static_cast<uint64_t>(uuid) == 0)
                return entt::null;

            auto it = Entities.find(uuid);
            return it != Entities.end() ? it->second : entt::null;
        }

        entt::entity ResolveReference(UUID uuid) const
        {
            return FindByUUID(uuid);
        }
    };

    inline uint32_t EntityKey(entt::entity entity)
    {
        return static_cast<uint32_t>(entity);
    }

    inline Rect WidgetToLocalRect(const UIWidgetComponent& widget)
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

        return { centerX - halfW, centerX + halfW, centerY - halfH, centerY + halfH };
    }

    inline Rect ResolveRect(Context& context,
        entt::entity entity,
        std::unordered_set<uint32_t>& visiting)
    {
        if (!context.ScenePtr || entity == entt::null)
            return {};

        const uint32_t key = EntityKey(entity);
        if (auto it = context.RectCache.find(key); it != context.RectCache.end())
            return it->second;
        if (visiting.find(key) != visiting.end())
            return {};

        auto& registry = context.ScenePtr->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
            return {};

        visiting.insert(key);

        const auto& widget = registry.get<UIWidgetComponent>(entity);
        Rect rect = WidgetToLocalRect(widget);
        const entt::entity parent = context.ResolveReference(widget.ParentEntity);
        if (parent != entt::null && parent != entity)
        {
            Rect localRect = rect;
            if (registry.valid(parent) && registry.all_of<UIScrollViewComponent>(parent))
            {
                auto& scrollView = registry.get<UIScrollViewComponent>(parent);
                scrollView.ClampOffset();
                localRect.Top -= scrollView.OffsetY;
                localRect.Bottom -= scrollView.OffsetY;
            }

            const Rect parentRect = ResolveRect(context, parent, visiting);
            const float parentWidth = parentRect.Right - parentRect.Left;
            const float parentHeight = parentRect.Bottom - parentRect.Top;
            rect = {
                parentRect.Left + localRect.Left * parentWidth,
                parentRect.Left + localRect.Right * parentWidth,
                parentRect.Top + localRect.Top * parentHeight,
                parentRect.Top + localRect.Bottom * parentHeight
            };
        }

        visiting.erase(key);
        context.RectCache[key] = rect;
        return rect;
    }

    inline Rect ResolveRect(Context& context, entt::entity entity)
    {
        context.ScratchVisiting.clear();
        return ResolveRect(context, entity, context.ScratchVisiting);
    }

    inline bool ResolveVisible(Context& context,
        entt::entity entity,
        std::unordered_set<uint32_t>& visiting)
    {
        if (!context.ScenePtr || entity == entt::null)
            return false;

        const uint32_t key = EntityKey(entity);
        if (auto it = context.VisibilityCache.find(key); it != context.VisibilityCache.end())
            return it->second;
        if (visiting.find(key) != visiting.end())
            return false;

        auto& registry = context.ScenePtr->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
            return false;

        visiting.insert(key);

        const auto& widget = registry.get<UIWidgetComponent>(entity);
        bool visible = widget.Visible;
        if (visible && registry.all_of<UIPageItemComponent>(entity))
        {
            const auto& pageItem = registry.get<UIPageItemComponent>(entity);
            const entt::entity pagerEntity = context.ResolveReference(pageItem.PagerEntity);
            if (pagerEntity != entt::null)
            {
                visible = pagerEntity != entt::null
                    && registry.valid(pagerEntity)
                    && registry.all_of<UIPagerComponent>(pagerEntity)
                    && registry.get<UIPagerComponent>(pagerEntity).GetClampedCurrentPage() == std::max(pageItem.Page, 1);
            }
        }
        if (visible)
        {
            const entt::entity parent = context.ResolveReference(widget.ParentEntity);
            if (parent != entt::null)
            {
                visible = parent != entity
                    && ResolveVisible(context, parent, visiting);
            }
        }

        visiting.erase(key);
        context.VisibilityCache[key] = visible;
        return visible;
    }

    inline bool ResolveVisible(Context& context, entt::entity entity)
    {
        context.ScratchVisiting.clear();
        return ResolveVisible(context, entity, context.ScratchVisiting);
    }

    inline bool ResolveEditorVisible(Context& context,
        entt::entity entity,
        std::unordered_set<uint32_t>& visiting)
    {
        if (!context.ScenePtr || entity == entt::null)
            return false;

        const uint32_t key = EntityKey(entity);
        if (visiting.find(key) != visiting.end())
            return false;

        auto& registry = context.ScenePtr->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
            return false;

        visiting.insert(key);

        const auto& widget = registry.get<UIWidgetComponent>(entity);
        bool visible = widget.EditorVisible;
        if (visible)
        {
            const entt::entity parent = context.ResolveReference(widget.ParentEntity);
            if (parent != entt::null)
                visible = parent != entity && ResolveEditorVisible(context, parent, visiting);
        }

        visiting.erase(key);
        return visible;
    }

    inline bool ResolveEditorVisible(Context& context, entt::entity entity)
    {
        context.ScratchVisiting.clear();
        return ResolveEditorVisible(context, entity, context.ScratchVisiting);
    }

    inline UIWidgetComponent ResolveWidget(Context& context, entt::entity entity)
    {
        auto& registry = context.ScenePtr->GetRegistry();
        UIWidgetComponent resolved = registry.get<UIWidgetComponent>(entity);
        const Rect rect = ResolveRect(context, entity);
        resolved.Visible = ResolveVisible(context, entity);
        resolved.ParentEntity = 0;
        resolved.Anchor = UIAnchor::TopLeft;
        resolved.Position = { rect.Left, rect.Top };
        resolved.Size = { std::max(0.0f, rect.Right - rect.Left),
                          std::max(0.0f, rect.Bottom - rect.Top) };
        return resolved;
    }

    inline UIWidgetComponent ResolveEditorWidget(Context& context, entt::entity entity)
    {
        auto& registry = context.ScenePtr->GetRegistry();
        UIWidgetComponent resolved = registry.get<UIWidgetComponent>(entity);
        const Rect rect = ResolveRect(context, entity);
        resolved.Visible = ResolveEditorVisible(context, entity);
        resolved.ParentEntity = 0;
        resolved.Anchor = UIAnchor::TopLeft;
        resolved.Position = { rect.Left, rect.Top };
        resolved.Size = { std::max(0.0f, rect.Right - rect.Left),
                          std::max(0.0f, rect.Bottom - rect.Top) };
        return resolved;
    }

    inline bool PointInRect(const Rect& rect, float x, float y)
    {
        return x >= rect.Left && x <= rect.Right && y >= rect.Top && y <= rect.Bottom;
    }

    inline bool IntersectRects(const Rect& a, const Rect& b, Rect& out)
    {
        out.Left = std::max(a.Left, b.Left);
        out.Right = std::min(a.Right, b.Right);
        out.Top = std::max(a.Top, b.Top);
        out.Bottom = std::min(a.Bottom, b.Bottom);
        return out.Left < out.Right && out.Top < out.Bottom;
    }

    inline std::optional<Rect> ResolveParentClipRect(Context& context, entt::entity entity)
    {
        if (!context.ScenePtr || entity == entt::null)
            return std::nullopt;

        auto& registry = context.ScenePtr->GetRegistry();
        if (!registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
            return std::nullopt;

        std::optional<Rect> clip;
        std::unordered_set<uint32_t> visiting;
        entt::entity current = entity;

        while (registry.valid(current) && registry.all_of<UIWidgetComponent>(current))
        {
            const auto& widget = registry.get<UIWidgetComponent>(current);
            const entt::entity parent = context.ResolveReference(widget.ParentEntity);
            if (parent == entt::null || parent == current)
                break;

            const uint32_t parentKey = EntityKey(parent);
            if (visiting.find(parentKey) != visiting.end())
                break;
            visiting.insert(parentKey);

            if (registry.valid(parent)
                && registry.all_of<UIWidgetComponent>(parent)
                && registry.all_of<UIPanelComponent>(parent)
                && registry.get<UIPanelComponent>(parent).ClipChildren)
            {
                const Rect parentRect = ResolveRect(context, parent);
                if (clip)
                {
                    Rect combined;
                    if (!IntersectRects(*clip, parentRect, combined))
                        return Rect{};
                    clip = combined;
                }
                else
                {
                    clip = parentRect;
                }
            }

            current = parent;
        }

        return clip;
    }

    inline Rect ExpandRect(Rect rect, float padding)
    {
        rect.Left -= padding;
        rect.Right += padding;
        rect.Top -= padding;
        rect.Bottom += padding;
        return rect;
    }

    inline bool RectHasArea(const Rect& rect)
    {
        return rect.Left < rect.Right && rect.Top < rect.Bottom;
    }

    inline bool RectIntersects(const Rect& a, const Rect& b)
    {
        Rect intersection;
        return IntersectRects(a, b, intersection);
    }

    inline bool RectVisibleInClipAndViewport(Context& context,
        entt::entity entity,
        const Rect& rect,
        float padding = 0.0f)
    {
        const Rect expandedRect = ExpandRect(rect, std::max(0.0f, padding));
        if (!RectHasArea(expandedRect))
            return false;

        constexpr Rect viewportRect = { 0.0f, 1.0f, 0.0f, 1.0f };
        if (!RectIntersects(expandedRect, viewportRect))
            return false;

        const auto clipRect = ResolveParentClipRect(context, entity);
        return !clipRect || RectIntersects(expandedRect, *clipRect);
    }

    inline bool EntityVisibleInClipAndViewport(Context& context,
        entt::entity entity,
        float padding = 0.0f)
    {
        if (!ResolveVisible(context, entity))
            return false;

        return RectVisibleInClipAndViewport(context, entity, ResolveRect(context, entity), padding);
    }

} // namespace Wheatear::UIWidgetLayout
