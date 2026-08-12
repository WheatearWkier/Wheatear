#include "wepch.h"
#include "UITemplateFactory.h"

#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/EntityReference.h"
#include "Wheatear/Scene/SceneSerializer.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>

namespace Wheatear {

    namespace {

        struct TemplateBuildContext
        {
            Scene* ScenePtr = nullptr;
            UUID ParentID = 0;
            std::string Prefix;
            std::vector<Entity> Entities;
        };

        static std::string DefaultPrefix(UITemplateKind kind)
        {
            switch (kind)
            {
            case UITemplateKind::TitledScrollText: return "UI_TitledScrollText";
            case UITemplateKind::PagedGrid: return "UI_PagedGrid";
            case UITemplateKind::PagedInventoryGrid: return "UI_PagedInventoryGrid";
            case UITemplateKind::SkillButton: return "UI_SkillButton";
            case UITemplateKind::EquipmentSlot: return "UI_EquipmentSlot";
            case UITemplateKind::Tooltip: return "UI_Tooltip";
            case UITemplateKind::SaveSlot: return "UI_SaveSlot";
            case UITemplateKind::SkillTreeNode: return "UI_SkillTreeNode";
            case UITemplateKind::CombatSkillSlot: return "UI_CombatSkillSlot";
            default: return "UI_Template";
            }
        }

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

        static Entity CreateNode(TemplateBuildContext& context, const std::string& suffix)
        {
            Entity entity = context.ScenePtr->CreateEntityWithUUID(UUID(), context.Prefix + suffix);
            context.Entities.push_back(entity);
            return entity;
        }

        static void ConfigureWidget(Entity entity,
            UUID parentID,
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
            widget.ParentEntity = parentID;
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
            text.FontPath = AssetAliasRegistry::Path("font.ui_default");
            text.ShadowColor = { 0.01f, 0.015f, 0.018f, 0.80f };
            text.ShadowOffset = { 1.5f, 1.5f };
            text.OutlineColor = { 0.0f, 0.0f, 0.0f, 0.84f };
            text.OutlineThickness = 1.0f;
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

        static void ConfigureImage(Entity entity,
            const std::string& texturePath,
            glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f })
        {
            auto& image = entity.AddOrReplaceComponent<UIImageComponent>();
            image.Color = color;
            if (!texturePath.empty())
                image.Texture = Texture2D::Create(texturePath);
        }

        static void ConfigurePageItem(Entity entity, UUID pagerID, int page)
        {
            auto& pageItem = entity.AddOrReplaceComponent<UIPageItemComponent>();
            pageItem.PagerEntity = pagerID;
            pageItem.Page = std::max(page, 1);
        }

        static Entity CreateRoot(TemplateBuildContext& context,
            glm::vec2 position,
            glm::vec2 size,
            glm::vec4 background,
            glm::vec4 border)
        {
            Entity root = CreateNode(context, "");
            ConfigureWidget(root, context.ParentID, position, size, 30);
            ConfigurePanel(root, background, border, 1.6f);
            return root;
        }

        static void CreateTitledScrollText(TemplateBuildContext& context)
        {
            Entity root = CreateRoot(context,
                { 0.12f, 0.14f },
                { 0.46f, 0.52f },
                { 0.035f, 0.042f, 0.048f, 0.90f },
                { 0.45f, 0.78f, 0.76f, 0.86f });

            Entity title = CreateNode(context, "_Title");
            ConfigureWidget(title, root.GetUUID(), { 0.045f, 0.035f }, { 0.78f, 0.12f }, 42);
            ConfigureText(title, "标题", 24.0f, { 0.68f, 0.96f, 0.92f, 1.0f });

            Entity scroll = CreateNode(context, "_ScrollView");
            ConfigureWidget(scroll, root.GetUUID(), { 0.045f, 0.18f }, { 0.86f, 0.74f }, 36);
            ConfigurePanel(scroll, { 0.012f, 0.016f, 0.018f, 0.50f }, { 0.24f, 0.44f, 0.44f, 0.58f }, 1.0f, true);
            auto& scrollView = scroll.AddOrReplaceComponent<UIScrollViewComponent>();
            scrollView.ContentHeight = 2.0f;
            scrollView.WheelStep = 0.08f;
            scrollView.ScrollbarWidth = 0.018f;

            Entity body = CreateNode(context, "_BodyText");
            ConfigureWidget(body, scroll.GetUUID(), { 0.035f, 0.035f }, { 0.84f, 1.68f }, 44);
            ConfigureText(body, "这里放长文本。鼠标滚轮或拖动滚动条查看内容。", 18.0f);
        }

        static void CreatePagedGrid(TemplateBuildContext& context, bool inventoryStyle)
        {
            Entity root = CreateRoot(context,
                { 0.12f, 0.14f },
                { 0.56f, 0.56f },
                inventoryStyle ? glm::vec4{ 0.045f, 0.038f, 0.032f, 0.90f } : glm::vec4{ 0.032f, 0.040f, 0.050f, 0.90f },
                inventoryStyle ? glm::vec4{ 0.80f, 0.62f, 0.32f, 0.86f } : glm::vec4{ 0.40f, 0.74f, 0.76f, 0.84f });

            Entity pager = CreateNode(context, "_Pager");
            ConfigureWidget(pager, root.GetUUID(), { 0.0f, 0.0f }, { 0.01f, 0.01f }, 0, false);
            auto& pagerComponent = pager.AddOrReplaceComponent<UIPagerComponent>();
            pagerComponent.PageCount = 2;
            pagerComponent.CurrentPage = 1;

            const std::string pagerSelector = EntityReferences::MakeUUIDSelector(pager.GetUUID());

            Entity pageText = CreateNode(context, "_PageText");
            ConfigureWidget(pageText, root.GetUUID(), { 0.41f, 0.88f }, { 0.16f, 0.07f }, 45);
            ConfigureText(pageText, "1 / 2", 18.0f, { 0.94f, 0.90f, 0.76f, 1.0f });

            Entity prev = CreateNode(context, "_PrevButton");
            ConfigureWidget(prev, root.GetUUID(), { 0.28f, 0.88f }, { 0.08f, 0.07f }, 55);
            ConfigureButton(prev, "ui:pager:" + pagerSelector + ":prev");
            ConfigureText(prev, "<", 20.0f);

            Entity next = CreateNode(context, "_NextButton");
            ConfigureWidget(next, root.GetUUID(), { 0.60f, 0.88f }, { 0.08f, 0.07f }, 55);
            ConfigureButton(next, "ui:pager:" + pagerSelector + ":next");
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
                Entity frame = CreateNode(context, "_Slot_" + std::to_string(i + 1));
                ConfigureWidget(frame, root.GetUUID(), positions[i], { 0.13f, 0.20f }, 34);
                ConfigurePanel(frame,
                    inventoryStyle ? glm::vec4{ 0.020f, 0.024f, 0.026f, 0.82f } : glm::vec4{ 0.030f, 0.040f, 0.052f, 0.74f },
                    inventoryStyle ? glm::vec4{ 0.72f, 0.56f, 0.30f, 0.78f } : glm::vec4{ 0.36f, 0.66f, 0.72f, 0.70f },
                    1.4f);
                ConfigureButton(frame, "");
                ConfigurePageItem(frame, pager.GetUUID(), page);

                Entity label = CreateNode(context, "_SlotText_" + std::to_string(i + 1));
                ConfigureWidget(label, root.GetUUID(), positions[i] + glm::vec2{ 0.018f, 0.070f }, { 0.094f, 0.06f }, 46);
                ConfigureText(label, inventoryStyle ? "道具" : "Item", 16.0f);
                ConfigurePageItem(label, pager.GetUUID(), page);
            }
        }

        static void CreateSkillButton(TemplateBuildContext& context)
        {
            Entity root = CreateRoot(context,
                { 0.20f, 0.20f },
                { 0.11f, 0.15f },
                { 0.020f, 0.028f, 0.030f, 0.86f },
                { 0.36f, 0.90f, 0.72f, 0.92f });
            ConfigureButton(root, "progression:select_skill_node:sample");

            Entity icon = CreateNode(context, "_Icon");
            ConfigureWidget(icon, root.GetUUID(), { 0.16f, 0.08f }, { 0.68f, 0.62f }, 42);
            ConfigureImage(icon, AssetAliasRegistry::Path("ui.template.skill_slash"));

            Entity label = CreateNode(context, "_Hotkey");
            ConfigureWidget(label, root.GetUUID(), { 0.08f, 0.74f }, { 0.84f, 0.20f }, 45);
            ConfigureText(label, "技能", 14.0f, { 0.76f, 1.0f, 0.92f, 1.0f });
        }

        static void CreateEquipmentSlot(TemplateBuildContext& context)
        {
            Entity root = CreateRoot(context,
                { 0.20f, 0.20f },
                { 0.12f, 0.16f },
                { 0.030f, 0.026f, 0.022f, 0.88f },
                { 0.82f, 0.64f, 0.30f, 0.92f });
            ConfigureButton(root, "progression:select_equipment:sample");

            Entity icon = CreateNode(context, "_Icon");
            ConfigureWidget(icon, root.GetUUID(), { 0.15f, 0.08f }, { 0.70f, 0.58f }, 42);
            ConfigureImage(icon, AssetAliasRegistry::Path("ui.template.equipment_training_blade"));

            Entity rarity = CreateNode(context, "_Rarity");
            ConfigureWidget(rarity, root.GetUUID(), { 0.09f, 0.72f }, { 0.82f, 0.18f }, 45);
            ConfigureText(rarity, "R1", 14.0f, { 1.0f, 0.86f, 0.48f, 1.0f });
        }

        static void CreateTooltip(TemplateBuildContext& context)
        {
            Entity root = CreateRoot(context,
                { 0.36f, 0.20f },
                { 0.28f, 0.20f },
                { 0.018f, 0.021f, 0.024f, 0.94f },
                { 0.56f, 0.88f, 0.82f, 0.84f });

            Entity title = CreateNode(context, "_Title");
            ConfigureWidget(title, root.GetUUID(), { 0.07f, 0.08f }, { 0.86f, 0.22f }, 42);
            ConfigureText(title, "名称", 17.0f, { 0.72f, 1.0f, 0.92f, 1.0f });

            Entity body = CreateNode(context, "_Body");
            ConfigureWidget(body, root.GetUUID(), { 0.07f, 0.34f }, { 0.86f, 0.54f }, 43);
            ConfigureText(body, "悬浮说明写在这里。", 15.0f);
        }

        static void CreateSaveSlot(TemplateBuildContext& context)
        {
            Entity root = CreateRoot(context,
                { 0.16f, 0.20f },
                { 0.48f, 0.13f },
                { 0.035f, 0.045f, 0.050f, 0.88f },
                { 0.42f, 0.78f, 0.82f, 0.84f });
            ConfigureButton(root, "progression:save:1");

            Entity icon = CreateNode(context, "_Icon");
            ConfigureWidget(icon, root.GetUUID(), { 0.04f, 0.17f }, { 0.10f, 0.62f }, 42);
            ConfigureImage(icon, AssetAliasRegistry::Path("ui.template.save_slot"));

            Entity title = CreateNode(context, "_Title");
            ConfigureWidget(title, root.GetUUID(), { 0.18f, 0.16f }, { 0.72f, 0.26f }, 43);
            ConfigureText(title, "存档 1", 18.0f, { 0.76f, 1.0f, 0.94f, 1.0f });

            Entity detail = CreateNode(context, "_Detail");
            ConfigureWidget(detail, root.GetUUID(), { 0.18f, 0.52f }, { 0.72f, 0.24f }, 43);
            ConfigureText(detail, "章节 / 等级 / 时间", 14.0f);
        }

        static void CreateSkillTreeNode(TemplateBuildContext& context)
        {
            Entity root = CreateRoot(context,
                { 0.34f, 0.28f },
                { 0.10f, 0.14f },
                { 0.025f, 0.038f, 0.034f, 0.92f },
                { 0.34f, 0.96f, 0.68f, 0.96f });
            ConfigureButton(root, "progression:select_skill_node:sample");

            Entity icon = CreateNode(context, "_Icon");
            ConfigureWidget(icon, root.GetUUID(), { 0.17f, 0.11f }, { 0.66f, 0.54f }, 42);
            ConfigureImage(icon, AssetAliasRegistry::Path("ui.template.skill_tree_core"));

            Entity name = CreateNode(context, "_Name");
            ConfigureWidget(name, root.GetUUID(), { 0.07f, 0.73f }, { 0.86f, 0.18f }, 43);
            ConfigureText(name, "节点", 13.0f, { 0.72f, 1.0f, 0.88f, 1.0f });
        }

        static void CreateCombatSkillSlot(TemplateBuildContext& context)
        {
            Entity root = CreateRoot(context,
                { 0.36f, 0.82f },
                { 0.075f, 0.105f },
                { 0.014f, 0.022f, 0.026f, 0.92f },
                { 0.20f, 0.84f, 0.88f, 0.90f });
            ConfigureButton(root, "sidecombat:skill:sample");

            Entity icon = CreateNode(context, "_Icon");
            ConfigureWidget(icon, root.GetUUID(), { 0.12f, 0.09f }, { 0.76f, 0.58f }, 42);
            ConfigureImage(icon, AssetAliasRegistry::Path("ui.template.combat_launcher_slash"));

            Entity overlay = CreateNode(context, "_CooldownMask");
            ConfigureWidget(overlay, root.GetUUID(), { 0.0f, 0.0f }, { 1.0f, 1.0f }, 44, false);
            ConfigurePanel(overlay, { 0.0f, 0.0f, 0.0f, 0.58f }, { 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);

            Entity cooldownText = CreateNode(context, "_CooldownText");
            ConfigureWidget(cooldownText, root.GetUUID(), { 0.20f, 0.34f }, { 0.60f, 0.26f }, 45, false);
            ConfigureText(cooldownText, "3.2", 16.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

            Entity keyText = CreateNode(context, "_Key");
            ConfigureWidget(keyText, root.GetUUID(), { 0.08f, 0.72f }, { 0.84f, 0.20f }, 46);
            ConfigureText(keyText, "U", 14.0f, { 0.74f, 1.0f, 1.0f, 1.0f });
        }

        static void WriteTemplateAsset(const std::filesystem::path& path, const UITemplateDescriptor& descriptor)
        {
            std::filesystem::create_directories(path.parent_path());

            YAML::Emitter out;
            out << YAML::BeginMap;
            out << YAML::Key << "UITemplate" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Version" << YAML::Value << 1;
            out << YAML::Key << "Kind" << YAML::Value << YAML::DoubleQuoted << descriptor.KindName;
            out << YAML::Key << "DisplayName" << YAML::Value << YAML::DoubleQuoted << descriptor.DisplayName;
            out << YAML::Key << "Category" << YAML::Value << YAML::DoubleQuoted << descriptor.Category;
            out << YAML::Key << "Description" << YAML::Value << YAML::DoubleQuoted << descriptor.Description;
            out << YAML::EndMap;
            out << YAML::EndMap;

            std::ofstream output(path, std::ios::binary);
            if (output.is_open())
                output << out.c_str();
        }

    } // namespace

    std::vector<UITemplateDescriptor> UITemplateFactory::GetBuiltinTemplates()
    {
        return {
            { UITemplateKind::TitledScrollText, "TitledScrollText", "Titled Scroll Text", "Containers", "Panel with a title and continuous scroll text.", "assets/ui_templates/titled_scroll_text.wtuit" },
            { UITemplateKind::PagedGrid, "PagedGrid", "Paged Grid", "Containers", "Known-size paged grid for gallery, quest, mail, and records.", "assets/ui_templates/paged_grid.wtuit" },
            { UITemplateKind::PagedInventoryGrid, "PagedInventoryGrid", "Paged Inventory Grid", "Inventory", "Inventory-style paged item grid with item slots.", "assets/ui_templates/paged_inventory_grid.wtuit" },
            { UITemplateKind::SkillButton, "SkillButton", "Skill Button", "Progression", "Reusable skill icon button with text label.", "assets/ui_templates/skill_button.wtuit" },
            { UITemplateKind::EquipmentSlot, "EquipmentSlot", "Equipment Slot", "Inventory", "Reusable equipment slot with icon and rarity text.", "assets/ui_templates/equipment_slot.wtuit" },
            { UITemplateKind::Tooltip, "Tooltip", "Tooltip", "Common", "Compact hover/detail tooltip panel.", "assets/ui_templates/tooltip.wtuit" },
            { UITemplateKind::SaveSlot, "SaveSlot", "Save Slot", "System", "Reusable save/load slot row.", "assets/ui_templates/save_slot.wtuit" },
            { UITemplateKind::SkillTreeNode, "SkillTreeNode", "Skill Tree Node", "Progression", "Reusable icon node for skill graph authoring.", "assets/ui_templates/skill_tree_node.wtuit" },
            { UITemplateKind::CombatSkillSlot, "CombatSkillSlot", "Combat Skill Slot", "HUD", "Battle HUD skill slot with cooldown mask and key label.", "assets/ui_templates/combat_skill_slot.wtuit" }
        };
    }

    const UITemplateDescriptor* UITemplateFactory::FindBuiltinTemplate(UITemplateKind kind)
    {
        static const std::vector<UITemplateDescriptor> templates = GetBuiltinTemplates();
        for (const auto& descriptor : templates)
            if (descriptor.Kind == kind)
                return &descriptor;
        return nullptr;
    }

    UITemplateKind UITemplateFactory::KindFromString(const std::string& value)
    {
        if (value == "Composite")
            return UITemplateKind::Composite;
        for (const auto& descriptor : GetBuiltinTemplates())
            if (descriptor.KindName == value)
                return descriptor.Kind;
        return UITemplateKind::Unknown;
    }

    std::string UITemplateFactory::KindToString(UITemplateKind kind)
    {
        if (kind == UITemplateKind::Composite)
            return "Composite";
        if (const UITemplateDescriptor* descriptor = FindBuiltinTemplate(kind))
            return descriptor->KindName;
        return "Unknown";
    }

    std::vector<Entity> UITemplateFactory::Create(Scene* scene,
        UITemplateKind kind,
        UUID parentID,
        const std::string& namePrefix)
    {
        if (!scene || kind == UITemplateKind::Unknown)
            return {};

        TemplateBuildContext context;
        context.ScenePtr = scene;
        context.ParentID = parentID;
        context.Prefix = MakeUniqueEntityName(scene, namePrefix.empty() ? DefaultPrefix(kind) : namePrefix);

        switch (kind)
        {
        case UITemplateKind::TitledScrollText: CreateTitledScrollText(context); break;
        case UITemplateKind::PagedGrid: CreatePagedGrid(context, false); break;
        case UITemplateKind::PagedInventoryGrid: CreatePagedGrid(context, true); break;
        case UITemplateKind::SkillButton: CreateSkillButton(context); break;
        case UITemplateKind::EquipmentSlot: CreateEquipmentSlot(context); break;
        case UITemplateKind::Tooltip: CreateTooltip(context); break;
        case UITemplateKind::SaveSlot: CreateSaveSlot(context); break;
        case UITemplateKind::SkillTreeNode: CreateSkillTreeNode(context); break;
        case UITemplateKind::CombatSkillSlot: CreateCombatSkillSlot(context); break;
        default: break;
        }

        return context.Entities;
    }

    std::vector<Entity> UITemplateFactory::CreateFromAsset(Scene* scene,
        const std::filesystem::path& assetPath,
        UUID parentID)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(assetPath);
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(resolvedPath.string());
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("UITemplateFactory: failed to load '{}': {}", resolvedPath.string(), e.what());
            return {};
        }

        YAML::Node node = root["UITemplate"];
        if (!node || node["Version"].as<int>(0) != 1)
        {
            WT_CORE_ERROR("UITemplateFactory: invalid template asset '{}'", resolvedPath.string());
            return {};
        }

        const UITemplateKind kind = KindFromString(node["Kind"].as<std::string>(""));
        const std::string displayName = node["DisplayName"].as<std::string>("");

        // Composite templates carry an embedded Prefab Version 2 body authored by
        // the Hierarchy "Save selection as UI Template" action. Deserialize it
        // through the prefab path and reparent the lone root back onto parentID,
        // matching how the C++ builders attach their root widget to the canvas.
        if (kind == UITemplateKind::Composite || root["Entities"])
        {
            std::vector<Entity> entities = SceneSerializer::DeserializePrefabEntities(resolvedPath, scene);
            if (entities.empty())
                return {};

            Entity rootEntity;
            for (Entity e : entities)
            {
                if (e.HasComponent<UIWidgetComponent>())
                {
                    UIWidgetComponent& widget = e.GetComponent<UIWidgetComponent>();
                    // A prefab root carries no parent (ParentEntity == 0); children
                    // already had their intra-tree references remapped. Adopt the
                    // first orphan we find as the root and attach it to the canvas.
                    if (widget.ParentEntity == 0)
                    {
                        if (!rootEntity)
                            rootEntity = e;
                        widget.ParentEntity = parentID;
                    }
                }
            }

            // Fallback: if no orphaned root was found (unexpected), attach the
            // first entity so the composite still lands under the chosen parent.
            if (!rootEntity && !entities.empty() && entities.front().HasComponent<UIWidgetComponent>())
                entities.front().GetComponent<UIWidgetComponent>().ParentEntity = parentID;

            return entities;
        }

        return Create(scene, kind, parentID, displayName.empty() ? std::string{} : "UI_" + displayName);
    }

    bool UITemplateFactory::WriteBuiltinTemplateAssets(const std::filesystem::path& projectRoot)
    {
        const std::filesystem::path root = projectRoot.empty() ? AssetPath::GetProjectRoot() : projectRoot;
        for (const auto& descriptor : GetBuiltinTemplates())
        {
            const std::filesystem::path path = root / descriptor.DefaultAssetPath;
            if (!std::filesystem::is_regular_file(path))
                WriteTemplateAsset(path, descriptor);
        }
        return true;
    }

} // namespace Wheatear
