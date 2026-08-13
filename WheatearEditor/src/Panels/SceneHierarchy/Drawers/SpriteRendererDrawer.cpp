#include "SpriteRendererDrawer.h"

#include "../ComponentDrawers.h"
#include "Panels/SpriteSheetPickerPanel.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#include "Wheatear/Core/AssetPath.h"
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
                ImGui::Checkbox("Flip X", &c.FlipX);
                // ImGui::Checkbox("Flip Y", &c.FlipY);
                ImGui::DragFloat("Tiling Factor", &c.TilingFactor, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat2("Draw Offset", glm::value_ptr(c.DrawOffset), 0.01f);
                ImGui::DragFloat2("Draw Scale", glm::value_ptr(c.DrawScale), 0.01f);
                if (ImGui::Button("Open Sprite Sheet Picker"))
                    SpriteSheetPickerPanel::RequestOpen(entity);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Pick a cell from an atlas or generate sprite animation frames.");
                if (ImGui::TreeNode("Advanced UV"))
                {
                    ImGui::DragFloat2("UV Min", glm::value_ptr(c.UVMin), 0.001f, 0.0f, 1.0f, "%.3f");
                    ImGui::DragFloat2("UV Max", glm::value_ptr(c.UVMax), 0.001f, 0.0f, 1.0f, "%.3f");
                    ImGui::TreePop();
                }
            });
    }

} // namespace Wheatear
