#include "wepch.h"
#include "Mesh3DDrawers.h"
#include "../ComponentDrawers.h"
#include "Editor/EditorLocale.h"
#include "Assets/AssetRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Assets/Import/ModelImporter.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Systems/TerrainSystem.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Renderer/Mesh.h"
#include "Wheatear/Renderer/Model.h"
#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Wheatear {

    // -------------------------------------------------------------------------
    // Mesh Renderer
    // -------------------------------------------------------------------------
    void DrawMeshRendererComponent(Entity entity)
    {
        DrawComponent<MeshRendererComponent>("Mesh Renderer", entity, [entity](auto& c)
            {
                std::string meshName = "None";
                if (c.Model)
                {
                    meshName = std::filesystem::path(c.Model->GetPath()).filename().string();
                }
                else if (c.Mesh)
                {
                    auto& path = c.Mesh->GetFilepath();
                    meshName = path.empty() ? "[Built-in]"
                        : std::filesystem::path(path).filename().string();
                }
                ImGui::Text(EditorLocale::Text("Mesh", "网格"));
                ImGui::SameLine();
                const std::string meshButtonLabel = meshName + "##MeshDropSlot";
                ImGui::Button(meshButtonLabel.c_str(), ImVec2(-1, 0));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                        std::filesystem::path fullPath =
                            AssetPath::GetAssetRoot() / wpath;
                        std::string extension = fullPath.extension().string();
                        std::transform(extension.begin(), extension.end(), extension.begin(),
                            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

                        const std::string relative =
                            AssetPath::ToProjectRelative(fullPath).generic_string();
                        if (extension == ".wtmodel")
                        {
                            c.Model = Model::Create(relative);
                            c.Mesh = nullptr;
                            c.Material = nullptr;
                            c.ModelAsset = AssetRegistry::Get().EnsureAsset(relative);
                            c.MaterialAsset = UUID(0);

                            Entity owner = entity;
                            if (c.Model && c.Model->HasSkin()
                                && !owner.HasComponent<Animator3DComponent>())
                            {
                                auto& animator =
                                    owner.AddComponent<Animator3DComponent>();
                                if (!c.Model->GetAnimations().empty())
                                    animator.DefaultClip =
                                        c.Model->GetAnimations().front().Name;
                            }
                        }
                        else if (extension == ".fbx" || extension == ".gltf" || extension == ".glb")
                        {
                            const EditorAssetMetadata* metadata =
                                AssetRegistry::Get().FindMutableByPath(relative);
                            if (metadata)
                            {
                                std::filesystem::path cookedPath = fullPath;
                                cookedPath.replace_extension(".wtmodel");
                                std::string cookError;
                                if (!ModelImporter::Cook(fullPath, cookedPath,
                                        metadata->Model, &cookError))
                                {
                                    WT_CORE_ERROR("Model cook failed for '{}': {}",
                                        relative, cookError);
                                }
                            }
                            else
                            {
                                Model::Cook(relative);
                            }
                            c.Model = Model::Create(relative);
                            c.Mesh = nullptr;
                            c.Material = nullptr;
                            c.ModelAsset = AssetRegistry::Get().EnsureAsset(relative);
                            c.MaterialAsset = UUID(0);

                            Entity owner = entity;
                            if (c.Model && c.Model->HasSkin()
                                && !owner.HasComponent<Animator3DComponent>())
                            {
                                auto& animator =
                                    owner.AddComponent<Animator3DComponent>();
                                if (!c.Model->GetAnimations().empty())
                                    animator.DefaultClip =
                                        c.Model->GetAnimations().front().Name;
                            }
                        }
                        else if (extension == ".obj")
                        {
                            c.Model = nullptr;
                            c.Mesh = Mesh::Create(relative);
                            c.ModelAsset = UUID(0);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (c.Model)
                {
                    ImGui::TextDisabled("%s", c.Model->GetPath().c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(EditorLocale::Text("Clear Model", "清除模型")))
                    {
                        c.Model = nullptr;
                        c.ModelAsset = UUID(0);
                    }
                    if (static_cast<uint64_t>(c.ModelAsset) != 0)
                    {
                        ImGui::TextDisabled("Asset UUID %llu",
                            static_cast<unsigned long long>(static_cast<uint64_t>(c.ModelAsset)));
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button(EditorLocale::Text("Cube", "立方体")))
                {
                    c.Model = nullptr;
                    c.ModelAsset = UUID(0);
                    c.Mesh = Mesh::CreateCube();
                }
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Sphere", "球体")))
                {
                    c.Model = nullptr;
                    c.ModelAsset = UUID(0);
                    c.Mesh = Mesh::CreateSphere();
                }
                ImGui::Separator();

                if (c.Model)
                {
                    ImGui::TextDisabled(EditorLocale::Text(
                        "Model materials are imported per submesh.",
                        "模型材质按子网格导入。"));
                    ImGui::Separator();
                    return;
                }

                ImGui::Text(EditorLocale::Text("Material", "材质"));
                ImGui::SameLine();

                std::string matName = c.Material && !c.Material->GetPath().empty()
                    ? std::filesystem::path(c.Material->GetPath()).filename().string()
                    : "[Unsaved Material]";

                const std::string materialButtonLabel = matName + "##MaterialDropSlot";
                ImGui::Button(materialButtonLabel.c_str(), ImVec2(-1, 0));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        const wchar_t* wpath = static_cast<const wchar_t*>(payload->Data);
                        std::filesystem::path fullPath =
                            AssetPath::GetAssetRoot() / wpath;
                        if (fullPath.extension() == AssetFileType::MaterialExtension)
                        {
                            const std::string relative =
                                AssetPath::ToProjectRelative(fullPath).generic_string();
                            c.Material = Material::Load(relative);
                            c.MaterialAsset = AssetRegistry::Get().EnsureAsset(relative);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::Button(EditorLocale::Text("New Material", "新建材质")))
                {
                    c.Material = Material::Create();
                    c.MaterialAsset = UUID(0);
                }

                ImGui::SameLine();

                if (ImGui::Button(EditorLocale::Text("Save Material", "保存材质")))
                {
                    if (c.Material)
                    {
                        if (c.Material->GetPath().empty())
                        {
                            std::string savePath = std::string("assets/materials/NewMaterial") + AssetFileType::MaterialExtension;
                            c.Material->Save(savePath);
                            c.MaterialAsset = AssetRegistry::Get().EnsureAsset(savePath);
                        }
                        else
                        {
                            c.Material->Save();
                            c.MaterialAsset = AssetRegistry::Get().EnsureAsset(c.Material->GetPath());
                        }
                    }
                }

                if (c.Material && ImGui::TreeNode("Edit Material"))
                {
                    auto& mat = *c.Material;

                    ImGui::ColorEdit4("Albedo", glm::value_ptr(mat.Albedo));
                    ImGui::DragFloat(EditorLocale::Text("Metallic", "金属度"), &mat.Metallic, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat(EditorLocale::Text("Roughness", "粗糙度"), &mat.Roughness, 0.01f, 0.0f, 1.0f);
                    ImGui::Checkbox(EditorLocale::Text("Flip Normals", "翻转法线"), &mat.FlipNormals);
                    ImGui::Separator();

                    // AlbedoMap
                    const ImVec2 thumbSize = { 64.0f, 64.0f };
                    auto drawTexSlot = [&](const char* label, Ref<Texture2D>& tex,
                        const char* dropText)
                        {
                            ImGui::Text("%s", label);
                            ImGui::SameLine();
                            const ImTextureID texID = tex
                                ? static_cast<ImTextureID>(static_cast<uintptr_t>(tex->GetRendererID()))
                                : static_cast<ImTextureID>(0);
                            ImGui::PushID(&tex);
                            ImGui::ImageButton("##MaterialTexture", texID, thumbSize, ImVec2(0, 1), ImVec2(1, 0));
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
                ImGui::DragFloat(EditorLocale::Text("Intensity", "强度"), &c.Intensity, 0.01f, 0.0f, 10.0f);
                ImGui::Spacing();
                ImGui::TextDisabled(EditorLocale::Text("Direction is controlled by Transform Rotation", "方向由 Transform 旋转控制"));
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
                ImGui::DragFloat(EditorLocale::Text("Intensity", "强度"), &c.Intensity, 0.01f, 0.0f, 10.0f);
                ImGui::Separator();
                ImGui::Text(EditorLocale::Text("Attenuation", "衰减"));
                ImGui::DragFloat(EditorLocale::Text("Constant", "常数"), &c.Constant, 0.001f, 0.001f, 2.0f);
                ImGui::DragFloat(EditorLocale::Text("Linear", "线性"), &c.Linear, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat(EditorLocale::Text("Quadratic", "二次"), &c.Quadratic, 0.001f, 0.0f, 1.0f);

                if (c.Constant > 0.0f)
                {
                    float a = c.Quadratic;
                    float b = c.Linear;
                    float cc = c.Constant - 20.0f * c.Intensity;
                    float disc = b * b - 4.0f * a * cc;
                    if (a > 0.0f && disc >= 0.0f)
                    {
                        float r = (-b + std::sqrt(disc)) / (2.0f * a);
                        ImGui::Spacing();
                        ImGui::TextDisabled(EditorLocale::Text("Effective radius: ~%.1f units", "有效半径: ~%.1f 单位"), r > 0 ? r : 0.0f);
                    }
                }
            });
    }

    void DrawTerrainComponent(Entity entity)
    {
        DrawComponent<TerrainComponent>("Terrain", entity, [entity](auto& c)
            {
                bool changed = false;
                changed |= ImGui::DragInt(EditorLocale::Text("Cells X", "X 网格"), &c.CellsX, 1.0f, 4, 512);
                changed |= ImGui::DragInt(EditorLocale::Text("Cells Z", "Z 网格"), &c.CellsZ, 1.0f, 4, 512);
                changed |= ImGui::DragFloat(EditorLocale::Text("Cell Size", "格子尺寸"), &c.CellSize, 0.05f, 0.25f, 8.0f);
                changed |= ImGui::DragFloat(EditorLocale::Text("Height Scale", "高度缩放"), &c.HeightScale, 0.01f, 0.0f, 8.0f);
                changed |= ImGui::InputScalar(EditorLocale::Text("Seed", "种子"), ImGuiDataType_U32, &c.Seed);

                if (changed)
                    c.Dirty = true;

                if (ImGui::Button(EditorLocale::Text("Regenerate Heightmap", "重新生成高度图")))
                {
                    c.GenerateProcedural();
                    c.Dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Rebuild Mesh", "重建网格")))
                {
                    if (entity.GetScene())
                        TerrainSystem::BuildTerrain(entity.GetScene(), c);
                }

                ImGui::TextDisabled(EditorLocale::Text(
                    "Vertices: %d x %d  Heights: %d",
                    "顶点: %d x %d  高度: %d"),
                    c.GetVertexCountX(), c.GetVertexCountZ(),
                    static_cast<int>(c.Heights.size()));
            });
    }

    void DrawNavObstacleComponent(Entity entity)
    {
        DrawComponent<NavObstacleComponent>("Nav Obstacle", entity, [](auto& c)
            {
                ImGui::Checkbox(EditorLocale::Text("Enabled", "启用"), &c.Enabled);
                ImGui::TextDisabled(EditorLocale::Text(
                    "Blocks nav-grid cells covered by this entity's mesh bounds.",
                    "用该实体的网格包围盒阻挡导航格子。"));
            });
    }

} // namespace Wheatear
