#include "SpriteRendererDrawer.h"

#include "../ComponentDrawers.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Renderer/Texture.h"

namespace Wheatear {

    void DrawSpriteRendererComponent(Entity entity)
    {
        DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& c)
            {
                ImGui::ColorEdit4("Color", glm::value_ptr(c.Color));

                const ImVec2      buttonSize = { 100.0f, 100.0f };
                const ImTextureID textureID = c.Texture
                    ? reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(c.Texture->GetRendererID()))
                    : reinterpret_cast<ImTextureID>(0);

                // 1. 处理 X 轴翻转
                float uv0x = c.FlipX ? 1.0f : 0.0f;
                float uv1x = c.FlipX ? 0.0f : 1.0f;

                // 2. 处理 Y 轴翻转 (核心：由于 OpenGL 默认倒置，基础映射是 1 -> 0)
                // 如果以后在组件加了 FlipY 字段，把下面的 false 换成 c.FlipY
                bool isFlippedY = false;
                float uv0y = isFlippedY ? 0.0f : 1.0f; // 默认 1.0 (顶部)
                float uv1y = isFlippedY ? 1.0f : 0.0f; // 默认 0.0 (底部)

                ImGui::PushID(&c);
                // 注意图片的垂直翻转
                ImGui::ImageButton(textureID, buttonSize, ImVec2(uv0x, uv0y), ImVec2(uv1x, uv1y));

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
            });
    }

} // namespace Wheatear