#include "wepch.h"
#include "SceneHierarchyPanel.h"
#include "Editor/EditorCommands.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/EditorLocale.h"

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

    Entity SceneHierarchyPanel::FindSingleUICanvas() const
    {
        if (!m_Context)
            return {};

        Entity found;
        for (auto entityID : m_Context->GetRegistry().view<UICanvasComponent>())
        {
            if (found)
                return {};
            found = Entity{ entityID, m_Context.get() };
        }

        return found;
    }

    UUID SceneHierarchyPanel::ResolveUIParentID(Entity entity) const
    {
        if (!entity || !entity.HasComponent<UIWidgetComponent>() || !entity.HasComponent<IDComponent>())
            return 0;

        return entity.GetUUID();
    }

    bool SceneHierarchyPanel::IsEntityHiddenInEditor(Entity entity) const
    {
        return entity && entity.HasComponent<EditorHiddenComponent>();
    }

    bool SceneHierarchyPanel::HasHiddenEditorEntities() const
    {
        if (!m_Context)
            return false;

        auto view = m_Context->GetRegistry().view<EditorHiddenComponent>();
        return view.begin() != view.end();
    }

    void SceneHierarchyPanel::SetEntityHiddenInEditor(Entity entity, bool hidden)
    {
        if (!entity)
            return;

        const bool currentlyHidden = IsEntityHiddenInEditor(entity);
        if (currentlyHidden == hidden)
            return;

        std::unique_ptr<ICommand> command;
        if (hidden)
            command = std::make_unique<AddComponentCommand<EditorHiddenComponent>>(entity);
        else
            command = std::make_unique<RemoveComponentCommand<EditorHiddenComponent>>(entity);
        command->Execute();
        CommandHistory::Get().Push(std::move(command));
    }

    void SceneHierarchyPanel::ShowAllHiddenEditorEntities()
    {
        if (!m_Context)
            return;

        std::vector<Entity> hiddenEntities;
        for (auto entityID : m_Context->GetRegistry().view<EditorHiddenComponent>())
            hiddenEntities.emplace_back(entityID, m_Context.get());

        auto composite = std::make_unique<CompositeCommand>();
        for (Entity entity : hiddenEntities)
        {
            if (!entity || !entity.HasComponent<EditorHiddenComponent>())
                continue;

            auto command = std::make_unique<RemoveComponentCommand<EditorHiddenComponent>>(entity);
            command->Execute();
            composite->Add(std::move(command));
        }

        if (!composite->Empty())
            CommandHistory::Get().Push(std::move(composite));
    }

    Entity SceneHierarchyPanel::CreateEntityWithUndo(const std::string& name,
        const std::function<void(Entity)>& configure)
    {
        if (!m_Context)
            return {};

        auto command = std::make_unique<EntityCreateCommand>(m_Context.get(), name);
        command->SetOnCreate([configure](Entity created)
        {
            if (configure)
                configure(created);
        });
        command->Execute();

        Entity created = command->GetEntity();
        m_SelectionContext = created;
        m_ScrollToSelection = true;
        CommandHistory::Get().Push(std::move(command));
        return created;
    }

    Entity SceneHierarchyPanel::CreateUITemplateWithUndo(UITemplateKind kind, UUID parentID)
    {
        if (!m_Context || static_cast<uint64_t>(parentID) == 0)
            return {};

        auto command = std::make_unique<UITemplateFactoryCreateCommand>(
            m_Context.get(),
            kind,
            parentID);
        command->Execute();

        Entity root = command->GetRootEntity();
        if (!root)
            return {};

        m_SelectionContext = root;
        m_ScrollToSelection = true;
        CommandHistory::Get().Push(std::move(command));
        return root;
    }

    void SceneHierarchyPanel::DrawCreateUIMenuItems(UUID parentID, bool includeCanvas)
    {
        const bool canCreateUIChild = static_cast<uint64_t>(parentID) != 0;

        if (includeCanvas)
        {
            if (ImGui::MenuItem("Canvas"))
            {
                CreateEntityWithUndo("UI Canvas", [](Entity e)
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
        }

        if (!canCreateUIChild)
            ImGui::TextDisabled(EditorLocale::Text("Select a Canvas or UI child first.", "请先选择画布或 UI 子对象。"));

        ImGui::BeginDisabled(!canCreateUIChild);
        if (ImGui::MenuItem("Panel"))
        {
            CreateEntityWithUndo("UI Panel", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.45f, 0.25f };
                widget.SortOrder = 10;
                widget.ParentEntity = parentID;
                e.AddComponent<UIPanelComponent>();
            });
        }
        if (ImGui::MenuItem("Image"))
        {
            CreateEntityWithUndo("UI Image", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.25f, 0.18f };
                widget.SortOrder = 20;
                widget.ParentEntity = parentID;
                e.AddComponent<UIImageComponent>();
            });
        }
        if (ImGui::MenuItem(EditorLocale::Text("Circle", "圆形")))
        {
            CreateEntityWithUndo("UI Circle", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.12f, 0.12f };
                widget.SortOrder = 25;
                widget.ParentEntity = parentID;
                e.AddComponent<UICircleComponent>();
            });
        }
        if (ImGui::MenuItem("Text"))
        {
            CreateEntityWithUndo("UI Text", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.30f, 0.08f };
                widget.SortOrder = 30;
                widget.ParentEntity = parentID;
                e.AddComponent<UITextComponent>();
            });
        }
        if (ImGui::MenuItem("Button"))
        {
            CreateEntityWithUndo("UI Button", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.20f, 0.07f };
                widget.SortOrder = 40;
                widget.ParentEntity = parentID;
                e.AddComponent<UIButtonComponent>();
                auto& text = e.AddComponent<UITextComponent>();
                text.Text = "Button";
            });
        }
        if (ImGui::MenuItem("Slider"))
        {
            CreateEntityWithUndo("UI Slider", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.32f, 0.04f };
                widget.SortOrder = 40;
                widget.ParentEntity = parentID;
                e.AddComponent<UISliderComponent>();
            });
        }
        if (ImGui::MenuItem("Checkbox"))
        {
            CreateEntityWithUndo("UI Checkbox", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.04f, 0.04f };
                widget.SortOrder = 40;
                widget.ParentEntity = parentID;
                e.AddComponent<UICheckboxComponent>();
            });
        }
        if (ImGui::MenuItem("Progress Bar"))
        {
            CreateEntityWithUndo("UI Progress Bar", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.32f, 0.04f };
                widget.SortOrder = 35;
                widget.ParentEntity = parentID;
                e.AddComponent<UIProgressBarComponent>();
            });
        }
        if (ImGui::MenuItem("Path"))
        {
            CreateEntityWithUndo("UI Path", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.42f, 0.24f };
                widget.SortOrder = 25;
                widget.ParentEntity = parentID;
                e.AddComponent<UIPathComponent>();
            });
        }
        if (ImGui::MenuItem("Pager"))
        {
            CreateEntityWithUndo("UI Pager", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Visible = false;
                widget.Size = { 0.01f, 0.01f };
                widget.SortOrder = 0;
                widget.ParentEntity = parentID;
                e.AddComponent<UIPagerComponent>();
            });
        }
        if (ImGui::MenuItem("Scroll View"))
        {
            CreateEntityWithUndo("UI Scroll View", [parentID](Entity e)
            {
                auto& widget = e.AddComponent<UIWidgetComponent>();
                widget.Size = { 0.36f, 0.42f };
                widget.SortOrder = 10;
                widget.ParentEntity = parentID;
                auto& panel = e.AddComponent<UIPanelComponent>();
                panel.ClipChildren = true;
                e.AddComponent<UIScrollViewComponent>();
            });
        }
        if (ImGui::BeginMenu("Templates"))
        {
            if (ImGui::MenuItem("Titled Scroll Text"))
                CreateUITemplateWithUndo(UITemplateKind::TitledScrollText, parentID);
            if (ImGui::MenuItem("Paged Grid"))
                CreateUITemplateWithUndo(UITemplateKind::PagedGrid, parentID);
            if (ImGui::MenuItem("Paged Inventory Grid"))
                CreateUITemplateWithUndo(UITemplateKind::PagedInventoryGrid, parentID);
            ImGui::Separator();
            if (ImGui::MenuItem("Skill Button"))
                CreateUITemplateWithUndo(UITemplateKind::SkillButton, parentID);
            if (ImGui::MenuItem("Equipment Slot"))
                CreateUITemplateWithUndo(UITemplateKind::EquipmentSlot, parentID);
            if (ImGui::MenuItem("Tooltip"))
                CreateUITemplateWithUndo(UITemplateKind::Tooltip, parentID);
            if (ImGui::MenuItem("Save Slot"))
                CreateUITemplateWithUndo(UITemplateKind::SaveSlot, parentID);
            if (ImGui::MenuItem("Skill Tree Node"))
                CreateUITemplateWithUndo(UITemplateKind::SkillTreeNode, parentID);
            if (ImGui::MenuItem("Combat Skill Slot"))
                CreateUITemplateWithUndo(UITemplateKind::CombatSkillSlot, parentID);
            ImGui::EndMenu();
        }
        ImGui::EndDisabled();
    }

    void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
    {
        m_Context = context;
        m_SelectionContext = {};
        m_ScrollToSelection = false;
        m_SceneSettingsEditing = false;
        m_SceneSettingsEditStartCanSave = true;
        m_SceneSettingsEditStartCanLoad = true;
        m_SceneSettingsEditStartSaveDirectory.clear();
        m_SceneSettingsEditStartAutoLoadSlot = 0;
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        ImGui::Begin("Scene Hierarchy");

        if (m_Context)
        {
            // Keyboard navigation: arrows move the selection, F2 starts an
            // inline rename (guarded so typing in the search box is untouched).
            if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                {
                    std::vector<Entity> order;
                    order.reserve(64);
                    for (auto id : m_Context->m_Registry.view<IDComponent>())
                    {
                        Entity e{ id, m_Context.get() };
                        if (e)
                            order.push_back(e);
                    }
                    if (!order.empty())
                    {
                        int index = -1;
                        for (int i = 0; i < static_cast<int>(order.size()); ++i)
                        {
                            if (order[i] == m_SelectionContext) { index = i; break; }
                        }
                        const int step = ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : -1;
                        if (index < 0)
                            index = step > 0 ? 0 : static_cast<int>(order.size()) - 1;
                        else
                            index = std::clamp(index + step, 0, static_cast<int>(order.size()) - 1);
                        m_SelectionContext = order[index];
                        m_ScrollToSelection = true;
                    }
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_F2))
                {
                    if (m_SelectionContext)
                        m_RenameRequested = true;
                }
            }

            ImGui::SetNextItemWidth(-168.0f);
            ImGui::InputTextWithHint("##HierarchySearch", "Search entity...", m_SearchBuffer, sizeof(m_SearchBuffer));
            ImGui::SameLine();
            ImGui::Checkbox("UI", &m_ShowOnlyUI);
            ImGui::SameLine();
            ImGui::BeginDisabled(!HasHiddenEditorEntities());
            if (ImGui::SmallButton("Show All"))
                ShowAllHiddenEditorEntities();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(EditorLocale::Text("Show all entities hidden in the editor", "显示所有在编辑器中隐藏的实体"));
            ImGui::EndDisabled();
            ImGui::Separator();

            auto& registry = m_Context->m_Registry;
            UIChildMap uiChildren;
            std::unordered_map<UUID, Entity> uiEntityLookup;
            std::unordered_set<uint32_t> uiChildKeys;
            Entity fallbackCanvas = FindSingleUICanvas();

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
                if (child.HasComponent<UICanvasComponent>())
                    continue;

                const auto& widget = child.GetComponent<UIWidgetComponent>();
                Entity parent = resolveUIReference(widget.ParentEntity);
                if (!parent && static_cast<uint64_t>(widget.ParentEntity) == 0)
                    parent = fallbackCanvas;

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

            // Editor folder grouping: non-UI entities carrying
            // EditorFolderComponent with a non-zero ParentFolderUUID are
            // nested under the folder entity in the tree (UI entities keep
            // their widget hierarchy and are never placed inside folders).
            FolderChildMap folderChildren;
            std::unordered_map<uint64_t, uint32_t> folderKeyByUUID;
            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                if (!entity.HasComponent<EditorFolderComponent>()
                    || entity.HasComponent<UIWidgetComponent>())
                    continue;
                const uint64_t id = static_cast<uint64_t>(entity.GetComponent<IDComponent>().ID);
                if (id != 0)
                    folderKeyByUUID[id] = EntityKey(entity);
            }
            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                if (!entity.HasComponent<EditorFolderComponent>()
                    || entity.HasComponent<UIWidgetComponent>())
                    continue;
                const uint64_t parentUUID =
                    entity.GetComponent<EditorFolderComponent>().ParentFolderUUID;
                if (parentUUID == 0)
                    continue;
                auto folderIt = folderKeyByUUID.find(parentUUID);
                if (folderIt == folderKeyByUUID.end())
                    continue;   // dangling parent -> member falls back to root
                folderChildren[folderIt->second].push_back(entity);
            }
            for (auto& [parentKey, children] : folderChildren)
            {
                std::sort(children.begin(), children.end(), [](Entity a, Entity b)
                {
                    return a.GetName() < b.GetName();
                });
            }

            std::unordered_set<uint32_t> folderMemberKeys;
            for (const auto& [parentKey, children] : folderChildren)
            {
                for (Entity child : children)
                    folderMemberKeys.insert(EntityKey(child));
            }

            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                const uint32_t key = EntityKey(entity);
                if (drawn.find(key) != drawn.end())
                    continue;
                if (uiChildKeys.find(key) != uiChildKeys.end())
                    continue;
                if (folderMemberKeys.find(key) != folderMemberKeys.end())
                    continue;
                DrawEntityNode(entity, uiChildren, folderChildren, drawn, selectionVisible);
            }

            // Cycles or broken authoring data can leave UI entities without a root.
            // Draw anything still missing so the user can fix Parent in Inspector.
            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                const uint32_t key = EntityKey(entity);
                if (drawn.find(key) != drawn.end())
                    continue;
                DrawEntityNode(entity, uiChildren, folderChildren, drawn, selectionVisible);
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
                DrawCreateEntityPopupItems();
                ImGui::EndPopup();
            }
        }

        ImGui::End();

        ImGui::Begin("Properties");
        if (m_RuntimeMode)
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                "%s", EditorLocale::Text(
                    "Play (runtime copy) - edits are discarded on stop",
                    "播放（运行副本）- 停止后修改将丢弃"));
        }
        if (m_Context)
            DrawSceneSettings();
        if (m_SelectionContext)
            DrawComponents(m_SelectionContext);
        ImGui::End();
    }

    void SceneHierarchyPanel::DrawCreateEntityPopupItems()
    {
        if (ImGui::MenuItem(EditorLocale::Text("Create Empty Entity", "创建空实体")))
            CreateEntityWithUndo("Empty Entity", [](Entity) {});

        if (ImGui::MenuItem(EditorLocale::Text("Create Folder", "创建文件夹")))
            CreateFolderWithUndo();

        ImGui::BeginDisabled(!HasHiddenEditorEntities());
        if (ImGui::MenuItem("Show All Hidden"))
            ShowAllHiddenEditorEntities();
        ImGui::EndDisabled();

        ImGui::Separator();

        if (ImGui::BeginMenu("2D Object"))
        {
            if (ImGui::MenuItem(EditorLocale::Text("Sprite", "精灵")))
            {
                CreateEntityWithUndo("Sprite", [](Entity e)
                {
                    e.AddComponent<SpriteRendererComponent>();
                });
            }
            if (ImGui::MenuItem(EditorLocale::Text("Circle", "圆形")))
            {
                CreateEntityWithUndo("Circle", [](Entity e)
                {
                    e.AddComponent<CircleRendererComponent>();
                });
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(EditorLocale::Text("Camera", "相机")))
        {
            if (ImGui::MenuItem(EditorLocale::Text("Camera", "相机")))
            {
                CreateEntityWithUndo("Camera", [](Entity e)
                {
                    e.AddComponent<CameraComponent>();
                });
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("UI"))
        {
            DrawCreateUIMenuItems(ResolveUIParentID(m_SelectionContext), true);
            ImGui::EndMenu();
        }
    }

    void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
    {
        if (m_SelectionContext != entity)
            m_ScrollToSelection = true;
        m_SelectionContext = entity;
    }

    void SceneHierarchyPanel::SetEntityActivatedCallback(std::function<void(Entity, bool)> callback)
    {
        m_EntityActivatedCallback = std::move(callback);
    }

} // namespace Wheatear
