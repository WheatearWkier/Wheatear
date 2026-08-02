#include "wtpch.h"
#include "SceneHierarchyPanel.h"
#include "EditorCommands.h"
#include "Assets/UITemplateFactory.h"

#include <imgui/imgui.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    namespace {

        static uint32_t EntityKey(Entity entity)
        {
            return static_cast<uint32_t>(static_cast<entt::entity>(entity));
        }

        class UITemplateFactoryCreateCommand final : public ICommand
        {
        public:
            UITemplateFactoryCreateCommand(Scene* scene, UITemplateKind kind, UUID parentID)
                : m_Scene(scene), m_Kind(kind), m_ParentID(parentID)
            {
            }

            void Execute() override
            {
                if (!m_Scene)
                    return;

                m_UUIDs.clear();
                std::vector<Entity> entities = UITemplateFactory::Create(m_Scene, m_Kind, m_ParentID);
                for (Entity entity : entities)
                {
                    if (entity)
                        m_UUIDs.push_back(entity.GetUUID());
                }
            }

            void Undo() override
            {
                if (!m_Scene)
                    return;

                for (auto it = m_UUIDs.rbegin(); it != m_UUIDs.rend(); ++it)
                {
                    Entity entity = m_Scene->GetEntityByUUID(*it);
                    if (entity)
                        m_Scene->DestroyEntityImmediate(entity);
                }
            }

            Entity GetRootEntity() const
            {
                return m_Scene && !m_UUIDs.empty() ? m_Scene->GetEntityByUUID(m_UUIDs.front()) : Entity{};
            }

        private:
            Scene* m_Scene = nullptr;
            UITemplateKind m_Kind = UITemplateKind::Unknown;
            UUID m_ParentID = 0;
            std::vector<UUID> m_UUIDs;
        };

    } // namespace

    SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
    {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
    {
        m_Context = context;
        m_SelectionContext = {};
        m_ScrollToSelection = false;
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Scene Hierarchy");

        if (m_Context)
        {
            ImGui::SetNextItemWidth(-80.0f);
            ImGui::InputTextWithHint("##HierarchySearch", "Search entity...", m_SearchBuffer, sizeof(m_SearchBuffer));
            ImGui::SameLine();
            ImGui::Checkbox("UI", &m_ShowOnlyUI);
            ImGui::Separator();

            auto& registry = m_Context->m_Registry;
            UIChildMap uiChildren;
            std::unordered_map<UUID, Entity> uiEntityLookup;
            std::unordered_set<uint32_t> uiChildKeys;

            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                if (!entity.HasComponent<UIWidgetComponent>() || !entity.HasComponent<IDComponent>())
                    continue;

                const UUID id = entity.GetComponent<IDComponent>().ID;
                if (static_cast<uint64_t>(id) != 0)
                    uiEntityLookup[id] = entity;
            }

            auto resolveUIReference = [&](UUID id) -> Entity
            {
                if (static_cast<uint64_t>(id) != 0)
                {
                    auto idIt = uiEntityLookup.find(id);
                    if (idIt != uiEntityLookup.end())
                        return idIt->second;
                }

                return Entity{};
            };

            for (auto entityID : registry.view<IDComponent>())
            {
                Entity child{ entityID, m_Context.get() };
                if (!child.HasComponent<UIWidgetComponent>())
                    continue;

                const auto& widget = child.GetComponent<UIWidgetComponent>();
                Entity parent = resolveUIReference(widget.ParentEntity);
                if (!parent)
                    continue;

                if (!parent || parent == child || !parent.HasComponent<UIWidgetComponent>())
                    continue;

                uiChildren[EntityKey(parent)].push_back(child);
                uiChildKeys.insert(EntityKey(child));
            }

            for (auto& [parentKey, children] : uiChildren)
            {
                std::sort(children.begin(), children.end(), [](Entity a, Entity b)
                {
                    const auto& aw = a.GetComponent<UIWidgetComponent>();
                    const auto& bw = b.GetComponent<UIWidgetComponent>();
                    if (aw.SortOrder != bw.SortOrder)
                        return aw.SortOrder < bw.SortOrder;
                    return a.GetName() < b.GetName();
                });
            }

            bool selectionVisible = !m_SelectionContext;
            std::unordered_set<uint32_t> drawn;

            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                const uint32_t key = EntityKey(entity);
                if (drawn.find(key) != drawn.end())
                    continue;
                if (uiChildKeys.find(key) != uiChildKeys.end())
                    continue;
                DrawEntityNode(entity, uiChildren, drawn, selectionVisible);
            }

            // Cycles or broken authoring data can leave UI entities without a root.
            // Draw anything still missing so the user can fix Parent in Inspector.
            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                const uint32_t key = EntityKey(entity);
                if (drawn.find(key) != drawn.end())
                    continue;
                DrawEntityNode(entity, uiChildren, drawn, selectionVisible);
            }

            if (!selectionVisible)
            {
                ImGui::Separator();
                ImGui::TextDisabled("Selected entity is hidden by filters.");
                if (ImGui::SmallButton("Clear Hierarchy Filters"))
                {
                    m_SearchBuffer[0] = '\0';
                    m_ShowOnlyUI = false;
                    m_ScrollToSelection = true;
                }
            }

            if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(0) && !ImGui::IsAnyItemHovered())
                m_SelectionContext = {};

            if (ImGui::BeginPopupContextWindow("##HierarchyCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            {
                auto createEntityWithUndo = [&](const std::string& name, auto configure)
                {
                    auto command = std::make_unique<EntityCreateCommand>(m_Context.get(), name);
                    command->SetOnCreate([configure](Entity created) { configure(created); });
                    command->Execute();
                    m_SelectionContext = command->GetEntity();
                    CommandHistory::Get().Push(std::move(command));
                };

                if (ImGui::MenuItem("Create Empty Entity"))
                    createEntityWithUndo("Empty Entity", [](Entity) {});

                ImGui::Separator();

                if (ImGui::BeginMenu("2D Object"))
                {
                    if (ImGui::MenuItem("Sprite"))
                    {
                        createEntityWithUndo("Sprite", [](Entity e)
                        {
                            e.AddComponent<SpriteRendererComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Circle"))
                    {
                        createEntityWithUndo("Circle", [](Entity e)
                        {
                            e.AddComponent<CircleRendererComponent>();
                        });
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera"))
                {
                    if (ImGui::MenuItem("Camera"))
                    {
                        createEntityWithUndo("Camera", [](Entity e)
                        {
                            e.AddComponent<CameraComponent>();
                        });
                    }
                    ImGui::EndMenu();
                }


                if (ImGui::BeginMenu("UI"))
                {
                    UUID uiParentID = 0;

                    std::function<bool(Entity)> canUseAsUIParent = [&](Entity entity)
                    {
                        if (!entity || !entity.HasComponent<UIWidgetComponent>())
                            return false;
                        if (entity.HasComponent<UICanvasComponent>())
                            return true;

                        std::unordered_set<uint32_t> visited;
                        Entity current = entity;
                        while (current && current.HasComponent<UIWidgetComponent>())
                        {
                            const uint32_t key = EntityKey(current);
                            if (!visited.insert(key).second)
                                return false;

                            const auto& widget = current.GetComponent<UIWidgetComponent>();
                            Entity parent = resolveUIReference(widget.ParentEntity);
                            if (!parent)
                                return false;
                            if (parent.HasComponent<UICanvasComponent>())
                                return true;
                            current = parent;
                        }
                        return false;
                    };

                    if (m_SelectionContext
                        && canUseAsUIParent(m_SelectionContext))
                    {
                        uiParentID = m_SelectionContext.GetUUID();
                    }
                    const bool canCreateUIChild = static_cast<uint64_t>(uiParentID) != 0;
                    auto createUITemplateWithUndo = [&](UITemplateKind kind)
                    {
                        auto command = std::make_unique<UITemplateFactoryCreateCommand>(
                            m_Context.get(),
                            kind,
                            uiParentID);
                        command->Execute();
                        m_SelectionContext = command->GetRootEntity();
                        m_ScrollToSelection = true;
                        CommandHistory::Get().Push(std::move(command));
                    };

                    if (ImGui::MenuItem("Canvas"))
                    {
                        createEntityWithUndo("UI Canvas", [](Entity e)
                        {
                            e.AddComponent<UICanvasComponent>();
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Anchor = UIAnchor::TopLeft;
                            widget.Position = { 0.0f, 0.0f };
                            widget.Size = { 1.0f, 1.0f };
                            widget.SortOrder = 0;
                        });
                    }
                    ImGui::Separator();
                    if (!canCreateUIChild)
                        ImGui::TextDisabled("Select a Canvas or UI child first.");
                    ImGui::BeginDisabled(!canCreateUIChild);
                    if (ImGui::MenuItem("Panel"))
                    {
                        createEntityWithUndo("UI Panel", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.45f, 0.25f };
                            widget.SortOrder = 10;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UIPanelComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Image"))
                    {
                        createEntityWithUndo("UI Image", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.25f, 0.18f };
                            widget.SortOrder = 20;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UIImageComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Text"))
                    {
                        createEntityWithUndo("UI Text", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.30f, 0.08f };
                            widget.SortOrder = 30;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UITextComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Button"))
                    {
                        createEntityWithUndo("UI Button", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.20f, 0.07f };
                            widget.SortOrder = 40;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UIButtonComponent>();
                            auto& text = e.AddComponent<UITextComponent>();
                            text.Text = "Button";
                        });
                    }
                    if (ImGui::MenuItem("Slider"))
                    {
                        createEntityWithUndo("UI Slider", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.32f, 0.04f };
                            widget.SortOrder = 40;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UISliderComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Checkbox"))
                    {
                        createEntityWithUndo("UI Checkbox", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.04f, 0.04f };
                            widget.SortOrder = 40;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UICheckboxComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Progress Bar"))
                    {
                        createEntityWithUndo("UI Progress Bar", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.32f, 0.04f };
                            widget.SortOrder = 35;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UIProgressBarComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Path"))
                    {
                        createEntityWithUndo("UI Path", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.42f, 0.24f };
                            widget.SortOrder = 25;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UIPathComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Pager"))
                    {
                        createEntityWithUndo("UI Pager", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Visible = false;
                            widget.Size = { 0.01f, 0.01f };
                            widget.SortOrder = 0;
                            widget.ParentEntity = uiParentID;
                            e.AddComponent<UIPagerComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Scroll View"))
                    {
                        createEntityWithUndo("UI Scroll View", [uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.36f, 0.42f };
                            widget.SortOrder = 10;
                            widget.ParentEntity = uiParentID;
                            auto& panel = e.AddComponent<UIPanelComponent>();
                            panel.ClipChildren = true;
                            e.AddComponent<UIScrollViewComponent>();
                        });
                    }
                    if (ImGui::BeginMenu("Templates"))
                    {
                        if (ImGui::MenuItem("Titled Scroll Text"))
                            createUITemplateWithUndo(UITemplateKind::TitledScrollText);
                        if (ImGui::MenuItem("Paged Grid"))
                            createUITemplateWithUndo(UITemplateKind::PagedGrid);
                        if (ImGui::MenuItem("Paged Inventory Grid"))
                            createUITemplateWithUndo(UITemplateKind::PagedInventoryGrid);
                        ImGui::Separator();
                        if (ImGui::MenuItem("Skill Button"))
                            createUITemplateWithUndo(UITemplateKind::SkillButton);
                        if (ImGui::MenuItem("Equipment Slot"))
                            createUITemplateWithUndo(UITemplateKind::EquipmentSlot);
                        if (ImGui::MenuItem("Tooltip"))
                            createUITemplateWithUndo(UITemplateKind::Tooltip);
                        if (ImGui::MenuItem("Save Slot"))
                            createUITemplateWithUndo(UITemplateKind::SaveSlot);
                        if (ImGui::MenuItem("Skill Tree Node"))
                            createUITemplateWithUndo(UITemplateKind::SkillTreeNode);
                        if (ImGui::MenuItem("Combat Skill Slot"))
                            createUITemplateWithUndo(UITemplateKind::CombatSkillSlot);
                        ImGui::EndMenu();
                    }
                    ImGui::EndDisabled();
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
        if (m_SelectionContext != entity)
            m_ScrollToSelection = true;
        m_SelectionContext = entity;
    }

} // namespace Wheatear
