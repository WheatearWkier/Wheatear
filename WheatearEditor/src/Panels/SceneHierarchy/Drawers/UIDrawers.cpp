#include "UIDrawers.h"
#include "../ComponentDrawers.h"
#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <cstring>
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    // ── UICanvas ──────────────────────────────────────────────────────────
    void DrawUICanvasComponent(Entity entity)
    {
        DrawComponent<UICanvasComponent>("UI Canvas", entity, [](auto& canvas)
            {
                ImGui::Checkbox("Visible", &canvas.Visible);
                ImGui::DragFloat("Ref Width", &canvas.ReferenceWidth, 1.0f, 1.0f, 7680.0f);
                ImGui::DragFloat("Ref Height", &canvas.ReferenceHeight, 1.0f, 1.0f, 4320.0f);
            });
    }

    // ── UIWidget ──────────────────────────────────────────────────────────
    void DrawUIWidgetComponent(Entity entity)
    {
        DrawComponent<UIWidgetComponent>("UI Widget", entity, [](auto& widget)
            {
                ImGui::Checkbox("Visible", &widget.Visible);

                ImGui::DragFloat2("Position", glm::value_ptr(widget.Position), 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat2("Size", glm::value_ptr(widget.Size), 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Rotation", &widget.Rotation, 0.5f);
                ImGui::DragInt("Sort Order", &widget.SortOrder);

                static const char* anchorLabels[] = {
                    "Top Left",    "Top Center",    "Top Right",
                    "Middle Left", "Middle Center", "Middle Right",
                    "Bottom Left", "Bottom Center", "Bottom Right"
                };
                int anchorIndex = (int)widget.Anchor;
                if (ImGui::Combo("Anchor", &anchorIndex, anchorLabels, 9))
                    widget.Anchor = (UIAnchor)anchorIndex;
            });
    }

    // ── UIImage ───────────────────────────────────────────────────────────
    void DrawUIImageComponent(Entity entity)
    {
        DrawComponent<UIImageComponent>("UI Image", entity, [](auto& image)
            {
                ImGui::ColorEdit4("Color", glm::value_ptr(image.Color));

                // 图标预览 + 拖放区域（与 SpriteRenderer 保持一致）
                const ImVec2      buttonSize = { 80.0f, 80.0f };
                const ImTextureID textureID = image.Texture
                    ? reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(image.Texture->GetRendererID()))
                    : reinterpret_cast<ImTextureID>(0);

                ImGui::PushID(&image);
                ImGui::ImageButton(textureID, buttonSize);

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
            });
    }

    // ── UIText ────────────────────────────────────────────────────────────
    void DrawUITextComponent(Entity entity)
    {
        DrawComponent<UITextComponent>("UI Text", entity, [entity](auto& text)
            {
                // BUG FIX: 原来用 static buffer，多个 UIText 共享缓冲，
                // 导致输入焦点混乱。改为局部 buffer + entity PushID。
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));
                strncpy_s(buffer, sizeof(buffer), text.Text.c_str(), _TRUNCATE);

                ImGui::PushID((int)(uint32_t)entity);
                if (ImGui::InputTextMultiline("Text", buffer, sizeof(buffer),
                    ImVec2(-1.0f, ImGui::GetTextLineHeight() * 3)))
                    text.Text = buffer;
                ImGui::PopID();

                ImGui::ColorEdit4("Color", glm::value_ptr(text.Color));
                ImGui::DragFloat("Font Size", &text.FontSize, 0.5f, 1.0f, 256.0f);

                char fontBuffer[260];
                memset(fontBuffer, 0, sizeof(fontBuffer));
                strncpy_s(fontBuffer, sizeof(fontBuffer), text.FontPath.c_str(), _TRUNCATE);
                if (ImGui::InputText("Font Path", fontBuffer, sizeof(fontBuffer)))
                    text.FontPath = fontBuffer;

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

    // ── UIButton ──────────────────────────────────────────────────────────
    void DrawUIButtonComponent(Entity entity)
    {
        DrawComponent<UIButtonComponent>("UI Button", entity, [entity](auto& button)
            {
                ImGui::ColorEdit4("Normal Color", glm::value_ptr(button.NormalColor));
                ImGui::ColorEdit4("Hover Color", glm::value_ptr(button.HoverColor));
                ImGui::ColorEdit4("Pressed Color", glm::value_ptr(button.PressedColor));

                // BUG FIX: 同上，改为局部 buffer + entity PushID
                char funcBuffer[64];
                memset(funcBuffer, 0, sizeof(funcBuffer));
                strncpy_s(funcBuffer, sizeof(funcBuffer), button.OnClickFunction.c_str(), _TRUNCATE);

                ImGui::PushID((int)(uint32_t)entity);
                if (ImGui::InputText("On Click", funcBuffer, sizeof(funcBuffer)))
                    button.OnClickFunction = funcBuffer;
                ImGui::PopID();

                // 只读运行时状态
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

    // ── UIProgressBar ─────────────────────────────────────────────────────
    void DrawUIProgressBarComponent(Entity entity)
    {
        DrawComponent<UIProgressBarComponent>("UI Progress Bar", entity, [](auto& bar)
            {
                ImGui::DragFloat("Value", &bar.Value, 0.1f, 0.0f, bar.MaxValue);
                ImGui::DragFloat("Max Value", &bar.MaxValue, 0.1f, 0.1f, 99999.0f);

                // 预览进度条
                float normalized = bar.GetNormalized();
                char overlay[32];
                sprintf_s(overlay, "%.0f / %.0f", bar.Value, bar.MaxValue);
                ImGui::ProgressBar(normalized, ImVec2(-1.0f, 0.0f), overlay);

                ImGui::ColorEdit4("Foreground", glm::value_ptr(bar.ForegroundColor));
                ImGui::ColorEdit4("Background", glm::value_ptr(bar.BackgroundColor));
            });
    }

    // -- UIPanel -----------------------------------------------------------
    void DrawUIPanelComponent(Entity entity)
    {
        DrawComponent<UIPanelComponent>("UI Panel", entity, [](auto& panel)
            {
                ImGui::ColorEdit4("Background", glm::value_ptr(panel.BackgroundColor));
                ImGui::ColorEdit4("Border", glm::value_ptr(panel.BorderColor));
                ImGui::DragFloat("Border Thickness", &panel.BorderThickness, 0.1f, 0.0f, 12.0f);
            });
    }

    // -- UISlider ----------------------------------------------------------
    void DrawUISliderComponent(Entity entity)
    {
        DrawComponent<UISliderComponent>("UI Slider", entity, [entity](auto& slider)
            {
                ImGui::DragFloat("Min", &slider.MinValue, 0.01f);
                ImGui::DragFloat("Max", &slider.MaxValue, 0.01f);
                if (slider.MaxValue < slider.MinValue)
                    slider.MaxValue = slider.MinValue;
                ImGui::SliderFloat("Value", &slider.Value, slider.MinValue, slider.MaxValue);

                ImGui::ColorEdit4("Track", glm::value_ptr(slider.TrackColor));
                ImGui::ColorEdit4("Fill", glm::value_ptr(slider.FillColor));
                ImGui::ColorEdit4("Handle", glm::value_ptr(slider.HandleColor));
                ImGui::ColorEdit4("Hover", glm::value_ptr(slider.HoverColor));

                char funcBuffer[96];
                memset(funcBuffer, 0, sizeof(funcBuffer));
                strncpy_s(funcBuffer, sizeof(funcBuffer), slider.OnValueChangedFunction.c_str(), _TRUNCATE);

                ImGui::PushID((int)(uint32_t)entity);
                if (ImGui::InputText("On Value Changed", funcBuffer, sizeof(funcBuffer)))
                    slider.OnValueChangedFunction = funcBuffer;
                ImGui::PopID();

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

    // -- UICheckbox --------------------------------------------------------
    void DrawUICheckboxComponent(Entity entity)
    {
        DrawComponent<UICheckboxComponent>("UI Checkbox", entity, [entity](auto& checkbox)
            {
                ImGui::Checkbox("Checked", &checkbox.Checked);
                ImGui::ColorEdit4("Box", glm::value_ptr(checkbox.BoxColor));
                ImGui::ColorEdit4("Check", glm::value_ptr(checkbox.CheckColor));
                ImGui::ColorEdit4("Hover", glm::value_ptr(checkbox.HoverColor));
                ImGui::ColorEdit4("Pressed", glm::value_ptr(checkbox.PressedColor));

                char funcBuffer[96];
                memset(funcBuffer, 0, sizeof(funcBuffer));
                strncpy_s(funcBuffer, sizeof(funcBuffer), checkbox.OnValueChangedFunction.c_str(), _TRUNCATE);

                ImGui::PushID((int)(uint32_t)entity);
                if (ImGui::InputText("On Value Changed", funcBuffer, sizeof(funcBuffer)))
                    checkbox.OnValueChangedFunction = funcBuffer;
                ImGui::PopID();

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
