#include "wtpch.h"
#include "ProgressionEquipmentPageService.h"

#include "GameProgress.h"
#include "ProgressionContent.h"
#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/EntityReference.h"
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
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        using GameplayUILayoutService::FindAuthoredPager;
        using GameplayUILayoutService::FindAuthoredScrollView;
        using GameplayUILayoutService::SetButtonCommand;
        using GameplayUILayoutService::SetPageItem;

        struct EquipmentSlotView
        {
            const char* SlotId;
            const char* IconTag;
            const char* ButtonTag;
            glm::vec2 FramePosition;
        };

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

        static constexpr std::array<EquipmentSlotView, 4> kSlots = {
            EquipmentSlotView{ "armor", SystemBindings::Progression::EquipmentSlotArmor, SystemBindings::Progression::EquipmentSlotArmorButton, { 0.105f, 0.335f } },
            EquipmentSlotView{ "ring", SystemBindings::Progression::EquipmentSlotRing, SystemBindings::Progression::EquipmentSlotRingButton, { 0.205f, 0.335f } },
            EquipmentSlotView{ "charm", SystemBindings::Progression::EquipmentSlotCharm, SystemBindings::Progression::EquipmentSlotCharmButton, { 0.105f, 0.470f } },
            EquipmentSlotView{ "boots", SystemBindings::Progression::EquipmentSlotBoots, SystemBindings::Progression::EquipmentSlotBootsButton, { 0.205f, 0.470f } }
        };

    } // namespace

    int SyncPager(Scene* scene)
    {
        if (!HasEntity(scene, SystemBindings::Progression::EquipmentPager))
            return GameProgress::GetState().EquipmentPage;

        Entity pager = FindAuthoredPager(scene, SystemBindings::Progression::EquipmentPager);
        if (!pager)
            return GameProgress::GetState().EquipmentPage;

        auto& pagerComponent = pager.GetComponent<UIPagerComponent>();
        pagerComponent.PageCount = 2;
        pagerComponent.CurrentPage = std::clamp(pagerComponent.CurrentPage, 1, pagerComponent.PageCount);

        auto& state = GameProgress::GetState();
        state.EquipmentPage = pagerComponent.CurrentPage;
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
        const auto& state = GameProgress::GetState();
        Entity pager = HasEntity(scene, SystemBindings::Progression::EquipmentPager)
            ? FindAuthoredPager(scene, SystemBindings::Progression::EquipmentPager)
            : Entity{};
        const glm::vec2 frameSize = { 0.075f, 0.098f };
        const glm::vec2 origin = { 0.385f, 0.335f };
        const glm::vec2 step = { 0.105f, 0.135f };
        std::string hoveredEquipmentId;
        glm::vec2 hoveredPosition = { 0.0f, 0.0f };
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

        for (int i = 1; i <= 8; ++i)
        {
            const int page = i <= 4 ? 1 : 2;
            const int slot = (i - 1) % 4;
            const glm::vec2 pos = { origin.x + static_cast<float>(slot % 2) * step.x,
                                    origin.y + static_cast<float>(slot / 2) * step.y };
            const std::string item = SystemBindings::IndexedName(SystemBindings::Progression::EquipmentItemPrefix, i);
            const std::string frame = item + "_Frame";
            const std::string button = item + "_Button";
            const size_t itemIndex = static_cast<size_t>((page - 1) * 4 + slot);
            const bool hasEquipment = itemIndex < bagEquipment.size();
            const std::string equipmentId = hasEquipment ? bagEquipment[itemIndex] : std::string{};
            const bool selected = hasEquipment && state.SelectedEquipmentId == equipmentId;

            if (pager)
            {
                SetPageItem(scene, frame, pager, page);
                SetPageItem(scene, item, pager, page);
                SetPageItem(scene, button, pager, page);
            }

            SetWidgetVisible(scene, frame, hasEquipment);
            SetWidgetVisible(scene, item, hasEquipment);
            SetWidgetVisible(scene, button, hasEquipment);
            SetButtonCommand(scene, button, hasEquipment
                ? std::string("progression:select_equipment_") + equipmentId
                : std::string{});
            if (hasEquipment)
            {
                SetImageTexture(scene, item, GameProgress::GetEquipmentIconPath(equipmentId));
                SetImageColor(scene, item, selected
                    ? glm::vec4(1.0f, 0.95f, 0.68f, 1.0f)
                    : glm::vec4(1.0f));
            }
            if (hasEquipment && IsButtonHovered(scene, button))
            {
                hoveredEquipmentId = equipmentId;
                hoveredPosition = GetWidgetTopLeft(scene, button, pos);
            }
        }

        Entity equipmentPager = FindEntityByName(scene, SystemBindings::Progression::EquipmentPager);
        const std::string pagerSelector = equipmentPager
            ? EntityReferences::MakeUUIDSelector(equipmentPager.GetUUID())
            : std::string{};
        if (!pagerSelector.empty())
        {
            SetButtonCommand(scene, SystemBindings::Progression::EquipmentButtonPage1, "ui:pager:" + pagerSelector + ":page:1");
            SetButtonCommand(scene, SystemBindings::Progression::EquipmentButtonPage2, "ui:pager:" + pagerSelector + ":page:2");
        }
        SetWidgetVisible(scene, SystemBindings::Progression::EquipmentPageSlider, false);

        SetText(scene, SystemBindings::Progression::EquipmentToggleButton, GameProgress::GetEquipmentToggleButtonText());
        SetButtonCommand(scene, SystemBindings::Progression::EquipmentToggleButton, "progression:toggle_selected_equipment");

        const bool showTooltip = !hoveredEquipmentId.empty();
        SetWidgetVisible(scene, SystemBindings::Progression::EquipmentTooltipPanel, showTooltip);
        SetWidgetVisible(scene, SystemBindings::Progression::EquipmentTooltipText, showTooltip);
        if (!showTooltip)
            return;

        const glm::vec2 tooltipSize = { 0.235f, 0.112f };
        glm::vec2 tooltipPosition = hoveredPosition + glm::vec2(frameSize.x + 0.012f, 0.0f);
        tooltipPosition.x = std::clamp(tooltipPosition.x, 0.055f, 0.915f - tooltipSize.x);
        tooltipPosition.y = std::clamp(tooltipPosition.y, 0.125f, 0.835f - tooltipSize.y);
        SetWidgetTopLeft(scene, SystemBindings::Progression::EquipmentTooltipPanel, tooltipPosition, tooltipSize);
        SetWidgetTopLeft(scene, SystemBindings::Progression::EquipmentTooltipText,
            tooltipPosition + glm::vec2(0.012f, 0.010f),
            tooltipSize - glm::vec2(0.024f, 0.020f));
        SetText(scene, SystemBindings::Progression::EquipmentTooltipText, GameProgress::BuildEquipmentTooltip(hoveredEquipmentId));
    }

} // namespace Wheatear::ProgressionEquipmentPageService
