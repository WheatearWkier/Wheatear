#include "wtpch.h"
#include "SideCombatHudService.h"

#include "SideCombatTuningService.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Modules/Common/GameplayTextService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

namespace Wheatear::SideCombatHudService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::IsButtonHovered;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetImageTexture;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        struct CombatItemSlot
        {
            std::string Key;
            std::string Shortcut;
            std::string IconPath;
            std::string DisplayName;
            std::string Usage;
        };

        static std::string FormatFloat(float value, int precision = 0)
        {
            return GameplayTextService::FormatFloat(value, precision);
        }

        static std::string FormatCooldownSeconds(float value)
        {
            return GameplayTextService::FormatFloat(std::max(0.0f, value), 1);
        }

        static const char* GetCombatStateLabel(SideCombatState state)
        {
            switch (state)
            {
            case SideCombatState::HitStun: return "硬直";
            case SideCombatState::Launched: return "浮空";
            case SideCombatState::Knockdown: return "倒地";
            case SideCombatState::Recovery: return "起身";
            case SideCombatState::SuperArmor: return "霸体";
            case SideCombatState::Broken: return "破防";
            case SideCombatState::Dead: return "战败";
            case SideCombatState::Normal:
            default: return "通常";
            }
        }

        static void SetSkillSlotVisible(Scene* scene, const std::string& key, bool visible)
        {
            SetWidgetVisible(scene, "SC_SkillSlot_" + key, visible);
            SetWidgetVisible(scene, "SC_SkillIcon_" + key, visible);
            SetWidgetVisible(scene, "SC_SkillCooldown_" + key, visible);
            SetWidgetVisible(scene, "SC_SkillCooldownText_" + key, visible);
            SetWidgetVisible(scene, "SC_SkillKey_" + key, visible);
        }

        static void UpdateSkillSlot(Scene* scene,
            const std::string& key,
            const std::string& keyLabel,
            bool unlocked,
            float cooldown,
            float maxCooldown)
        {
            const std::string slot = "SC_SkillSlot_" + key;
            const std::string icon = "SC_SkillIcon_" + key;
            const std::string overlay = "SC_SkillCooldown_" + key;
            const std::string text = "SC_SkillCooldownText_" + key;
            const std::string keyText = "SC_SkillKey_" + key;

            if (!FindEntityByName(scene, slot))
                return;

            SetWidgetVisible(scene, slot, true);
            SetWidgetVisible(scene, icon, true);
            SetWidgetVisible(scene, keyText, true);
            SetText(scene, keyText, keyLabel);
            SetImageColor(scene, icon, unlocked
                ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
                : glm::vec4(0.35f, 0.37f, 0.40f, 0.86f));

            if (!unlocked)
            {
                SetProgress(scene, overlay, 1.0f, 1.0f);
                SetWidgetVisible(scene, overlay, true);
                SetText(scene, text, "锁定");
                SetWidgetVisible(scene, text, true);
                return;
            }

            if (cooldown > 0.05f)
            {
                SetProgress(scene, overlay, cooldown, std::max(0.05f, maxCooldown));
                SetWidgetVisible(scene, overlay, true);
                SetText(scene, text, FormatCooldownSeconds(cooldown));
                SetWidgetVisible(scene, text, true);
            }
            else
            {
                SetWidgetVisible(scene, overlay, false);
                SetWidgetVisible(scene, text, false);
            }
        }

        static void UpdateSkillTooltip(Scene* scene,
            const std::string& key,
            const std::string& text)
        {
            const bool visible = !key.empty() && !text.empty();
            SetWidgetVisible(scene, "SC_SkillTooltipPanel", visible);
            SetWidgetVisible(scene, "SC_SkillTooltipText", visible);
            if (!visible)
                return;

            float x = 0.58f;
            float y = 0.725f;
            if (key == "U")
                x = 0.64f;
            else if (key == "I")
                x = 0.70f;
            else if (key == "L")
                x = 0.76f;
            else if (key == "ItemSlot1")
            {
                x = 0.04f;
                y = 0.705f;
            }
            else if (key == "ItemSlot2")
            {
                x = 0.10f;
                y = 0.705f;
            }
            else if (key == "ItemSlot3")
            {
                x = 0.16f;
                y = 0.705f;
            }

            const glm::vec2 size = { 0.225f, 0.090f };
            const glm::vec2 position = {
                std::clamp(x, 0.04f, 0.96f - size.x),
                y
            };
            SetWidgetTopLeft(scene, "SC_SkillTooltipPanel", position, size);
            SetWidgetTopLeft(scene, "SC_SkillTooltipText",
                position + glm::vec2(0.012f, 0.010f),
                size - glm::vec2(0.024f, 0.020f));
            SetText(scene, "SC_SkillTooltipText", text);
        }

        static const std::array<CombatItemSlot, 3>& GetCombatItemSlots()
        {
            static const std::array<CombatItemSlot, 3> slots = {
                CombatItemSlot{
                    "1",
                    "1",
                    AssetAliasRegistry::Path("side.skill.hud.heal_potion"),
                    "治疗药水",
                    "恢复生命。正式消耗品系统完成前的占位道具。" },
                CombatItemSlot{
                    "2",
                    "2",
                    AssetAliasRegistry::Path("side.skill.hud.focus_vial"),
                    "专注药剂",
                    "正式道具体系中用于提升魔剑槽恢复效率。" },
                CombatItemSlot{
                    "3",
                    "3",
                    AssetAliasRegistry::Path("side.skill.hud.burst_bomb"),
                    "破阵爆弹",
                    "正式道具体系中用于打断附近小怪。" }
            };
            return slots;
        }

        static void SetItemSlotVisible(Scene* scene, const CombatItemSlot& slot, bool visible)
        {
            const std::string prefix = std::string("SC_ItemSlot_") + slot.Key;
            SetWidgetVisible(scene, prefix + "_Frame", visible);
            SetWidgetVisible(scene, prefix + "_Icon", visible);
            SetWidgetVisible(scene, prefix + "_Button", visible);
            SetWidgetVisible(scene, prefix + "_Count", visible);
        }

        static void UpdateCombatItemSlots(Scene* scene)
        {
            if (!FindEntityByName(scene, "SC_ItemSlot_1_Frame"))
                return;

            int index = 0;
            for (const CombatItemSlot& slot : GetCombatItemSlots())
            {
                const std::string prefix = std::string("SC_ItemSlot_") + slot.Key;
                SetItemSlotVisible(scene, slot, true);
                SetImageTexture(scene, prefix + "_Icon", slot.IconPath);
                SetImageColor(scene, prefix + "_Icon", glm::vec4(1.0f));
                SetWidgetTopLeft(scene, prefix + "_Count",
                    { 0.044f + 0.058f * static_cast<float>(index), 0.811f },
                    { 0.018f, 0.018f });
                SetText(scene, prefix + "_Count", slot.Shortcut);
                ++index;
            }
        }

        static void ApplyCombatItemTooltip(Scene* scene,
            std::string& hoveredKey,
            std::string& tooltip)
        {
            if (!tooltip.empty())
                return;

            int index = 1;
            for (const CombatItemSlot& slot : GetCombatItemSlots())
            {
                const std::string prefix = std::string("SC_ItemSlot_") + slot.Key;
                if (!IsButtonHovered(scene, prefix + "_Button") &&
                    !IsButtonHovered(scene, prefix + "_Icon"))
                {
                    ++index;
                    continue;
                }

                hoveredKey = "ItemSlot" + std::to_string(index);
                std::ostringstream stream;
                stream << slot.Shortcut << "  " << slot.DisplayName << "\n";
                stream << slot.Usage;
                tooltip = stream.str();
                return;
            }
        }

        static Entity EnsureWorldHealthSprite(Scene* scene,
            const std::string& name,
            const glm::vec4& color)
        {
            if (!scene)
                return {};

            Entity entity = FindEntityByName(scene, name);
            if (!entity)
                entity = scene->CreateEntity(name);

            if (!entity.HasComponent<SpriteRendererComponent>())
                entity.AddComponent<SpriteRendererComponent>();

            auto& sprite = entity.GetComponent<SpriteRendererComponent>();
            sprite.Texture = nullptr;
            sprite.Color = color;
            sprite.DrawOffset = { 0.0f, 0.0f };
            sprite.DrawScale = { 1.0f, 1.0f };
            return entity;
        }

        static void HideWorldHealthSprite(Scene* scene, const std::string& name)
        {
            Entity entity = FindEntityByName(scene, name);
            if (entity && entity.HasComponent<SpriteRendererComponent>())
                entity.GetComponent<SpriteRendererComponent>().Color.a = 0.0f;
        }

        static void UpdateEnemyHealthBars(Scene* scene,
            const SideCombatLevelComponent& level,
            const SideCombatTuningService::SideCombatTuning& tuning)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<SideCombatantComponent, SideEnemyAIComponent, TagComponent>())
            {
                const auto& combatant = registry.get<SideCombatantComponent>(e);
                const auto& ai = registry.get<SideEnemyAIComponent>(e);
                if (combatant.Team != (int)SideCombatTeam::Enemy ||
                    ai.Kind == SideEnemyKind::BearBoss)
                {
                    continue;
                }

                const std::string& tag = registry.get<TagComponent>(e).Tag;
                const std::string backName = tag + "_HPBack";
                const std::string fillName = tag + "_HPFill";
                const bool visible = combatant.Alive && ai.RuntimeAwake;
                if (!visible)
                {
                    HideWorldHealthSprite(scene, backName);
                    HideWorldHealthSprite(scene, fillName);
                    continue;
                }

                const float healthRatio = std::clamp(
                    combatant.Health / std::max(1.0f, combatant.MaxHealth),
                    0.0f,
                    1.0f);
                const float fullWidth = 0.84f;
                const float fillWidth = std::max(0.01f, fullWidth * healthRatio);
                const float baseX = combatant.RuntimeGroundPosition.x;
                const float baseY = combatant.RuntimeGroundPosition.y +
                    combatant.RuntimeAirHeight +
                    combatant.CollisionHeight +
                    0.36f;
                const float z = SideCombatTuningService::CalculateSortZ(combatant.RuntimeGroundPosition.y, tuning) + 0.10f;

                Entity back = EnsureWorldHealthSprite(scene, backName, { 0.025f, 0.020f, 0.025f, 0.82f });
                Entity fill = EnsureWorldHealthSprite(scene, fillName, healthRatio > 0.45f
                    ? glm::vec4(0.12f, 0.82f, 0.38f, 0.96f)
                    : (healthRatio > 0.22f
                        ? glm::vec4(0.95f, 0.72f, 0.18f, 0.96f)
                        : glm::vec4(0.95f, 0.18f, 0.15f, 0.96f)));

                if (back && back.HasComponent<TransformComponent>())
                {
                    auto& transform = back.GetComponent<TransformComponent>();
                    transform.Translation = { baseX, baseY, z };
                    transform.Scale = { fullWidth + 0.08f, 0.095f, 1.0f };
                }

                if (fill && fill.HasComponent<TransformComponent>())
                {
                    auto& transform = fill.GetComponent<TransformComponent>();
                    transform.Translation = {
                        baseX - fullWidth * 0.5f + fillWidth * 0.5f,
                        baseY,
                        z + 0.01f
                    };
                    transform.Scale = { fillWidth, 0.052f, 1.0f };
                }
            }
        }

    } // namespace

    void UpdateUI(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        Entity boss)
    {
        const SideCombatantComponent* playerCombatant =
            player && player.HasComponent<SideCombatantComponent>()
            ? &player.GetComponent<SideCombatantComponent>()
            : nullptr;
        const SideCombatantComponent* bossCombatant =
            boss && boss.HasComponent<SideCombatantComponent>()
            ? &boss.GetComponent<SideCombatantComponent>()
            : nullptr;
        const SidePlayerControllerComponent* controller =
            player && player.HasComponent<SidePlayerControllerComponent>()
            ? &player.GetComponent<SidePlayerControllerComponent>()
            : nullptr;
        const auto& tuning = SideCombatTuningService::GetTuning(level);

        if (playerCombatant)
        {
            SetProgress(scene, level.PlayerHealthBarEntityName, playerCombatant->Health, playerCombatant->MaxHealth);
            SetText(scene, level.PlayerHealthTextEntityName,
                "生命 " + FormatFloat(playerCombatant->Health) + "/" + FormatFloat(playerCombatant->MaxHealth));
        }
        if (bossCombatant)
        {
            const bool bossVisible = !level.WaveModeEnabled ||
                !boss.HasComponent<SideEnemyAIComponent>() ||
                boss.GetComponent<SideEnemyAIComponent>().RuntimeAwake ||
                !bossCombatant->Alive;
            SetWidgetVisible(scene, level.BossHealthBarEntityName, bossVisible);
            SetWidgetVisible(scene, level.BossHealthTextEntityName, bossVisible);
            if (bossVisible)
            {
            SetProgress(scene, level.BossHealthBarEntityName, bossCombatant->Health, bossCombatant->MaxHealth);
            std::string bossText = "黑熊丈夫 " + FormatFloat(bossCombatant->Health) + "/" + FormatFloat(bossCombatant->MaxHealth);
            if (SideCombatTuningService::ShouldShowCombatStateHud(level, tuning))
                bossText += "  " + std::string(GetCombatStateLabel(bossCombatant->RuntimeState));
            if (SideCombatTuningService::ShouldShowBossProtectionHud(level, tuning) && bossCombatant->RuntimeProtectionMax > 0.0f)
            {
                bossText += " 保护 " + FormatFloat(bossCombatant->RuntimeProtection)
                    + "/" + FormatFloat(bossCombatant->RuntimeProtectionMax);
            }
            SetText(scene, level.BossHealthTextEntityName, bossText);
            }
        }

        UpdateEnemyHealthBars(scene, level, tuning);

        const bool showBreakLimitUi = SideCombatTuningService::ShouldShowBreakLimitUi(level, tuning);
        const std::string breakLimitInputText = SideCombatTuningService::IsBreakLimitOfficiallyAvailable(level, tuning)
            ? "L 断限"
            : "L 调试";
        std::string message = "A/D移动  W/S纵深  K跳跃  J斩击  S+J上挑";
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "basic_attack") ||
            SideCombatTuningService::IsSkillUnlocked(level, tuning, "air_basic"))
        {
            message += "  空中J";
        }
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "magic_bolt"))
            message += "  U魔法";
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "ally_support"))
            message += "  I支援";
        if (showBreakLimitUi)
            message += "  " + breakLimitInputText;
        if (level.RuntimeVictory)
            message = "敌人已击败。材料正在被魔剑吸附。";
        else if (level.RuntimeDefeat)
            message = "主角倒下了。可从战斗入口重试。";
        else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
            playerCombatant->RuntimeAirHeight >= tuning.AirCombo.HighAirSafetyHeight)
        {
            message = "高空连击中，地面攻击不容易打断你。";
        }
        else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
            playerCombatant->RuntimeAirHeight <= tuning.AirCombo.GroundThreatHeight)
        {
            message = "低空连击仍会被地面敌人打断，注意高度。";
        }
        else if (level.RuntimeComboCount >= 6 && showBreakLimitUi)
        {
            message = "靠近下坠且保护临界的目标使用断限，可刷新空中行动。";
        }

        SetText(scene, level.MessageTextEntityName, message);

        if (level.RuntimeComboCount > 0)
        {
            SetText(scene, level.ComboTextEntityName,
                "连击 x" + std::to_string(level.RuntimeComboCount)
                + "  最佳 x" + std::to_string(level.RuntimeBestCombo));
        }
        else
        {
            SetText(scene, level.ComboTextEntityName,
                "最佳连击 x" + std::to_string(level.RuntimeBestCombo));
        }

        if (controller)
        {
            const bool launcherUnlocked = SideCombatTuningService::IsSkillUnlocked(level, tuning, "launcher") ||
                SideCombatTuningService::IsSkillUnlocked(level, tuning, "air_chase");
            const bool magicUnlocked = SideCombatTuningService::IsSkillUnlocked(level, tuning, "magic_bolt");
            const bool supportUnlocked = SideCombatTuningService::IsSkillUnlocked(level, tuning, "ally_support");

            SetSkillSlotVisible(scene, "J", false);
            SetSkillSlotVisible(scene, "K", false);
            UpdateSkillSlot(scene, "SJ", "S+J", launcherUnlocked, controller->RuntimeLauncherCooldown,
                std::max(controller->LauncherCooldown, tuning.AirCombo.AirChaseCooldown));
            UpdateSkillSlot(scene, "U", "U", magicUnlocked, controller->RuntimeMagicBoltCooldown,
                controller->MagicBoltCooldown);
            UpdateSkillSlot(scene, "I", "I", supportUnlocked, controller->RuntimeAllySupportCooldown,
                controller->AllySupportCooldown);
            if (showBreakLimitUi)
            {
                UpdateSkillSlot(scene, "L", "L", true, controller->RuntimeBreakLimitCooldown,
                    tuning.AirCombo.BreakLimitCooldown);
            }
            else
            {
                SetSkillSlotVisible(scene, "L", false);
            }

            SetText(scene, level.SkillTextEntityName,
                "魔剑槽 " + FormatFloat(controller->RuntimeMagicSwordGauge, 1)
                + "/" + FormatFloat(controller->RuntimeMagicSwordGaugeMax, 0)
                + "  空中行动 " + std::to_string(controller->RuntimeAirActionsRemaining));

            std::string hoveredKey;
            std::string tooltip;
            if (IsButtonHovered(scene, "SC_SkillIcon_SJ"))
            {
                hoveredKey = "SJ";
                tooltip = "裂空上挑\nS+J 将目标打浮空。按 K 跟跳后，可继续空中 J 追击。";
                if (controller->RuntimeLauncherCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeLauncherCooldown) + "秒";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_U"))
            {
                hoveredKey = "U";
                tooltip = "火球术\n远程命中，可用来维持空中连击。";
                if (controller->RuntimeMagicBoltCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeMagicBoltCooldown) + "秒";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_I"))
            {
                hoveredKey = "I";
                tooltip = "队友支援\n召唤支援延长浮空控制，降低连段失误惩罚。";
                if (controller->RuntimeAllySupportCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeAllySupportCooldown) + "秒";
            }
            else if (showBreakLimitUi && IsButtonHovered(scene, "SC_SkillIcon_L"))
            {
                hoveredKey = "L";
                tooltip = breakLimitInputText + "\n高阶空连工具，可刷新跳跃和空中行动节奏。";
                if (controller->RuntimeBreakLimitCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeBreakLimitCooldown) + "秒";
            }
            ApplyCombatItemTooltip(scene, hoveredKey, tooltip);
            UpdateSkillTooltip(scene, hoveredKey, tooltip);
        }
        else
        {
            std::string hoveredKey;
            std::string tooltip;
            ApplyCombatItemTooltip(scene, hoveredKey, tooltip);
            UpdateSkillTooltip(scene, hoveredKey, tooltip);
        }

        std::string reward = level.RuntimeVictory
            ? (level.RuntimeResultSummary.empty()
                ? level.FirstClearRewardText
                : level.RuntimeResultSummary)
            : "主要掉落";
        if (level.RuntimeCollectedPickups > 0)
            reward += "  已吸附 " + std::to_string(level.RuntimeCollectedPickups);
        SetText(scene, level.RewardTextEntityName, reward);
        UpdateCombatItemSlots(scene);
    }

} // namespace Wheatear::SideCombatHudService
