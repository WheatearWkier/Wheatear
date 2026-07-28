#include "Mesh3DDrawers.h"
#include "../ComponentDrawers.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Renderer/Mesh.h"
#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

namespace Wheatear {

    // -------------------------------------------------------------------------
    // Mesh Renderer
    // -------------------------------------------------------------------------
    void DrawMeshRendererComponent(Entity entity)
    {
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [](auto& c)
            {
                // ---- Mesh 部分不变 ----
                std::string meshName = "None";
                if (c.Mesh)
                {
                    auto& path = c.Mesh->GetFilepath();
                    meshName = path.empty() ? "[Built-in]"
                        : std::filesystem::path(path).filename().string();
                }
                ImGui::Text("Mesh");
                ImGui::SameLine();
                ImGui::Button(meshName.c_str(), ImVec2(-1, 0));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                        std::filesystem::path fullPath =
                            AssetPath::GetAssetRoot() / wpath;
                        if (fullPath.extension() == ".obj")
                            c.Mesh = Mesh::Create(AssetPath::ToProjectRelative(fullPath).generic_string());
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::Spacing();
                if (ImGui::Button("Cube"))   c.Mesh = Mesh::CreateCube();
                ImGui::SameLine();
                if (ImGui::Button("Sphere")) c.Mesh = Mesh::CreateSphere();
                ImGui::Separator();

                // ---- Material 部分 ----
                ImGui::Text("Material");
                ImGui::SameLine();

                std::string matName = c.Material && !c.Material->GetPath().empty()
                    ? std::filesystem::path(c.Material->GetPath()).filename().string()
                    : "[Unsaved Material]";

                ImGui::Button(matName.c_str(), ImVec2(-1, 0));

                // 拖拽 .wtmaterial 文件替换材质
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                        std::filesystem::path fullPath =
                            AssetPath::GetAssetRoot() / wpath;
                        if (fullPath.extension() == AssetFileType::MaterialExtension)
                            c.Material = Material::Load(AssetPath::ToProjectRelative(fullPath).generic_string());
                    }
                    ImGui::EndDragDropTarget();
                }

                // 新建材质 / 保存材质
                if (ImGui::Button("New Material"))
                    c.Material = Material::Create();

                ImGui::SameLine();

                if (ImGui::Button("Save Material"))
                {
                    if (c.Material)
                    {
                        if (c.Material->GetPath().empty())
                        {
                            // 还没有路径，弹出保存对话框
                            std::string savePath = std::string("assets/materials/NewMaterial") + AssetFileType::MaterialExtension;
                            c.Material->Save(savePath);
                        }
                        else
                        {
                            c.Material->Save();
                        }
                    }
                }

                // 材质内容编辑（折叠）
                if (c.Material && ImGui::TreeNode("Edit Material"))
                {
                    auto& mat = *c.Material;

                    ImGui::ColorEdit4("Albedo", glm::value_ptr(mat.Albedo));
                    ImGui::DragFloat("Metallic", &mat.Metallic, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Roughness", &mat.Roughness, 0.01f, 0.0f, 1.0f);
                    ImGui::Checkbox("Flip Normals", &mat.FlipNormals);
                    ImGui::Separator();

                    // 贴图拖拽，和之前一样，复用原来那套代码
                    // AlbedoMap
                    const ImVec2 thumbSize = { 64.0f, 64.0f };
                    auto drawTexSlot = [&](const char* label, Ref<Texture2D>& tex,
                        const char* dropText)
                        {
                            ImGui::Text("%s", label);
                            ImGui::SameLine();
                            const ImTextureID texID = tex
                                ? reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(tex->GetRendererID()))
                                : reinterpret_cast<ImTextureID>(0);
                            ImGui::PushID(&tex);
                            ImGui::ImageButton(texID, thumbSize, ImVec2(0, 1), ImVec2(1, 0));
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* payload =
                                    ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                                {
                                    const wchar_t* wpath =
                                        static_cast<const wchar_t*>(payload->Data);
                                    std::filesystem::path fp =
                                        AssetPath::GetAssetRoot() / wpath;
                                    auto ext = fp.extension();
                                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
                                        tex = Texture2D::Create(AssetPath::ToProjectRelative(fp).generic_string());
                                }
                                ImGui::EndDragDropTarget();
                            }
                            if (!tex)
                            {
                                const ImVec2 min = ImGui::GetItemRectMin();
                                const ImVec2 size = ImGui::CalcTextSize(dropText);
                                ImGui::GetWindowDrawList()->AddText(
                                    { min.x + (thumbSize.x - size.x) * 0.5f,
                                      min.y + (thumbSize.y - size.y) * 0.5f },
                                    IM_COL32(200, 200, 200, 255), dropText);
                            }
                            ImGui::SameLine();
                            std::string clearId = std::string("Clear##") + label;
                            if (ImGui::Button(clearId.c_str())) tex = nullptr;
                            ImGui::PopID();
                        };

                    drawTexSlot("Albedo Map", mat.AlbedoMap, "Drop Texture");
                    drawTexSlot("Normal Map", mat.NormalMap, "Drop Normal");
                    drawTexSlot("Roughness Map", mat.RoughnessMap, "Drop Roughness");
                    drawTexSlot("Metallic Map", mat.MetallicMap, "Drop Metallic");

                    ImGui::TreePop();
                }
            });
    }

    // -------------------------------------------------------------------------
    // Directional Light
    // -------------------------------------------------------------------------
    void DrawDirectionalLightComponent(Entity entity)
    {
        DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](auto& c)
            {
                ImGui::ColorEdit3("Color", glm::value_ptr(c.Color));
                ImGui::DragFloat("Intensity", &c.Intensity, 0.01f, 0.0f, 10.0f);
                ImGui::Spacing();
                ImGui::TextDisabled("Direction is controlled by Transform Rotation");
            });
    }

    // -------------------------------------------------------------------------
    // Point Light
    // -------------------------------------------------------------------------
    void DrawPointLightComponent(Entity entity)
    {
        DrawComponent<PointLightComponent>("Point Light", entity, [](auto& c)
            {
                ImGui::ColorEdit3("Color", glm::value_ptr(c.Color));
                ImGui::DragFloat("Intensity", &c.Intensity, 0.01f, 0.0f, 10.0f);
                ImGui::Separator();
                ImGui::Text("Attenuation");
                ImGui::DragFloat("Constant", &c.Constant, 0.001f, 0.001f, 2.0f);
                ImGui::DragFloat("Linear", &c.Linear, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &c.Quadratic, 0.001f, 0.0f, 1.0f);

                // 衰减预览：显示有效照射半径估算值
                // 解方程 att < 0.05 → 有效范围约为这个距离
                if (c.Constant > 0.0f)
                {
                    // 用二次公式估算亮度降到 5% 时的距离
                    float a = c.Quadratic;
                    float b = c.Linear;
                    float cc = c.Constant - 20.0f * c.Intensity; // 1/att = 20 时
                    float disc = b * b - 4.0f * a * cc;
                    if (a > 0.0f && disc >= 0.0f)
                    {
                        float r = (-b + std::sqrt(disc)) / (2.0f * a);
                        ImGui::Spacing();
                        ImGui::TextDisabled("Effective radius: ~%.1f units", r > 0 ? r : 0.0f);
                    }
                }
            });
    }

} // namespace Wheatear