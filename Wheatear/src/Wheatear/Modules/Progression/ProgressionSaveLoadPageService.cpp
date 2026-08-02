#include "wtpch.h"
#include "ProgressionSaveLoadPageService.h"

#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/EntityReference.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <string>

namespace Wheatear::ProgressionSaveLoadPageService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetParent;
        using UIRuntimeTools::SetWidgetTopLeft;

        using GameplayUILayoutService::EnsureButton;
        using GameplayUILayoutService::EnsurePager;
        using GameplayUILayoutService::EnsureScrollView;
        using GameplayUILayoutService::EnsureText;
        using GameplayUILayoutService::SetButtonPalette;
        using GameplayUILayoutService::SetPageItem;

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

    } // namespace

    void EnsureLayout(Scene* scene)
    {
        if (!HasEntity(scene, "SaveLoad_Status"))
            return;

        Entity pager = EnsurePager(scene, "SaveLoad_Pager", 2);
        auto& pagerComponent = pager.GetComponent<UIPagerComponent>();
        pagerComponent.PageCount = 2;
        pagerComponent.CurrentPage = std::clamp(pagerComponent.CurrentPage, 1, pagerComponent.PageCount);

        EnsureScrollView(scene, "SaveLoad_StatusScroll", "WT_UI_Canvas",
            { 0.205f, 0.292f }, { 0.50f, 0.124f }, 36, 1.35f);
        SetWidgetParent(scene, "SaveLoad_Status", "SaveLoad_StatusScroll");
        SetWidgetTopLeft(scene, "SaveLoad_Status", { 0.025f, 0.025f }, { 0.90f, 1.04f });

        EnsureScrollView(scene, "SaveLoad_LockedScroll", "WT_UI_Canvas",
            { 0.125f, 0.502f }, { 0.56f, 0.094f }, 36, 1.22f);
        SetWidgetParent(scene, "SaveLoad_EmptySlotText", "SaveLoad_LockedScroll");
        SetWidgetTopLeft(scene, "SaveLoad_EmptySlotText", { 0.025f, 0.045f }, { 0.90f, 0.84f });

        EnsureText(scene, "SaveLoad_PageText", "WT_UI_Canvas",
            { 0.525f, 0.745f }, { 0.12f, 0.045f }, 46,
            "第 1 / 2 页",
            18.0f,
            { 0.94f, 0.90f, 0.76f, 1.0f });
        const std::string saveLoadPagerSelector = EntityReferences::MakeUUIDSelector(pager.GetUUID());
        EnsureButton(scene, "SaveLoad_PagePrev", "WT_UI_Canvas",
            { 0.455f, 0.74f }, { 0.055f, 0.052f }, 56,
            "<",
            "ui:pager:" + saveLoadPagerSelector + ":prev");
        EnsureButton(scene, "SaveLoad_PageNext", "WT_UI_Canvas",
            { 0.655f, 0.74f }, { 0.055f, 0.052f }, 56,
            ">",
            "ui:pager:" + saveLoadPagerSelector + ":next");

        SetPageItem(scene, "SaveLoad_SlotCard_1", pager, 1);
        SetPageItem(scene, "SaveLoad_SlotIcon_1", pager, 1);
        SetPageItem(scene, "SaveLoad_StatusScroll", pager, 1);
        SetPageItem(scene, "SaveLoad_Status", pager, 1);
        SetPageItem(scene, "SaveLoad_Button_1", pager, 1);
        SetPageItem(scene, "SaveLoad_Button_2", pager, 1);
        SetPageItem(scene, "SaveLoad_SlotCard_2", pager, 2);
        SetPageItem(scene, "SaveLoad_LockedScroll", pager, 2);
        SetPageItem(scene, "SaveLoad_EmptySlotText", pager, 2);

        const bool pageOne = pagerComponent.GetClampedCurrentPage() == 1;
        SetText(scene, "SaveLoad_PageText", pageOne ? "第 1 / 2 页" : "第 2 / 2 页");
        SetButtonPalette(scene, "SaveLoad_PagePrev",
            pageOne ? glm::vec4(0.08f, 0.09f, 0.10f, 0.62f) : glm::vec4(0.16f, 0.21f, 0.20f, 0.90f),
            glm::vec4(0.32f, 0.50f, 0.46f, 0.96f),
            glm::vec4(0.06f, 0.08f, 0.08f, 0.98f));
        SetButtonPalette(scene, "SaveLoad_PageNext",
            pageOne ? glm::vec4(0.16f, 0.21f, 0.20f, 0.90f) : glm::vec4(0.08f, 0.09f, 0.10f, 0.62f),
            glm::vec4(0.32f, 0.50f, 0.46f, 0.96f),
            glm::vec4(0.06f, 0.08f, 0.08f, 0.98f));
    }

} // namespace Wheatear::ProgressionSaveLoadPageService
