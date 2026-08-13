#include "wepch.h"
#include "Wheatear/Utils/StringUtils.h"
#include "SceneHierarchyPanel.h"
#include "Editor/EditorCommands.h"
#include "Editor/EditorLocale.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/SceneSerializer.h"
#include "Wheatear/UI/UIWidgetLayout.h"

#include <imgui/imgui.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    namespace {


        static bool ContainsInsensitive(const std::string& value, const char* query)
        {
            if (!query || query[0] == '\0')
                return true;
            return StringUtils::ToLower(value).find(StringUtils::ToLower(query)) != std::string::npos;
        }

        static const char* EntityKindPrefix(Entity entity)
        {
            if (entity.HasComponent<UIWidgetComponent>()) return "[UI]";
            if (entity.HasComponent<CameraComponent>()) return "[Camera]";
            if (entity.HasComponent<MeshRendererComponent>()) return "[3D]";
            if (entity.HasComponent<SpriteRendererComponent>() || entity.HasComponent<CircleRendererComponent>()) return "[2D]";
            return "[Entity]";
        }

        static uint32_t EntityKey(Entity entity)
        {
            return static_cast<uint32_t>(static_cast<entt::entity>(entity));
        }

        static constexpr const char* kHierarchyUIEntityPayload = "WT_HIERARCHY_UI_ENTITY";

    } // namespace
    bool SceneHierarchyPanel::EntityPassesFilter(Entity entity) const
    {
        if (!entity || !entity.HasComponent<TagComponent>())
            return false;

        if (m_ShowOnlyUI && !entity.HasComponent<UIWidgetComponent>())
            return false;

        const std::string& tag = entity.GetComponent<TagComponent>().Tag;
        return ContainsInsensitive(tag, m_SearchBuffer);
    }

    bool SceneHierarchyPanel::EntityOrDescendantPassesFilter(Entity entity,
        const UIChildMap& childMap,
        std::unordered_set<uint32_t>& visiting) const
    {
        if (!entity)
            return false;

        const uint32_t key = EntityKey(entity);
        if (!visiting.insert(key).second)
            return false;

        bool passes = EntityPassesFilter(entity);
        if (!passes)
        {
            if (auto it = childMap.find(key); it != childMap.end())
            {
                for (Entity child : it->second)
                {
                    if (EntityOrDescendantPassesFilter(child, childMap, visiting))
                    {
                        passes = true;
                        break;
                    }
                }
            }
        }

        visiting.erase(key);
        return passes;
    }

    bool SceneHierarchyPanel::IsUIDescendantOf(Entity parent,
        Entity candidate,
        const UIChildMap& childMap,
        std::unordered_set<uint32_t>& visiting) const
    {
        if (!parent || !candidate || parent == candidate)
            return false;

        const uint32_t parentKey = EntityKey(parent);
        if (!visiting.insert(parentKey).second)
            return false;

        bool found = false;
        if (auto it = childMap.find(parentKey); it != childMap.end())
        {
            for (Entity child : it->second)
            {
                if (child == candidate || IsUIDescendantOf(child, candidate, childMap, visiting))
                {
                    found = true;
                    break;
                }
            }
        }

        visiting.erase(parentKey);
        return found;
    }

    bool SceneHierarchyPanel::CanReparentUI(Entity child,
        Entity parent,
        const UIChildMap& childMap) const
    {
        if (!m_Context || !child || !parent || child == parent)
            return false;
        if (!child.HasComponent<UIWidgetComponent>() || !parent.HasComponent<UIWidgetComponent>())
            return false;
        if (!child.HasComponent<IDComponent>() || !parent.HasComponent<IDComponent>())
            return false;
        if (child.HasComponent<UICanvasComponent>())
            return false;

        std::unordered_set<uint32_t> visiting;
        if (IsUIDescendantOf(child, parent, childMap, visiting))
            return false;

        const auto& currentWidget = child.GetComponent<UIWidgetComponent>();
        return currentWidget.ParentEntity != parent.GetUUID();
    }

    void SceneHierarchyPanel::ReparentUIWithUndo(Entity child,
        Entity parent,
        const UIChildMap& childMap)
    {
        if (!CanReparentUI(child, parent, childMap))
            return;

        auto& widget = child.GetComponent<UIWidgetComponent>();
        UIWidgetComponent before = widget;
        UIWidgetComponent after = before;

        UIWidgetLayout::Context layout(m_Context.get());
        const UIWidgetLayout::Rect childRect = UIWidgetLayout::ResolveRect(layout, static_cast<entt::entity>(child));
        const UIWidgetLayout::Rect parentRect = UIWidgetLayout::ResolveRect(layout, static_cast<entt::entity>(parent));
        const float parentWidth = parentRect.Right - parentRect.Left;
        const float parentHeight = parentRect.Bottom - parentRect.Top;
        if (parentWidth <= 0.0001f || parentHeight <= 0.0001f)
            return;

        after.Anchor = UIAnchor::TopLeft;
        after.ParentEntity = parent.GetUUID();
        after.Position = {
            (childRect.Left - parentRect.Left) / parentWidth,
            (childRect.Top - parentRect.Top) / parentHeight
        };
        after.Size = {
            std::max(0.001f, (childRect.Right - childRect.Left) / parentWidth),
            std::max(0.001f, (childRect.Bottom - childRect.Top) / parentHeight)
        };

        auto command = MakeComponentValueCommand(child, before, after);
        command->Execute();
        CommandHistory::Get().Push(std::move(command));
        m_SelectionContext = child;
        m_ScrollToSelection = true;
    }

    std::vector<Entity> SceneHierarchyPanel::CollectUIChildrenRecursive(Entity entity,
        const UIChildMap& childMap) const
    {
        std::vector<Entity> result;
        std::unordered_set<uint32_t> visited;

        std::function<void(Entity)> collect = [&](Entity parent)
        {
            const uint32_t parentKey = EntityKey(parent);
            auto it = childMap.find(parentKey);
            if (it == childMap.end())
                return;

            for (Entity child : it->second)
            {
                const uint32_t childKey = EntityKey(child);
                if (!visited.insert(childKey).second)
                    continue;

                result.push_back(child);
                collect(child);
            }
        };

        collect(entity);
        return result;
    }

    void SceneHierarchyPanel::MarkUIDescendantsDrawn(Entity entity,
        const UIChildMap& childMap,
        std::unordered_set<uint32_t>& drawn,
        std::unordered_set<uint32_t>& visiting) const
    {
        if (!entity)
            return;

        const uint32_t key = EntityKey(entity);
        if (!visiting.insert(key).second)
            return;

        if (auto it = childMap.find(key); it != childMap.end())
        {
            for (Entity child : it->second)
            {
                if (!child)
                    continue;

                drawn.insert(EntityKey(child));
                MarkUIDescendantsDrawn(child, childMap, drawn, visiting);
            }
        }

        visiting.erase(key);
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity entity,
        const UIChildMap& childMap,
        std::unordered_set<uint32_t>& drawn,
        bool& selectionVisible)
    {
        if (!entity || !entity.HasComponent<TagComponent>())
            return;

        const uint32_t key = EntityKey(entity);
        if (drawn.find(key) != drawn.end())
            return;

        std::unordered_set<uint32_t> filterVisiting;
        if (!EntityOrDescendantPassesFilter(entity, childMap, filterVisiting))
            return;

        drawn.insert(key);

        const auto& tag = entity.GetComponent<TagComponent>().Tag;
        const std::string displayName = std::string(EntityKindPrefix(entity)) + " " + tag;
        bool hiddenInEditor = IsEntityHiddenInEditor(entity);

        std::vector<Entity> visibleChildren;
        if (auto it = childMap.find(key); it != childMap.end())
        {
            for (Entity child : it->second)
            {
                std::unordered_set<uint32_t> childFilterVisiting;
                if (EntityOrDescendantPassesFilter(child, childMap, childFilterVisiting))
                    visibleChildren.push_back(child);
            }
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (visibleChildren.empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        else if (entity.HasComponent<UICanvasComponent>())
            flags |= ImGuiTreeNodeFlags_DefaultOpen;

        const bool selected = m_SelectionContext == entity;
        std::unordered_set<uint32_t> canvasSelectionVisiting;
        const bool canvasOwnsSelection = entity.HasComponent<UICanvasComponent>()
            && IsUIDescendantOf(entity, m_SelectionContext, childMap, canvasSelectionVisiting);

        if (selected || canvasOwnsSelection)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
            if (selected)
                selectionVisible = true;
        }

        if (canvasOwnsSelection && !selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.28f, 0.25f, 0.72f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.36f, 0.32f, 0.82f));
        }

        ImGui::PushID(static_cast<int>(key));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 1.0f));
        if (ImGui::SmallButton(hiddenInEditor ? "Show" : "Hide"))
        {
            SetEntityHiddenInEditor(entity, !hiddenInEditor);
            hiddenInEditor = !hiddenInEditor;
        }
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(hiddenInEditor ? "Show this entity in the editor" : "Hide this entity in the editor");
        ImGui::PopID();
        ImGui::SameLine();

        if (hiddenInEditor)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

        const bool inlineRenaming = selected && m_RenameRequested && entity == m_SelectionContext;
        if (inlineRenaming)
            std::strncpy(m_RenameBuffer, displayName.c_str(), sizeof(m_RenameBuffer) - 1);

        const bool opened = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<uint32_t>(entity))),
            flags,
            "%s", inlineRenaming ? "" : displayName.c_str()
        );

        if (inlineRenaming)
        {
            // Inline rename: Enter commits, Esc or clicking elsewhere cancels.
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
            ImGui::SetKeyboardFocusHere();
            const bool committed = ImGui::InputText("##inlineRename", m_RenameBuffer,
                sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape);
            const bool deactivated = ImGui::IsItemDeactivated();
            if (committed)
            {
                entity.GetComponent<TagComponent>().Tag = m_RenameBuffer;
                if (Scene* scene = entity.GetScene())
                    scene->InvalidateEntityLookupCache();
                m_RenameRequested = false;
            }
            else if (cancelled || (deactivated && !committed))
            {
                m_RenameRequested = false;
            }
        }

        if (hiddenInEditor)
            ImGui::PopStyleColor();

        if (canvasOwnsSelection && !selected)
            ImGui::PopStyleColor(2);

        if (ImGui::IsItemClicked())
            m_SelectionContext = entity;

        if (entity.HasComponent<UIWidgetComponent>()
            && entity.HasComponent<IDComponent>()
            && !entity.HasComponent<UICanvasComponent>())
        {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const uint64_t payload = static_cast<uint64_t>(entity.GetUUID());
                ImGui::SetDragDropPayload(kHierarchyUIEntityPayload, &payload, sizeof(payload));
                ImGui::TextUnformatted(displayName.c_str());
                ImGui::EndDragDropSource();
            }
        }

        if (entity.HasComponent<UIWidgetComponent>() && entity.HasComponent<IDComponent>())
        {
            if (ImGui::BeginDragDropTarget())
            {
                const ImGuiPayload* previewPayload = ImGui::GetDragDropPayload();
                if (previewPayload
                    && previewPayload->IsDataType(kHierarchyUIEntityPayload)
                    && previewPayload->DataSize == sizeof(uint64_t))
                {
                    const uint64_t childID = *static_cast<const uint64_t*>(previewPayload->Data);
                    Entity child = m_Context ? m_Context->GetEntityByUUID(UUID(childID)) : Entity{};
                    if (CanReparentUI(child, entity, childMap))
                    {
                        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                            kHierarchyUIEntityPayload,
                            ImGuiDragDropFlags_AcceptBeforeDelivery);
                        if (payload && payload->IsDelivery())
                            ReparentUIWithUndo(child, entity, childMap);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            m_SelectionContext = entity;
            if (m_EntityActivatedCallback)
                m_EntityActivatedCallback(entity);
        }

        if (selected && m_ScrollToSelection)
        {
            ImGui::SetScrollHereY(0.5f);
            m_ScrollToSelection = false;
        }

        bool entityDeleted = false;

        if (ImGui::BeginPopupContextItem())
        {
            m_SelectionContext = entity;

            const UUID uiParentID = ResolveUIParentID(entity);
            if (static_cast<uint64_t>(uiParentID) != 0)
            {
                if (ImGui::BeginMenu("Create UI"))
                {
                    DrawCreateUIMenuItems(uiParentID, false);
                    ImGui::EndMenu();
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem(EditorLocale::Text("Rename", "重命名")))
            {
                m_RenameRequested = true;
            }

            if (ImGui::MenuItem(hiddenInEditor ? "Show in Editor" : "Hide in Editor"))
            {
                SetEntityHiddenInEditor(entity, !hiddenInEditor);
                hiddenInEditor = !hiddenInEditor;
            }

            if (ImGui::MenuItem(EditorLocale::Text("Duplicate Entity", "复制实体")))
            {
                auto command = std::make_unique<EntityDuplicateCommand>(m_Context.get(), entity);
                command->Execute();
                m_SelectionContext = command->GetEntity();
                CommandHistory::Get().Push(std::move(command));
            }

            ImGui::Separator();

            if (ImGui::MenuItem(EditorLocale::Text("Save as Prefab", "另存为 Prefab")))
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

            if (ImGui::MenuItem(EditorLocale::Text("Save as UI Template", "另存为 UI 模板")))
            {
                // Composite UI templates are .wtuit files embedding a Prefab v2
                // body; designers use this to author reusable widget bundles (new
                // HUD elements, popups, slots) without touching C++ or YAML.
                std::filesystem::path templateDir = AssetPath::Resolve("assets/ui_templates");
                std::filesystem::create_directories(templateDir);

                std::string baseName = entity.GetName();
                if (baseName.empty())
                    baseName = "UI_Template";
                std::filesystem::path savePath;

                int index = 0;
                do
                {
                    std::string filename = baseName;
                    if (index > 0)
                        filename += std::to_string(index);
                    filename += AssetFileType::UITemplateExtension;
                    savePath = templateDir / filename;
                    index++;
                } while (std::filesystem::exists(savePath));

                if (SceneSerializer::SerializeUITemplate(entity, savePath,
                        baseName,
                        "Composite",
                        "Designer-authored UI template."))
                {
                    WT_CORE_INFO("Saved UI template: {}", savePath.string());
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem(EditorLocale::Text("Delete Entity", "删除实体")))
                entityDeleted = true;

            ImGui::EndPopup();
        }

        if (opened)
        {
            for (Entity child : visibleChildren)
                DrawEntityNode(child, childMap, drawn, selectionVisible);

            if (!visibleChildren.empty())
                ImGui::TreePop();
        }
        else if (!visibleChildren.empty())
        {
            std::unordered_set<uint32_t> visiting;
            MarkUIDescendantsDrawn(entity, childMap, drawn, visiting);
        }

        if (entityDeleted)
        {
            auto deleteTargets = CollectUIChildrenRecursive(entity, childMap);
            deleteTargets.push_back(entity);

            const bool selectionWillBeDeleted = std::any_of(
                deleteTargets.begin(),
                deleteTargets.end(),
                [&](Entity target) { return target == m_SelectionContext; });

            if (selectionWillBeDeleted)
                m_SelectionContext = {};

            auto composite = std::make_unique<CompositeCommand>();
            for (Entity target : deleteTargets)
            {
                if (!target)
                    continue;

                auto command = std::make_unique<EntityCreateCommand>(m_Context.get(), target, false);
                command->Execute();
                composite->Add(std::move(command));
            }

            if (!composite->Empty())
                CommandHistory::Get().Push(std::move(composite));
        }
    }

} // namespace Wheatear
