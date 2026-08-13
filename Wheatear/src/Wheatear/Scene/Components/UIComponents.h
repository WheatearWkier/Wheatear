#pragma once

// Screen-space UI widget components (canvas, widget, animator, image, panel,
// text, button, progress, slider, pager, scroll view, path, skill tree,
// page item, checkbox).

#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"
#include "Wheatear/Renderer/Texture.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Wheatear {

    enum class UIAnchor
    {
        TopLeft, TopCenter, TopRight,
        MiddleLeft, MiddleCenter, MiddleRight,
        BottomLeft, BottomCenter, BottomRight
    };

    struct UICanvasComponent
    {
        bool  Visible = true;
        float ReferenceWidth = 1920.0f;
        float ReferenceHeight = 1080.0f;

        UICanvasComponent() = default;
        UICanvasComponent(const UICanvasComponent&) = default;
    };

    struct UIWidgetComponent
    {
        bool      Visible = true;
        bool      EditorVisible = true;
        glm::vec2 Position = { 0.5f, 0.5f };
        glm::vec2 Size = { 0.1f, 0.05f };
        float     Rotation = 0.0f;
        UIAnchor  Anchor = UIAnchor::MiddleCenter;
        int       SortOrder = 0;
        UUID      ParentEntity = 0;

        UIWidgetComponent() = default;
        UIWidgetComponent(const UIWidgetComponent&) = default;
    };

    // -- UIAnimator: declarative runtime motion for panels, buttons, text, and icons.
    struct UIAnimatorComponent
    {
        std::string Preset = "fade_in"; // fade_in, slide_fade_in, result_pop, pulse, hover_pulse
        bool PlayOnStart = true;
        bool Loop = false;
        float Delay = 0.0f;
        float Duration = 0.35f;
        float Amplitude = 0.035f;
        float Speed = 1.0f;
        glm::vec2 FromOffset = { 0.0f, 0.0f };

        // Runtime cache. Stored on the component so UI animations survive system-only updates.
        bool RuntimeInitialized = false;
        bool RuntimeWasVisible = false;
        std::string RuntimePresetLower; // ToLower(Preset), computed once to avoid per-frame allocation
        float RuntimeTime = 0.0f;
        glm::vec2 RuntimeBasePosition = { 0.5f, 0.5f };
        glm::vec2 RuntimeBaseSize = { 0.1f, 0.05f };
        glm::vec4 RuntimeBaseImageColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 RuntimeBasePanelBackground = { 0.06f, 0.07f, 0.09f, 0.85f };
        glm::vec4 RuntimeBasePanelBorder = { 0.28f, 0.44f, 0.52f, 0.85f };
        glm::vec4 RuntimeBaseTextColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 RuntimeBaseButtonNormal = { 0.3f, 0.3f, 0.8f, 1.0f };
        glm::vec4 RuntimeBaseButtonHover = { 0.4f, 0.4f, 1.0f, 1.0f };
        glm::vec4 RuntimeBaseButtonPressed = { 0.2f, 0.2f, 0.6f, 1.0f };
        glm::vec4 RuntimeBaseProgressForeground = { 0.2f, 0.8f, 0.2f, 1.0f };
        glm::vec4 RuntimeBaseProgressBackground = { 0.2f, 0.2f, 0.2f, 1.0f };

        UIAnimatorComponent() = default;
        UIAnimatorComponent(const UIAnimatorComponent&) = default;
    };

    struct UIImageComponent
    {
        glm::vec4      Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        Ref<Texture2D> Texture = nullptr;
        glm::vec2      UVMin = { 0.0f, 0.0f };
        glm::vec2      UVMax = { 1.0f, 1.0f };

        UIImageComponent() = default;
        UIImageComponent(const UIImageComponent&) = default;
    };

    struct UIRadialCooldownComponent
    {
        float     Progress = 0.0f;
        float     StartAngle = 1.57079632679f;
        float     Thickness = 1.0f;
        float     Fade = 0.005f;
        glm::vec4 Color = { 0.0f, 0.0f, 0.0f, 0.58f };

        UIRadialCooldownComponent() = default;
        UIRadialCooldownComponent(const UIRadialCooldownComponent&) = default;
    };

    // -- UIPanel: screen-space UI background/window panel.
    struct UIPanelComponent
    {
        glm::vec4 BackgroundColor = { 0.06f, 0.07f, 0.09f, 0.85f };
        glm::vec4 BorderColor = { 0.28f, 0.44f, 0.52f, 0.85f };
        float     BorderThickness = 0.0f;
        bool      ClipChildren = false;
        bool      Draggable = false;
        bool      ConstrainDragToParent = true;
        float     DragHandleHeight = 0.12f; // 0 means the whole panel can start a drag.

        bool      RuntimeIsDragging = false;
        glm::vec2 RuntimeDragPointerOffset = { 0.0f, 0.0f };

        UIPanelComponent() = default;
        UIPanelComponent(const UIPanelComponent&) = default;
    };

    enum class UITextHorizontalAlign : int
    {
        Left = 0,
        Center = 1,
        Right = 2
    };

    enum class UITextVerticalAlign : int
    {
        Top = 0,
        Middle = 1,
        Bottom = 2
    };

    struct UITextComponent
    {
        std::string Text = "Text";
        glm::vec4   Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float       FontSize = 24.0f;
        std::string FontPath = "font.ui_default";
        glm::vec4   Padding = { -1.0f, -1.0f, -1.0f, -1.0f };
        bool        AutoFit = true;
        UITextHorizontalAlign HorizontalAlign = UITextHorizontalAlign::Left;
        UITextVerticalAlign VerticalAlign = UITextVerticalAlign::Top;
        glm::vec4   ShadowColor = { 0.02f, 0.03f, 0.04f, 0.78f };
        glm::vec2   ShadowOffset = { 2.0f, 2.0f };
        glm::vec4   OutlineColor = { 0.02f, 0.02f, 0.025f, 0.86f };
        float       OutlineThickness = 1.25f;

        UITextComponent() = default;
        UITextComponent(const UITextComponent&) = default;
        explicit UITextComponent(const std::string& text) : Text(text) {}
    };

    struct UIButtonComponent
    {
        glm::vec4 NormalColor = { 0.3f, 0.3f, 0.8f, 1.0f };
        glm::vec4 HoverColor = { 0.4f, 0.4f, 1.0f, 1.0f };
        glm::vec4 PressedColor = { 0.2f, 0.2f, 0.6f, 1.0f };

        bool IsHovered = false;
        bool IsPressed = false;

        std::string OnClickFunction = "";
        std::string TooltipText = "";

        UIButtonComponent() = default;
        UIButtonComponent(const UIButtonComponent&) = default;
    };

    struct UIProgressBarComponent
    {
        float     Value = 1.0f;  // 0.0 ~ 1.0
        float     MaxValue = 1.0f;
        glm::vec4 ForegroundColor = { 0.2f, 0.8f, 0.2f, 1.0f };
        glm::vec4 BackgroundColor = { 0.2f, 0.2f, 0.2f, 1.0f };

        float GetNormalized() const
        {
            if (MaxValue <= 0.0f) return 0.0f;
            return glm::clamp(Value / MaxValue, 0.0f, 1.0f);
        }

        UIProgressBarComponent() = default;
        UIProgressBarComponent(const UIProgressBarComponent&) = default;
    };

    // -- UISlider: numeric setting such as volume/text speed.
    struct UISliderComponent
    {
        float     Value = 0.5f;
        float     MinValue = 0.0f;
        float     MaxValue = 1.0f;
        glm::vec4 TrackColor = { 0.18f, 0.20f, 0.24f, 0.90f };
        glm::vec4 FillColor = { 0.35f, 0.70f, 0.78f, 0.95f };
        glm::vec4 HandleColor = { 0.92f, 0.96f, 0.98f, 1.0f };
        glm::vec4 HoverColor = { 0.48f, 0.82f, 0.90f, 1.0f };

        bool IsHovered = false;
        bool IsDragging = false;
        std::string OnValueChangedFunction = "";

        float GetNormalized() const
        {
            const float range = MaxValue - MinValue;
            if (range <= 0.0f) return 0.0f;
            return glm::clamp((Value - MinValue) / range, 0.0f, 1.0f);
        }

        void SetNormalized(float normalized)
        {
            Value = MinValue + glm::clamp(normalized, 0.0f, 1.0f) * (MaxValue - MinValue);
        }

        UISliderComponent() = default;
        UISliderComponent(const UISliderComponent&) = default;
    };

    // -- UIPager: known-length page switching controller.
    struct UIPagerComponent
    {
        int CurrentPage = 1;
        int PageCount = 1;
        bool Wrap = false;

        int GetClampedCurrentPage() const
        {
            return std::clamp(CurrentPage, 1, std::max(PageCount, 1));
        }

        UIPagerComponent() = default;
        UIPagerComponent(const UIPagerComponent&) = default;
    };

    // -- UIScrollView: continuous scrolling for clipped UI panels.
    struct UIScrollViewComponent
    {
        float OffsetY = 0.0f;       // Normalized to the viewport height of this widget.
        float ContentHeight = 1.0f; // 1.0 means no vertical overflow.
        float WheelStep = 0.08f;
        float ScrollbarWidth = 0.018f;
        bool EnableWheel = true;
        bool ShowScrollbar = true;
        bool DragScrollbar = true;
        bool ClampToContent = true;

        bool RuntimeThumbHovered = false;
        bool RuntimeThumbDragging = false;
        glm::vec2 RuntimeDragStartMouse = { 0.0f, 0.0f };
        float RuntimeDragStartOffsetY = 0.0f;

        float GetMaxOffset() const
        {
            return std::max(ContentHeight - 1.0f, 0.0f);
        }

        float GetNormalized() const
        {
            const float maxOffset = GetMaxOffset();
            if (maxOffset <= 0.0f)
                return 0.0f;
            return glm::clamp(OffsetY / maxOffset, 0.0f, 1.0f);
        }

        void ClampOffset()
        {
            ContentHeight = std::max(ContentHeight, 1.0f);
            if (ClampToContent)
                OffsetY = glm::clamp(OffsetY, 0.0f, GetMaxOffset());
        }

        UIScrollViewComponent() = default;
        UIScrollViewComponent(const UIScrollViewComponent&) = default;
    };

    struct UISkillTreeNodeView
    {
        std::string Id;
        std::string ParentId;
        glm::vec2 Position = { 0.5f, 0.5f };
        std::string IconPath;
        std::string Branch;
        int UnlockChapter = 1;
        bool Learned = false;
        bool Available = true;
        bool Selected = false;
        bool Locked = false;
    };

    enum class UIPathMode : int
    {
        Polyline = 0,
        QuadraticBezier = 1,
        CubicBezier = 2
    };

    // -- UIPath: reusable local-space UI lines for skill trees, maps, and route previews.
    struct UIPathComponent
    {
        UIPathMode Mode = UIPathMode::QuadraticBezier;
        std::vector<glm::vec2> Points = {
            { 0.10f, 0.62f },
            { 0.50f, 0.18f },
            { 0.90f, 0.62f }
        };
        glm::vec4 Color = { 0.36f, 0.90f, 0.68f, 0.86f };
        glm::vec4 GlowColor = { 0.20f, 0.72f, 0.56f, 0.24f };
        float Thickness = 0.006f;
        float GlowThicknessMultiplier = 2.6f;
        int Segments = 24;
        bool Closed = false;
        bool DrawGlow = true;

        UIPathComponent() = default;
        UIPathComponent(const UIPathComponent&) = default;
    };

    // -- UISkillTreeView: virtualized node graph for skill, map, and route UIs.
    struct UISkillTreeViewComponent
    {
        std::vector<UISkillTreeNodeView> Nodes;
        glm::vec2 Pan = { 0.0f, 0.0f };
        glm::vec2 MinPan = { -0.46f, -0.46f };
        glm::vec2 MaxPan = { 0.46f, 0.46f };
        glm::vec2 NodeSize = { 0.076f, 0.104f };
        float NodeEdgeInset = 0.050f;
        float LineThickness = 0.0095f;
        float CurveAmount = 0.045f;
        float VirtualizationMargin = 0.12f;
        int LineSegments = 24;
        int BackgroundRingCount = 4;
        bool DrawLineGlow = true;
        std::string CommandPrefix = "progression:select_skill_node:";

        glm::vec4 BackgroundColor = { 0.010f, 0.018f, 0.020f, 0.22f };
        glm::vec4 GridColor = { 0.13f, 0.27f, 0.24f, 0.22f };
        glm::vec4 LineColor = { 0.18f, 0.34f, 0.30f, 0.58f };
        glm::vec4 ActiveLineColor = { 0.38f, 0.96f, 0.72f, 0.82f };
        glm::vec4 LineGlowColor = { 0.28f, 0.88f, 0.64f, 0.18f };
        glm::vec4 NodeColor = { 0.08f, 0.12f, 0.12f, 0.95f };
        glm::vec4 LockedNodeColor = { 0.05f, 0.06f, 0.065f, 0.90f };
        glm::vec4 HoverNodeColor = { 0.42f, 0.86f, 0.72f, 0.96f };
        glm::vec4 SelectedNodeColor = { 1.00f, 0.82f, 0.38f, 0.98f };
        glm::vec4 CoreNodeColor = { 0.18f, 0.42f, 0.40f, 0.98f };
        glm::vec4 LockColor = { 0.0f, 0.0f, 0.0f, 0.54f };

        std::string SelectedNodeId;
        std::string RuntimeHoveredNodeId;
        bool RuntimeDragging = false;
        glm::vec2 RuntimeDragStartMouse = { 0.0f, 0.0f };
        glm::vec2 RuntimeDragStartPan = { 0.0f, 0.0f };
        float RuntimeDragDistance = 0.0f;

        void ClampPan()
        {
            Pan.x = glm::clamp(Pan.x, MinPan.x, MaxPan.x);
            Pan.y = glm::clamp(Pan.y, MinPan.y, MaxPan.y);
        }

        UISkillTreeViewComponent() = default;
        UISkillTreeViewComponent(const UISkillTreeViewComponent&) = default;
    };

    // -- UIPageItem: shows this widget only while its pager is on Page.
    struct UIPageItemComponent
    {
        UUID PagerEntity = 0;
        int Page = 1;

        UIPageItemComponent() = default;
        UIPageItemComponent(const UIPageItemComponent&) = default;
    };

    // -- UICheckbox: boolean option/toggle state.
    struct UICheckboxComponent
    {
        bool      Checked = false;
        glm::vec4 BoxColor = { 0.16f, 0.19f, 0.23f, 0.92f };
        glm::vec4 CheckColor = { 0.35f, 0.82f, 0.66f, 1.0f };
        glm::vec4 HoverColor = { 0.24f, 0.32f, 0.38f, 0.98f };
        glm::vec4 PressedColor = { 0.10f, 0.14f, 0.18f, 1.0f };

        bool IsHovered = false;
        bool IsPressed = false;
        std::string OnValueChangedFunction = "";

        UICheckboxComponent() = default;
        UICheckboxComponent(const UICheckboxComponent&) = default;
    };

} // namespace Wheatear
