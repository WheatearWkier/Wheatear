#include "wtpch.h"
#include "ProgressionResultPageService.h"

#include "GameProgress.h"
#include "Wheatear/Modules/Common/GameplayRewardService.h"
#include "Wheatear/Modules/Common/GameplayUIService.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

namespace Wheatear::ProgressionResultPageService {

    namespace {

        using ResultDropIcon = GameplayRewardService::RewardIconDefinition;
        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::IsButtonHovered;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        static const std::array<ResultDropIcon, 3>& GetResultDropIcons()
        {
            static const std::array<ResultDropIcon, 3> icons = {
                ResultDropIcon{
                    "Core",
                    "MAT-MAGIC-CORE-T0",
                    "魔核碎片",
                    "assets/vertical_slice/side_combat/ui/icon_drop_magic_core.png",
                    "用于魔剑觉醒、魔法分支技能与高级装备升级。" },
                ResultDropIcon{
                    "Sinew",
                    "MAT-BEAST-SINEW",
                    "兽筋",
                    "assets/vertical_slice/side_combat/ui/icon_drop_beast_sinew.png",
                    "用于护甲升级、皮革装备与机动训练。" },
                ResultDropIcon{
                    "Claw",
                    "MAT-BEAST-CLAW",
                    "熊爪",
                    "assets/vertical_slice/side_combat/ui/icon_drop_beast_claw.png",
                    "用于近战剑技、护甲强化与好感礼物。" }
            };
            return icons;
        }

        static void SetImageTexture(Scene* scene, const std::string& entityName, const std::string& texturePath)
        {
            UIRuntimeTools::SetImageTexture(scene, entityName, texturePath, true);
        }

        static std::string BuildMaterialTooltip(const ResultDropIcon& icon, const std::string& amount)
        {
            std::ostringstream stream;
            stream << icon.DisplayName << "\n";
            stream << "本次 x" << amount << "  背包 x" << GameProgress::GetMaterialAmount(icon.ItemId) << "\n";
            stream << icon.Usage;
            return stream.str();
        }

        static void SetResultDropVisible(Scene* scene, const ResultDropIcon& icon, bool visible)
        {
            const std::string prefix = std::string("Result_Drop_") + icon.Key;
            SetWidgetVisible(scene, prefix + "_Frame", visible);
            SetWidgetVisible(scene, prefix + "_Icon", visible);
            SetWidgetVisible(scene, prefix + "_Button", visible);
            SetWidgetVisible(scene, prefix + "_Count", visible);
        }

    } // namespace

    void UpdateDrops(Scene* scene)
    {
        if (!FindEntityByName(scene, "Result_Drop_Core_Frame"))
            return;

        const auto& state = GameProgress::GetState();
        const bool hasResult = state.LastDungeonResult.Valid;
        std::string hoveredTooltip;
        glm::vec2 hoveredPosition = { 0.115f, 0.705f };

        int index = 0;
        for (const ResultDropIcon& icon : GetResultDropIcons())
        {
            const std::string prefix = std::string("Result_Drop_") + icon.Key;
            const std::string amount = hasResult
                ? GameplayRewardService::ExtractRewardAmount(state.LastDungeonResult.RewardSummary, icon.DisplayName)
                : "0";
            const bool visible = hasResult && (!GameplayRewardService::IsZeroAmount(amount) || std::string(icon.Key) == "Core");
            SetResultDropVisible(scene, icon, visible);
            if (!visible)
            {
                ++index;
                continue;
            }

            SetImageTexture(scene, prefix + "_Icon", icon.IconPath);
            SetImageColor(scene, prefix + "_Icon", GameplayRewardService::IsZeroAmount(amount)
                ? glm::vec4(0.45f, 0.47f, 0.50f, 0.80f)
                : glm::vec4(1.0f));
            SetText(scene, prefix + "_Count", std::string("x") + amount);

            const bool hovered = IsButtonHovered(scene, prefix + "_Button")
                || IsButtonHovered(scene, prefix + "_Icon");
            if (hovered)
            {
                hoveredTooltip = BuildMaterialTooltip(icon, amount);
                hoveredPosition = { 0.105f + index * 0.082f, 0.615f };
            }
            ++index;
        }

        const bool showTooltip = !hoveredTooltip.empty();
        const glm::vec2 tooltipSize = { 0.265f, 0.120f };
        const glm::vec2 tooltipPosition = {
            std::clamp(hoveredPosition.x, 0.080f, 0.650f - tooltipSize.x),
            hoveredPosition.y
        };
        GameplayUIService::SetTooltip(scene,
            "Result_DropTooltipPanel",
            "Result_DropTooltipText",
            showTooltip,
            tooltipPosition,
            tooltipSize,
            hoveredTooltip);
    }

} // namespace Wheatear::ProgressionResultPageService
