#include "wtpch.h"
#include "UISystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Core/Input.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/UI/UIRenderer.h"
#include "Wheatear/UI/UIInputSystem.h"
#include "Wheatear/UI/UIWidgetLayout.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    namespace {

        constexpr float PI = 3.14159265359f;

        static float Saturate(float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        static float EaseOutCubic(float value)
        {
            const float t = Saturate(value);
            const float inv = 1.0f - t;
            return 1.0f - inv * inv * inv;
        }

        static float EaseOutBack(float value)
        {
            const float t = Saturate(value);
            constexpr float c1 = 1.70158f;
            constexpr float c3 = c1 + 1.0f;
            const float inv = t - 1.0f;
            return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
        }

        static std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        static glm::vec4 WithAlpha(glm::vec4 color, float alphaScale)
        {
            color.a *= Saturate(alphaScale);
            return color;
        }

        static bool IsEditorUIHidden(Scene* scene,
            UIWidgetLayout::Context& layout,
            entt::entity entity,
            std::unordered_set<uint32_t>& visiting)
        {
            if (!scene || scene->GetExecutionMode() != SceneExecutionMode::Edit)
                return false;

            auto& registry = scene->GetRegistry();
            if (!registry.valid(entity))
                return false;

            if (registry.all_of<EditorHiddenComponent>(entity))
                return true;

            if (!registry.all_of<UIWidgetComponent>(entity))
                return false;

            const uint32_t key = static_cast<uint32_t>(entity);
            if (!visiting.insert(key).second)
                return false;

            const auto& widget = registry.get<UIWidgetComponent>(entity);
            const entt::entity parent = layout.ResolveReference(widget.ParentEntity);
            const bool hidden = parent != entt::null && registry.valid(parent)
                ? IsEditorUIHidden(scene, layout, parent, visiting)
                : false;
            visiting.erase(key);
            return hidden;
        }

        struct UIClipRect
        {
            float Left = 0.0f;
            float Right = 0.0f;
            float Top = 0.0f;
            float Bottom = 0.0f;
        };

        struct UIClipRegion
        {
            int SortOrder = 0;
            UIClipRect Rect;
            float Area = 0.0f;
        };

        static UIClipRect WidgetToClipRect(const UIWidgetComponent& widget)
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

        static bool ClipRectsIntersect(const UIClipRect& a, const UIClipRect& b)
        {
            return a.Left < b.Right && a.Right > b.Left
                && a.Top < b.Bottom && a.Bottom > b.Top;
        }

        static float ClipRectArea(const UIClipRect& rect)
        {
            return std::max(0.0f, rect.Right - rect.Left)
                * std::max(0.0f, rect.Bottom - rect.Top);
        }

        static std::vector<UIClipRegion> GatherClipRegions(Scene* scene)
        {
            std::vector<UIClipRegion> regions;
            if (!scene)
                return regions;

            UIWidgetLayout::Context layout(scene);
            const bool editMode = scene->GetExecutionMode() == SceneExecutionMode::Edit;
            auto& registry = scene->GetRegistry();
            auto view = registry.view<UIWidgetComponent, UIPanelComponent>();
            for (auto entity : view)
            {
                const auto& widget = view.get<UIWidgetComponent>(entity);
                const auto& panel = view.get<UIPanelComponent>(entity);
                const bool visible = editMode
                    ? UIWidgetLayout::ResolveEditorVisible(layout, entity)
                    : UIWidgetLayout::ResolveVisible(layout, entity);
                if (!visible || !panel.ClipChildren)
                    continue;

                UIClipRect rect = WidgetToClipRect(widget);
                const float area = ClipRectArea(rect);
                if (area <= 0.0f)
                    continue;

                regions.push_back({ widget.SortOrder, rect, area });
            }
            return regions;
        }

        static const UIClipRegion* ResolveClipRegion(const std::vector<UIClipRegion>& regions,
            int widgetSortOrder,
            const UIClipRect& widgetRect)
        {
            const UIClipRegion* best = nullptr;

            for (const auto& region : regions)
            {
                if (region.SortOrder >= widgetSortOrder)
                    continue;
                if (!ClipRectsIntersect(region.Rect, widgetRect))
                    continue;

                if (!best
                    || region.SortOrder > best->SortOrder
                    || (region.SortOrder == best->SortOrder && region.Area < best->Area))
                {
                    best = &region;
                }
            }

            return best;
        }

        struct UIScissorRect
        {
            uint32_t X = 0;
            uint32_t Y = 0;
            uint32_t Width = 0;
            uint32_t Height = 0;
        };

        static UIScissorRect ClipRectToScissor(const UIClipRect& rect,
            uint32_t viewportWidth,
            uint32_t viewportHeight)
        {
            const float left = std::clamp(rect.Left, 0.0f, 1.0f);
            const float right = std::clamp(rect.Right, 0.0f, 1.0f);
            const float top = std::clamp(rect.Top, 0.0f, 1.0f);
            const float bottom = std::clamp(rect.Bottom, 0.0f, 1.0f);

            const int x0 = static_cast<int>(std::floor(left * viewportWidth));
            const int x1 = static_cast<int>(std::ceil(right * viewportWidth));
            const int y0 = static_cast<int>(std::floor((1.0f - bottom) * viewportHeight));
            const int y1 = static_cast<int>(std::ceil((1.0f - top) * viewportHeight));

            const int clampedX0 = std::clamp(x0, 0, static_cast<int>(viewportWidth));
            const int clampedX1 = std::clamp(x1, 0, static_cast<int>(viewportWidth));
            const int clampedY0 = std::clamp(y0, 0, static_cast<int>(viewportHeight));
            const int clampedY1 = std::clamp(y1, 0, static_cast<int>(viewportHeight));

            return {
                static_cast<uint32_t>(clampedX0),
                static_cast<uint32_t>(clampedY0),
                static_cast<uint32_t>(std::max(0, clampedX1 - clampedX0)),
                static_cast<uint32_t>(std::max(0, clampedY1 - clampedY0))
            };
        }

        static bool SameScissorRect(const UIScissorRect& a, const UIScissorRect& b)
        {
            return a.X == b.X && a.Y == b.Y
                && a.Width == b.Width && a.Height == b.Height;
        }

        static void CaptureBaseState(entt::registry& registry,
            entt::entity entity,
            const UIWidgetComponent& widget,
            UIAnimatorComponent& animator)
        {
            animator.RuntimeBasePosition = widget.Position;
            animator.RuntimeBaseSize = widget.Size;

            if (auto* image = registry.try_get<UIImageComponent>(entity))
                animator.RuntimeBaseImageColor = image->Color;
            if (auto* panel = registry.try_get<UIPanelComponent>(entity))
            {
                animator.RuntimeBasePanelBackground = panel->BackgroundColor;
                animator.RuntimeBasePanelBorder = panel->BorderColor;
            }
            if (auto* text = registry.try_get<UITextComponent>(entity))
                animator.RuntimeBaseTextColor = text->Color;
            if (auto* button = registry.try_get<UIButtonComponent>(entity))
            {
                animator.RuntimeBaseButtonNormal = button->NormalColor;
                animator.RuntimeBaseButtonHover = button->HoverColor;
                animator.RuntimeBaseButtonPressed = button->PressedColor;
            }
            if (auto* bar = registry.try_get<UIProgressBarComponent>(entity))
            {
                animator.RuntimeBaseProgressForeground = bar->ForegroundColor;
                animator.RuntimeBaseProgressBackground = bar->BackgroundColor;
            }

            animator.RuntimeInitialized = true;
        }

        static void RestoreBaseState(entt::registry& registry,
            entt::entity entity,
            UIWidgetComponent& widget,
            const UIAnimatorComponent& animator)
        {
            widget.Position = animator.RuntimeBasePosition;
            widget.Size = animator.RuntimeBaseSize;

            if (auto* image = registry.try_get<UIImageComponent>(entity))
                image->Color = animator.RuntimeBaseImageColor;
            if (auto* panel = registry.try_get<UIPanelComponent>(entity))
            {
                panel->BackgroundColor = animator.RuntimeBasePanelBackground;
                panel->BorderColor = animator.RuntimeBasePanelBorder;
            }
            if (auto* text = registry.try_get<UITextComponent>(entity))
                text->Color = animator.RuntimeBaseTextColor;
            if (auto* button = registry.try_get<UIButtonComponent>(entity))
            {
                button->NormalColor = animator.RuntimeBaseButtonNormal;
                button->HoverColor = animator.RuntimeBaseButtonHover;
                button->PressedColor = animator.RuntimeBaseButtonPressed;
            }
            if (auto* bar = registry.try_get<UIProgressBarComponent>(entity))
            {
                bar->ForegroundColor = animator.RuntimeBaseProgressForeground;
                bar->BackgroundColor = animator.RuntimeBaseProgressBackground;
            }
        }

        static void ApplyAlpha(entt::registry& registry,
            entt::entity entity,
            const UIAnimatorComponent& animator,
            float alpha)
        {
            if (auto* image = registry.try_get<UIImageComponent>(entity))
                image->Color = WithAlpha(animator.RuntimeBaseImageColor, alpha);
            if (auto* panel = registry.try_get<UIPanelComponent>(entity))
            {
                panel->BackgroundColor = WithAlpha(animator.RuntimeBasePanelBackground, alpha);
                panel->BorderColor = WithAlpha(animator.RuntimeBasePanelBorder, alpha);
            }
            if (auto* text = registry.try_get<UITextComponent>(entity))
                text->Color = WithAlpha(animator.RuntimeBaseTextColor, alpha);
            if (auto* button = registry.try_get<UIButtonComponent>(entity))
            {
                button->NormalColor = WithAlpha(animator.RuntimeBaseButtonNormal, alpha);
                button->HoverColor = WithAlpha(animator.RuntimeBaseButtonHover, alpha);
                button->PressedColor = WithAlpha(animator.RuntimeBaseButtonPressed, alpha);
            }
            if (auto* bar = registry.try_get<UIProgressBarComponent>(entity))
            {
                bar->ForegroundColor = WithAlpha(animator.RuntimeBaseProgressForeground, alpha);
                bar->BackgroundColor = WithAlpha(animator.RuntimeBaseProgressBackground, alpha);
            }
        }

        static void ApplyScale(UIWidgetComponent& widget,
            const UIAnimatorComponent& animator,
            float scale)
        {
            const float clampedScale = std::max(0.01f, scale);
            widget.Size = animator.RuntimeBaseSize * clampedScale;
        }

        static void ResetTimeline(entt::registry& registry,
            entt::entity entity,
            UIWidgetComponent& widget,
            UIAnimatorComponent& animator)
        {
            if (!animator.RuntimeInitialized)
                CaptureBaseState(registry, entity, widget, animator);

            RestoreBaseState(registry, entity, widget, animator);
            animator.RuntimeTime = 0.0f;
        }

        static void ResetUIAnimators(Scene* scene)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            auto view = registry.view<UIWidgetComponent, UIAnimatorComponent>();
            for (auto entity : view)
            {
                auto& widget = view.get<UIWidgetComponent>(entity);
                auto& animator = view.get<UIAnimatorComponent>(entity);
                animator.RuntimeInitialized = false;
                animator.RuntimeWasVisible = widget.Visible;
                ResetTimeline(registry, entity, widget, animator);
            }
        }

        static void PreloadSceneUIText(Scene* scene)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            auto view = registry.view<UITextComponent>();
            for (auto entity : view)
                UIRenderer::PreloadUIText(view.get<UITextComponent>(entity));
        }

        static void UpdateUIAnimators(Scene* scene, float dt)
        {
            if (!scene)
                return;

            dt = std::clamp(dt, 0.0f, 1.0f / 30.0f);

            auto& registry = scene->GetRegistry();
            auto view = registry.view<UIWidgetComponent, UIAnimatorComponent>();
            for (auto entity : view)
            {
                auto& widget = view.get<UIWidgetComponent>(entity);
                auto& animator = view.get<UIAnimatorComponent>(entity);

                if (!animator.RuntimeInitialized)
                    CaptureBaseState(registry, entity, widget, animator);

                if (!widget.Visible)
                {
                    animator.RuntimeWasVisible = false;
                    continue;
                }

                if (!animator.RuntimeWasVisible)
                    ResetTimeline(registry, entity, widget, animator);
                animator.RuntimeWasVisible = true;

                RestoreBaseState(registry, entity, widget, animator);

                const std::string preset = ToLower(animator.Preset);
                if (!animator.PlayOnStart && preset != "pulse" && preset != "hover_pulse")
                    continue;

                const float duration = std::max(animator.Duration, 0.001f);
                const float localTime = animator.RuntimeTime - std::max(animator.Delay, 0.0f);
                const float progress = Saturate(localTime / duration);
                const float eased = EaseOutCubic(progress);

                if (localTime < 0.0f)
                {
                    if (preset == "fade_in" || preset == "slide_fade_in" || preset == "result_pop")
                        ApplyAlpha(registry, entity, animator, 0.0f);

                    if (preset == "slide_fade_in")
                        widget.Position = animator.RuntimeBasePosition + animator.FromOffset;
                    if (preset == "result_pop")
                        ApplyScale(widget, animator, 0.82f);

                    animator.RuntimeTime += dt;
                    continue;
                }

                if (preset == "fade_in")
                {
                    ApplyAlpha(registry, entity, animator, eased);
                }
                else if (preset == "slide_fade_in")
                {
                    ApplyAlpha(registry, entity, animator, eased);
                    widget.Position = animator.RuntimeBasePosition + animator.FromOffset * (1.0f - eased);
                }
                else if (preset == "result_pop")
                {
                    ApplyAlpha(registry, entity, animator, eased);
                    const float pop = EaseOutBack(progress);
                    const float settle = std::sin(Saturate(progress) * PI) * animator.Amplitude;
                    ApplyScale(widget, animator, 0.82f + 0.18f * pop + settle);
                }
                else if (preset == "pulse")
                {
                    const float wave = std::sin(animator.RuntimeTime * animator.Speed * PI * 2.0f);
                    ApplyScale(widget, animator, 1.0f + wave * animator.Amplitude);
                }
                else if (preset == "hover_pulse")
                {
                    float target = 0.0f;
                    if (auto* button = registry.try_get<UIButtonComponent>(entity))
                        target = button->IsPressed ? 1.4f : (button->IsHovered ? 1.0f : 0.0f);
                    ApplyScale(widget, animator, 1.0f + animator.Amplitude * target);
                }

                if (preset == "pulse" || preset == "hover_pulse")
                    animator.RuntimeTime += dt;
                else if (animator.Loop && localTime >= duration)
                    animator.RuntimeTime = std::max(animator.Delay, 0.0f);
                else if (!animator.Loop && animator.RuntimeTime < animator.Delay + duration)
                    animator.RuntimeTime += dt;
            }
        }

    } // namespace

    void UISystem::OnRuntimeStart(Scene* scene)
    {
        ResetUIAnimators(scene);
        PreloadSceneUIText(scene);
    }

    void UISystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        UIInputSystem::OnUpdate(
            scene,
            Input::GetMouseX() - m_ViewportOffset.x,
            Input::GetMouseY() - m_ViewportOffset.y,
            scene->GetViewportWidth(),
            scene->GetViewportHeight());

        UpdateUIAnimators(scene, ts.GetSeconds());
    }

    void UISystem::OnUpdateEditor(Scene* scene, Timestep ts)
    {
    }

    void UISystem::RenderUI(Scene* scene)
    {
        auto& registry = scene->GetRegistry();
        UIWidgetLayout::Context layout(scene);
        const uint32_t viewportWidth = scene->GetViewportWidth();
        const uint32_t viewportHeight = scene->GetViewportHeight();

        std::vector<std::pair<int, entt::entity>> entries;
        for (auto e : registry.view<UIWidgetComponent>())
            entries.emplace_back(registry.get<UIWidgetComponent>(e).SortOrder, e);

        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        bool scissorEnabled = false;
        UIScissorRect currentScissor{};

        auto applyClip = [&](const std::optional<UIWidgetLayout::Rect>& clipRegion)
        {
            if (!clipRegion)
            {
                if (scissorEnabled)
                {
                    Renderer2D::Flush();
                    RenderCommand::SetScissorTest(false);
                    scissorEnabled = false;
                    currentScissor = {};
                }
                return;
            }

            const UIClipRect clipRect = {
                clipRegion->Left,
                clipRegion->Right,
                clipRegion->Top,
                clipRegion->Bottom
            };
            const UIScissorRect nextScissor = ClipRectToScissor(clipRect, viewportWidth, viewportHeight);
            if (!scissorEnabled || !SameScissorRect(currentScissor, nextScissor))
            {
                Renderer2D::Flush();
                RenderCommand::SetScissor(nextScissor.X, nextScissor.Y, nextScissor.Width, nextScissor.Height);
                RenderCommand::SetScissorTest(true);
                scissorEnabled = true;
                currentScissor = nextScissor;
            }
        };

        for (auto [order, e] : entries)
        {
            (void)order;
            std::unordered_set<uint32_t> hiddenVisiting;
            if (IsEditorUIHidden(scene, layout, e, hiddenVisiting))
                continue;

            const bool editMode = scene->GetExecutionMode() == SceneExecutionMode::Edit;
            if (editMode
                ? !UIWidgetLayout::ResolveEditorVisible(layout, e)
                : !UIWidgetLayout::ResolveVisible(layout, e))
                continue;

            const UIWidgetLayout::Rect resolvedRect = UIWidgetLayout::ResolveRect(layout, e);
            if (!UIWidgetLayout::RectVisibleInClipAndViewport(layout, e, resolvedRect, 0.004f))
                continue;

            const auto parentClipRect = UIWidgetLayout::ResolveParentClipRect(layout, e);
            const UIWidgetComponent resolvedWidget = editMode
                ? UIWidgetLayout::ResolveEditorWidget(layout, e)
                : UIWidgetLayout::ResolveWidget(layout, e);
            if (!resolvedWidget.Visible) continue;
            applyClip(parentClipRect);
            int id = static_cast<int>(static_cast<uint32_t>(e));

            if (auto* panel = registry.try_get<UIPanelComponent>(e))
                UIRenderer::DrawUIPanel(resolvedWidget, *panel, id);
            if (auto* pb = registry.try_get<UIProgressBarComponent>(e))
                UIRenderer::DrawUIProgressBar(resolvedWidget, *pb, id);
            if (auto* btn = registry.try_get<UIButtonComponent>(e))
                UIRenderer::DrawUIButton(resolvedWidget, *btn, id);
            if (auto* slider = registry.try_get<UISliderComponent>(e))
                UIRenderer::DrawUISlider(resolvedWidget, *slider, id);
            if (auto* scrollView = registry.try_get<UIScrollViewComponent>(e))
                UIRenderer::DrawUIScrollView(resolvedWidget, *scrollView, id);
            if (auto* path = registry.try_get<UIPathComponent>(e))
                UIRenderer::DrawUIPath(resolvedWidget, *path, id);
            if (auto* skillTreeView = registry.try_get<UISkillTreeViewComponent>(e))
            {
                applyClip(std::optional<UIWidgetLayout::Rect>(resolvedRect));
                UIRenderer::DrawUISkillTreeView(resolvedWidget, *skillTreeView, id);
                applyClip(parentClipRect);
            }
            if (auto* checkbox = registry.try_get<UICheckboxComponent>(e))
                UIRenderer::DrawUICheckbox(resolvedWidget, *checkbox, id);
            if (auto* img = registry.try_get<UIImageComponent>(e))
                UIRenderer::DrawUIImage(resolvedWidget, *img, id);
            if (auto* radial = registry.try_get<UIRadialCooldownComponent>(e))
                UIRenderer::DrawUIRadialCooldown(resolvedWidget, *radial, id);
            if (auto* text = registry.try_get<UITextComponent>(e))
                UIRenderer::DrawUIText(resolvedWidget, *text, id);
        }

        if (scissorEnabled)
        {
            Renderer2D::Flush();
            RenderCommand::SetScissorTest(false);
        }
    }

} // namespace Wheatear
