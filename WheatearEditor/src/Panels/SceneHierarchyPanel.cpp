#include "wtpch.h"
#include "SceneHierarchyPanel.h"
#include "ContentBrowserPanel.h"

#include <imgui/imgui.h>
#include <cstring>
#include <filesystem>
#include <string>

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/SceneSerializer.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Editor/EditorComponentRegistry.h"

namespace Wheatear {

    SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
    {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
    {
        m_Context = context;
        m_SelectionContext = {};
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Scene Hierarchy");

        if (m_Context)
        {
            m_Context->m_Registry.each([&](auto entityID)
                {
                    DrawEntityNode(Entity{ entityID, m_Context.get() });
                });

            if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(0) && !ImGui::IsAnyItemHovered())
                m_SelectionContext = {};

            if (ImGui::BeginPopupContextWindow("##HierarchyCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::MenuItem("Create Empty Entity"))
                    m_SelectionContext = m_Context->CreateEntity("Empty Entity");

                ImGui::Separator();

                if (ImGui::BeginMenu("2D Object"))
                {
                    if (ImGui::MenuItem("Sprite"))
                    {
                        auto e = m_Context->CreateEntity("Sprite");
                        e.AddComponent<SpriteRendererComponent>();
                        m_SelectionContext = e;
                    }
                    if (ImGui::MenuItem("Circle"))
                    {
                        auto e = m_Context->CreateEntity("Circle");
                        e.AddComponent<CircleRendererComponent>();
                        m_SelectionContext = e;
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera"))
                {
                    if (ImGui::MenuItem("Camera"))
                    {
                        auto e = m_Context->CreateEntity("Camera");
                        e.AddComponent<CameraComponent>();
                        m_SelectionContext = e;
                    }
                    ImGui::EndMenu();
                }


                if (ImGui::BeginMenu("UI"))
                {
                    if (ImGui::MenuItem("Panel"))
                    {
                        auto e = m_Context->CreateEntity("UI Panel");
                        auto& widget = e.AddComponent<UIWidgetComponent>();
                        widget.Size = { 0.45f, 0.25f };
                        e.AddComponent<UIPanelComponent>();
                        m_SelectionContext = e;
                    }
                    if (ImGui::MenuItem("Image"))
                    {
                        auto e = m_Context->CreateEntity("UI Image");
                        auto& widget = e.AddComponent<UIWidgetComponent>();
                        widget.Size = { 0.25f, 0.18f };
                        e.AddComponent<UIImageComponent>();
                        m_SelectionContext = e;
                    }
                    if (ImGui::MenuItem("Text"))
                    {
                        auto e = m_Context->CreateEntity("UI Text");
                        auto& widget = e.AddComponent<UIWidgetComponent>();
                        widget.Size = { 0.30f, 0.08f };
                        e.AddComponent<UITextComponent>();
                        m_SelectionContext = e;
                    }
                    if (ImGui::MenuItem("Button"))
                    {
                        auto e = m_Context->CreateEntity("UI Button");
                        auto& widget = e.AddComponent<UIWidgetComponent>();
                        widget.Size = { 0.20f, 0.07f };
                        e.AddComponent<UIButtonComponent>();
                        auto& text = e.AddComponent<UITextComponent>();
                        text.Text = "Button";
                        m_SelectionContext = e;
                    }
                    if (ImGui::MenuItem("Slider"))
                    {
                        auto e = m_Context->CreateEntity("UI Slider");
                        auto& widget = e.AddComponent<UIWidgetComponent>();
                        widget.Size = { 0.32f, 0.04f };
                        e.AddComponent<UISliderComponent>();
                        m_SelectionContext = e;
                    }
                    if (ImGui::MenuItem("Checkbox"))
                    {
                        auto e = m_Context->CreateEntity("UI Checkbox");
                        auto& widget = e.AddComponent<UIWidgetComponent>();
                        widget.Size = { 0.04f, 0.04f };
                        e.AddComponent<UICheckboxComponent>();
                        m_SelectionContext = e;
                    }
                    if (ImGui::MenuItem("Progress Bar"))
                    {
                        auto e = m_Context->CreateEntity("UI Progress Bar");
                        auto& widget = e.AddComponent<UIWidgetComponent>();
                        widget.Size = { 0.32f, 0.04f };
                        e.AddComponent<UIProgressBarComponent>();
                        m_SelectionContext = e;
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndPopup();
            }
        }

        ImGui::End();

        ImGui::Begin("Properties");
        if (m_SelectionContext)
            DrawComponents(m_SelectionContext);
        ImGui::End();
    }

    void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
    {
        m_SelectionContext = entity;
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity entity)
    {
        const auto& tag = entity.GetComponent<TagComponent>().Tag;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (m_SelectionContext == entity)
            flags |= ImGuiTreeNodeFlags_Selected;

        const bool opened = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<uint32_t>(entity))),
            flags,
            "%s", tag.c_str()
        );

        if (ImGui::IsItemClicked())
            m_SelectionContext = entity;

        bool entityDeleted = false;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Rename"))
            {
                m_SelectionContext = entity;
                m_RenameRequested = true;
            }

            if (ImGui::MenuItem("Duplicate Entity"))
            {
                Entity dup = m_Context->DuplicateEntity(entity);
                m_SelectionContext = dup;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save as Prefab"))
            {
                std::filesystem::path prefabDir = AssetPath::Resolve("assets/prefabs");
                std::filesystem::create_directories(prefabDir);

                std::string baseName = entity.GetName() + "Prefab";
                std::filesystem::path savePath;

                int index = 0;
                do
                {
                    std::string filename = baseName;
                    if (index > 0)
                        filename += std::to_string(index);
                    filename += AssetFileType::PrefabExtension;
                    savePath = prefabDir / filename;
                    index++;
                } while (std::filesystem::exists(savePath));

                SceneSerializer::SerializePrefab(entity, savePath);
                WT_CORE_INFO("Saved prefab: {}", savePath.string());
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete Entity"))
                entityDeleted = true;

            ImGui::EndPopup();
        }

        if (opened)
            ImGui::TreePop();

        if (entityDeleted)
        {
            if (m_SelectionContext == entity)
                m_SelectionContext = {};
            m_Context->DestroyEntity(entity);
        }
    }

    void SceneHierarchyPanel::DrawComponents(Entity entity)
    {
        if (entity.HasComponent<TagComponent>())
        {
            auto& tag = entity.GetComponent<TagComponent>().Tag;

            char buffer[256] = {};
            std::strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);

            if (m_RenameRequested)
            {
                ImGui::SetKeyboardFocusHere();
                m_RenameRequested = false;
            }

            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
                tag = buffer;
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(-1);

        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            std::string currentCategory;
            EditorComponentRegistry::ForEach([&](const EditorComponentDescriptor& descriptor)
            {
                if (!descriptor.CanAdd(m_SelectionContext))
                    return;

                if (currentCategory != descriptor.Category)
                {
                    if (!currentCategory.empty())
                        ImGui::Separator();
                    currentCategory = descriptor.Category;
                    ImGui::TextDisabled("%s", currentCategory.c_str());
                }

                if (ImGui::MenuItem(descriptor.Label.c_str()))
                {
                    descriptor.Add(m_SelectionContext);
                    ImGui::CloseCurrentPopup();
                }
            });
            ImGui::EndPopup();
        }

        ImGui::PopItemWidth();

        EditorComponentRegistry::ForEach([entity](const EditorComponentDescriptor& descriptor)
        {
            descriptor.Draw(entity);
        });
    }

} // namespace Wheatear
