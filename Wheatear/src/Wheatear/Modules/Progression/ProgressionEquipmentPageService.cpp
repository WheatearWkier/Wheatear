#include "wtpch.h"
#include "ProgressionEquipmentPageService.h"

#include "GameProgress.h"
#include "ProgressionContent.h"
#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"
#include "Wheatear/UI/UIWidgetLayout.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace Wheatear::ProgressionEquipmentPageService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::IsButtonHovered;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        using GameplayUILayoutService::FindAuthoredScrollView;
        using GameplayUILayoutService::SetButtonCommand;

        struct EquipmentSlotView
        {
            const char* SlotId;
            const char* IconTag;
            const char* ButtonTag;
            glm::vec2 FramePosition;
        };

        struct BagLayout
        {
            glm::vec2 Origin = { 0.385f, 0.335f };
            glm::vec2 FrameSize = { 0.075f, 0.098f };
            glm::vec2 IconOffset = { 0.010f, 0.011f };
            glm::vec2 IconSize = { 0.055f, 0.075f };
            glm::vec2 Step = { 0.105f, 0.135f };
        };

        static constexpr int kBagItemsPerPage = 4;
        static constexpr int kBagColumns = 2;
        static constexpr const char* kDynamicBagItemPrefix = "Equipment_DynamicItem_";
        static constexpr const char* kBagPanelName = "Equipment_BagPanel";
        static constexpr const char* kBagGroupName = "Equipment_BagGroup";

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        static void SetImageTexture(Scene* scene, const std::string& entityName, const std::string& texturePath)
        {
            UIRuntimeTools::SetImageTexture(scene, entityName, texturePath, true);
        }

        static glm::vec2 GetWidgetTopLeft(Scene* scene, const std::string& entityName, glm::vec2 fallback)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UIWidgetComponent>())
                return fallback;

            const UIWidgetLayout::Rect rect = UIWidgetLayout::WidgetToLocalRect(entity.GetComponent<UIWidgetComponent>());
            return { rect.Left, rect.Top };
        }

        static BagLayout ResolveBagLayout(Scene* scene)
        {
            BagLayout layout;
            if (Entity panel = FindEntityByName(scene, kBagPanelName);
                panel && panel.HasComponent<UIWidgetComponent>())
            {
                const UIWidgetLayout::Rect panelRect =
                    UIWidgetLayout::WidgetToLocalRect(panel.GetComponent<UIWidgetComponent>());
                layout.Origin = { panelRect.Left + 0.035f, panelRect.Top + 0.085f };
                layout.Step = {
                    std::max(0.090f, (panelRect.Right - panelRect.Left) * 0.350f),
                    std::max(0.120f, (panelRect.Bottom - panelRect.Top) * 0.273f)
                };
            }
            return layout;
        }

        static std::vector<std::string> BuildBagEquipment()
        {
            std::vector<std::string> bagEquipment;
            const auto& equipmentCatalog = ProgressionContent::Get().Equipment;
            bagEquipment.reserve(equipmentCatalog.size());
            for (const auto& equipment : equipmentCatalog)
            {
                if (GameProgress::IsEquipmentOwned(equipment.Id)
                    && !GameProgress::IsEquipmentEquipped(equipment.Id))
                {
                    bagEquipment.emplace_back(equipment.Id);
                }
            }
            return bagEquipment;
        }

        static int GetBagPageCount(size_t itemCount)
        {
            return std::max(1, static_cast<int>((itemCount + kBagItemsPerPage - 1) / kBagItemsPerPage));
        }

        static UUID FindParentUUID(Scene* scene, const std::string& parentName)
        {
            Entity parent = FindEntityByName(scene, parentName);
            return parent ? parent.GetUUID() : UUID(0);
        }

        static Entity EnsureRuntimeEntity(Scene* scene,
            const std::string& name,
            UUID parent,
            const glm::vec2& position,
            const glm::vec2& size,
            int sortOrder,
            bool visible)
        {
            if (!scene)
                return {};

            Entity entity = FindEntityByName(scene, name);
            if (!entity)
                entity = scene->CreateEntity(name);

            auto& widget = entity.HasComponent<UIWidgetComponent>()
                ? entity.GetComponent<UIWidgetComponent>()
                : entity.AddComponent<UIWidgetComponent>();
            widget.Visible = visible;
            widget.EditorVisible = false;
            widget.Position = position;
            widget.Size = size;
            widget.Rotation = 0.0f;
            widget.Anchor = UIAnchor::TopLeft;
            widget.SortOrder = sortOrder;
            widget.ParentEntity = parent;
            return entity;
        }

        static std::string DynamicBagItemName(int slot, const char* suffix)
        {
            return std::string(kDynamicBagItemPrefix) + std::to_string(slot + 1) + suffix;
        }

        static glm::vec2 BagSlotPosition(const BagLayout& layout, int slot)
        {
            return {
                layout.Origin.x + static_cast<float>(slot % kBagColumns) * layout.Step.x,
                layout.Origin.y + static_cast<float>(slot / kBagColumns) * layout.Step.y
            };
        }

        static void ConfigureBagFrame(Entity frame, bool selected)
        {
            if (!frame)
                return;

            auto& panel = frame.HasComponent<UIPanelComponent>()
                ? frame.GetComponent<UIPanelComponent>()
                : frame.AddComponent<UIPanelComponent>();
            panel.BackgroundColor = { 0.025f, 0.030f, 0.035f, 0.78f };
            panel.BorderColor = selected
                ? glm::vec4(1.0f, 0.12f, 0.08f, 1.0f)
                : glm::vec4(0.58f, 0.48f, 0.31f, 0.78f);
            panel.BorderThickness = selected ? 3.5f : 2.0f;
            panel.ClipChildren = false;
            panel.Draggable = false;
            panel.ConstrainDragToParent = true;
        }

        static void ConfigureBagButton(Entity button, const std::string& command)
        {
            if (!button)
                return;

            auto& buttonComponent = button.HasComponent<UIButtonComponent>()
                ? button.GetComponent<UIButtonComponent>()
                : button.AddComponent<UIButtonComponent>();
            buttonComponent.NormalColor = { 0.0f, 0.0f, 0.0f, 0.0f };
            buttonComponent.HoverColor = { 0.95f, 1.0f, 0.82f, 0.14f };
            buttonComponent.PressedColor = { 1.0f, 0.90f, 0.45f, 0.24f };
            buttonComponent.OnClickFunction = command;
            buttonComponent.TooltipText.clear();
        }

        static void ConfigureBagIcon(Entity icon, const std::string& texturePath, bool selected)
        {
            if (!icon)
                return;

            auto& image = icon.HasComponent<UIImageComponent>()
                ? icon.GetComponent<UIImageComponent>()
                : icon.AddComponent<UIImageComponent>();
            image.Color = selected
                ? glm::vec4(1.0f, 0.95f, 0.68f, 1.0f)
                : glm::vec4(1.0f);
            image.UVMin = { 0.0f, 0.0f };
            image.UVMax = { 1.0f, 1.0f };
            image.SpriteSheet.clear();
            image.CellIndex = -1;
            image.SubRect.clear();

            SetImageTexture(icon.GetScene(), icon.GetName(), texturePath);
        }

        static void HideRuntimeBagSlot(Scene* scene, int slot)
        {
            SetWidgetVisible(scene, DynamicBagItemName(slot, "_Frame"), false);
            SetWidgetVisible(scene, DynamicBagItemName(slot, ""), false);
            SetWidgetVisible(scene, DynamicBagItemName(slot, "_Button"), false);
        }

        static void HideLegacyBagItems(Scene* scene)
        {
            for (int i = 1; i <= 8; ++i)
            {
                const std::string legacyItem = std::string("Equipment_Item_") + std::to_string(i);
                SetWidgetVisible(scene, legacyItem + "_Frame", false);
                SetWidgetVisible(scene, legacyItem, false);
                SetWidgetVisible(scene, legacyItem + "_Button", false);
            }
            SetWidgetVisible(scene, "Equipment_Button_Page1", false);
            SetWidgetVisible(scene, "Equipment_Button_Page2", false);
        }

        static void SyncPageControls(Scene* scene, int pageCount)
        {
            auto& state = GameProgress::GetState();
            const int clampedPage = std::clamp(state.EquipmentPage, 1, std::max(pageCount, 1));
            state.EquipmentPage = clampedPage;

            SetWidgetVisible(scene, SystemBindings::Progression::EquipmentButtonPagePrev, true);
            SetWidgetVisible(scene, SystemBindings::Progression::EquipmentButtonPageNext, true);
            SetButtonCommand(scene, SystemBindings::Progression::EquipmentButtonPagePrev, "progression:equipment_page_prev");
            SetButtonCommand(scene, SystemBindings::Progression::EquipmentButtonPageNext, "progression:equipment_page_next");

            if (Entity slider = FindEntityByName(scene, SystemBindings::Progression::EquipmentPageSlider);
                slider && slider.HasComponent<UIWidgetComponent>() && slider.HasComponent<UISliderComponent>())
            {
                slider.GetComponent<UIWidgetComponent>().Visible = true;
                auto& sliderComponent = slider.GetComponent<UISliderComponent>();
                sliderComponent.MinValue = 1.0f;
                sliderComponent.MaxValue = static_cast<float>(std::max(pageCount, 1));
                if (!sliderComponent.IsDragging)
                    sliderComponent.Value = static_cast<float>(clampedPage);
                sliderComponent.OnValueChangedFunction = "progression:equipment_page_slider";
            }
        }

        static constexpr std::array<EquipmentSlotView, 4> kSlots = {
            EquipmentSlotView{ "armor", SystemBindings::Progression::EquipmentSlotArmor, SystemBindings::Progression::EquipmentSlotArmorButton, { 0.105f, 0.335f } },
            EquipmentSlotView{ "ring", SystemBindings::Progression::EquipmentSlotRing, SystemBindings::Progression::EquipmentSlotRingButton, { 0.205f, 0.335f } },
            EquipmentSlotView{ "charm", SystemBindings::Progression::EquipmentSlotCharm, SystemBindings::Progression::EquipmentSlotCharmButton, { 0.105f, 0.470f } },
            EquipmentSlotView{ "boots", SystemBindings::Progression::EquipmentSlotBoots, SystemBindings::Progression::EquipmentSlotBootsButton, { 0.205f, 0.470f } }
        };

    } // namespace

    int SyncPager(Scene* scene)
    {
        const std::vector<std::string> bagEquipment = BuildBagEquipment();
        const int pageCount = GetBagPageCount(bagEquipment.size());
        auto& state = GameProgress::GetState();
        state.EquipmentPage = std::clamp(state.EquipmentPage, 1, pageCount);
        SyncPageControls(scene, pageCount);
        return state.EquipmentPage;
    }

    void EnsureLayout(Scene* scene)
    {
        if (!HasEntity(scene, SystemBindings::Progression::EquipmentDetails))
            return;

        if (HasEntity(scene, SystemBindings::Progression::EquipmentDetailsScroll))
        {
            FindAuthoredScrollView(scene, SystemBindings::Progression::EquipmentDetailsScroll);
        }

        if (HasEntity(scene, SystemBindings::Progression::EquipmentMaterialsScroll))
        {
            FindAuthoredScrollView(scene, SystemBindings::Progression::EquipmentMaterialsScroll);
        }
    }

    void UpdateItems(Scene* scene)
    {
        auto& state = GameProgress::GetState();
        const BagLayout bagLayout = ResolveBagLayout(scene);
        std::string hoveredEquipmentId;
        glm::vec2 hoveredPosition = { 0.0f, 0.0f };
        const std::vector<std::string> bagEquipment = BuildBagEquipment();
        const int pageCount = GetBagPageCount(bagEquipment.size());
        state.EquipmentPage = std::clamp(state.EquipmentPage, 1, pageCount);
        SyncPageControls(scene, pageCount);
        HideLegacyBagItems(scene);

        for (const auto& slot : kSlots)
        {
            const std::string equipmentId = GameProgress::GetEquippedEquipmentForSlot(slot.SlotId);
            const bool hasEquipment = !equipmentId.empty();
            SetWidgetVisible(scene, slot.ButtonTag, true);
            SetButtonCommand(scene, slot.ButtonTag, std::string("progression:select_equipment_slot:") + slot.SlotId);
            SetWidgetVisible(scene, slot.IconTag, hasEquipment);
            if (hasEquipment)
            {
                SetImageTexture(scene, slot.IconTag, GameProgress::GetEquipmentIconPath(equipmentId));
                SetImageColor(scene, slot.IconTag,
                    equipmentId == state.SelectedEquipmentId
                        ? glm::vec4(1.0f, 0.95f, 0.68f, 1.0f)
                        : glm::vec4(1.0f));
            }

            if (hasEquipment && IsButtonHovered(scene, slot.ButtonTag))
            {
                hoveredEquipmentId = equipmentId;
                hoveredPosition = GetWidgetTopLeft(scene, slot.ButtonTag, slot.FramePosition);
            }
        }

        const UUID bagParent = FindParentUUID(scene, kBagGroupName);
        const size_t pageStart = static_cast<size_t>(state.EquipmentPage - 1) * kBagItemsPerPage;
        for (int slot = 0; slot < kBagItemsPerPage; ++slot)
        {
            const size_t itemIndex = pageStart + static_cast<size_t>(slot);
            const bool hasEquipment = itemIndex < bagEquipment.size();
            if (!hasEquipment)
            {
                HideRuntimeBagSlot(scene, slot);
                continue;
            }

            const std::string& equipmentId = bagEquipment[itemIndex];
            const bool selected = state.SelectedEquipmentId == equipmentId;
            const glm::vec2 pos = BagSlotPosition(bagLayout, slot);

            const std::string frameName = DynamicBagItemName(slot, "_Frame");
            const std::string itemName = DynamicBagItemName(slot, "");
            const std::string buttonName = DynamicBagItemName(slot, "_Button");

            Entity frame = EnsureRuntimeEntity(scene, frameName, bagParent, pos, bagLayout.FrameSize, 25, true);
            Entity item = EnsureRuntimeEntity(scene, itemName, bagParent,
                pos + bagLayout.IconOffset,
                bagLayout.IconSize,
                34,
                true);
            Entity button = EnsureRuntimeEntity(scene, buttonName, bagParent, pos, bagLayout.FrameSize, 58, true);
            ConfigureBagFrame(frame, selected);
            ConfigureBagIcon(item, GameProgress::GetEquipmentIconPath(equipmentId), selected);
            ConfigureBagButton(button, std::string("progression:select_equipment_") + equipmentId);

            if (IsButtonHovered(scene, buttonName))
            {
                hoveredEquipmentId = equipmentId;
                hoveredPosition = GetWidgetTopLeft(scene, buttonName, pos);
            }
        }

        SetText(scene, SystemBindings::Progression::EquipmentToggleButton, GameProgress::GetEquipmentToggleButtonText());
        SetButtonCommand(scene, SystemBindings::Progression::EquipmentToggleButton, "progression:toggle_selected_equipment");

        const bool showTooltip = !hoveredEquipmentId.empty();
        SetWidgetVisible(scene, SystemBindings::Progression::EquipmentTooltipPanel, showTooltip);
        SetWidgetVisible(scene, SystemBindings::Progression::EquipmentTooltipText, showTooltip);
        if (!showTooltip)
            return;

        const glm::vec2 tooltipSize = { 0.265f, 0.120f };
        glm::vec2 tooltipPosition = hoveredPosition + glm::vec2(bagLayout.FrameSize.x + 0.012f, 0.0f);
        tooltipPosition.x = std::clamp(tooltipPosition.x, 0.055f, 0.915f - tooltipSize.x);
        tooltipPosition.y = std::clamp(tooltipPosition.y, 0.125f, 0.835f - tooltipSize.y);
        SetWidgetTopLeft(scene, SystemBindings::Progression::EquipmentTooltipPanel, tooltipPosition, tooltipSize);
        SetWidgetTopLeft(scene, SystemBindings::Progression::EquipmentTooltipText,
            tooltipPosition + glm::vec2(0.012f, 0.010f),
            tooltipSize - glm::vec2(0.024f, 0.020f));
        SetText(scene, SystemBindings::Progression::EquipmentTooltipText, GameProgress::BuildEquipmentTooltip(hoveredEquipmentId));
    }

} // namespace Wheatear::ProgressionEquipmentPageService
