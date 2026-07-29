#include "wtpch.h"
#include "SceneHierarchyPanel.h"
#include "EditorCommands.h"

#include <imgui/imgui.h>
#include <algorithm>
#include <array>
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

        enum class UITemplateKind
        {
            TitledScrollText = 0,
            PagedGrid,
            PagedInventory
        };

        struct UIParentRef
        {
            UUID ID = 0;
            std::string Tag;
        };

        static std::string MakeUniqueEntityName(Scene* scene, const std::string& base)
        {
            if (!scene || !scene->GetEntityByName(base))
                return base;

            for (int index = 2; index < 10000; ++index)
            {
                const std::string candidate = base + "_" + std::to_string(index);
                if (!scene->GetEntityByName(candidate))
                    return candidate;
            }
            return base + "_Copy";
        }

        static void ConfigureUIWidget(Entity entity,
            const UIParentRef& parent,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            bool visible = true)
        {
            auto& widget = entity.AddOrReplaceComponent<UIWidgetComponent>();
            widget.Visible = visible;
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = position;
            widget.Size = size;
            widget.Rotation = 0.0f;
            widget.SortOrder = sortOrder;
            widget.ParentEntity = parent.ID;
            widget.ParentTag = parent.Tag;
        }

        static void ConfigurePanel(Entity entity,
            glm::vec4 background,
            glm::vec4 border,
            float borderThickness,
            bool clipChildren = false)
        {
            auto& panel = entity.AddOrReplaceComponent<UIPanelComponent>();
            panel.BackgroundColor = background;
            panel.BorderColor = border;
            panel.BorderThickness = borderThickness;
            panel.ClipChildren = clipChildren;
        }

        static void ConfigureText(Entity entity,
            const std::string& value,
            float fontSize,
            glm::vec4 color = { 0.94f, 0.91f, 0.82f, 1.0f })
        {
            auto& text = entity.AddOrReplaceComponent<UITextComponent>();
            text.Text = value;
            text.FontSize = fontSize;
            text.Color = color;
            text.FontPath = "assets/fonts/wqy-microhei.ttc";
            text.ShadowColor = { 0.01f, 0.015f, 0.018f, 0.80f };
            text.ShadowOffset = { 1.6f, 1.6f };
            text.OutlineColor = { 0.0f, 0.0f, 0.0f, 0.86f };
            text.OutlineThickness = 1.15f;
        }

        static void ConfigureButton(Entity entity,
            const std::string& command,
            glm::vec4 normal = { 0.12f, 0.16f, 0.18f, 0.90f },
            glm::vec4 hover = { 0.25f, 0.45f, 0.46f, 0.96f },
            glm::vec4 pressed = { 0.07f, 0.10f, 0.11f, 0.98f })
        {
            auto& button = entity.AddOrReplaceComponent<UIButtonComponent>();
            button.OnClickFunction = command;
            button.NormalColor = normal;
            button.HoverColor = hover;
            button.PressedColor = pressed;
        }

        static void ConfigurePageItem(Entity entity, UUID pagerID, const std::string& pagerTag, int page)
        {
            auto& pageItem = entity.AddOrReplaceComponent<UIPageItemComponent>();
            pageItem.PagerEntity = pagerID;
            pageItem.PagerTag = pagerTag;
            pageItem.Page = std::max(page, 1);
        }

        class UITemplateCreateCommand final : public ICommand
        {
        public:
            UITemplateCreateCommand(Scene* scene, UITemplateKind kind, UIParentRef parent)
                : m_Scene(scene), m_Kind(kind), m_Parent(std::move(parent))
            {
                const std::string base = [&]()
                {
                    switch (m_Kind)
                    {
                    case UITemplateKind::TitledScrollText: return std::string("UI_TitledScrollText");
                    case UITemplateKind::PagedInventory:   return std::string("UI_PagedInventory");
                    case UITemplateKind::PagedGrid:        return std::string("UI_PagedGrid");
                    }
                    return std::string("UI_Template");
                }();

                m_NamePrefix = MakeUniqueEntityName(scene, base);
                const size_t count = m_Kind == UITemplateKind::TitledScrollText ? 4 : 24;
                m_UUIDs.reserve(count);
                for (size_t i = 0; i < count; ++i)
                    m_UUIDs.emplace_back(UUID());
            }

            void Execute() override
            {
                if (!m_Scene)
                    return;
                switch (m_Kind)
                {
                case UITemplateKind::TitledScrollText: CreateTitledScrollText(); break;
                case UITemplateKind::PagedGrid:        CreatePagedGrid(false); break;
                case UITemplateKind::PagedInventory:   CreatePagedGrid(true); break;
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
                return m_Scene ? m_Scene->GetEntityByUUID(m_UUIDs.empty() ? UUID(0) : m_UUIDs.front()) : Entity{};
            }

        private:
            Entity CreateNode(size_t index, const std::string& suffix)
            {
                return m_Scene->CreateEntityWithUUID(m_UUIDs.at(index), m_NamePrefix + suffix);
            }

            void CreateTitledScrollText()
            {
                const UIParentRef rootParent = m_Parent;
                Entity root = CreateNode(0, "");
                ConfigureUIWidget(root, rootParent, { 0.12f, 0.14f }, { 0.46f, 0.52f }, 30);
                ConfigurePanel(root, { 0.035f, 0.042f, 0.048f, 0.90f }, { 0.45f, 0.78f, 0.76f, 0.86f }, 1.8f);

                const UIParentRef rootRef{ root.GetUUID(), root.GetName() };
                Entity title = CreateNode(1, "_Title");
                ConfigureUIWidget(title, rootRef, { 0.045f, 0.035f }, { 0.78f, 0.12f }, 42);
                ConfigureText(title, "标题", 24.0f, { 0.68f, 0.96f, 0.92f, 1.0f });

                Entity scroll = CreateNode(2, "_ScrollView");
                ConfigureUIWidget(scroll, rootRef, { 0.045f, 0.18f }, { 0.86f, 0.74f }, 36);
                ConfigurePanel(scroll, { 0.012f, 0.016f, 0.018f, 0.50f }, { 0.24f, 0.44f, 0.44f, 0.58f }, 1.0f, true);
                auto& scrollView = scroll.AddOrReplaceComponent<UIScrollViewComponent>();
                scrollView.ContentHeight = 2.0f;
                scrollView.WheelStep = 0.08f;
                scrollView.ScrollbarWidth = 0.018f;

                Entity body = CreateNode(3, "_BodyText");
                ConfigureUIWidget(body, { scroll.GetUUID(), scroll.GetName() }, { 0.035f, 0.035f }, { 0.84f, 1.68f }, 44);
                ConfigureText(body,
                    "这里放长文本。滚轮或拖动右侧滚动条查看超出面板的内容。\n\n"
                    "这个模板适合教程说明、任务详情、信件正文、VN 历史记录等场景。",
                    18.0f);
            }

            void CreatePagedGrid(bool inventoryStyle)
            {
                Entity root = CreateNode(0, "");
                ConfigureUIWidget(root, m_Parent, { 0.12f, 0.14f }, { 0.56f, 0.56f }, 30);
                ConfigurePanel(root,
                    inventoryStyle ? glm::vec4{ 0.045f, 0.038f, 0.032f, 0.90f } : glm::vec4{ 0.032f, 0.040f, 0.050f, 0.90f },
                    inventoryStyle ? glm::vec4{ 0.80f, 0.62f, 0.32f, 0.86f } : glm::vec4{ 0.40f, 0.74f, 0.76f, 0.84f },
                    1.8f);

                const UIParentRef rootRef{ root.GetUUID(), root.GetName() };
                Entity pager = CreateNode(1, "_Pager");
                ConfigureUIWidget(pager, rootRef, { 0.0f, 0.0f }, { 0.01f, 0.01f }, 0, false);
                auto& pagerComponent = pager.AddOrReplaceComponent<UIPagerComponent>();
                pagerComponent.PageCount = 2;
                pagerComponent.CurrentPage = 1;

                Entity pageText = CreateNode(2, "_PageText");
                ConfigureUIWidget(pageText, rootRef, { 0.38f, 0.88f }, { 0.20f, 0.07f }, 45);
                ConfigureText(pageText, "1 / 2", 18.0f, { 0.94f, 0.90f, 0.76f, 1.0f });

                Entity prev = CreateNode(3, "_PrevButton");
                ConfigureUIWidget(prev, rootRef, { 0.28f, 0.88f }, { 0.08f, 0.07f }, 55);
                ConfigureButton(prev, "ui:pager:" + pager.GetName() + ":prev");
                ConfigureText(prev, "<", 20.0f);

                Entity next = CreateNode(4, "_NextButton");
                ConfigureUIWidget(next, rootRef, { 0.60f, 0.88f }, { 0.08f, 0.07f }, 55);
                ConfigureButton(next, "ui:pager:" + pager.GetName() + ":next");
                ConfigureText(next, ">", 20.0f);

                const std::array<glm::vec2, 8> positions = {
                    glm::vec2{ 0.08f, 0.14f }, glm::vec2{ 0.25f, 0.14f },
                    glm::vec2{ 0.42f, 0.14f }, glm::vec2{ 0.59f, 0.14f },
                    glm::vec2{ 0.08f, 0.43f }, glm::vec2{ 0.25f, 0.43f },
                    glm::vec2{ 0.42f, 0.43f }, glm::vec2{ 0.59f, 0.43f }
                };

                for (size_t i = 0; i < positions.size(); ++i)
                {
                    const int page = i < 4 ? 1 : 2;
                    const glm::vec2 localPosition = positions[i];
                    const size_t frameIndex = 5 + i * 2;
                    const size_t labelIndex = frameIndex + 1;

                    Entity frame = CreateNode(frameIndex, "_Slot_" + std::to_string(i + 1));
                    ConfigureUIWidget(frame, rootRef, localPosition, { 0.13f, 0.20f }, 34);
                    ConfigurePanel(frame,
                        inventoryStyle ? glm::vec4{ 0.020f, 0.024f, 0.026f, 0.82f } : glm::vec4{ 0.030f, 0.040f, 0.052f, 0.74f },
                        inventoryStyle ? glm::vec4{ 0.72f, 0.56f, 0.30f, 0.78f } : glm::vec4{ 0.36f, 0.66f, 0.72f, 0.70f },
                        1.4f);
                    ConfigureButton(frame, "");
                    ConfigurePageItem(frame, pager.GetUUID(), pager.GetName(), page);

                    Entity label = CreateNode(labelIndex, "_SlotText_" + std::to_string(i + 1));
                    ConfigureUIWidget(label, rootRef, localPosition + glm::vec2{ 0.018f, 0.070f }, { 0.094f, 0.06f }, 46);
                    ConfigureText(label, inventoryStyle ? "道具" : "Item", 16.0f);
                    ConfigurePageItem(label, pager.GetUUID(), pager.GetName(), page);
                }
            }

        private:
            Scene* m_Scene = nullptr;
            UITemplateKind m_Kind = UITemplateKind::TitledScrollText;
            UIParentRef m_Parent;
            std::string m_NamePrefix;
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
            std::unordered_map<std::string, Entity> uiTagLookup;
            std::unordered_map<UUID, Entity> uiEntityLookup;
            std::unordered_set<uint32_t> uiChildKeys;

            for (auto entityID : registry.view<IDComponent>())
            {
                Entity entity{ entityID, m_Context.get() };
                if (!entity.HasComponent<UIWidgetComponent>() || !entity.HasComponent<TagComponent>())
                    continue;

                const std::string& tag = entity.GetComponent<TagComponent>().Tag;
                if (!tag.empty())
                    uiTagLookup[tag] = entity;
                if (entity.HasComponent<IDComponent>())
                {
                    const UUID id = entity.GetComponent<IDComponent>().ID;
                    if (static_cast<uint64_t>(id) != 0)
                        uiEntityLookup[id] = entity;
                }
            }

            auto resolveUIReference = [&](UUID id, const std::string& fallbackTag) -> Entity
            {
                if (static_cast<uint64_t>(id) != 0)
                {
                    auto idIt = uiEntityLookup.find(id);
                    if (idIt != uiEntityLookup.end())
                        return idIt->second;
                }

                auto tagIt = uiTagLookup.find(fallbackTag);
                return tagIt != uiTagLookup.end() ? tagIt->second : Entity{};
            };

            for (auto entityID : registry.view<IDComponent>())
            {
                Entity child{ entityID, m_Context.get() };
                if (!child.HasComponent<UIWidgetComponent>())
                    continue;

                const auto& widget = child.GetComponent<UIWidgetComponent>();
                Entity parent = resolveUIReference(widget.ParentEntity, widget.ParentTag);
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
            // Draw anything still missing so the user can fix Parent Tag in Inspector.
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
                    std::string uiParentTag;
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
                            Entity parent = resolveUIReference(widget.ParentEntity, widget.ParentTag);
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
                        uiParentTag = m_SelectionContext.GetName();
                        uiParentID = m_SelectionContext.GetUUID();
                    }
                    const bool canCreateUIChild = static_cast<uint64_t>(uiParentID) != 0 || !uiParentTag.empty();
                    auto createUITemplateWithUndo = [&](UITemplateKind kind)
                    {
                        auto command = std::make_unique<UITemplateCreateCommand>(
                            m_Context.get(),
                            kind,
                            UIParentRef{ uiParentID, uiParentTag });
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
                        createEntityWithUndo("UI Panel", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.45f, 0.25f };
                            widget.SortOrder = 10;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UIPanelComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Image"))
                    {
                        createEntityWithUndo("UI Image", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.25f, 0.18f };
                            widget.SortOrder = 20;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UIImageComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Text"))
                    {
                        createEntityWithUndo("UI Text", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.30f, 0.08f };
                            widget.SortOrder = 30;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UITextComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Button"))
                    {
                        createEntityWithUndo("UI Button", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.20f, 0.07f };
                            widget.SortOrder = 40;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UIButtonComponent>();
                            auto& text = e.AddComponent<UITextComponent>();
                            text.Text = "Button";
                        });
                    }
                    if (ImGui::MenuItem("Slider"))
                    {
                        createEntityWithUndo("UI Slider", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.32f, 0.04f };
                            widget.SortOrder = 40;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UISliderComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Checkbox"))
                    {
                        createEntityWithUndo("UI Checkbox", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.04f, 0.04f };
                            widget.SortOrder = 40;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UICheckboxComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Progress Bar"))
                    {
                        createEntityWithUndo("UI Progress Bar", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.32f, 0.04f };
                            widget.SortOrder = 35;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UIProgressBarComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Path"))
                    {
                        createEntityWithUndo("UI Path", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.42f, 0.24f };
                            widget.SortOrder = 25;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UIPathComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Pager"))
                    {
                        createEntityWithUndo("UI Pager", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Visible = false;
                            widget.Size = { 0.01f, 0.01f };
                            widget.SortOrder = 0;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
                            e.AddComponent<UIPagerComponent>();
                        });
                    }
                    if (ImGui::MenuItem("Scroll View"))
                    {
                        createEntityWithUndo("UI Scroll View", [uiParentTag, uiParentID](Entity e)
                        {
                            auto& widget = e.AddComponent<UIWidgetComponent>();
                            widget.Size = { 0.36f, 0.42f };
                            widget.SortOrder = 10;
                            widget.ParentEntity = uiParentID;
                            widget.ParentTag = uiParentTag;
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
                            createUITemplateWithUndo(UITemplateKind::PagedInventory);
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
