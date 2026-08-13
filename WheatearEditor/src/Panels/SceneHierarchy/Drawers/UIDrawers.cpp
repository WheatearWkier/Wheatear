#include "wepch.h"
#include "UIDrawers.h"
#include "../ComponentDrawers.h"
#include "Editor/CommandBuilder.h"
#include "Editor/EditorCanvasTools.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Panels/SpriteSheetPickerPanel.h"
#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/UI/UIWidgetLayout.h"

namespace Wheatear {

    namespace {

        static const char* AnchorLabel(UIAnchor anchor)
        {
            switch (anchor)
            {
            case UIAnchor::TopLeft: return "Top Left";
            case UIAnchor::TopCenter: return "Top Center";
            case UIAnchor::TopRight: return "Top Right";
            case UIAnchor::MiddleLeft: return "Middle Left";
            case UIAnchor::MiddleCenter: return "Middle Center";
            case UIAnchor::MiddleRight: return "Middle Right";
            case UIAnchor::BottomLeft: return "Bottom Left";
            case UIAnchor::BottomCenter: return "Bottom Center";
            case UIAnchor::BottomRight: return "Bottom Right";
            }
            return "Unknown";
        }

        static UIAnchor AnchorFromGridIndex(int index)
        {
            static const UIAnchor anchors[] = {
                UIAnchor::TopLeft, UIAnchor::TopCenter, UIAnchor::TopRight,
                UIAnchor::MiddleLeft, UIAnchor::MiddleCenter, UIAnchor::MiddleRight,
                UIAnchor::BottomLeft, UIAnchor::BottomCenter, UIAnchor::BottomRight
            };
            return anchors[std::clamp(index, 0, 8)];
        }

        static int AnchorToGridIndex(UIAnchor anchor)
        {
            switch (anchor)
            {
            case UIAnchor::TopLeft: return 0;
            case UIAnchor::TopCenter: return 1;
            case UIAnchor::TopRight: return 2;
            case UIAnchor::MiddleLeft: return 3;
            case UIAnchor::MiddleCenter: return 4;
            case UIAnchor::MiddleRight: return 5;
            case UIAnchor::BottomLeft: return 6;
            case UIAnchor::BottomCenter: return 7;
            case UIAnchor::BottomRight: return 8;
            }
            return 4;
        }

        static void DrawAnchorGrid(UIAnchor& anchor)
        {
            ImGui::TextUnformatted(EditorLocale::Text("Anchor", "锚点"));
            ImGui::SameLine();
            ImGui::TextDisabled("%s", AnchorLabel(anchor));

            ImGui::PushID("AnchorGrid");
            const int selected = AnchorToGridIndex(anchor);
            const ImVec2 buttonSize = { 28.0f, 24.0f };
            for (int i = 0; i < 9; ++i)
            {
                if (i % 3 != 0)
                    ImGui::SameLine(0.0f, 4.0f);

                const bool active = i == selected;
                if (active)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.58f, 0.62f, 1.0f));

                const char* label = i == 4 ? "C" : ".";
                if (ImGui::Button(label, buttonSize))
                    anchor = AnchorFromGridIndex(i);

                if (active)
                    ImGui::PopStyleColor();
            }
            ImGui::PopID();
        }

        static void DrawUIWidgetPresets(UIWidgetComponent& widget)
        {
            ImGui::TextUnformatted(EditorLocale::Text("Layout Presets", "布局预设"));
            if (ImGui::Button(EditorLocale::Text("Center Panel", "居中面板")))
            {
                widget.Anchor = UIAnchor::MiddleCenter;
                widget.Position = { 0.5f, 0.5f };
                widget.Size = { 0.42f, 0.28f };
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Full Screen", "全屏")))
            {
                widget.Anchor = UIAnchor::TopLeft;
                widget.Position = { 0.0f, 0.0f };
                widget.Size = { 1.0f, 1.0f };
            }

            if (ImGui::Button(EditorLocale::Text("Top Bar", "顶部栏")))
            {
                widget.Anchor = UIAnchor::TopLeft;
                widget.Position = { 0.0f, 0.0f };
                widget.Size = { 1.0f, 0.12f };
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Bottom Bar", "底部栏")))
            {
                widget.Anchor = UIAnchor::BottomLeft;
                widget.Position = { 0.0f, 1.0f };
                widget.Size = { 1.0f, 0.14f };
            }

            if (ImGui::Button(EditorLocale::Text("Left Panel", "左面板")))
            {
                widget.Anchor = UIAnchor::TopLeft;
                widget.Position = { 0.0f, 0.0f };
                widget.Size = { 0.28f, 1.0f };
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::Text("Right Panel", "右面板")))
            {
                widget.Anchor = UIAnchor::TopRight;
                widget.Position = { 1.0f, 0.0f };
                widget.Size = { 0.28f, 1.0f };
            }
        }

        static void SectionLabel(const char* label)
        {
            ImGui::Separator();
            ImGui::TextDisabled("%s", label);
        }

        static std::string EntityReferenceLabel(Scene* scene, entt::entity entity)
        {
            if (!scene || entity == entt::null)
                return "<None>";

            auto& registry = scene->GetRegistry();
            if (!registry.valid(entity))
                return "<Missing>";

            std::string name = registry.all_of<TagComponent>(entity)
                ? registry.get<TagComponent>(entity).Tag
                : std::string("Entity");

            if (registry.all_of<IDComponent>(entity))
                name += "  [" + std::to_string(static_cast<uint64_t>(registry.get<IDComponent>(entity).ID)) + "]";
            return name;
        }

        static void DrawUIReferenceCombo(Entity owner,
            const char* label,
            UUID& targetID,
            bool requirePager,
            bool allowSelf)
        {
            Scene* scene = owner.GetScene();
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            UIWidgetLayout::Context layout(scene);
            const entt::entity current = layout.ResolveReference(targetID);
            std::string preview = EntityReferenceLabel(scene, current);

            if (ImGui::BeginCombo(label, preview.c_str()))
            {
                const bool noneSelected = current == entt::null
                    && static_cast<uint64_t>(targetID) == 0;
                if (ImGui::Selectable("<None>", noneSelected))
                    targetID = 0;

                for (auto candidate : registry.view<IDComponent, TagComponent, UIWidgetComponent>())
                {
                    if (!allowSelf && candidate == static_cast<entt::entity>(owner))
                        continue;
                    if (requirePager && !registry.all_of<UIPagerComponent>(candidate))
                        continue;

                    const auto& id = registry.get<IDComponent>(candidate).ID;
                    const bool selected = candidate == current;
                    const std::string displayLabel = EntityReferenceLabel(scene, candidate);
                    const std::string itemLabel = EditorWidgets::LabelWithId(
                        displayLabel,
                        "ui_reference:" + std::to_string(static_cast<uint64_t>(id)));
                    if (ImGui::Selectable(itemLabel.c_str(), selected))
                        targetID = id;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
        }

    } // namespace

    void DrawUICanvasComponent(Entity entity)
    {
        DrawComponent<UICanvasComponent>("UI Canvas", entity, [entity](auto& canvas)
            {
                ImGui::Checkbox(EditorLocale::Text("Visible", "可见"), &canvas.Visible);
                ImGui::DragFloat(EditorLocale::Text("Ref Width", "参考宽度"), &canvas.ReferenceWidth, 1.0f, 1.0f, 7680.0f);
                ImGui::DragFloat("Ref Height", &canvas.ReferenceHeight, 1.0f, 1.0f, 4320.0f);
                EditorCanvasTools::DrawCanvasInspectorTools(entity);
            });
    }

    void DrawUIWidgetComponent(Entity entity)
    {
        DrawComponent<UIWidgetComponent>("UI Widget", entity, [entity](auto& widget)
            {
                ImGui::Checkbox(EditorLocale::Text("Visible At Runtime", "运行时可见"), &widget.Visible);
                ImGui::Checkbox(EditorLocale::Text("Show In Editor", "编辑器中显示"), &widget.EditorVisible);
                if (ImGui::Button("Show In Editor##UIWidgetShowInEditor"))
                    widget.EditorVisible = true;
                ImGui::SameLine();
                if (ImGui::Button("Hide In Editor##UIWidgetHideInEditor"))
                    widget.EditorVisible = false;

                SectionLabel("Rect");
                ImGui::TextDisabled("Normalized screen-space values. Drag the widget in Viewport for rough layout.");
                ImGui::DragFloat2(EditorLocale::Text("Position", "位置"), glm::value_ptr(widget.Position), 0.001f, -2.0f, 2.0f, "%.3f");
                ImGui::DragFloat2(EditorLocale::Text("Size", "大小"), glm::value_ptr(widget.Size), 0.001f, 0.001f, 2.0f, "%.3f");
                widget.Size.x = std::max(widget.Size.x, 0.001f);
                widget.Size.y = std::max(widget.Size.y, 0.001f);

                if (ImGui::Button(EditorLocale::Text("Nudge Left", "微调左移"))) widget.Position.x -= 0.001f;
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Right", "右"))) widget.Position.x += 0.001f;
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Up", "上"))) widget.Position.y -= 0.001f;
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Down", "下"))) widget.Position.y += 0.001f;

                ImGui::DragFloat(EditorLocale::Text("Rotation", "旋转"), &widget.Rotation, 0.5f);

                SectionLabel("Layering");
                ImGui::DragInt(EditorLocale::Text("Sort Order", "排序"), &widget.SortOrder);
                if (ImGui::Button(EditorLocale::Text("Send Back", "置底"))) widget.SortOrder -= 10;
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Bring Front", "置顶"))) widget.SortOrder += 10;

                SectionLabel("Hierarchy");
                DrawUIReferenceCombo(entity, "Parent", widget.ParentEntity, false, false);

                DrawAnchorGrid(widget.Anchor);

                SectionLabel("Presets");
                DrawUIWidgetPresets(widget);
            });
    }

    // -- UIAnimator -------------------------------------------------------
    void DrawUIAnimatorComponent(Entity entity)
    {
        DrawComponent<UIAnimatorComponent>("UI Animator", entity, [](auto& animator)
            {
                static const char* presets[] = {
                    "fade_in",
                    "slide_fade_in",
                    "result_pop",
                    "pulse",
                    "hover_pulse"
                };

                int currentPreset = 0;
                for (int i = 0; i < IM_ARRAYSIZE(presets); ++i)
                {
                    if (animator.Preset == presets[i])
                    {
                        currentPreset = i;
                        break;
                    }
                }
                if (ImGui::Combo(EditorLocale::Text("Preset", "预设"), &currentPreset, presets, IM_ARRAYSIZE(presets)))
                    animator.Preset = presets[currentPreset];

                ImGui::Checkbox("Play On Start", &animator.PlayOnStart);
                ImGui::Checkbox("Loop", &animator.Loop);
                ImGui::DragFloat(EditorLocale::Text("Delay", "延迟"), &animator.Delay, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Duration", &animator.Duration, 0.01f, 0.01f, 10.0f);
                ImGui::DragFloat(EditorLocale::Text("Amplitude", "振幅"), &animator.Amplitude, 0.001f, 0.0f, 0.5f);
                ImGui::DragFloat(EditorLocale::Text("Speed", "速度"), &animator.Speed, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat2(EditorLocale::Text("From Offset", "起始偏移"), glm::value_ptr(animator.FromOffset), 0.001f, -1.0f, 1.0f);

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                bool initialized = animator.RuntimeInitialized;
                float runtimeTime = animator.RuntimeTime;
                ImGui::Checkbox(EditorLocale::Text("Runtime Initialized", "运行时已初始化"), &initialized);
                ImGui::DragFloat(EditorLocale::Text("Runtime Time", "运行时时间"), &runtimeTime, 0.01f);
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            });
    }

    void DrawUIImageComponent(Entity entity)
    {
        DrawComponent<UIImageComponent>("UI Image", entity, [entity](auto& image)
            {
                ImGui::ColorEdit4(EditorLocale::Text("Color", "颜色"), glm::value_ptr(image.Color));

                ImVec2 buttonSize = { 80.0f, 80.0f };
                const ImTextureID textureID = image.Texture
                    ? static_cast<ImTextureID>(static_cast<uintptr_t>(image.Texture->GetRendererID()))
                    : static_cast<ImTextureID>(0);
                if (image.Texture)
                {
                    const float regionWidth = std::max(1.0f,
                        std::abs(image.UVMax.x - image.UVMin.x) * static_cast<float>(image.Texture->GetWidth()));
                    const float regionHeight = std::max(1.0f,
                        std::abs(image.UVMax.y - image.UVMin.y) * static_cast<float>(image.Texture->GetHeight()));
                    const float aspect = regionWidth / regionHeight;
                    constexpr float maxPreviewSide = 160.0f;
                    constexpr float minPreviewSide = 28.0f;
                    if (aspect >= 1.0f)
                        buttonSize = { maxPreviewSide, std::clamp(maxPreviewSide / aspect, minPreviewSide, maxPreviewSide) };
                    else
                        buttonSize = { std::clamp(maxPreviewSide * aspect, minPreviewSide, maxPreviewSide), maxPreviewSide };
                }

                ImGui::PushID(&image);
                ImGui::ImageButton("##UIImageTexture", textureID, buttonSize,
                    ImVec2(image.UVMin.x, image.UVMax.y),
                    ImVec2(image.UVMax.x, image.UVMin.y));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        const wchar_t* path = (const wchar_t*)payload->Data;
                        const std::filesystem::path texturePath =
                            AssetPath::ToProjectRelative(AssetPath::GetAssetRoot() / path);
                        image.Texture = Texture2D::Create(texturePath.generic_string());
                    }
                    ImGui::EndDragDropTarget();
                }

                if (!image.Texture)
                {
                    constexpr const char* hint = "Drop Texture";
                    const ImVec2 itemMin = ImGui::GetItemRectMin();
                    const ImVec2 textSize = ImGui::CalcTextSize(hint);
                    const ImVec2 textPos = {
                        itemMin.x + (buttonSize.x - textSize.x) * 0.5f,
                        itemMin.y + (buttonSize.y - textSize.y) * 0.5f
                    };
                    ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(200, 200, 200, 255), hint);
                }

                ImGui::PopID();

                if (image.Texture)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear"))
                        image.Texture = nullptr;
                }

                std::string texturePath = image.Texture ? image.Texture->GetPath() : std::string{};
                if (EditorWidgets::DrawAssetReferenceField(EditorLocale::Text("Texture", "纹理"),
                    texturePath,
                    EditorWidgets::AssetReferenceKind::Texture))
                {
                    image.Texture = texturePath.empty()
                        ? nullptr
                        : Texture2D::Create(texturePath);
                }

                if (ImGui::Button("Open Sprite Sheet Picker"))
                    SpriteSheetPickerPanel::RequestOpen(entity);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Pick a cell from an icon atlas for this UI image.");
                if (ImGui::TreeNode("Advanced UV"))
                {
                    ImGui::DragFloat2("UV Min", glm::value_ptr(image.UVMin), 0.001f, 0.0f, 1.0f, "%.3f");
                    ImGui::DragFloat2("UV Max", glm::value_ptr(image.UVMax), 0.001f, 0.0f, 1.0f, "%.3f");
                    ImGui::TreePop();
                }
            });
    }

    void DrawUITextComponent(Entity entity)
    {
        DrawComponent<UITextComponent>("UI Text", entity, [entity](auto& text)
            {
                ImGui::PushID((int)(uint32_t)entity);
                EditorWidgets::InputMultilineString(EditorLocale::Text("Text", "文本"),
                    text.Text,
                    ImVec2(-1.0f, ImGui::GetTextLineHeight() * 3),
                    1024);
                ImGui::PopID();

                ImGui::ColorEdit4(EditorLocale::Text("Color", "颜色"), glm::value_ptr(text.Color));
                ImGui::DragFloat("Font Size", &text.FontSize, 0.5f, 1.0f, 256.0f);
                ImGui::DragFloat4("Padding px", glm::value_ptr(text.Padding), 0.5f, -1.0f, 256.0f);
                ImGui::Checkbox("Auto Fit", &text.AutoFit);
                int horizontalAlign = static_cast<int>(text.HorizontalAlign);
                const char* horizontalItems[] = { "Left", "Center", EditorLocale::Text("Right", "右") };
                if (ImGui::Combo("Horizontal Align", &horizontalAlign, horizontalItems, IM_ARRAYSIZE(horizontalItems)))
                    text.HorizontalAlign = static_cast<UITextHorizontalAlign>(horizontalAlign);
                int verticalAlign = static_cast<int>(text.VerticalAlign);
                const char* verticalItems[] = { "Top", "Middle", "Bottom" };
                if (ImGui::Combo("Vertical Align", &verticalAlign, verticalItems, IM_ARRAYSIZE(verticalItems)))
                    text.VerticalAlign = static_cast<UITextVerticalAlign>(verticalAlign);
                ImGui::ColorEdit4("Shadow Color", glm::value_ptr(text.ShadowColor));
                ImGui::DragFloat2("Shadow Offset px", glm::value_ptr(text.ShadowOffset), 0.25f, -32.0f, 32.0f);
                ImGui::ColorEdit4("Outline Color", glm::value_ptr(text.OutlineColor));
                ImGui::DragFloat("Outline px", &text.OutlineThickness, 0.1f, 0.0f, 8.0f);

                EditorWidgets::DrawAssetReferenceField("Font",
                    text.FontPath,
                    EditorWidgets::AssetReferenceKind::Font,
                    260);

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        const wchar_t* path = static_cast<const wchar_t*>(payload->Data);
                        const std::filesystem::path assetPath = AssetPath::GetAssetRoot() / path;
                        const std::filesystem::path extension = assetPath.extension();
                        if (extension == ".ttf" || extension == ".otf" || extension == ".ttc")
                            text.FontPath = AssetPath::ToProjectRelative(assetPath).generic_string();
                    }
                    ImGui::EndDragDropTarget();
                }
            });
    }

    void DrawUIButtonComponent(Entity entity)
    {
        DrawComponent<UIButtonComponent>("UI Button", entity, [](auto& button)
            {
                ImGui::ColorEdit4("Normal Color", glm::value_ptr(button.NormalColor));
                ImGui::ColorEdit4("Hover Color", glm::value_ptr(button.HoverColor));
                ImGui::ColorEdit4("Pressed Color", glm::value_ptr(button.PressedColor));

                EditorCommandBuilder::DrawCommandBuilder(EditorLocale::Text("On Click", "点击时"), button.OnClickFunction, 256);

                char tooltipBuffer[128];
                memset(tooltipBuffer, 0, sizeof(tooltipBuffer));
                strncpy_s(tooltipBuffer, sizeof(tooltipBuffer), button.TooltipText.c_str(), _TRUNCATE);

                if (ImGui::InputText(EditorLocale::Text("Tooltip Text", "提示文本"), tooltipBuffer, sizeof(tooltipBuffer)))
                    button.TooltipText = tooltipBuffer;

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                bool hovered = button.IsHovered;
                bool pressed = button.IsPressed;
                ImGui::Checkbox("Is Hovered", &hovered);
                ImGui::SameLine();
                ImGui::Checkbox("Is Pressed", &pressed);
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            });
    }

    void DrawUIProgressBarComponent(Entity entity)
    {
        DrawComponent<UIProgressBarComponent>("UI Progress Bar", entity, [](auto& bar)
            {
                ImGui::DragFloat(EditorLocale::Text("Value", "值"), &bar.Value, 0.1f, 0.0f, bar.MaxValue);
                ImGui::DragFloat(EditorLocale::Text("Max Value", "最大值"), &bar.MaxValue, 0.1f, 0.1f, 99999.0f);

                float normalized = bar.GetNormalized();
                char overlay[32];
                sprintf_s(overlay, "%.0f / %.0f", bar.Value, bar.MaxValue);
                ImGui::ProgressBar(normalized, ImVec2(-1.0f, 0.0f), overlay);

                ImGui::ColorEdit4("Foreground", glm::value_ptr(bar.ForegroundColor));
                ImGui::ColorEdit4(EditorLocale::Text("Background", "背景"), glm::value_ptr(bar.BackgroundColor));
            });
    }

    void DrawUIRadialCooldownComponent(Entity entity)
    {
        DrawComponent<UIRadialCooldownComponent>("UI Radial Cooldown", entity, [](auto& cooldown)
            {
                ImGui::SliderFloat("Progress", &cooldown.Progress, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Start Angle", "起始角度"), &cooldown.StartAngle, 0.01f, -6.283185f, 6.283185f);
                ImGui::DragFloat(EditorLocale::Text("Thickness", "厚度"), &cooldown.Thickness, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Fade", &cooldown.Fade, 0.001f, 0.0f, 0.25f);
                ImGui::ColorEdit4(EditorLocale::Text("Color", "颜色"), glm::value_ptr(cooldown.Color));
            });
    }

    // -- UIPanel -----------------------------------------------------------
    void DrawUIPanelComponent(Entity entity)
    {
        DrawComponent<UIPanelComponent>("UI Panel", entity, [](auto& panel)
            {
                ImGui::ColorEdit4(EditorLocale::Text("Background", "背景"), glm::value_ptr(panel.BackgroundColor));
                ImGui::ColorEdit4("Border", glm::value_ptr(panel.BorderColor));
                ImGui::DragFloat(EditorLocale::Text("Border Thickness", "边框厚度"), &panel.BorderThickness, 0.1f, 0.0f, 12.0f);
                ImGui::Checkbox("Clip Children", &panel.ClipChildren);
                ImGui::Separator();
                ImGui::TextDisabled("Drag");
                ImGui::Checkbox("Draggable", &panel.Draggable);
                ImGui::BeginDisabled(!panel.Draggable);
                ImGui::Checkbox("Constrain To Parent", &panel.ConstrainDragToParent);
                ImGui::DragFloat("Handle Height", &panel.DragHandleHeight, 0.01f, 0.0f, 1.0f);
                ImGui::TextDisabled("0 = drag from the whole panel; otherwise top part of panel.");
                ImGui::EndDisabled();
            });
    }

    // -- UISlider ----------------------------------------------------------
    void DrawUISliderComponent(Entity entity)
    {
        DrawComponent<UISliderComponent>("UI Slider", entity, [](auto& slider)
            {
                ImGui::DragFloat("Min", &slider.MinValue, 0.01f);
                ImGui::DragFloat("Max", &slider.MaxValue, 0.01f);
                if (slider.MaxValue < slider.MinValue)
                    slider.MaxValue = slider.MinValue;
                ImGui::SliderFloat(EditorLocale::Text("Value", "值"), &slider.Value, slider.MinValue, slider.MaxValue);

                ImGui::ColorEdit4("Track", glm::value_ptr(slider.TrackColor));
                ImGui::ColorEdit4("Fill", glm::value_ptr(slider.FillColor));
                ImGui::ColorEdit4("Handle", glm::value_ptr(slider.HandleColor));
                ImGui::ColorEdit4("Hover", glm::value_ptr(slider.HoverColor));

                EditorCommandBuilder::DrawCommandBuilder(EditorLocale::Text("On Value Changed", "值变化时"), slider.OnValueChangedFunction, 256);

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                bool hovered = slider.IsHovered;
                bool dragging = slider.IsDragging;
                ImGui::Checkbox("Is Hovered", &hovered);
                ImGui::SameLine();
                ImGui::Checkbox("Is Dragging", &dragging);
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            });
    }

    // -- UIPager ----------------------------------------------------------
    void DrawUIPagerComponent(Entity entity)
    {
        DrawComponent<UIPagerComponent>("UI Pager", entity, [entity](auto& pager)
            {
                pager.PageCount = std::max(pager.PageCount, 1);
                pager.CurrentPage = std::clamp(pager.CurrentPage, 1, pager.PageCount);

                ImGui::DragInt(EditorLocale::Text("Current Page", "当前页"), &pager.CurrentPage, 1.0f, 1, pager.PageCount);
                ImGui::DragInt(EditorLocale::Text("Page Count", "页数"), &pager.PageCount, 1.0f, 1, 999);
                if (pager.PageCount < 1)
                    pager.PageCount = 1;
                pager.CurrentPage = std::clamp(pager.CurrentPage, 1, pager.PageCount);
                ImGui::Checkbox(EditorLocale::Text("Wrap", "循环翻页"), &pager.Wrap);

                const std::string tag = entity.HasComponent<TagComponent>()
                    ? entity.GetComponent<TagComponent>().Tag
                    : std::string{};
                ImGui::Separator();
                ImGui::TextDisabled("Button Commands");
                ImGui::TextDisabled("Next: ui:pager:%s:next", tag.c_str());
                ImGui::TextDisabled("Prev: ui:pager:%s:prev", tag.c_str());
                ImGui::TextDisabled("Page: ui:pager:%s:page:<number>", tag.c_str());
            });
    }

    // -- UIScrollView -----------------------------------------------------
    void DrawUIScrollViewComponent(Entity entity)
    {
        DrawComponent<UIScrollViewComponent>("UI Scroll View", entity, [entity](auto& scrollView) mutable
            {
                scrollView.ClampOffset();

                ImGui::DragFloat("Offset Y", &scrollView.OffsetY, 0.005f, 0.0f, std::max(scrollView.GetMaxOffset(), 0.0f), "%.3f");
                ImGui::DragFloat("Content Height", &scrollView.ContentHeight, 0.01f, 1.0f, 20.0f, "%.3f");
                ImGui::DragFloat("Wheel Step", &scrollView.WheelStep, 0.005f, 0.001f, 1.0f, "%.3f");
                ImGui::DragFloat("Scrollbar Width", &scrollView.ScrollbarWidth, 0.001f, 0.004f, 0.10f, "%.3f");
                ImGui::Checkbox("Enable Wheel", &scrollView.EnableWheel);
                ImGui::Checkbox(EditorLocale::Text("Show Scrollbar", "显示滚动条"), &scrollView.ShowScrollbar);
                ImGui::Checkbox("Drag Scrollbar", &scrollView.DragScrollbar);
                ImGui::Checkbox("Clamp To Content", &scrollView.ClampToContent);
                scrollView.ClampOffset();

                ImGui::ProgressBar(scrollView.GetNormalized(), ImVec2(-1.0f, 0.0f), "Scroll");
                ImGui::TextDisabled("Use with a UI Panel that has Clip Children enabled.");

                if (entity.HasComponent<UIPanelComponent>())
                {
                    auto& panel = entity.GetComponent<UIPanelComponent>();
                    if (ImGui::SmallButton(panel.ClipChildren ? "Clip Children Enabled" : "Enable Clip Children"))
                        panel.ClipChildren = true;
                }

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                bool hovered = scrollView.RuntimeThumbHovered;
                bool dragging = scrollView.RuntimeThumbDragging;
                ImGui::Checkbox("Thumb Hovered", &hovered);
                ImGui::SameLine();
                ImGui::Checkbox("Thumb Dragging", &dragging);
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            });
    }

    // -- UIPath -----------------------------------------------------------
    void DrawUIPathComponent(Entity entity)
    {
        DrawComponent<UIPathComponent>("UI Path", entity, [entity](auto& path) mutable
            {
                static const char* modes[] = { "Polyline", "Quadratic Bezier", "Cubic Bezier" };
                int mode = static_cast<int>(path.Mode);
                if (ImGui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes)))
                    path.Mode = static_cast<UIPathMode>(std::clamp(mode, 0, 2));

                ImGui::DragFloat(EditorLocale::Text("Thickness", "厚度"), &path.Thickness, 0.0005f, 0.001f, 0.05f, "%.4f");
                ImGui::DragInt("Segments", &path.Segments, 1.0f, 2, 96);
                ImGui::Checkbox(EditorLocale::Text("Closed", "闭合"), &path.Closed);
                ImGui::Checkbox("Draw Glow", &path.DrawGlow);
                ImGui::DragFloat("Glow Multiplier", &path.GlowThicknessMultiplier, 0.05f, 1.0f, 8.0f, "%.2f");
                ImGui::ColorEdit4(EditorLocale::Text("Color", "颜色"), glm::value_ptr(path.Color));
                ImGui::ColorEdit4("Glow Color", glm::value_ptr(path.GlowColor));

                ImGui::Separator();
                if (ImGui::SmallButton("Arc Preset"))
                {
                    path.Mode = UIPathMode::QuadraticBezier;
                    path.Points = {
                        { 0.10f, 0.62f },
                        { 0.50f, 0.18f },
                        { 0.90f, 0.62f }
                    };
                    path.Closed = false;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Circle-ish Preset"))
                {
                    path.Mode = UIPathMode::CubicBezier;
                    path.Points = {
                        { 0.50f, 0.08f },
                        { 0.92f, 0.08f },
                        { 0.92f, 0.92f },
                        { 0.50f, 0.92f },
                        { 0.08f, 0.92f },
                        { 0.08f, 0.08f },
                        { 0.50f, 0.08f }
                    };
                    path.Closed = false;
                }

                if (ImGui::TreeNode("Points"))
                {
                    for (size_t i = 0; i < path.Points.size(); ++i)
                    {
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::DragFloat2("##point", glm::value_ptr(path.Points[i]), 0.005f, -2.0f, 3.0f, "%.3f");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Remove") && path.Points.size() > 2)
                        {
                            path.Points.erase(path.Points.begin() + static_cast<std::ptrdiff_t>(i));
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }

                    if (ImGui::Button("Add Point"))
                        path.Points.push_back(path.Points.empty() ? glm::vec2(0.5f) : path.Points.back());
                    ImGui::TreePop();
                }

                ImGui::TextDisabled("Points are local to this widget rectangle. Use Panel Clip Children to mask overflow.");
            });
    }

    // -- UISkillTreeView --------------------------------------------------
    void DrawUISkillTreeViewComponent(Entity entity)
    {
        DrawComponent<UISkillTreeViewComponent>("UI Skill Tree View", entity, [entity](auto& tree) mutable
            {
                tree.ClampPan();

                ImGui::DragFloat2("Pan", glm::value_ptr(tree.Pan), 0.005f, -2.0f, 2.0f, "%.3f");
                ImGui::DragFloat2("Min Pan", glm::value_ptr(tree.MinPan), 0.005f, -2.0f, 2.0f, "%.3f");
                ImGui::DragFloat2("Max Pan", glm::value_ptr(tree.MaxPan), 0.005f, -2.0f, 2.0f, "%.3f");
                ImGui::DragFloat2(EditorLocale::Text("Node Size", "节点大小"), glm::value_ptr(tree.NodeSize), 0.001f, 0.01f, 0.30f, "%.3f");
                ImGui::DragFloat("Node Edge Inset", &tree.NodeEdgeInset, 0.001f, 0.0f, 0.20f, "%.3f");
                ImGui::DragFloat("Line Thickness", &tree.LineThickness, 0.0005f, 0.001f, 0.05f, "%.4f");
                ImGui::DragFloat("Curve Amount", &tree.CurveAmount, 0.001f, -0.30f, 0.30f, "%.3f");
                ImGui::DragFloat("Virtualization Margin", &tree.VirtualizationMargin, 0.005f, 0.0f, 0.50f, "%.3f");
                ImGui::DragInt("Line Segments", &tree.LineSegments, 1.0f, 2, 96);
                ImGui::DragInt("Background Rings", &tree.BackgroundRingCount, 1.0f, 0, 8);
                ImGui::Checkbox("Draw Line Glow", &tree.DrawLineGlow);

                char commandBuffer[128];
                memset(commandBuffer, 0, sizeof(commandBuffer));
                strncpy_s(commandBuffer, sizeof(commandBuffer), tree.CommandPrefix.c_str(), _TRUNCATE);
                ImGui::PushID((int)(uint32_t)entity);
                if (ImGui::InputText("Command Prefix", commandBuffer, sizeof(commandBuffer)))
                    tree.CommandPrefix = commandBuffer;
                ImGui::PopID();

                if (ImGui::TreeNode("Colors"))
                {
                    ImGui::ColorEdit4(EditorLocale::Text("Background", "背景"), glm::value_ptr(tree.BackgroundColor));
                    ImGui::ColorEdit4("Grid", glm::value_ptr(tree.GridColor));
                    ImGui::ColorEdit4("Line", glm::value_ptr(tree.LineColor));
                    ImGui::ColorEdit4("Active Line", glm::value_ptr(tree.ActiveLineColor));
                    ImGui::ColorEdit4("Line Glow", glm::value_ptr(tree.LineGlowColor));
                    ImGui::ColorEdit4("Node", glm::value_ptr(tree.NodeColor));
                    ImGui::ColorEdit4("Locked Node", glm::value_ptr(tree.LockedNodeColor));
                    ImGui::ColorEdit4("Hover Node", glm::value_ptr(tree.HoverNodeColor));
                    ImGui::ColorEdit4("Selected Node", glm::value_ptr(tree.SelectedNodeColor));
                    ImGui::ColorEdit4("Core Node", glm::value_ptr(tree.CoreNodeColor));
                    ImGui::ColorEdit4("Lock Overlay", glm::value_ptr(tree.LockColor));
                    ImGui::TreePop();
                }

                ImGui::Separator();
                ImGui::Text(EditorLocale::Text("Nodes: %d", "节点数: %d"), static_cast<int>(tree.Nodes.size()));
                ImGui::TextDisabled(EditorLocale::Text("Selected: %s", "选中: %s"), tree.SelectedNodeId.empty() ? "-" : tree.SelectedNodeId.c_str());
                ImGui::TextDisabled(EditorLocale::Text("Hovered: %s", "悬停: %s"), tree.RuntimeHoveredNodeId.empty() ? "-" : tree.RuntimeHoveredNodeId.c_str());

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                bool dragging = tree.RuntimeDragging;
                ImGui::Checkbox("Dragging", &dragging);
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();

                tree.ClampPan();
            });
    }

    // -- UIPageItem -------------------------------------------------------
    void DrawUIPageItemComponent(Entity entity)
    {
        DrawComponent<UIPageItemComponent>("UI Page Item", entity, [entity](auto& pageItem)
            {
                DrawUIReferenceCombo(entity, "Pager", pageItem.PagerEntity, true, false);

                ImGui::DragInt("Page", &pageItem.Page, 1.0f, 1, 999);
                if (pageItem.Page < 1)
                    pageItem.Page = 1;

                ImGui::TextDisabled("This widget is visible only while the pager is on this page.");
            });
    }

    // -- UICheckbox --------------------------------------------------------
    void DrawUICheckboxComponent(Entity entity)
    {
        DrawComponent<UICheckboxComponent>("UI Checkbox", entity, [](auto& checkbox)
            {
                ImGui::Checkbox(EditorLocale::Text("Checked", "已勾选"), &checkbox.Checked);
                ImGui::ColorEdit4("Box", glm::value_ptr(checkbox.BoxColor));
                ImGui::ColorEdit4("Check", glm::value_ptr(checkbox.CheckColor));
                ImGui::ColorEdit4("Hover", glm::value_ptr(checkbox.HoverColor));
                ImGui::ColorEdit4("Pressed", glm::value_ptr(checkbox.PressedColor));

                EditorCommandBuilder::DrawCommandBuilder(EditorLocale::Text("On Value Changed", "值变化时"), checkbox.OnValueChangedFunction, 256);

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                bool hovered = checkbox.IsHovered;
                bool pressed = checkbox.IsPressed;
                ImGui::Checkbox("Is Hovered", &hovered);
                ImGui::SameLine();
                ImGui::Checkbox("Is Pressed", &pressed);
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
            });
    }

} // namespace Wheatear
