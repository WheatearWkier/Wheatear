#include "wtpch.h"
#include "ProgressionEquipmentPageService.h"

#include "GameProgress.h"
#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/EntityReference.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

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

        using GameplayUILayoutService::EnsurePager;
        using GameplayUILayoutService::EnsureScrollView;
        using GameplayUILayoutService::SetButtonCommand;
        using GameplayUILayoutService::SetButtonPalette;
        using GameplayUILayoutService::SetPageItem;
        using GameplayUILayoutService::SetPanelColors;

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

        static constexpr std::array<const char*, 8> kBagEquipment = {
            "traveler_armor",
            "black_forest_armor",
            "beast_tooth_pendant",
            "novice_magic_ring",
            "wind_boots",
            "old_ward_charm",
            "training_blade",
            "angel_feather"
        };

        static constexpr std::array<EquipmentSlotView, 4> kSlots = {
            EquipmentSlotView{ "armor", "Equipment_SlotArmor", "Equipment_SlotArmor_Button", { 0.105f, 0.335f } },
            EquipmentSlotView{ "ring", "Equipment_SlotRing", "Equipment_SlotRing_Button", { 0.205f, 0.335f } },
            EquipmentSlotView{ "charm", "Equipment_SlotCharm", "Equipment_SlotCharm_Button", { 0.105f, 0.470f } },
            EquipmentSlotView{ "boots", "Equipment_SlotBoots", "Equipment_SlotBoots_Button", { 0.205f, 0.470f } }
        };

    } // namespace

    int SyncPager(Scene* scene)
    {
        Entity pager = EnsurePager(scene, "Equipment_Pager", 2);
        auto& pagerComponent = pager.GetComponent<UIPagerComponent>();
        pagerComponent.PageCount = 2;
        pagerComponent.CurrentPage = std::clamp(pagerComponent.CurrentPage, 1, pagerComponent.PageCount);

        auto& state = GameProgress::GetState();
        state.EquipmentPage = pagerComponent.CurrentPage;
        return state.EquipmentPage;
    }

    void EnsureLayout(Scene* scene)
    {
        if (!HasEntity(scene, "Equipment_Details"))
            return;

        EnsureScrollView(scene, "Equipment_DetailsScroll", "WT_UI_Canvas",
            { 0.675f, 0.280f }, { 0.215f, 0.262f }, 34, 1.55f);
        UIRuntimeTools::SetWidgetParent(scene, "Equipment_Details", "Equipment_DetailsScroll");
        SetWidgetTopLeft(scene, "Equipment_Details", { 0.025f, 0.025f }, { 0.84f, 1.24f });

        EnsureScrollView(scene, "Equipment_MaterialsScroll", "WT_UI_Canvas",
            { 0.675f, 0.555f }, { 0.215f, 0.142f }, 34, 1.35f);
        UIRuntimeTools::SetWidgetParent(scene, "Equipment_Materials", "Equipment_MaterialsScroll");
        SetWidgetTopLeft(scene, "Equipment_Materials", { 0.025f, 0.030f }, { 0.84f, 0.92f });
    }

    void UpdateItems(Scene* scene)
    {
        const auto& state = GameProgress::GetState();
        Entity pager = EnsurePager(scene, "Equipment_Pager", 2);
        const glm::vec2 frameSize = { 0.075f, 0.098f };
        const glm::vec2 iconSize = { 0.055f, 0.075f };
        const glm::vec2 origin = { 0.385f, 0.335f };
        const glm::vec2 step = { 0.105f, 0.135f };
        std::string hoveredEquipmentId;
        glm::vec2 hoveredPosition = { 0.0f, 0.0f };
        std::vector<std::string> bagEquipment;
        bagEquipment.reserve(kBagEquipment.size());
        for (const char* equipmentId : kBagEquipment)
        {
            if (GameProgress::IsEquipmentOwned(equipmentId)
                && !GameProgress::IsEquipmentEquipped(equipmentId))
            {
                bagEquipment.emplace_back(equipmentId);
            }
        }

        for (const auto& slot : kSlots)
        {
            const std::string equipmentId = GameProgress::GetEquippedEquipmentForSlot(slot.SlotId);
            const bool hasEquipment = !equipmentId.empty();
            SetWidgetTopLeft(scene, slot.ButtonTag, slot.FramePosition, frameSize);
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
                hoveredPosition = slot.FramePosition;
            }
        }

        for (int i = 1; i <= 8; ++i)
        {
            const int page = i <= 4 ? 1 : 2;
            const int slot = (i - 1) % 4;
            const glm::vec2 pos = { origin.x + static_cast<float>(slot % 2) * step.x,
                                    origin.y + static_cast<float>(slot / 2) * step.y };
            const std::string item = "Equipment_Item_" + std::to_string(i);
            const std::string frame = item + "_Frame";
            const std::string button = item + "_Button";
            const size_t itemIndex = static_cast<size_t>((page - 1) * 4 + slot);
            const bool hasEquipment = itemIndex < bagEquipment.size();
            const std::string equipmentId = hasEquipment ? bagEquipment[itemIndex] : std::string{};
            const bool selected = hasEquipment && state.SelectedEquipmentId == equipmentId;

            SetPageItem(scene, frame, pager, page);
            SetPageItem(scene, item, pager, page);
            SetPageItem(scene, button, pager, page);

            SetWidgetTopLeft(scene, frame, pos, frameSize);
            SetWidgetTopLeft(scene, item, pos + glm::vec2(0.010f, 0.011f), iconSize);
            SetWidgetTopLeft(scene, button, pos, frameSize);
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
            SetPanelColors(scene, frame,
                selected ? glm::vec4(0.18f, 0.15f, 0.09f, 0.86f) : glm::vec4(0.025f, 0.03f, 0.035f, 0.78f),
                selected ? glm::vec4(0.98f, 0.78f, 0.30f, 0.96f) : glm::vec4(0.58f, 0.48f, 0.31f, 0.78f));

            if (hasEquipment && IsButtonHovered(scene, button))
            {
                hoveredEquipmentId = equipmentId;
                hoveredPosition = pos;
            }
        }

        const bool pageOne = state.EquipmentPage == 1;
        SetButtonPalette(scene, "Equipment_Button_Page1",
            pageOne ? glm::vec4(0.80f, 0.58f, 0.22f, 0.94f) : glm::vec4(0.10f, 0.11f, 0.13f, 0.86f),
            glm::vec4(0.95f, 0.78f, 0.36f, 0.96f),
            glm::vec4(0.58f, 0.38f, 0.16f, 0.96f));
        SetButtonPalette(scene, "Equipment_Button_Page2",
            !pageOne ? glm::vec4(0.80f, 0.58f, 0.22f, 0.94f) : glm::vec4(0.10f, 0.11f, 0.13f, 0.86f),
            glm::vec4(0.95f, 0.78f, 0.36f, 0.96f),
            glm::vec4(0.58f, 0.38f, 0.16f, 0.96f));
        Entity equipmentPager = FindEntityByName(scene, "Equipment_Pager");
        const std::string pagerSelector = equipmentPager
            ? EntityReferences::MakeUUIDSelector(equipmentPager.GetUUID())
            : std::string{};
        SetButtonCommand(scene, "Equipment_Button_Page1",
            pagerSelector.empty() ? std::string{} : "ui:pager:" + pagerSelector + ":page:1");
        SetButtonCommand(scene, "Equipment_Button_Page2",
            pagerSelector.empty() ? std::string{} : "ui:pager:" + pagerSelector + ":page:2");
        SetWidgetVisible(scene, "Equipment_PageSlider", false);

        SetText(scene, "Equipment_Button_Toggle", GameProgress::GetEquipmentToggleButtonText());
        SetButtonCommand(scene, "Equipment_Button_Toggle", "progression:toggle_selected_equipment");

        const bool showTooltip = !hoveredEquipmentId.empty();
        SetWidgetVisible(scene, "Equipment_TooltipPanel", showTooltip);
        SetWidgetVisible(scene, "Equipment_TooltipText", showTooltip);
        if (!showTooltip)
            return;

        const glm::vec2 tooltipSize = { 0.235f, 0.112f };
        glm::vec2 tooltipPosition = hoveredPosition + glm::vec2(frameSize.x + 0.012f, 0.0f);
        tooltipPosition.x = std::clamp(tooltipPosition.x, 0.055f, 0.915f - tooltipSize.x);
        tooltipPosition.y = std::clamp(tooltipPosition.y, 0.125f, 0.835f - tooltipSize.y);
        SetWidgetTopLeft(scene, "Equipment_TooltipPanel", tooltipPosition, tooltipSize);
        SetWidgetTopLeft(scene, "Equipment_TooltipText",
            tooltipPosition + glm::vec2(0.012f, 0.010f),
            tooltipSize - glm::vec2(0.024f, 0.020f));
        SetText(scene, "Equipment_TooltipText", GameProgress::BuildEquipmentTooltip(hoveredEquipmentId));
    }

} // namespace Wheatear::ProgressionEquipmentPageService
