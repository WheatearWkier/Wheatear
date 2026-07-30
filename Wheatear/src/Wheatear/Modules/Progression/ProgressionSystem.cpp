#include "wtpch.h"
#include "ProgressionSystem.h"

#include "GameProgress.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRenderer.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::IsButtonHovered;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetCenter;
        using UIRuntimeTools::SetWidgetParent;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        struct WidgetRect
        {
            float Left = 0.0f;
            float Right = 0.0f;
            float Top = 0.0f;
            float Bottom = 0.0f;
        };

        struct SkillTreeVisualNode
        {
            std::string Id;
            std::string ParentId;
            glm::vec2 Position = { 0.5f, 0.5f };
        };

        struct ResultDropIcon
        {
            const char* Key;
            const char* ItemId;
            const char* DisplayName;
            const char* IconPath;
            const char* Usage;
        };

        constexpr float kPi = 3.14159265359f;

        static WidgetRect WidgetToRect(const UIWidgetComponent& widget)
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

        static bool PointInRect(const WidgetRect& rect, float x, float y)
        {
            return x >= rect.Left && x <= rect.Right && y >= rect.Top && y <= rect.Bottom;
        }

        static bool GetMouseNormalized(float& x, float& y)
        {
            const Window& window = Application::Get().GetWindow();
            if (window.GetWidth() == 0 || window.GetHeight() == 0)
                return false;

            x = Input::GetMouseX() / static_cast<float>(window.GetWidth());
            y = Input::GetMouseY() / static_cast<float>(window.GetHeight());
            return x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f;
        }

        static Entity EnsurePager(Scene* scene, const std::string& pagerName, int pageCount)
        {
            Entity pager = FindEntityByName(scene, pagerName);
            if (!pager)
            {
                pager = scene->CreateEntity(pagerName);
                auto& widget = pager.AddComponent<UIWidgetComponent>();
                widget.Visible = false;
                widget.Anchor = UIAnchor::TopLeft;
                widget.Position = { 0.0f, 0.0f };
                widget.Size = { 0.001f, 0.001f };
                widget.SortOrder = 0;
            }

            if (!pager.HasComponent<UIWidgetComponent>())
            {
                auto& widget = pager.AddComponent<UIWidgetComponent>();
                widget.Visible = false;
                widget.Anchor = UIAnchor::TopLeft;
                widget.Position = { 0.0f, 0.0f };
                widget.Size = { 0.001f, 0.001f };
            }

            auto& pagerComponent = pager.HasComponent<UIPagerComponent>()
                ? pager.GetComponent<UIPagerComponent>()
                : pager.AddComponent<UIPagerComponent>();
            pagerComponent.PageCount = std::max(pageCount, 1);
            pagerComponent.CurrentPage = std::clamp(pagerComponent.CurrentPage, 1, pagerComponent.PageCount);
            return pager;
        }

        static Entity EnsureUIWidget(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            bool visible = true)
        {
            if (!scene || entityName.empty())
                return {};

            Entity entity = FindEntityByName(scene, entityName);
            if (!entity)
                entity = scene->CreateEntity(entityName);

            auto& widget = entity.HasComponent<UIWidgetComponent>()
                ? entity.GetComponent<UIWidgetComponent>()
                : entity.AddComponent<UIWidgetComponent>();

            widget.Visible = visible;
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = position;
            widget.Size = size;
            widget.Rotation = 0.0f;
            widget.SortOrder = sortOrder;
            Entity parent = FindEntityByName(scene, parentTag);
            widget.ParentEntity = parent ? parent.GetUUID() : UUID(0);
            widget.ParentTag = parentTag;
            return entity;
        }

        static Entity EnsurePanel(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            glm::vec4 background,
            glm::vec4 border,
            float borderThickness,
            bool clipChildren = false)
        {
            Entity entity = EnsureUIWidget(scene, entityName, parentTag, position, size, sortOrder);
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

        static Entity EnsureScrollView(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            float contentHeight)
        {
            Entity entity = EnsurePanel(scene, entityName, parentTag, position, size, sortOrder,
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

        static Entity EnsureText(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const std::string& value,
            float fontSize,
            glm::vec4 color)
        {
            Entity entity = EnsureUIWidget(scene, entityName, parentTag, position, size, sortOrder);
            if (!entity)
                return {};

            auto& text = entity.HasComponent<UITextComponent>()
                ? entity.GetComponent<UITextComponent>()
                : entity.AddComponent<UITextComponent>();
            text.Text = value;
            text.FontSize = fontSize;
            text.Color = color;
            text.FontPath = "assets/fonts/wqy-microhei.ttc";
            text.ShadowColor = { 0.01f, 0.015f, 0.018f, 0.80f };
            text.ShadowOffset = { 1.6f, 1.6f };
            text.OutlineColor = { 0.0f, 0.0f, 0.0f, 0.86f };
            text.OutlineThickness = 1.15f;
            UIRenderer::PreloadUIText(text);
            return entity;
        }

        static Entity EnsureButton(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const std::string& label,
            const std::string& command)
        {
            Entity entity = EnsureText(scene, entityName, parentTag, position, size, sortOrder,
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

        static Entity EnsureSlider(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            float minValue,
            float maxValue,
            const std::string& command)
        {
            Entity entity = EnsureUIWidget(scene, entityName, parentTag, position, size, sortOrder);
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

        static void SetPageItem(Scene* scene, const std::string& entityName, Entity pager, int page)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !pager)
                return;

            auto& pageItem = entity.HasComponent<UIPageItemComponent>()
                ? entity.GetComponent<UIPageItemComponent>()
                : entity.AddComponent<UIPageItemComponent>();
            pageItem.PagerEntity = pager.GetUUID();
            pageItem.PagerTag = pager.GetName();
            pageItem.Page = std::max(page, 1);
        }

        static int SyncEquipmentPager(Scene* scene)
        {
            Entity pager = EnsurePager(scene, "Equipment_Pager", 2);
            auto& pagerComponent = pager.GetComponent<UIPagerComponent>();
            pagerComponent.PageCount = 2;
            pagerComponent.CurrentPage = std::clamp(pagerComponent.CurrentPage, 1, pagerComponent.PageCount);

            auto& state = GameProgress::GetState();
            state.EquipmentPage = pagerComponent.CurrentPage;
            return state.EquipmentPage;
        }

        static void SetButtonPalette(Scene* scene, const std::string& entityName, glm::vec4 normal, glm::vec4 hover, glm::vec4 pressed);

        static void EnsureEquipmentReusableLayout(Scene* scene)
        {
            if (!HasEntity(scene, "Equipment_Details"))
                return;

            EnsureScrollView(scene, "Equipment_DetailsScroll", "WT_UI_Canvas",
                { 0.675f, 0.280f }, { 0.215f, 0.262f }, 34, 1.55f);
            SetWidgetParent(scene, "Equipment_Details", "Equipment_DetailsScroll");
            SetWidgetTopLeft(scene, "Equipment_Details", { 0.025f, 0.025f }, { 0.84f, 1.24f });

            EnsureScrollView(scene, "Equipment_MaterialsScroll", "WT_UI_Canvas",
                { 0.675f, 0.555f }, { 0.215f, 0.142f }, 34, 1.35f);
            SetWidgetParent(scene, "Equipment_Materials", "Equipment_MaterialsScroll");
            SetWidgetTopLeft(scene, "Equipment_Materials", { 0.025f, 0.030f }, { 0.84f, 0.92f });
        }

        static void EnsureSaveLoadReusableLayout(Scene* scene)
        {
            if (!HasEntity(scene, "SaveLoad_Status"))
                return;

            Entity pager = EnsurePager(scene, "SaveLoad_Pager", 2);
            auto& pagerComponent = pager.GetComponent<UIPagerComponent>();
            pagerComponent.PageCount = 2;
            pagerComponent.CurrentPage = std::clamp(pagerComponent.CurrentPage, 1, pagerComponent.PageCount);

            EnsureScrollView(scene, "SaveLoad_StatusScroll", "WT_UI_Canvas",
                { 0.205f, 0.292f }, { 0.50f, 0.124f }, 36, 1.35f);
            SetWidgetParent(scene, "SaveLoad_Status", "SaveLoad_StatusScroll");
            SetWidgetTopLeft(scene, "SaveLoad_Status", { 0.025f, 0.025f }, { 0.90f, 1.04f });

            EnsureScrollView(scene, "SaveLoad_LockedScroll", "WT_UI_Canvas",
                { 0.125f, 0.502f }, { 0.56f, 0.094f }, 36, 1.22f);
            SetWidgetParent(scene, "SaveLoad_EmptySlotText", "SaveLoad_LockedScroll");
            SetWidgetTopLeft(scene, "SaveLoad_EmptySlotText", { 0.025f, 0.045f }, { 0.90f, 0.84f });

            EnsureText(scene, "SaveLoad_PageText", "WT_UI_Canvas",
                { 0.525f, 0.745f }, { 0.12f, 0.045f }, 46,
                "第 1 / 2 页",
                18.0f,
                { 0.94f, 0.90f, 0.76f, 1.0f });
            EnsureButton(scene, "SaveLoad_PagePrev", "WT_UI_Canvas",
                { 0.455f, 0.74f }, { 0.055f, 0.052f }, 56,
                "<",
                "ui:pager:SaveLoad_Pager:prev");
            EnsureButton(scene, "SaveLoad_PageNext", "WT_UI_Canvas",
                { 0.655f, 0.74f }, { 0.055f, 0.052f }, 56,
                ">",
                "ui:pager:SaveLoad_Pager:next");

            SetPageItem(scene, "SaveLoad_SlotCard_1", pager, 1);
            SetPageItem(scene, "SaveLoad_SlotIcon_1", pager, 1);
            SetPageItem(scene, "SaveLoad_StatusScroll", pager, 1);
            SetPageItem(scene, "SaveLoad_Status", pager, 1);
            SetPageItem(scene, "SaveLoad_Button_1", pager, 1);
            SetPageItem(scene, "SaveLoad_Button_2", pager, 1);
            SetPageItem(scene, "SaveLoad_SlotCard_2", pager, 2);
            SetPageItem(scene, "SaveLoad_LockedScroll", pager, 2);
            SetPageItem(scene, "SaveLoad_EmptySlotText", pager, 2);

            const bool pageOne = pagerComponent.GetClampedCurrentPage() == 1;
            SetText(scene, "SaveLoad_PageText", pageOne ? "第 1 / 2 页" : "第 2 / 2 页");
            SetButtonPalette(scene, "SaveLoad_PagePrev",
                pageOne ? glm::vec4(0.08f, 0.09f, 0.10f, 0.62f) : glm::vec4(0.16f, 0.21f, 0.20f, 0.90f),
                glm::vec4(0.32f, 0.50f, 0.46f, 0.96f),
                glm::vec4(0.06f, 0.08f, 0.08f, 0.98f));
            SetButtonPalette(scene, "SaveLoad_PageNext",
                pageOne ? glm::vec4(0.16f, 0.21f, 0.20f, 0.90f) : glm::vec4(0.08f, 0.09f, 0.10f, 0.62f),
                glm::vec4(0.32f, 0.50f, 0.46f, 0.96f),
                glm::vec4(0.06f, 0.08f, 0.08f, 0.98f));
        }

        static void SetImageTexture(Scene* scene, const std::string& entityName, const std::string& texturePath)
        {
            UIRuntimeTools::SetImageTexture(scene, entityName, texturePath, true);
        }

        static void SetButtonCommand(Scene* scene, const std::string& entityName, const std::string& command)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<UIButtonComponent>())
                entity.GetComponent<UIButtonComponent>().OnClickFunction = command;
        }

        static void SetPanelColors(Scene* scene, const std::string& entityName, glm::vec4 background, glm::vec4 border)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UIPanelComponent>())
                return;

            auto& panel = entity.GetComponent<UIPanelComponent>();
            panel.BackgroundColor = background;
            panel.BorderColor = border;
        }

        static void SetPanelClipChildren(Scene* scene, const std::string& entityName, bool clipChildren)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<UIPanelComponent>())
                entity.GetComponent<UIPanelComponent>().ClipChildren = clipChildren;
        }

        static void SetButtonPalette(Scene* scene, const std::string& entityName, glm::vec4 normal, glm::vec4 hover, glm::vec4 pressed)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UIButtonComponent>())
                return;

            auto& button = entity.GetComponent<UIButtonComponent>();
            button.NormalColor = normal;
            button.HoverColor = hover;
            button.PressedColor = pressed;
        }

        static std::string SkillSafeTag(const std::string& id)
        {
            std::string safe = id;
            for (char& c : safe)
            {
                if (c == '-' || c == ':')
                    c = '_';
            }
            return safe;
        }

        static std::string SkillNodeId(const std::string& prefix, int index)
        {
            return prefix + "-" + (index < 10 ? "0" : "") + std::to_string(index);
        }

        static const std::vector<SkillTreeVisualNode>& GetSkillTreeVisualNodes()
        {
            static const std::vector<SkillTreeVisualNode> nodes = []()
            {
                std::vector<SkillTreeVisualNode> result;
                result.push_back({ "magic_sword_core", "", { 0.50f, 0.50f } });

                auto appendBranch = [&result](const std::string& prefix, float baseDegrees, float curveDegrees)
                {
                    std::string parent = "magic_sword_core";
                    for (int i = 1; i <= 12; ++i)
                    {
                        const std::string id = SkillNodeId(prefix, i);
                        const float t = static_cast<float>(i - 1) / 11.0f;
                        const float radius = 0.11f + 0.045f * static_cast<float>(i);
                        const float angle = (baseDegrees + curveDegrees * t) * kPi / 180.0f;
                        const float ringOffset = (i % 3 == 0 ? 0.014f : (i % 3 == 1 ? -0.006f : 0.006f));
                        const glm::vec2 position = {
                            0.50f + std::cos(angle) * (radius + ringOffset),
                            0.50f + std::sin(angle) * (radius + ringOffset)
                        };
                        result.push_back({ id, parent, position });
                        parent = id;
                    }
                };

                appendBranch("ME", -90.0f, 50.0f);
                appendBranch("MA", -18.0f, 52.0f);
                appendBranch("FU", 54.0f, 52.0f);
                appendBranch("MO", 126.0f, 52.0f);
                appendBranch("LI", 198.0f, 50.0f);
                return result;
            }();
            return nodes;
        }

        static const std::array<ResultDropIcon, 3>& GetResultDropIcons()
        {
            static const std::array<ResultDropIcon, 3> icons = {
                ResultDropIcon{
                    "Core",
                    "MAT-MAGIC-CORE-T0",
                    "魔核碎片",
                    "assets/vertical_slice/side_combat/ui/icon_drop_magic_core.png",
                    "魔剑觉醒、魔法分支技能和高级装备强化材料。" },
                ResultDropIcon{
                    "Sinew",
                    "MAT-BEAST-SINEW",
                    "兽筋",
                    "assets/vertical_slice/side_combat/ui/icon_drop_beast_sinew.png",
                    "旅人护衣 +1、黑林皮甲和机动系训练材料。" },
                ResultDropIcon{
                    "Claw",
                    "MAT-BEAST-CLAW",
                    "熊爪",
                    "assets/vertical_slice/side_combat/ui/icon_drop_beast_claw.png",
                    "近战剑技、护甲强化和兽系饰品材料。" }
            };
            return icons;
        }

        static std::string ExtractRewardAmount(const std::string& rewardSummary, const char* displayName)
        {
            if (rewardSummary.empty() || !displayName)
                return "0";

            const size_t namePos = rewardSummary.find(displayName);
            if (namePos == std::string::npos)
                return "0";

            const size_t marker = rewardSummary.find('x', namePos);
            if (marker == std::string::npos)
                return "?";

            size_t cursor = marker + 1;
            while (cursor < rewardSummary.size() && rewardSummary[cursor] == ' ')
                ++cursor;

            std::string amount;
            while (cursor < rewardSummary.size())
            {
                const char ch = rewardSummary[cursor];
                if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '~')
                {
                    amount.push_back(ch);
                    ++cursor;
                    continue;
                }
                break;
            }

            return amount.empty() ? "?" : amount;
        }

        static bool IsZeroAmount(const std::string& amount)
        {
            return amount.empty() || amount == "0";
        }

        static std::string BuildMaterialTooltip(const ResultDropIcon& icon, const std::string& amount)
        {
            std::ostringstream stream;
            stream << icon.DisplayName << "\n";
            stream << "本次 x" << amount << "  背包 x" << GameProgress::GetMaterialAmount(icon.ItemId) << "\n";
            stream << icon.Usage;
            return stream.str();
        }

        static void SetResultDropVisible(Scene* scene, const ResultDropIcon& icon, bool visible)
        {
            const std::string prefix = std::string("Result_Drop_") + icon.Key;
            SetWidgetVisible(scene, prefix + "_Frame", visible);
            SetWidgetVisible(scene, prefix + "_Icon", visible);
            SetWidgetVisible(scene, prefix + "_Button", visible);
            SetWidgetVisible(scene, prefix + "_Count", visible);
        }

        static void UpdateResultDrops(Scene* scene)
        {
            if (!HasEntity(scene, "Result_Drop_Core_Frame"))
                return;

            const auto& state = GameProgress::GetState();
            const bool hasResult = state.LastDungeonResult.Valid;
            std::string hoveredTooltip;
            glm::vec2 hoveredPosition = { 0.115f, 0.705f };

            int index = 0;
            for (const ResultDropIcon& icon : GetResultDropIcons())
            {
                const std::string prefix = std::string("Result_Drop_") + icon.Key;
                const std::string amount = hasResult
                    ? ExtractRewardAmount(state.LastDungeonResult.RewardSummary, icon.DisplayName)
                    : "0";
                const bool visible = hasResult && (!IsZeroAmount(amount) || std::string(icon.Key) == "Core");
                SetResultDropVisible(scene, icon, visible);
                if (!visible)
                {
                    ++index;
                    continue;
                }

                SetImageTexture(scene, prefix + "_Icon", icon.IconPath);
                SetImageColor(scene, prefix + "_Icon", IsZeroAmount(amount)
                    ? glm::vec4(0.45f, 0.47f, 0.50f, 0.80f)
                    : glm::vec4(1.0f));
                SetText(scene, prefix + "_Count", std::string("x") + amount);

                const bool hovered = IsButtonHovered(scene, prefix + "_Button")
                    || IsButtonHovered(scene, prefix + "_Icon");
                if (hovered)
                {
                    hoveredTooltip = BuildMaterialTooltip(icon, amount);
                    hoveredPosition = { 0.105f + index * 0.082f, 0.615f };
                }
                ++index;
            }

            const bool showTooltip = !hoveredTooltip.empty();
            SetWidgetVisible(scene, "Result_DropTooltipPanel", showTooltip);
            SetWidgetVisible(scene, "Result_DropTooltipText", showTooltip);
            if (showTooltip)
            {
                const glm::vec2 tooltipSize = { 0.265f, 0.120f };
                const glm::vec2 tooltipPosition = {
                    std::clamp(hoveredPosition.x, 0.080f, 0.650f - tooltipSize.x),
                    hoveredPosition.y
                };
                SetWidgetTopLeft(scene, "Result_DropTooltipPanel", tooltipPosition, tooltipSize);
                SetWidgetTopLeft(scene, "Result_DropTooltipText",
                    tooltipPosition + glm::vec2(0.012f, 0.010f),
                    tooltipSize - glm::vec2(0.024f, 0.020f));
                SetText(scene, "Result_DropTooltipText", hoveredTooltip);
            }
        }

        static void SetSlider(Scene* scene, const std::string& entityName, float value, float minValue, float maxValue)
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

        static void EnsureSettingsAudioControls(Scene* scene)
        {
            if (!HasEntity(scene, "Settings_ControlPanel"))
                return;

            const std::string parentTag = "WT_UI_Canvas";
            const glm::vec4 labelColor = { 0.88f, 1.0f, 0.9f, 1.0f };
            const glm::vec2 labelSize = { 0.18f, 0.035f };
            const glm::vec2 sliderSize = { 0.25f, 0.035f };
            const glm::vec2 buttonSize = { 0.045f, 0.045f };

            EnsureText(scene, "Settings_MasterVolumeLabel", parentTag, { 0.13f, 0.39f }, labelSize, 42, "主音量", 20.0f, labelColor);
            EnsureSlider(scene, "Settings_MasterVolumeSlider", parentTag, { 0.31f, 0.395f }, sliderSize, 44, 0.0f, 100.0f, "progression:set_master_volume");
            EnsureButton(scene, "Settings_Button_VolumeDown", parentTag, { 0.58f, 0.38f }, buttonSize, 55, "-", "progression:master_volume_down");
            EnsureButton(scene, "Settings_Button_VolumeUp", parentTag, { 0.635f, 0.38f }, buttonSize, 55, "+", "progression:master_volume_up");

            EnsureText(scene, "Settings_BGMVolumeLabel", parentTag, { 0.13f, 0.47f }, labelSize, 42, "BGM 音量", 20.0f, labelColor);
            EnsureSlider(scene, "Settings_BGMVolumeSlider", parentTag, { 0.31f, 0.475f }, sliderSize, 44, 0.0f, 100.0f, "progression:set_bgm_volume");
            EnsureButton(scene, "Settings_Button_BGMDown", parentTag, { 0.58f, 0.46f }, buttonSize, 55, "-", "progression:bgm_volume_down");
            EnsureButton(scene, "Settings_Button_BGMUp", parentTag, { 0.635f, 0.46f }, buttonSize, 55, "+", "progression:bgm_volume_up");

            EnsureText(scene, "Settings_SFXVolumeLabel", parentTag, { 0.13f, 0.55f }, labelSize, 42, "音效音量", 20.0f, labelColor);
            EnsureSlider(scene, "Settings_SFXVolumeSlider", parentTag, { 0.31f, 0.555f }, sliderSize, 44, 0.0f, 100.0f, "progression:set_sfx_volume");
            EnsureButton(scene, "Settings_Button_SFXDown", parentTag, { 0.58f, 0.54f }, buttonSize, 55, "-", "progression:sfx_volume_down");
            EnsureButton(scene, "Settings_Button_SFXUp", parentTag, { 0.635f, 0.54f }, buttonSize, 55, "+", "progression:sfx_volume_up");

            SetWidgetTopLeft(scene, "Settings_Button_Shake", { 0.13f, 0.635f }, { 0.22f, 0.052f });
            SetWidgetTopLeft(scene, "Settings_Button_Fullscreen", { 0.38f, 0.635f }, { 0.22f, 0.052f });
        }

        static const SkillTreeVisualNode* FindSkillTreeVisualNode(const std::string& id)
        {
            const auto& nodes = GetSkillTreeVisualNodes();
            for (const auto& node : nodes)
            {
                if (node.Id == id)
                    return &node;
            }
            return nullptr;
        }

        static void UpdateSkillTreeDrag(Scene* scene)
        {
            Entity panelEntity = FindEntityByName(scene, "SkillTree_NetworkPanel");
            if (!panelEntity || !panelEntity.HasComponent<UIWidgetComponent>())
                return;

            static bool dragging = false;
            static float lastX = 0.0f;
            static float lastY = 0.0f;

            float mouseX = 0.0f;
            float mouseY = 0.0f;
            const bool hasMouse = GetMouseNormalized(mouseX, mouseY);
            const bool pressed = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
            if (!pressed || !hasMouse)
            {
                dragging = false;
                return;
            }

            const WidgetRect rect = WidgetToRect(panelEntity.GetComponent<UIWidgetComponent>());
            if (!dragging)
            {
                if (!PointInRect(rect, mouseX, mouseY))
                    return;
                dragging = true;
                lastX = mouseX;
                lastY = mouseY;
                return;
            }

            const float width = std::max(0.001f, rect.Right - rect.Left);
            const float height = std::max(0.001f, rect.Bottom - rect.Top);
            auto& state = GameProgress::GetState();
            state.SkillTreePanX = std::clamp(state.SkillTreePanX + (mouseX - lastX) / width, -0.46f, 0.46f);
            state.SkillTreePanY = std::clamp(state.SkillTreePanY + (mouseY - lastY) / height, -0.46f, 0.46f);
            lastX = mouseX;
            lastY = mouseY;
        }

        static glm::vec2 SkillTreeToCanvas(glm::vec2 local, glm::vec2 pan)
        {
            return local + pan;
        }

        static bool SkillTreeRectVisible(glm::vec2 topLeft, glm::vec2 size)
        {
            constexpr float margin = 0.10f;
            return topLeft.x + size.x > -margin && topLeft.x < 1.0f + margin
                && topLeft.y + size.y > -margin && topLeft.y < 1.0f + margin;
        }

        static bool SkillTreeSegmentVisible(glm::vec2 a, glm::vec2 b, float thickness)
        {
            constexpr float margin = 0.10f;
            const glm::vec2 minPoint = { std::min(a.x, b.x) - thickness, std::min(a.y, b.y) - thickness };
            const glm::vec2 maxPoint = { std::max(a.x, b.x) + thickness, std::max(a.y, b.y) + thickness };
            return maxPoint.x > -margin && minPoint.x < 1.0f + margin
                && maxPoint.y > -margin && minPoint.y < 1.0f + margin;
        }

        struct SkillTreeCanvasCache
        {
            Scene* ScenePtr = nullptr;
            uint64_t PanelId = 0;
            float PanX = std::numeric_limits<float>::quiet_NaN();
            float PanY = std::numeric_limits<float>::quiet_NaN();
            std::string SelectedNodeId;
            size_t UnlockedHash = 0;
            bool Initialized = false;
        };

        static SkillTreeCanvasCache s_SkillTreeCanvasCache;

        static void ResetSkillTreeCanvasCache()
        {
            s_SkillTreeCanvasCache = {};
        }

        template<typename TSkillSet>
        static size_t HashSkillSet(const TSkillSet& skills)
        {
            size_t seed = skills.size();
            for (const auto& skill : skills)
            {
                const size_t value = std::hash<std::string>{}(skill);
                seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
            }
            return seed;
        }

        static void UpdateSkillTreeCanvas(Scene* scene)
        {
            Entity panelEntity = FindEntityByName(scene, "SkillTree_NetworkPanel");
            if (!panelEntity || !panelEntity.HasComponent<UIWidgetComponent>())
                return;

            const auto& state = GameProgress::GetState();
            SetPanelClipChildren(scene, "SkillTree_NetworkPanel", true);
            const glm::vec2 pan = { state.SkillTreePanX, state.SkillTreePanY };
            const uint64_t panelId = static_cast<uint64_t>(panelEntity.GetUUID());
            if (s_SkillTreeCanvasCache.ScenePtr != scene || s_SkillTreeCanvasCache.PanelId != panelId)
            {
                s_SkillTreeCanvasCache = {};
                s_SkillTreeCanvasCache.ScenePtr = scene;
                s_SkillTreeCanvasCache.PanelId = panelId;
            }

            const size_t unlockedHash = HashSkillSet(state.UnlockedSkills);
            const bool dirty = !s_SkillTreeCanvasCache.Initialized
                || std::abs(s_SkillTreeCanvasCache.PanX - pan.x) > 0.0002f
                || std::abs(s_SkillTreeCanvasCache.PanY - pan.y) > 0.0002f
                || s_SkillTreeCanvasCache.SelectedNodeId != state.SelectedSkillNodeId
                || s_SkillTreeCanvasCache.UnlockedHash != unlockedHash;
            if (!dirty)
                return;

            s_SkillTreeCanvasCache.Initialized = true;
            s_SkillTreeCanvasCache.PanX = pan.x;
            s_SkillTreeCanvasCache.PanY = pan.y;
            s_SkillTreeCanvasCache.SelectedNodeId = state.SelectedSkillNodeId;
            s_SkillTreeCanvasCache.UnlockedHash = unlockedHash;

            auto& registry = scene->GetRegistry();
            std::unordered_map<std::string, entt::entity> tags;
            tags.reserve(512);
            for (auto entity : registry.view<TagComponent>())
            {
                const auto& tag = registry.get<TagComponent>(entity).Tag;
                if (!tag.empty())
                    tags[tag] = entity;
            }

            auto findEntity = [&tags](const std::string& entityName) -> entt::entity
            {
                auto it = tags.find(entityName);
                return it != tags.end() ? it->second : entt::null;
            };

            auto setWidgetParent = [&](const std::string& entityName, const std::string& parentTag)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity == entt::null || !registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
                    return;

                const entt::entity parent = findEntity(parentTag);
                auto& widget = registry.get<UIWidgetComponent>(entity);
                widget.ParentEntity = parent != entt::null && registry.valid(parent) && registry.all_of<IDComponent>(parent)
                    ? registry.get<IDComponent>(parent).ID
                    : UUID(0);
                widget.ParentTag = parentTag;
            };

            auto setWidgetTopLeft = [&](const std::string& entityName, glm::vec2 position, glm::vec2 size)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity == entt::null || !registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
                    return;

                auto& widget = registry.get<UIWidgetComponent>(entity);
                widget.Anchor = UIAnchor::TopLeft;
                widget.Position = position;
                widget.Size = size;
                widget.Rotation = 0.0f;
            };

            auto setWidgetCenter = [&](const std::string& entityName, glm::vec2 position, glm::vec2 size, float rotation = 0.0f)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity == entt::null || !registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
                    return;

                auto& widget = registry.get<UIWidgetComponent>(entity);
                widget.Anchor = UIAnchor::MiddleCenter;
                widget.Position = position;
                widget.Size = size;
                widget.Rotation = rotation;
            };

            auto setWidgetVisible = [&](const std::string& entityName, bool visible)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity != entt::null && registry.valid(entity) && registry.all_of<UIWidgetComponent>(entity))
                    registry.get<UIWidgetComponent>(entity).Visible = visible;
            };

            auto setImageColor = [&](const std::string& entityName, glm::vec4 color)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity != entt::null && registry.valid(entity) && registry.all_of<UIImageComponent>(entity))
                    registry.get<UIImageComponent>(entity).Color = color;
            };

            auto setPanelColors = [&](const std::string& entityName, glm::vec4 background, glm::vec4 border)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity != entt::null && registry.valid(entity) && registry.all_of<UIPanelComponent>(entity))
                {
                    auto& panel = registry.get<UIPanelComponent>(entity);
                    panel.BackgroundColor = background;
                    panel.BorderColor = border;
                }
            };

            const glm::vec2 nodeSize = { 0.076f, 0.104f };
            const glm::vec2 labelSize = { 0.128f, 0.044f };
            constexpr float lineThickness = 0.0095f;
            constexpr float nodeEdgeInset = 0.050f;

            const auto& nodes = GetSkillTreeVisualNodes();
            for (const auto& node : nodes)
            {
                const std::string safe = SkillSafeTag(node.Id);
                const std::string tag = "SkillTree_Node_" + safe;
                const glm::vec2 center = SkillTreeToCanvas(node.Position, pan);
                const glm::vec2 topLeft = center - nodeSize * 0.5f;
                const bool visible = SkillTreeRectVisible(topLeft, nodeSize);
                const bool learned = state.UnlockedSkills.find(node.Id) != state.UnlockedSkills.end();
                const bool selected = state.SelectedSkillNodeId == node.Id;
                const glm::vec4 iconColor = learned ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
                                                     : glm::vec4(0.32f, 0.35f, 0.36f, 0.90f);

                setWidgetParent(tag, "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Button", "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Lock", "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Selected", "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Label", "SkillTree_NetworkPanel");

                setWidgetVisible(tag, visible);
                setWidgetVisible(tag + "_Button", visible);
                setWidgetVisible(tag + "_Lock", visible && !learned);
                setWidgetVisible(tag + "_Selected", visible && selected);
                setWidgetVisible(tag + "_Label", visible && selected);
                if (!visible)
                    continue;

                setWidgetTopLeft(tag, topLeft, nodeSize);
                setWidgetTopLeft(tag + "_Button", topLeft, nodeSize);
                setWidgetTopLeft(tag + "_Lock", topLeft, nodeSize);
                setWidgetTopLeft(tag + "_Selected", center - nodeSize * 0.58f, nodeSize * 1.16f);
                setWidgetTopLeft(tag + "_Label", { center.x - labelSize.x * 0.5f, center.y + nodeSize.y * 0.55f }, labelSize);
                setImageColor(tag, iconColor);
            }

            int lineIndex = 1;
            for (const auto& node : nodes)
            {
                if (node.ParentId.empty())
                    continue;

                const SkillTreeVisualNode* parent = FindSkillTreeVisualNode(node.ParentId);
                if (!parent)
                    continue;

                const glm::vec2 parentCenter = SkillTreeToCanvas(parent->Position, pan);
                const glm::vec2 nodeCenter = SkillTreeToCanvas(node.Position, pan);
                const bool active = state.UnlockedSkills.find(parent->Id) != state.UnlockedSkills.end()
                    && state.UnlockedSkills.find(node.Id) != state.UnlockedSkills.end();
                const std::string lineTag = "SkillTree_Line_" + std::to_string(lineIndex++);

                setWidgetParent(lineTag, "SkillTree_NetworkPanel");

                const glm::vec2 delta = nodeCenter - parentCenter;
                const float centerDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (centerDistance <= nodeEdgeInset * 2.0f)
                {
                    setWidgetVisible(lineTag, false);
                    continue;
                }

                const glm::vec2 direction = delta / centerDistance;
                const glm::vec2 a = parentCenter + direction * nodeEdgeInset;
                const glm::vec2 b = nodeCenter - direction * nodeEdgeInset;
                const bool visible = SkillTreeSegmentVisible(a, b, lineThickness);
                if (!visible)
                {
                    setWidgetVisible(lineTag, false);
                    continue;
                }

                const float length = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
                const float angle = std::atan2(b.y - a.y, b.x - a.x) * 57.2957795f;

                setWidgetCenter(lineTag, (a + b) * 0.5f, { length, lineThickness }, angle);
                setWidgetVisible(lineTag, true);
                setPanelColors(lineTag,
                    active ? glm::vec4(0.38f, 0.96f, 0.72f, 0.78f) : glm::vec4(0.18f, 0.34f, 0.30f, 0.54f),
                    glm::vec4(0.0f));
            }
        }

        static std::string SkillTreeIconPath(const std::string& nodeId)
        {
            if (nodeId == "magic_sword_core")
                return "assets/vertical_slice/ui/skill_tree/skill_magic_sword_core.png";
            return "assets/vertical_slice/ui/skill_tree/skill_" + SkillSafeTag(nodeId) + ".png";
        }

        static std::string SkillTreeBranchName(const std::string& nodeId)
        {
            if (nodeId == "magic_sword_core") return "Core";
            if (nodeId.rfind("ME-", 0) == 0) return "Melee";
            if (nodeId.rfind("MA-", 0) == 0) return "Magic";
            if (nodeId.rfind("FU-", 0) == 0) return "Aerial";
            if (nodeId.rfind("MO-", 0) == 0) return "Mobility";
            if (nodeId.rfind("LI-", 0) == 0) return "Limit";
            return "Unknown";
        }

        static int SkillTreeUnlockChapter(const std::string& nodeId)
        {
            if (nodeId == "magic_sword_core")
                return 1;

            const size_t separator = nodeId.find('-');
            if (separator == std::string::npos || separator + 1 >= nodeId.size())
                return 1;

            int index = 1;
            try
            {
                index = std::max(std::stoi(nodeId.substr(separator + 1)), 1);
            }
            catch (...)
            {
                index = 1;
            }

            if (index <= 2) return 2;
            if (index <= 4) return 3;
            if (index <= 6) return 7;
            if (index <= 8) return 10;
            if (index <= 10) return 13;
            return 17;
        }

        static bool SkillTreeViewNeedsRebuild(const UISkillTreeViewComponent& tree,
            const std::vector<SkillTreeVisualNode>& visualNodes)
        {
            if (tree.Nodes.size() != visualNodes.size())
                return true;

            for (size_t i = 0; i < visualNodes.size(); ++i)
            {
                if (tree.Nodes[i].Id != visualNodes[i].Id
                    || tree.Nodes[i].ParentId != visualNodes[i].ParentId)
                    return true;
            }
            return false;
        }

        static void PopulateSkillTreeViewNodes(UISkillTreeViewComponent& tree)
        {
            const auto& visualNodes = GetSkillTreeVisualNodes();
            tree.Nodes.clear();
            tree.Nodes.reserve(visualNodes.size());
            for (const auto& visualNode : visualNodes)
            {
                UISkillTreeNodeView node;
                node.Id = visualNode.Id;
                node.ParentId = visualNode.ParentId;
                node.Position = visualNode.Position;
                node.IconPath = SkillTreeIconPath(visualNode.Id);
                node.Branch = SkillTreeBranchName(visualNode.Id);
                node.UnlockChapter = SkillTreeUnlockChapter(visualNode.Id);
                tree.Nodes.push_back(node);
            }
        }

        static void HideLegacySkillTreeEntities(Scene* scene)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TagComponent, UIWidgetComponent>())
            {
                const auto& tag = registry.get<TagComponent>(e).Tag;
                if (tag.rfind("SkillTree_Node_", 0) == 0
                    || tag.rfind("SkillTree_Line_", 0) == 0)
                {
                    registry.get<UIWidgetComponent>(e).Visible = false;
                }
            }
        }

        static bool SyncSkillTreeView(Scene* scene)
        {
            Entity treeEntity = FindEntityByName(scene, "SkillTree_View");
            if (!treeEntity)
                treeEntity = FindEntityByName(scene, "SkillTree_NetworkPanel");
            if (!treeEntity || !treeEntity.HasComponent<UIWidgetComponent>())
                return false;

            auto& tree = treeEntity.HasComponent<UISkillTreeViewComponent>()
                ? treeEntity.GetComponent<UISkillTreeViewComponent>()
                : treeEntity.AddComponent<UISkillTreeViewComponent>();

            if (treeEntity.HasComponent<UIPanelComponent>())
                treeEntity.GetComponent<UIPanelComponent>().ClipChildren = true;

            const auto& visualNodes = GetSkillTreeVisualNodes();
            if (SkillTreeViewNeedsRebuild(tree, visualNodes))
                PopulateSkillTreeViewNodes(tree);

            auto& state = GameProgress::GetState();
            if (!tree.RuntimeDragging)
            {
                tree.Pan = { state.SkillTreePanX, state.SkillTreePanY };
                tree.ClampPan();
            }

            tree.SelectedNodeId = state.SelectedSkillNodeId;
            tree.CommandPrefix = tree.CommandPrefix.empty()
                ? "progression:select_skill_node:"
                : tree.CommandPrefix;

            for (auto& node : tree.Nodes)
            {
                node.IconPath = node.IconPath.empty() ? SkillTreeIconPath(node.Id) : node.IconPath;
                node.Branch = node.Branch.empty() ? SkillTreeBranchName(node.Id) : node.Branch;
                node.UnlockChapter = node.UnlockChapter <= 0 ? SkillTreeUnlockChapter(node.Id) : node.UnlockChapter;
                node.Learned = node.Id == "magic_sword_core"
                    || state.UnlockedSkills.find(node.Id) != state.UnlockedSkills.end();
                node.Available = node.UnlockChapter <= state.CurrentChapter;
                node.Locked = !node.Learned;
                node.Selected = node.Id == state.SelectedSkillNodeId;
            }

            HideLegacySkillTreeEntities(scene);
            return true;
        }

        static void UpdateEquipmentItems(Scene* scene)
        {
            const auto& state = GameProgress::GetState();
            Entity pager = EnsurePager(scene, "Equipment_Pager", 2);
            static constexpr std::array<const char*, 8> kBagEquipment = {
                "traveler_armor",
                "black_forest_armor",
                "beast_tooth_pendant",
                "novice_magic_ring",
                "wind_boots",
                "old_ward_charm",
                "training_blade",
                "angel_feather"
            };
            struct EquipmentSlotView
            {
                const char* SlotId;
                const char* IconTag;
                const char* ButtonTag;
                glm::vec2 FramePosition;
            };
            static constexpr std::array<EquipmentSlotView, 4> kSlots = {
                EquipmentSlotView{ "armor", "Equipment_SlotArmor", "Equipment_SlotArmor_Button", { 0.105f, 0.335f } },
                EquipmentSlotView{ "ring", "Equipment_SlotRing", "Equipment_SlotRing_Button", { 0.205f, 0.335f } },
                EquipmentSlotView{ "charm", "Equipment_SlotCharm", "Equipment_SlotCharm_Button", { 0.105f, 0.470f } },
                EquipmentSlotView{ "boots", "Equipment_SlotBoots", "Equipment_SlotBoots_Button", { 0.205f, 0.470f } }
            };
            const glm::vec2 frameSize = { 0.075f, 0.098f };
            const glm::vec2 iconSize = { 0.055f, 0.075f };
            const glm::vec2 origin = { 0.385f, 0.335f };
            const glm::vec2 step = { 0.105f, 0.135f };
            std::string hoveredEquipmentId;
            glm::vec2 hoveredPosition = { 0.0f, 0.0f };
            std::vector<std::string> bagEquipment;
            bagEquipment.reserve(kBagEquipment.size());
            for (const char* equipmentId : kBagEquipment)
            {
                if (GameProgress::IsEquipmentOwned(equipmentId)
                    && !GameProgress::IsEquipmentEquipped(equipmentId))
                {
                    bagEquipment.emplace_back(equipmentId);
                }
            }

            for (const auto& slot : kSlots)
            {
                const std::string equipmentId = GameProgress::GetEquippedEquipmentForSlot(slot.SlotId);
                const bool hasEquipment = !equipmentId.empty();
                SetWidgetTopLeft(scene, slot.ButtonTag, slot.FramePosition, frameSize);
                SetWidgetVisible(scene, slot.ButtonTag, true);
                SetButtonCommand(scene, slot.ButtonTag, std::string("progression:select_equipment_slot:") + slot.SlotId);
                SetWidgetVisible(scene, slot.IconTag, hasEquipment);
                if (hasEquipment)
                {
                    SetImageTexture(scene, slot.IconTag, GameProgress::GetEquipmentIconPath(equipmentId));
                    SetImageColor(scene, slot.IconTag,
                        equipmentId == state.SelectedEquipmentId
                            ? glm::vec4(1.0f, 0.95f, 0.68f, 1.0f)
                            : glm::vec4(1.0f));
                }

                if (hasEquipment && IsButtonHovered(scene, slot.ButtonTag))
                {
                    hoveredEquipmentId = equipmentId;
                    hoveredPosition = slot.FramePosition;
                }
            }

            for (int i = 1; i <= 8; ++i)
            {
                const int page = i <= 4 ? 1 : 2;
                const int slot = (i - 1) % 4;
                const glm::vec2 pos = { origin.x + static_cast<float>(slot % 2) * step.x,
                                        origin.y + static_cast<float>(slot / 2) * step.y };
                const std::string item = "Equipment_Item_" + std::to_string(i);
                const std::string frame = item + "_Frame";
                const std::string button = item + "_Button";
                const size_t itemIndex = static_cast<size_t>((page - 1) * 4 + slot);
                const bool hasEquipment = itemIndex < bagEquipment.size();
                const std::string equipmentId = hasEquipment ? bagEquipment[itemIndex] : std::string{};
                const bool selected = hasEquipment && state.SelectedEquipmentId == equipmentId;

                SetPageItem(scene, frame, pager, page);
                SetPageItem(scene, item, pager, page);
                SetPageItem(scene, button, pager, page);

                SetWidgetTopLeft(scene, frame, pos, frameSize);
                SetWidgetTopLeft(scene, item, pos + glm::vec2(0.010f, 0.011f), iconSize);
                SetWidgetTopLeft(scene, button, pos, frameSize);
                SetWidgetVisible(scene, frame, hasEquipment);
                SetWidgetVisible(scene, item, hasEquipment);
                SetWidgetVisible(scene, button, hasEquipment);
                SetButtonCommand(scene, button, hasEquipment
                    ? std::string("progression:select_equipment_") + equipmentId
                    : std::string{});
                if (hasEquipment)
                {
                    SetImageTexture(scene, item, GameProgress::GetEquipmentIconPath(equipmentId));
                    SetImageColor(scene, item, selected
                        ? glm::vec4(1.0f, 0.95f, 0.68f, 1.0f)
                        : glm::vec4(1.0f));
                }
                SetPanelColors(scene, frame,
                    selected ? glm::vec4(0.18f, 0.15f, 0.09f, 0.86f) : glm::vec4(0.025f, 0.03f, 0.035f, 0.78f),
                    selected ? glm::vec4(0.98f, 0.78f, 0.30f, 0.96f) : glm::vec4(0.58f, 0.48f, 0.31f, 0.78f));

                if (hasEquipment && IsButtonHovered(scene, button))
                {
                    hoveredEquipmentId = equipmentId;
                    hoveredPosition = pos;
                }
            }

            const bool pageOne = state.EquipmentPage == 1;
            SetButtonPalette(scene, "Equipment_Button_Page1",
                pageOne ? glm::vec4(0.80f, 0.58f, 0.22f, 0.94f) : glm::vec4(0.10f, 0.11f, 0.13f, 0.86f),
                glm::vec4(0.95f, 0.78f, 0.36f, 0.96f),
                glm::vec4(0.58f, 0.38f, 0.16f, 0.96f));
            SetButtonPalette(scene, "Equipment_Button_Page2",
                !pageOne ? glm::vec4(0.80f, 0.58f, 0.22f, 0.94f) : glm::vec4(0.10f, 0.11f, 0.13f, 0.86f),
                glm::vec4(0.95f, 0.78f, 0.36f, 0.96f),
                glm::vec4(0.58f, 0.38f, 0.16f, 0.96f));
            SetButtonCommand(scene, "Equipment_Button_Page1", "ui:pager:Equipment_Pager:page:1");
            SetButtonCommand(scene, "Equipment_Button_Page2", "ui:pager:Equipment_Pager:page:2");
            SetWidgetVisible(scene, "Equipment_PageSlider", false);

            SetText(scene, "Equipment_Button_Toggle", GameProgress::GetEquipmentToggleButtonText());
            SetButtonCommand(scene, "Equipment_Button_Toggle", "progression:toggle_selected_equipment");

            const bool showTooltip = !hoveredEquipmentId.empty();
            SetWidgetVisible(scene, "Equipment_TooltipPanel", showTooltip);
            SetWidgetVisible(scene, "Equipment_TooltipText", showTooltip);
            if (showTooltip)
            {
                const glm::vec2 tooltipSize = { 0.235f, 0.112f };
                glm::vec2 tooltipPosition = hoveredPosition + glm::vec2(frameSize.x + 0.012f, 0.0f);
                tooltipPosition.x = std::clamp(tooltipPosition.x, 0.055f, 0.915f - tooltipSize.x);
                tooltipPosition.y = std::clamp(tooltipPosition.y, 0.125f, 0.835f - tooltipSize.y);
                SetWidgetTopLeft(scene, "Equipment_TooltipPanel", tooltipPosition, tooltipSize);
                SetWidgetTopLeft(scene, "Equipment_TooltipText",
                    tooltipPosition + glm::vec2(0.012f, 0.010f),
                    tooltipSize - glm::vec2(0.024f, 0.020f));
                SetText(scene, "Equipment_TooltipText", GameProgress::BuildEquipmentTooltip(hoveredEquipmentId));
            }
        }

        static void UpdateHub(Scene* scene)
        {
            if (!HasEntity(scene, "Hub_Status"))
                return;

            SetText(scene, "Hub_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Hub_Status", GameProgress::BuildHubStatus());
            SetText(scene, "Hub_Button_Dungeon", GameProgress::GetDungeonButtonText());
            SetText(scene, "Hub_Button_Skill", GameProgress::GetSkillButtonText());
            SetText(scene, "Hub_Button_Equip", GameProgress::GetEquipmentButtonText());
        }

        static void UpdateResult(Scene* scene)
        {
            if (!HasEntity(scene, "Result_Stats"))
                return;

            const auto& state = GameProgress::GetState();
            SetText(scene, "Result_Title", GameProgress::BuildResultTitle());
            SetText(scene, "Result_Stats", GameProgress::BuildResultStats());
            SetText(scene, "Result_Rewards", state.LastDungeonResult.Valid
                ? "掉落奖励\n悬浮图标查看用途和背包数量。\n下一步: 回据点升级，或重刷继续练空连。"
                : "还没有掉落记录。\n完成副本后，这里会显示材料图标。");
            SetText(scene, "Result_GradeText", state.LastDungeonResult.Valid ? state.LastDungeonResult.Grade : "-");
            SetProgress(scene, "Result_EXPBar",
                static_cast<float>(state.Experience),
                static_cast<float>(state.ExperienceToNext));
            UpdateResultDrops(scene);
        }

        static void UpdateSkillTree(Scene* scene)
        {
            if (!HasEntity(scene, "SkillTree_Status"))
                return;

            const auto& state = GameProgress::GetState();
            SetText(scene, "SkillTree_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "SkillTree_Status", GameProgress::BuildSkillTreeStatusV2());
            SetText(scene, "SkillTree_Details", GameProgress::BuildSkillTreeDetailsV2());
            SetText(scene, "SkillTree_Materials", GameProgress::BuildSkillTreeMaterialsV2());
            SetText(scene, "SkillTree_Button_UpgradeMagicSword", GameProgress::GetMagicSwordUpgradeButtonTextV2());
            SetProgress(scene, "SkillTree_MagicSwordBar",
                static_cast<float>(state.MagicSwordLevel),
                2.0f);
            if (!SyncSkillTreeView(scene))
            {
                UpdateSkillTreeDrag(scene);
                UpdateSkillTreeCanvas(scene);
            }
        }

        static void UpdateEquipment(Scene* scene)
        {
            if (!HasEntity(scene, "Equipment_Status"))
                return;

            EnsureEquipmentReusableLayout(scene);
            SyncEquipmentPager(scene);
            const auto& state = GameProgress::GetState();
            SetText(scene, "Equipment_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Equipment_Status", GameProgress::BuildEquipmentStatus());
            SetText(scene, "Equipment_Details", GameProgress::BuildEquipmentDetails());
            SetText(scene, "Equipment_PageText", GameProgress::BuildEquipmentPageText());
            SetText(scene, "Equipment_Materials", GameProgress::BuildEquipmentMaterials());
            SetText(scene, "Equipment_Button_UpgradeArmor", GameProgress::GetTravelerArmorUpgradeButtonText());
            SetProgress(scene, "Equipment_ArmorBar",
                static_cast<float>(state.TravelerArmorLevel),
                1.0f);
            UpdateEquipmentItems(scene);
        }

        static void UpdateDungeonSelect(Scene* scene)
        {
            if (!HasEntity(scene, "Dungeon_Status"))
                return;

            SetText(scene, "Dungeon_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Dungeon_Status", GameProgress::BuildDungeonSelectStatus());
            SetText(scene, "Dungeon_Rewards", GameProgress::BuildDungeonSelectRewards());
        }

        static void UpdateRelationship(Scene* scene)
        {
            if (!HasEntity(scene, "Relationship_Status"))
                return;

            SetText(scene, "Relationship_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Relationship_Status", GameProgress::BuildRelationshipStatus());
        }

        static void UpdateSupport(Scene* scene)
        {
            if (!HasEntity(scene, "Support_Status"))
                return;

            SetText(scene, "Support_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Support_Status", GameProgress::BuildSupportStatus());
        }

        static void UpdateSettings(Scene* scene)
        {
            if (!HasEntity(scene, "Settings_Status"))
                return;

            EnsureSettingsAudioControls(scene);
            SetText(scene, "Settings_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Settings_Status", GameProgress::BuildSettingsStatus());
            const auto& settings = GameProgress::GetState().Settings;
            SetSlider(scene, "Settings_TextSpeedSlider", static_cast<float>(settings.TextSpeed), 12.0f, 180.0f);
            SetSlider(scene, "Settings_MasterVolumeSlider", static_cast<float>(settings.MasterVolume), 0.0f, 100.0f);
            SetSlider(scene, "Settings_BGMVolumeSlider", static_cast<float>(settings.BGMVolume), 0.0f, 100.0f);
            SetSlider(scene, "Settings_SFXVolumeSlider", static_cast<float>(settings.SFXVolume), 0.0f, 100.0f);
        }

        static void UpdateSaveLoad(Scene* scene)
        {
            if (!HasEntity(scene, "SaveLoad_Status"))
                return;

            EnsureSaveLoadReusableLayout(scene);
            SetText(scene, "SaveLoad_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "SaveLoad_Status", GameProgress::BuildSaveLoadStatus());
            SetText(scene, "SaveLoad_Button_1", GameProgress::GetSaveButtonText(1));
            SetText(scene, "SaveLoad_Button_2", GameProgress::GetLoadButtonText(1));
        }

        static void UpdateProgressionPages(Scene* scene)
        {
            UpdateHub(scene);
            UpdateResult(scene);
            UpdateSkillTree(scene);
            UpdateEquipment(scene);
            UpdateDungeonSelect(scene);
            UpdateRelationship(scene);
            UpdateSupport(scene);
            UpdateSettings(scene);
            UpdateSaveLoad(scene);
        }

    } // namespace

    void ProgressionSystem::OnRuntimeStart(Scene* scene)
    {
        ResetSkillTreeCanvasCache();
        UpdateProgressionPages(scene);
    }

    void ProgressionSystem::OnUpdateRuntime(Scene* scene, Timestep)
    {
        UpdateProgressionPages(scene);
    }

} // namespace Wheatear
