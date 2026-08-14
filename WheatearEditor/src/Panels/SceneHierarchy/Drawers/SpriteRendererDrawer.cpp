#include "wepch.h"
#include "SpriteRendererDrawer.h"

#include "../ComponentDrawers.h"
#include "Editor/EditorLocale.h"
#include "Panels/SpriteSheetPickerPanel.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Renderer/Texture.h"

namespace Wheatear {

    void DrawSpriteRendererComponent(Entity entity)
    {
        DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [entity](auto& c)
            {
                ImGui::ColorEdit4("Color", glm::value_ptr(c.Color));

                const ImVec2      buttonSize = { 100.0f, 100.0f };
                const ImTextureID textureID = c.Texture
                    ? static_cast<ImTextureID>(static_cast<uintptr_t>(c.Texture->GetRendererID()))
                    : static_cast<ImTextureID>(0);

                const float uv0x = c.FlipX ? c.UVMax.x : c.UVMin.x;
                const float uv1x = c.FlipX ? c.UVMin.x : c.UVMax.x;
                const float uv0y = c.UVMax.y;
                const float uv1y = c.UVMin.y;

                ImGui::PushID(&c);
                ImGui::ImageButton("##SpriteTexture", textureID, buttonSize, ImVec2(uv0x, uv0y), ImVec2(uv1x, uv1y));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        const wchar_t* path = static_cast<const wchar_t*>(payload->Data);
                        const std::filesystem::path texturePath =
                            AssetPath::ToProjectRelative(AssetPath::GetAssetRoot() / path);
                        c.Texture = Texture2D::Create(texturePath.generic_string());
                    }
                    ImGui::EndDragDropTarget();
                }

                if (!c.Texture)
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
                ImGui::Checkbox(EditorLocale::Text("Flip X", "水平翻转"), &c.FlipX);
                // ImGui::Checkbox("Flip Y", &c.FlipY);
                ImGui::DragFloat(EditorLocale::Text("Tiling Factor", "平铺系数"), &c.TilingFactor, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat(EditorLocale::Text("Pixels Per Unit", "像素/单位"), &c.PixelsPerUnit, 1.0f, 0.0f, 1000.0f, "%.1f");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", EditorLocale::Text(
                        "0 = quad is 1x1 x scale (legacy). >0 renders at true content size (content pixels / PPU), keeps aspect ratio and sizes colliders driven by sheet cells.",
                        "0 = 旧行为（quad 为 1×1×缩放）。>0 时按真实内容尺寸渲染（内容像素 ÷ PPU），保持宽高比，也是格子碰撞框的换算基准。"));
                ImGui::DragFloat2(EditorLocale::Text("Draw Offset", "绘制偏移"), glm::value_ptr(c.DrawOffset), 0.01f);
                ImGui::DragFloat2(EditorLocale::Text("Draw Scale", "绘制缩放"), glm::value_ptr(c.DrawScale), 0.01f);
                if (ImGui::Button(EditorLocale::Text("Open Sprite Sheet Picker", "打开序列帧选择器")))
                    SpriteSheetPickerPanel::RequestOpen(entity);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Pick a cell from an atlas or generate sprite animation frames.");
                if (ImGui::TreeNode(EditorLocale::Text("Advanced UV", "高级 UV")))
                {
                    ImGui::DragFloat2(EditorLocale::Text("UV Min", "UV 最小"), glm::value_ptr(c.UVMin), 0.001f, 0.0f, 1.0f, "%.3f");
                    ImGui::DragFloat2(EditorLocale::Text("UV Max", "UV 最大"), glm::value_ptr(c.UVMax), 0.001f, 0.0f, 1.0f, "%.3f");
                    ImGui::TreePop();
                }
            });
    }

} // namespace Wheatear
