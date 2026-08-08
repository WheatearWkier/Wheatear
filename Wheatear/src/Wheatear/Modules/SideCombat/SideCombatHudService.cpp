#include "wtpch.h"
#include "SideCombatHudService.h"

#include "SideCombatTuningService.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/InputBindingService.h"
#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Modules/Common/GameplayTextService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>

namespace Wheatear::SideCombatHudService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::IsButtonHovered;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetImageTexture;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetRadialProgress;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        static constexpr const char* kSideCombatCanvasName = "WT_UI_Canvas";
        static constexpr float kSideUISheetWidth = 2560.0f;
        static constexpr float kSideUISheetHeight = 1440.0f;
        static constexpr float kComboFontSheetWidth = 512.0f;
        static constexpr float kComboFontSheetHeight = 288.0f;

        struct SheetUVRect
        {
            glm::vec2 Min = { 0.0f, 0.0f };
            glm::vec2 Max = { 1.0f, 1.0f };
        };

        struct CombatItemSlot
        {
            std::string Key;
            std::string Shortcut;
            std::string IconPath;
            std::string DisplayName;
            std::string Usage;
            SheetUVRect IconUV;
        };

        struct FontGlyph
        {
            SheetUVRect UV;
            float Aspect = 1.0f;
        };

        static SheetUVRect SheetRect(float x0, float y0, float x1, float y1)
        {
            return {
                { x0 / kSideUISheetWidth, 1.0f - y1 / kSideUISheetHeight },
                { x1 / kSideUISheetWidth, 1.0f - y0 / kSideUISheetHeight }
            };
        }

        static FontGlyph FontRect(float x0, float y0, float x1, float y1)
        {
            const float width = std::max(1.0f, x1 - x0);
            const float height = std::max(1.0f, y1 - y0);
            return {
                {
                    { x0 / kComboFontSheetWidth, 1.0f - y1 / kComboFontSheetHeight },
                    { x1 / kComboFontSheetWidth, 1.0f - y0 / kComboFontSheetHeight }
                },
                width / height
            };
        }

        static std::string SideUISheetPath()
        {
            return AssetAliasRegistry::Path("side.ui.sheet",
                "assets/vertical_slice/side_combat/ui/sidecombat_ui_sheet.png");
        }

        static std::string ComboFontSheetPath()
        {
            return AssetAliasRegistry::Path("side.ui.combo_font",
                "assets/vertical_slice/side_combat/ui/font/sidecombat_combo_font.png");
        }

        static std::string BreakLimitIconPath()
        {
            return AssetAliasRegistry::Path("side.skill.icon.break_limit",
                "assets/vertical_slice/side_combat/ui/icon_skill_break_limit.png");
        }

        static SheetUVRect PlayerFrameUV() { return SheetRect(257.0f, 32.0f, 1099.0f, 383.0f); }
        static SheetUVRect BossFrameUV() { return SheetRect(1136.0f, 80.0f, 2327.0f, 348.0f); }
        static SheetUVRect ComboFrameUV() { return SheetRect(1270.0f, 363.0f, 2318.0f, 613.0f); }
        static SheetUVRect SwordMaskUV() { return SheetRect(437.0f, 427.0f, 1073.0f, 543.0f); }
        static SheetUVRect RedBarUV() { return SheetRect(1645.0f, 691.0f, 2261.0f, 738.0f); }
        static SheetUVRect BlueBarUV() { return SheetRect(1645.0f, 782.0f, 2261.0f, 828.0f); }
        static SheetUVRect CyanBarUV() { return SheetRect(1645.0f, 875.0f, 2261.0f, 922.0f); }
        static SheetUVRect GoldBarUV() { return SheetRect(1645.0f, 968.0f, 2260.0f, 1015.0f); }
        static SheetUVRect JoystickUV() { return SheetRect(492.0f, 787.0f, 701.0f, 996.0f); }
        static SheetUVRect ItemFrameUV() { return SheetRect(298.0f, 807.0f, 470.0f, 979.0f); }
        static SheetUVRect BasicAttackUV() { return SheetRect(289.0f, 1010.0f, 478.0f, 1194.0f); }
        static SheetUVRect SkillOneUV() { return SheetRect(509.0f, 1011.0f, 696.0f, 1194.0f); }
        static SheetUVRect SkillTwoUV() { return SheetRect(726.0f, 1014.0f, 907.0f, 1193.0f); }
        static SheetUVRect SkillThreeUV() { return SheetRect(934.0f, 1012.0f, 1117.0f, 1194.0f); }
        static SheetUVRect SkillFourUV() { return SheetRect(1143.0f, 1014.0f, 1325.0f, 1195.0f); }
        static SheetUVRect SupportSkillUV() { return SheetRect(1354.0f, 1014.0f, 1538.0f, 1195.0f); }
        static SheetUVRect HealItemUV() { return SheetRect(297.0f, 1239.0f, 429.0f, 1392.0f); }
        static SheetUVRect ManaItemUV() { return SheetRect(461.0f, 1223.0f, 561.0f, 1391.0f); }
        static SheetUVRect StatusItemUV() { return SheetRect(600.0f, 1233.0f, 738.0f, 1393.0f); }
        static SheetUVRect BuffAttackUV() { return SheetRect(1575.0f, 1233.0f, 1751.0f, 1408.0f); }
        static SheetUVRect BuffShieldUV() { return SheetRect(1767.0f, 1230.0f, 1913.0f, 1407.0f); }
        static SheetUVRect DebuffStateUV() { return SheetRect(1931.0f, 1230.0f, 2110.0f, 1409.0f); }
        static SheetUVRect DebuffBreakUV() { return SheetRect(2125.0f, 1232.0f, 2299.0f, 1408.0f); }

        static FontGlyph ComboLabelGlyph() { return FontRect(336.0f, 28.0f, 498.0f, 115.0f); }
        static FontGlyph ComboMultiplyGlyph() { return FontRect(416.0f, 218.0f, 468.0f, 270.0f); }

        static FontGlyph ComboDigitGlyph(char digit)
        {
            switch (digit)
            {
            case '1': return FontRect(125.0f, 140.0f, 162.0f, 196.0f);
            case '2': return FontRect(195.0f, 140.0f, 240.0f, 196.0f);
            case '3': return FontRect(270.0f, 140.0f, 314.0f, 196.0f);
            case '4': return FontRect(344.0f, 140.0f, 389.0f, 196.0f);
            case '5': return FontRect(419.0f, 140.0f, 464.0f, 196.0f);
            case '6': return FontRect(48.0f, 215.0f, 91.0f, 269.0f);
            case '7': return FontRect(197.0f, 215.0f, 241.0f, 269.0f);
            case '8': return FontRect(268.0f, 215.0f, 312.0f, 269.0f);
            case '9': return FontRect(345.0f, 215.0f, 388.0f, 270.0f);
            case '0':
            default: return FontRect(49.0f, 140.0f, 93.0f, 196.0f);
            }
        }

        static void ClearPanelVisual(Entity entity)
        {
            if (!entity || !entity.HasComponent<UIPanelComponent>())
                return;

            auto& panel = entity.GetComponent<UIPanelComponent>();
            panel.BackgroundColor.a = 0.0f;
            panel.BorderColor.a = 0.0f;
            panel.BorderThickness = 0.0f;
        }

        static void ClearProgressVisual(Entity entity)
        {
            if (!entity || !entity.HasComponent<UIProgressBarComponent>())
                return;

            auto& progress = entity.GetComponent<UIProgressBarComponent>();
            progress.BackgroundColor.a = 0.0f;
            progress.ForegroundColor.a = 0.0f;
        }

        static Entity EnsureTransparentWidget(Scene* scene,
            const std::string& name,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            bool visible = true,
            bool button = false)
        {
            Entity entity = GameplayUILayoutService::EnsureUIWidget(scene,
                name,
                kSideCombatCanvasName,
                position,
                size,
                sortOrder,
                visible);
            if (!entity)
                return {};

            if (entity.HasComponent<UIWidgetComponent>())
            {
                auto& widget = entity.GetComponent<UIWidgetComponent>();
                widget.Visible = visible;
                widget.SortOrder = sortOrder;
            }

            ClearPanelVisual(entity);
            ClearProgressVisual(entity);

            if (button)
            {
                auto& buttonComponent = entity.HasComponent<UIButtonComponent>()
                    ? entity.GetComponent<UIButtonComponent>()
                    : entity.AddComponent<UIButtonComponent>();
                buttonComponent.NormalColor = { 0.0f, 0.0f, 0.0f, 0.0f };
                buttonComponent.HoverColor = { 0.0f, 0.65f, 1.0f, 0.10f };
                buttonComponent.PressedColor = { 0.0f, 0.85f, 1.0f, 0.20f };
            }

            return entity;
        }

        static Entity EnsureSheetImage(Scene* scene,
            const std::string& name,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const SheetUVRect& uv,
            glm::vec4 color = glm::vec4(1.0f),
            bool button = false,
            bool visible = true,
            bool forceImage = false)
        {
            Entity entity = EnsureTransparentWidget(scene, name, position, size, sortOrder, visible, button);
            if (!entity)
                return {};

            const bool hadAuthoredImage = entity.HasComponent<UIImageComponent>() &&
                static_cast<bool>(entity.GetComponent<UIImageComponent>().Texture);
            auto& image = entity.HasComponent<UIImageComponent>()
                ? entity.GetComponent<UIImageComponent>()
                : entity.AddComponent<UIImageComponent>();
            if (forceImage || !hadAuthoredImage)
            {
                image.Color = color;
                image.UVMin = uv.Min;
                image.UVMax = uv.Max;
                SetImageTexture(scene, name, SideUISheetPath());
            }
            else if (visible && image.Color.a <= 0.001f)
            {
                image.Color.a = color.a;
            }
            return entity;
        }

        static Entity EnsureComboFontImage(Scene* scene,
            const std::string& name,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const FontGlyph& glyph,
            glm::vec4 color = glm::vec4(1.0f),
            bool visible = true)
        {
            Entity entity = EnsureTransparentWidget(scene, name, position, size, sortOrder, visible, false);
            if (!entity)
                return {};

            if (entity.HasComponent<UIWidgetComponent>())
            {
                auto& widget = entity.GetComponent<UIWidgetComponent>();
                widget.Position = position;
                widget.Size = size;
                widget.SortOrder = sortOrder;
                widget.Visible = visible;
            }

            auto& image = entity.HasComponent<UIImageComponent>()
                ? entity.GetComponent<UIImageComponent>()
                : entity.AddComponent<UIImageComponent>();
            image.Color = color;
            image.UVMin = glyph.UV.Min;
            image.UVMax = glyph.UV.Max;
            SetImageTexture(scene, name, ComboFontSheetPath());
            return entity;
        }

        static void EnsureSheetFill(Scene* scene,
            const std::string& name,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const SheetUVRect& uv,
            float ratio,
            glm::vec4 color = glm::vec4(1.0f),
            bool visible = true)
        {
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            const bool showFill = visible && ratio > 0.002f;
            const float width = std::max(0.001f, size.x * ratio);
            Entity entity = EnsureSheetImage(scene,
                name,
                position,
                { width, size.y },
                sortOrder,
                uv,
                color,
                false,
                showFill,
                true);
            if (!entity || !entity.HasComponent<UIImageComponent>())
                return;

            if (entity.HasComponent<UIWidgetComponent>())
            {
                auto& widget = entity.GetComponent<UIWidgetComponent>();
                widget.Position = position;
                widget.Size = { width, size.y };
                widget.SortOrder = sortOrder;
                widget.Visible = showFill;
            }

            auto& image = entity.GetComponent<UIImageComponent>();
            image.UVMax.x = uv.Min.x + (uv.Max.x - uv.Min.x) * ratio;
        }

        static Entity EnsureHudText(Scene* scene,
            const std::string& name,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const std::string& value,
            float fontSize,
            glm::vec4 color,
            bool visible = true)
        {
            Entity entity = GameplayUILayoutService::EnsureText(scene,
                name,
                kSideCombatCanvasName,
                position,
                size,
                sortOrder,
                value,
                fontSize,
                color);
            if (entity && entity.HasComponent<UIWidgetComponent>())
                entity.GetComponent<UIWidgetComponent>().Visible = visible;
            return entity;
        }

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
            case SideCombatState::HitStun: return "受击";
            case SideCombatState::Launched: return "浮空";
            case SideCombatState::Knockdown: return "倒地";
            case SideCombatState::Recovery: return "硬直";
            case SideCombatState::SuperArmor: return "霸体";
            case SideCombatState::Broken: return "破防";
            case SideCombatState::Dead: return "战败";
            case SideCombatState::Normal:
            default: return "正常";
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

        static void UpdateCooldownMask(Scene* scene,
            const std::string& sourceName,
            const std::string& overlayName,
            const std::string& textName,
            float cooldown,
            float maxCooldown);

        static Entity EnsureCooldownOverlay(Scene* scene,
            const std::string& name,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const SheetUVRect& uv);

        static void UpdateSkillSlot(Scene* scene,
            const std::string& key,
            const std::string& keyLabel,
            bool unlocked,
            float cooldown,
            float maxCooldown,
            const std::string& unavailableText = "未解锁")
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
                if (Entity source = FindEntityByName(scene, slot);
                    source && source.HasComponent<UIWidgetComponent>())
                {
                    const auto& sourceWidget = source.GetComponent<UIWidgetComponent>();
                    SetWidgetTopLeft(scene, overlay, sourceWidget.Position, sourceWidget.Size);
                }
                SetProgress(scene, overlay, 1.0f, 1.0f);
                SetRadialProgress(scene, overlay, 1.0f);
                SetWidgetVisible(scene, overlay, true);
                SetText(scene, text, unavailableText);
                SetWidgetVisible(scene, text, true);
                return;
            }

            UpdateCooldownMask(scene, slot, overlay, text, cooldown, maxCooldown);
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

            float x = 0.705f;
            float y = 0.690f;
            if (key == "J")
            {
                x = 0.690f;
                y = 0.790f;
            }
            else if (key == "S2")
            {
                x = 0.690f;
                y = 0.735f;
            }
            else if (key == "S3")
            {
                x = 0.690f;
                y = 0.685f;
            }
            else if (key == "U")
            {
                x = 0.690f;
                y = 0.590f;
            }
            else if (key == "I")
            {
                x = 0.690f;
                y = 0.485f;
            }
            else if (key == "L")
                x = 0.705f;
            else if (key == "ItemSlot1")
            {
                x = 0.04f;
                y = 0.535f;
            }
            else if (key == "ItemSlot2")
            {
                x = 0.095f;
                y = 0.535f;
            }
            else if (key == "ItemSlot3")
            {
                x = 0.150f;
                y = 0.535f;
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

        static void UpdateCooldownMask(Scene* scene,
            const std::string& sourceName,
            const std::string& overlayName,
            const std::string& textName,
            float cooldown,
            float maxCooldown)
        {
            Entity source = FindEntityByName(scene, sourceName);
            Entity overlay = FindEntityByName(scene, overlayName);
            if (!source || !overlay ||
                !source.HasComponent<UIWidgetComponent>() ||
                !overlay.HasComponent<UIWidgetComponent>())
            {
                return;
            }

            if (cooldown > 0.05f)
            {
                const auto& sourceWidget = source.GetComponent<UIWidgetComponent>();
                const float ratio = std::clamp(cooldown / std::max(0.05f, maxCooldown), 0.0f, 1.0f);
                SetWidgetTopLeft(scene,
                    overlayName,
                    sourceWidget.Position,
                    sourceWidget.Size);
                SetRadialProgress(scene, overlayName, ratio);
                SetWidgetVisible(scene, overlayName, ratio > 0.001f);
                SetText(scene, textName, FormatCooldownSeconds(cooldown));
                SetWidgetVisible(scene, textName, true);
            }
            else
            {
                SetRadialProgress(scene, overlayName, 0.0f);
                SetWidgetVisible(scene, overlayName, false);
                SetWidgetVisible(scene, textName, false);
            }
        }

        static const std::array<CombatItemSlot, 3>& GetCombatItemSlots()
        {
            static const std::array<CombatItemSlot, 3> slots = {
                CombatItemSlot{
                    "1",
                    "1",
                    SideUISheetPath(),
                    "生命药水",
                    "回复生命。按 1 使用。",
                    HealItemUV() },
                CombatItemSlot{
                    "2",
                    "2",
                    SideUISheetPath(),
                    "魔力药水",
                    "回复蓝量。按 2 使用。",
                    ManaItemUV() },
                CombatItemSlot{
                    "3",
                    "3",
                    SideUISheetPath(),
                    "力量药剂",
                    "临时提高攻击力。按 3 使用。",
                    StatusItemUV() }
            };
            return slots;
        }

        static float GetCombatItemCooldownRemaining(const SidePlayerControllerComponent* controller, const std::string& key)
        {
            if (!controller)
                return 0.0f;

            if (key == "1")
                return controller->RuntimeHealItemCooldown;
            if (key == "2")
                return controller->RuntimeManaItemCooldown;
            if (key == "3")
                return controller->RuntimeAttackBuffItemCooldown;
            return 0.0f;
        }

        static float GetCombatItemCooldownDuration(const SidePlayerControllerComponent* controller, const std::string& key)
        {
            if (!controller)
                return 0.0f;

            if (key == "1")
                return controller->HealItemCooldown;
            if (key == "2")
                return controller->ManaItemCooldown;
            if (key == "3")
                return controller->AttackBuffItemCooldown;
            return 0.0f;
        }

        static void SetItemSlotVisible(Scene* scene, const CombatItemSlot& slot, bool visible)
        {
            const std::string prefix = std::string("SC_ItemSlot_") + slot.Key;
            SetWidgetVisible(scene, prefix + "_Frame", visible);
            SetWidgetVisible(scene, prefix + "_Icon", visible);
            SetWidgetVisible(scene, prefix + "_Button", visible);
            SetWidgetVisible(scene, prefix + "_Count", visible);
            SetWidgetVisible(scene, prefix + "_Cooldown", visible);
            SetWidgetVisible(scene, prefix + "_CooldownText", visible);
        }

        static void UpdateCombatItemSlots(Scene* scene, const SidePlayerControllerComponent* controller)
        {
            int index = 0;
            const std::array<glm::vec2, 3> positions = {
                glm::vec2{ 0.038f, 0.662f },
                glm::vec2{ 0.096f, 0.662f },
                glm::vec2{ 0.154f, 0.662f }
            };
            const glm::vec2 frameSize = { 0.052f, 0.092f };
            const glm::vec2 iconInset = { 0.006f, 0.007f };
            const glm::vec2 iconSize = { 0.040f, 0.078f };

            for (const CombatItemSlot& slot : GetCombatItemSlots())
            {
                const std::string prefix = std::string("SC_ItemSlot_") + slot.Key;
                const glm::vec2 framePosition = positions[static_cast<size_t>(index)];
                SetItemSlotVisible(scene, slot, true);
                EnsureSheetImage(scene, prefix + "_Frame", framePosition, frameSize, 58, ItemFrameUV());
                EnsureSheetImage(scene, prefix + "_Icon", framePosition + iconInset, iconSize, 60, slot.IconUV,
                    glm::vec4(1.0f), true);
                EnsureTransparentWidget(scene, prefix + "_Button", framePosition, frameSize, 62, true, true);
                EnsureCooldownOverlay(scene, prefix + "_Cooldown", framePosition + iconInset, iconSize, 61, slot.IconUV);
                EnsureHudText(scene, prefix + "_CooldownText",
                    framePosition + iconInset + glm::vec2(0.003f, 0.026f),
                    { 0.034f, 0.020f },
                    64,
                    "",
                    11.0f,
                    { 0.95f, 0.98f, 1.0f, 1.0f },
                    false);
                GameplayUILayoutService::SetButtonCommand(scene, prefix + "_Icon", "side:item:" + slot.Key);
                GameplayUILayoutService::SetButtonCommand(scene, prefix + "_Button", "side:item:" + slot.Key);
                EnsureHudText(scene, prefix + "_Count",
                    framePosition + glm::vec2(0.005f, 0.004f),
                    { 0.018f, 0.018f },
                    63,
                    slot.Shortcut,
                    12.5f,
                    { 0.92f, 0.98f, 1.0f, 1.0f });
                SetImageTexture(scene, prefix + "_Icon", slot.IconPath);
                SetImageColor(scene, prefix + "_Icon", glm::vec4(1.0f));
                SetText(scene, prefix + "_Count", slot.Shortcut);
                UpdateCooldownMask(scene,
                    prefix + "_Icon",
                    prefix + "_Cooldown",
                    prefix + "_CooldownText",
                    GetCombatItemCooldownRemaining(controller, slot.Key),
                    GetCombatItemCooldownDuration(controller, slot.Key));
                ++index;
            }
        }

        static void ApplyCombatItemTooltip(Scene* scene,
            std::string& hoveredKey,
            std::string& tooltip,
            const SidePlayerControllerComponent* controller)
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
                const float cooldown = GetCombatItemCooldownRemaining(controller, slot.Key);
                if (cooldown > 0.05f)
                    stream << "\n冷却 " << FormatCooldownSeconds(cooldown) << " 秒";
                tooltip = stream.str();
                return;
            }
        }

        static Entity EnsureCooldownOverlay(Scene* scene,
            const std::string& name,
            glm::vec2 position,
            glm::vec2 size,
            int sortOrder,
            const SheetUVRect& uv)
        {
            Entity entity = GameplayUILayoutService::EnsureUIWidget(scene,
                name,
                kSideCombatCanvasName,
                position,
                size,
                sortOrder,
                false);
            if (!entity)
                return {};

            ClearPanelVisual(entity);
            ClearProgressVisual(entity);
            auto& radial = entity.HasComponent<UIRadialCooldownComponent>()
                ? entity.GetComponent<UIRadialCooldownComponent>()
                : entity.AddComponent<UIRadialCooldownComponent>();
            radial.Color = { 0.0f, 0.0f, 0.0f, 0.58f };
            radial.StartAngle = 1.57079632679f;
            radial.Thickness = 1.0f;
            radial.Fade = 0.005f;
            radial.Progress = 0.0f;
            auto& image = entity.HasComponent<UIImageComponent>()
                ? entity.GetComponent<UIImageComponent>()
                : entity.AddComponent<UIImageComponent>();
            image.Color.a = 0.0f;
            image.UVMin = uv.Min;
            image.UVMax = uv.Max;
            SetImageTexture(scene, name, SideUISheetPath());
            return entity;
        }

        static void ConfigureSkillButton(Scene* scene,
            const std::string& key,
            glm::vec2 position,
            glm::vec2 size,
            const SheetUVRect& uv,
            const std::string& keyLabel,
            glm::vec4 iconColor = glm::vec4(1.0f),
            bool visible = true)
        {
            const std::string prefix = "SC_Skill";
            EnsureTransparentWidget(scene, prefix + "Slot_" + key, position, size, 61, visible, true);
            EnsureSheetImage(scene, prefix + "Icon_" + key, position, size, 64, uv, iconColor, true, visible);
            EnsureCooldownOverlay(scene, prefix + "Cooldown_" + key, position, size, 66, uv);
            EnsureHudText(scene, prefix + "CooldownText_" + key,
                position + glm::vec2(size.x * 0.12f, size.y * 0.35f),
                { size.x * 0.76f, size.y * 0.24f },
                67,
                "",
                16.0f,
                { 0.95f, 0.98f, 1.0f, 1.0f },
                false);
            EnsureHudText(scene, prefix + "Key_" + key,
                position + glm::vec2(size.x * 0.18f, size.y * 0.74f),
                { size.x * 0.64f, size.y * 0.20f },
                68,
                keyLabel,
                13.0f,
                { 0.92f, 0.98f, 1.0f, 1.0f },
                visible && !keyLabel.empty());
        }

        static void ConfigureImageSkillButton(Scene* scene,
            const std::string& key,
            glm::vec2 position,
            glm::vec2 size,
            const std::string& texturePath,
            const std::string& keyLabel,
            bool visible = true)
        {
            const std::string prefix = "SC_Skill";
            EnsureTransparentWidget(scene, prefix + "Slot_" + key, position, size, 61, visible, true);

            Entity icon = EnsureTransparentWidget(scene, prefix + "Icon_" + key, position, size, 64, visible, true);
            if (icon)
            {
                const bool hadAuthoredImage = icon.HasComponent<UIImageComponent>() &&
                    static_cast<bool>(icon.GetComponent<UIImageComponent>().Texture);
                auto& image = icon.HasComponent<UIImageComponent>()
                    ? icon.GetComponent<UIImageComponent>()
                    : icon.AddComponent<UIImageComponent>();
                if (!hadAuthoredImage)
                {
                    image.Color = { 1.0f, 1.0f, 1.0f, visible ? 1.0f : 0.0f };
                    image.UVMin = { 0.0f, 0.0f };
                    image.UVMax = { 1.0f, 1.0f };
                    SetImageTexture(scene, prefix + "Icon_" + key, texturePath);
                }
                else if (visible && image.Color.a <= 0.001f)
                {
                    image.Color.a = 1.0f;
                }
            }

            Entity overlay = EnsureTransparentWidget(scene, prefix + "Cooldown_" + key, position, size, 66, false, false);
            if (overlay)
            {
                auto& image = overlay.HasComponent<UIImageComponent>()
                    ? overlay.GetComponent<UIImageComponent>()
                    : overlay.AddComponent<UIImageComponent>();
                image.Color.a = 0.0f;
                image.UVMin = { 0.0f, 0.0f };
                image.UVMax = { 1.0f, 1.0f };
                SetImageTexture(scene, prefix + "Cooldown_" + key, texturePath);
                auto& radial = overlay.HasComponent<UIRadialCooldownComponent>()
                    ? overlay.GetComponent<UIRadialCooldownComponent>()
                    : overlay.AddComponent<UIRadialCooldownComponent>();
                radial.Color = { 0.0f, 0.0f, 0.0f, 0.58f };
                radial.StartAngle = 1.57079632679f;
                radial.Thickness = 1.0f;
                radial.Fade = 0.005f;
                radial.Progress = 0.0f;
            }

            EnsureHudText(scene, prefix + "CooldownText_" + key,
                position + glm::vec2(size.x * 0.12f, size.y * 0.35f),
                { size.x * 0.76f, size.y * 0.24f },
                67,
                "",
                16.0f,
                { 0.95f, 0.98f, 1.0f, 1.0f },
                false);
            EnsureHudText(scene, prefix + "Key_" + key,
                position + glm::vec2(size.x * 0.18f, size.y * 0.74f),
                { size.x * 0.64f, size.y * 0.20f },
                68,
                keyLabel,
                13.0f,
                { 0.92f, 0.98f, 1.0f, 1.0f },
                visible && !keyLabel.empty());
        }

        static bool IsNegativeCombatState(SideCombatState state)
        {
            switch (state)
            {
            case SideCombatState::HitStun:
            case SideCombatState::Launched:
            case SideCombatState::Knockdown:
            case SideCombatState::Recovery:
            case SideCombatState::Broken:
            case SideCombatState::Dead:
                return true;
            case SideCombatState::Normal:
            case SideCombatState::SuperArmor:
            default:
                return false;
            }
        }

        static void UpdateStatusBadges(Scene* scene,
            const std::string& prefix,
            glm::vec2 buffStart,
            glm::vec2 debuffStart,
            const SideCombatantComponent* combatant,
            const SidePlayerControllerComponent* controller,
            bool playerSide,
            bool visible)
        {
            const glm::vec2 size = { 0.031f, 0.055f };
            const float gap = 0.040f;
            const bool magicBuff = visible && playerSide && controller &&
                controller->RuntimeAttackBuffTimer > 0.0f;
            const bool shieldBuff = visible && combatant &&
                (combatant->RuntimeInvulnerableTimer > 0.0f ||
                    combatant->Invulnerable ||
                    combatant->RuntimeState == SideCombatState::SuperArmor ||
                    combatant->RuntimeProtection > 0.0f);
            const bool stateDebuff = visible && combatant && IsNegativeCombatState(combatant->RuntimeState);
            const bool breakDebuff = visible && combatant &&
                (combatant->RuntimeState == SideCombatState::Broken ||
                    (!playerSide && combatant->RuntimeProtectionMax > 0.0f &&
                        combatant->RuntimeProtection <= 0.01f));

            EnsureSheetImage(scene, prefix + "_Buff_0", buffStart, size, 70, BuffAttackUV(),
                glm::vec4(1.0f), false, magicBuff);
            EnsureSheetImage(scene, prefix + "_Buff_1", buffStart + glm::vec2(gap, 0.0f), size, 70, BuffShieldUV(),
                glm::vec4(1.0f), false, shieldBuff);
            EnsureSheetImage(scene, prefix + "_Debuff_0", debuffStart, size, 70, DebuffStateUV(),
                glm::vec4(1.0f), false, stateDebuff);
            EnsureSheetImage(scene, prefix + "_Debuff_1", debuffStart + glm::vec2(gap, 0.0f), size, 70, DebuffBreakUV(),
                glm::vec4(1.0f), false, breakDebuff);
        }

        static void UpdateJoystickVisual(Scene* scene)
        {
            glm::vec2 basePosition = { 0.047f, 0.790f };
            glm::vec2 baseSize = { 0.080f, 0.142f };
            const glm::vec2 thumbSize = { 0.033f, 0.059f };
            glm::vec2 direction = { 0.0f, 0.0f };
            if (InputBindingService::IsActionDown("move.left"))
                direction.x -= 1.0f;
            if (InputBindingService::IsActionDown("move.right"))
                direction.x += 1.0f;
            if (InputBindingService::IsActionDown("move.up"))
                direction.y -= 1.0f;
            if (InputBindingService::IsActionDown("move.down"))
                direction.y += 1.0f;

            const float lengthSq = direction.x * direction.x + direction.y * direction.y;
            if (lengthSq > 1.0f)
                direction /= std::sqrt(lengthSq);

            Entity base = EnsureSheetImage(scene, "SC_JoystickBase", basePosition, baseSize, 58, JoystickUV(),
                { 1.0f, 1.0f, 1.0f, 0.86f });
            if (base && base.HasComponent<UIWidgetComponent>())
            {
                const auto& baseWidget = base.GetComponent<UIWidgetComponent>();
                basePosition = baseWidget.Position;
                baseSize = baseWidget.Size;
            }

            const glm::vec2 thumbCenter = basePosition + baseSize * 0.5f +
                direction * glm::vec2(0.022f, 0.039f);
            Entity thumb = EnsureSheetImage(scene,
                "SC_JoystickThumb",
                thumbCenter - thumbSize * 0.5f,
                thumbSize,
                59,
                JoystickUV(),
                lengthSq > 0.01f
                    ? glm::vec4(0.84f, 1.0f, 1.0f, 0.92f)
                    : glm::vec4(0.66f, 0.80f, 0.84f, 0.52f));
            if (thumb && thumb.HasComponent<UIWidgetComponent>())
            {
                auto& widget = thumb.GetComponent<UIWidgetComponent>();
                widget.Position = thumbCenter - thumbSize * 0.5f;
                widget.Size = thumbSize;
                widget.Visible = true;
                widget.SortOrder = 59;
            }
        }

        static void HideComboCounter(Scene* scene)
        {
            SetWidgetVisible(scene, "SC_ComboFrame", false);
            SetWidgetVisible(scene, "SC_ComboLabel", false);
            SetWidgetVisible(scene, "SC_ComboMultiply", false);
            for (int i = 0; i < 6; ++i)
                SetWidgetVisible(scene, "SC_ComboDigit_" + std::to_string(i), false);
        }

        static void GetComboFrameLayout(Scene* scene, glm::vec2& position, glm::vec2& size)
        {
            Entity frame = FindEntityByName(scene, "SC_ComboFrame");
            if (!frame || !frame.HasComponent<UIWidgetComponent>())
                return;

            const auto& widget = frame.GetComponent<UIWidgetComponent>();
            if (widget.Size.x > 0.001f && widget.Size.y > 0.001f)
            {
                position = widget.Position;
                size = widget.Size;
            }
        }

        static void UpdateComboCounter(Scene* scene, int comboCount)
        {
            if (!scene || comboCount <= 0)
            {
                HideComboCounter(scene);
                return;
            }

            glm::vec2 framePosition = { 0.370f, 0.205f };
            glm::vec2 frameSize = { 0.260f, 0.062f };
            GetComboFrameLayout(scene, framePosition, frameSize);
            const float horizontalPadding = frameSize.x * 0.12f;
            const float targetWidth = frameSize.x - horizontalPadding * 2.0f;
            const float gap = 0.0045f;

            const int displayCombo = std::clamp(comboCount, 1, 999999);
            const std::string digits = std::to_string(displayCombo);
            const FontGlyph label = ComboLabelGlyph();
            const FontGlyph multiply = ComboMultiplyGlyph();

            float glyphHeight = 0.036f;
            auto measureWidth = [&](float height)
            {
                float width = label.Aspect * height + multiply.Aspect * height + gap * 2.0f;
                for (char digit : digits)
                    width += ComboDigitGlyph(digit).Aspect * height + gap;
                return width;
            };

            const float measuredWidth = measureWidth(glyphHeight);
            if (measuredWidth > targetWidth)
                glyphHeight *= targetWidth / measuredWidth;

            const float totalWidth = measureWidth(glyphHeight);
            const float top = framePosition.y + (frameSize.y - glyphHeight) * 0.50f;
            float x = framePosition.x + (frameSize.x - totalWidth) * 0.50f;

            EnsureSheetImage(scene, "SC_ComboFrame", framePosition, frameSize, 38,
                ComboFrameUV(), { 1.0f, 1.0f, 1.0f, 0.94f }, false, true);

            const float labelWidth = label.Aspect * glyphHeight;
            EnsureComboFontImage(scene, "SC_ComboLabel", { x, top }, { labelWidth, glyphHeight }, 44, label);
            x += labelWidth + gap;

            const float multiplyWidth = multiply.Aspect * glyphHeight;
            EnsureComboFontImage(scene, "SC_ComboMultiply", { x, top }, { multiplyWidth, glyphHeight }, 44, multiply);
            x += multiplyWidth + gap;

            for (int i = 0; i < 6; ++i)
            {
                if (i >= static_cast<int>(digits.size()))
                {
                    SetWidgetVisible(scene, "SC_ComboDigit_" + std::to_string(i), false);
                    continue;
                }

                const FontGlyph digitGlyph = ComboDigitGlyph(digits[static_cast<size_t>(i)]);
                const float digitWidth = digitGlyph.Aspect * glyphHeight;
                EnsureComboFontImage(scene,
                    "SC_ComboDigit_" + std::to_string(i),
                    { x, top },
                    { digitWidth, glyphHeight },
                    44,
                    digitGlyph);
                x += digitWidth + gap;
            }
        }

        static void ConfigureSideCombatHudLayout(Scene* scene,
            const SideCombatLevelComponent& level,
            const SideCombatTuningService::SideCombatTuning& tuning,
            Entity boss,
            const SideCombatantComponent* playerCombatant,
            const SidePlayerControllerComponent* controller,
            const SideCombatantComponent* bossCombatant)
        {
            if (!scene)
                return;

            const float playerHealthRatio = playerCombatant
                ? std::clamp(playerCombatant->Health / std::max(1.0f, playerCombatant->MaxHealth), 0.0f, 1.0f)
                : 0.0f;
            const float manaRatio = controller
                ? std::clamp(controller->RuntimeMana / std::max(1.0f, controller->RuntimeManaMax), 0.0f, 1.0f)
                : 0.0f;
            const float ultimateRatio = controller
                ? std::clamp(controller->RuntimeMagicSwordGauge / std::max(1.0f, controller->RuntimeMagicSwordGaugeMax), 0.0f, 1.0f)
                : 0.0f;

            const bool bossVisible = bossCombatant &&
                (!level.WaveModeEnabled ||
                    !boss.HasComponent<SideEnemyAIComponent>() ||
                    boss.GetComponent<SideEnemyAIComponent>().RuntimeAwake ||
                    !bossCombatant->Alive);
            const float bossHealthRatio = bossCombatant
                ? std::clamp(bossCombatant->Health / std::max(1.0f, bossCombatant->MaxHealth), 0.0f, 1.0f)
                : 0.0f;
            const float bossProtectionRatio = bossCombatant
                ? std::clamp(bossCombatant->RuntimeProtection / std::max(1.0f, bossCombatant->RuntimeProtectionMax), 0.0f, 1.0f)
                : 0.0f;
            const bool breakLimitUiVisible = SideCombatTuningService::ShouldShowBreakLimitUi(level, tuning);

            SetWidgetVisible(scene, "SC_TutorialPanel", false);
            SetWidgetVisible(scene, "SC_SkillBarPanel", false);
            SetWidgetVisible(scene, level.MessageTextEntityName, false);
            SetWidgetVisible(scene, level.SkillTextEntityName, false);
            SetWidgetVisible(scene, level.RewardTextEntityName, false);

            EnsureSheetImage(scene, "SC_TopPanel", { 0.018f, 0.020f }, { 0.318f, 0.235f }, 18, PlayerFrameUV());
            EnsureSheetFill(scene, level.PlayerHealthBarEntityName, { 0.126f, 0.085f }, { 0.184f, 0.024f }, 24,
                RedBarUV(), playerHealthRatio);
            EnsureSheetFill(scene, "SC_PlayerMana", { 0.131f, 0.124f }, { 0.174f, 0.020f }, 24,
                BlueBarUV(), manaRatio);
            SetWidgetVisible(scene, "SC_PlayerUltimateFill", false);
            EnsureSheetFill(scene, "SC_PlayerUltimateMask", { 0.073f, 0.193f }, { 0.225f, 0.073f }, 25,
                SwordMaskUV(), ultimateRatio, { 1.0f, 1.0f, 1.0f, 0.92f });
            EnsureHudText(scene, level.PlayerHealthTextEntityName, { 0.126f, 0.056f }, { 0.178f, 0.024f }, 30,
                "", 13.0f, { 0.95f, 0.98f, 1.0f, 1.0f });

            EnsureSheetImage(scene, "SC_ComboPanel", { 0.502f, 0.020f }, { 0.455f, 0.182f }, 18,
                BossFrameUV(), glm::vec4(1.0f), false, bossVisible);
            EnsureSheetFill(scene, level.BossHealthBarEntityName, { 0.613f, 0.090f }, { 0.286f, 0.023f }, 24,
                CyanBarUV(), bossHealthRatio, glm::vec4(1.0f), bossVisible);
            EnsureSheetFill(scene, "SC_BossProtection", { 0.613f, 0.123f }, { 0.286f, 0.019f }, 25,
                GoldBarUV(), bossProtectionRatio, glm::vec4(1.0f), bossVisible && bossProtectionRatio > 0.002f);
            EnsureHudText(scene, level.BossHealthTextEntityName, { 0.612f, 0.058f }, { 0.270f, 0.024f }, 30,
                "", 13.0f, { 0.91f, 0.99f, 1.0f, 1.0f }, bossVisible);
            EnsureHudText(scene, level.ComboTextEntityName, { 0.392f, 0.038f }, { 0.170f, 0.032f }, 32,
                "", 16.0f, { 0.98f, 0.92f, 0.68f, 1.0f }, false);

            ConfigureSkillButton(scene, "J", { 0.898f, 0.810f }, { 0.072f, 0.128f }, BasicAttackUV(), "J");
            ConfigureSkillButton(scene, "SJ", { 0.812f, 0.823f }, { 0.056f, 0.099f }, SkillOneUV(), "S+J");
            ConfigureSkillButton(scene, "S2", { 0.834f, 0.744f }, { 0.054f, 0.096f }, SkillFourUV(), "U");
            ConfigureSkillButton(scene, "S3", { 0.873f, 0.691f }, { 0.054f, 0.096f }, SkillThreeUV(), "I");
            ConfigureSkillButton(scene, "U", { 0.916f, 0.677f }, { 0.056f, 0.099f }, SkillTwoUV(), "O");
            ConfigureSkillButton(scene, "I", { 0.916f, 0.568f }, { 0.052f, 0.092f }, SupportSkillUV(), "H");
            ConfigureImageSkillButton(scene, "L", { 0.858f, 0.576f }, { 0.052f, 0.092f },
                BreakLimitIconPath(), "L", breakLimitUiVisible);
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillSlot_J", "side:basic");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillIcon_J", "side:basic");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillSlot_SJ", "side:launcher");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillIcon_SJ", "side:launcher");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillSlot_S2", "side:magic");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillIcon_S2", "side:magic");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillSlot_S3", "side:dash");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillIcon_S3", "side:dash");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillSlot_I", "side:support");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillIcon_I", "side:support");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillSlot_L", "side:break_limit");
            GameplayUILayoutService::SetButtonCommand(scene, "SC_SkillIcon_L", "side:break_limit");

            UpdateJoystickVisual(scene);
            UpdateStatusBadges(scene, "SC_PlayerStatus", { 0.388f, 0.848f }, { 0.388f, 0.908f },
                playerCombatant, controller, true, playerCombatant != nullptr);
            UpdateStatusBadges(scene, "SC_EnemyStatus", { 0.602f, 0.202f }, { 0.602f, 0.260f },
                bossCombatant, nullptr, false, bossVisible);
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

        ConfigureSideCombatHudLayout(scene, level, tuning, boss, playerCombatant, controller, bossCombatant);

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
                std::string bossText = "首领生命 " + FormatFloat(bossCombatant->Health) + "/" + FormatFloat(bossCombatant->MaxHealth);
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
            : "L 调试断限";
        std::string message = "A/D 移动  W/S 纵深  K 跳跃  J 攻击  S+J 上挑";
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "basic_attack") ||
            SideCombatTuningService::IsSkillUnlocked(level, tuning, "air_basic"))
        {
            message += "  空中 J";
        }
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "magic_bolt"))
            message += "  U 魔法弹";
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "dash"))
            message += "  I 冲刺";
        message += "  O 未开放";
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "ally_support"))
            message += "  H 支援";
        if (showBreakLimitUi)
            message += "  " + breakLimitInputText;
        if (level.RuntimeVictory)
            message = "胜利。奖励正在吸收。";
        else if (level.RuntimeDefeat)
            message = "战败。可以从战斗入口重试。";
        else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
            playerCombatant->RuntimeAirHeight >= tuning.AirCombo.HighAirSafetyHeight)
        {
            message = "高空连击：地面攻击更难打断你。";
        }
        else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
            playerCombatant->RuntimeAirHeight <= tuning.AirCombo.GroundThreatHeight)
        {
            message = "低空连击：注意地面敌人，尽量保持高度。";
        }
        else if (level.RuntimeComboCount >= 6 && showBreakLimitUi)
        {
            message = "断限可以在首领霸体时清空保护条。";
        }

        SetText(scene, level.MessageTextEntityName, message);

        SetWidgetVisible(scene, level.ComboTextEntityName, false);
        UpdateComboCounter(scene, level.RuntimeComboCount);
        UpdateCombatItemSlots(scene, controller);

        if (controller)
        {
            const bool launcherUnlocked = SideCombatTuningService::IsSkillUnlocked(level, tuning, "launcher") ||
                SideCombatTuningService::IsSkillUnlocked(level, tuning, "air_chase");
            const bool dashUnlocked = SideCombatTuningService::IsSkillUnlocked(level, tuning, "dash");
            const bool magicUnlocked = SideCombatTuningService::IsSkillUnlocked(level, tuning, "magic_bolt");
            const bool supportUnlocked = SideCombatTuningService::IsSkillUnlocked(level, tuning, "ally_support");
            const bool launcherHasMana = controller->RuntimeMana + 0.001f >= controller->LauncherManaCost;
            const bool dashHasMana = controller->RuntimeMana + 0.001f >= controller->DashManaCost;
            const bool magicHasMana = controller->RuntimeMana + 0.001f >= controller->MagicBoltManaCost;
            const bool supportHasMana = controller->RuntimeMana + 0.001f >= controller->AllySupportManaCost;
            const bool breakLimitUnlocked = SideCombatTuningService::IsBreakLimitOfficiallyAvailable(level, tuning) ||
                SideCombatTuningService::IsBreakLimitDebugAvailable(level, tuning);
            const float breakLimitGaugeCost = std::max(tuning.AirCombo.BreakLimitGaugeCost,
                controller->RuntimeMagicSwordGaugeMax * 0.5f);
            const bool breakLimitHasGauge = controller->RuntimeMagicSwordGauge + 0.001f >= breakLimitGaugeCost;
            const bool breakLimitComboReady = level.RuntimeComboCount >= tuning.AirCombo.BreakLimitMinCombo;
            const bool breakLimitBossReady = bossCombatant &&
                bossCombatant->Alive &&
                bossCombatant->RuntimeState == SideCombatState::SuperArmor &&
                bossCombatant->RuntimeProtection > 0.0f;
            const bool breakLimitReady = breakLimitUnlocked &&
                breakLimitHasGauge &&
                breakLimitComboReady &&
                breakLimitBossReady;
            std::string breakLimitUnavailableText = "条件";
            if (!breakLimitUnlocked)
                breakLimitUnavailableText = "未解锁";
            else if (!breakLimitHasGauge)
                breakLimitUnavailableText = "蓝剑";
            else if (!breakLimitComboReady)
                breakLimitUnavailableText = "连击";
            else if (!breakLimitBossReady)
                breakLimitUnavailableText = "霸体";

            UpdateSkillSlot(scene, "J", "J", true, controller->RuntimeBasicCooldown,
                controller->BasicCooldown);
            SetSkillSlotVisible(scene, "K", false);
            UpdateSkillSlot(scene, "SJ", "S+J", launcherUnlocked && launcherHasMana, controller->RuntimeLauncherCooldown,
                std::max(controller->LauncherCooldown, tuning.AirCombo.AirChaseCooldown),
                launcherUnlocked ? "缺蓝" : "未解锁");
            UpdateSkillSlot(scene, "S2", "U", magicUnlocked && magicHasMana, controller->RuntimeMagicBoltCooldown,
                controller->MagicBoltCooldown,
                magicUnlocked ? "缺蓝" : "未解锁");
            UpdateSkillSlot(scene, "S3", "I", dashUnlocked && dashHasMana, controller->RuntimeDashCooldown,
                controller->DashCooldown,
                dashUnlocked ? "缺蓝" : "未解锁");
            UpdateSkillSlot(scene, "U", "O", false, 0.0f, 1.0f, "未开放");
            UpdateSkillSlot(scene, "I", "H", supportUnlocked && supportHasMana, controller->RuntimeAllySupportCooldown,
                controller->AllySupportCooldown,
                supportUnlocked ? "缺蓝" : "未解锁");
            if (showBreakLimitUi)
            {
                if (controller->RuntimeBreakLimitCooldown > 0.05f)
                {
                    UpdateSkillSlot(scene, "L", "L", true, controller->RuntimeBreakLimitCooldown,
                        tuning.AirCombo.BreakLimitCooldown);
                }
                else
                {
                    UpdateSkillSlot(scene, "L", "L", breakLimitReady, 0.0f,
                        tuning.AirCombo.BreakLimitCooldown,
                        breakLimitUnavailableText);
                }
            }
            else
            {
                SetSkillSlotVisible(scene, "L", false);
            }
            SetText(scene, level.SkillTextEntityName,
                "蓝剑 " + FormatFloat(controller->RuntimeMagicSwordGauge, 1)
                + "/" + FormatFloat(controller->RuntimeMagicSwordGaugeMax, 0)
                + "  空中行动 " + std::to_string(controller->RuntimeAirActionsRemaining));
            std::string hoveredKey;
            std::string tooltip;
            if (IsButtonHovered(scene, "SC_SkillIcon_J"))
            {
                hoveredKey = "J";
                tooltip = "普通攻击\nJ 或鼠标左键。";
                if (controller->RuntimeBasicCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeBasicCooldown) + " 秒";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_SJ"))
            {
                hoveredKey = "SJ";
                tooltip = "上挑\nS+J，消耗蓝量 " + FormatFloat(controller->LauncherManaCost) + "。";
                if (!launcherHasMana)
                    tooltip += "\n蓝量不足。";
                if (controller->RuntimeLauncherCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeLauncherCooldown) + " 秒";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_S2"))
            {
                hoveredKey = "S2";
                tooltip = "魔法弹\nU，消耗蓝量 " + FormatFloat(controller->MagicBoltManaCost) + "。";
                if (!magicHasMana)
                    tooltip += "\n蓝量不足。";
                if (controller->RuntimeMagicBoltCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeMagicBoltCooldown) + " 秒";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_S3"))
            {
                hoveredKey = "S3";
                tooltip = "冲刺攻击\nI，消耗蓝量 " + FormatFloat(controller->DashManaCost) + "。";
                tooltip += "\n短暂无敌并造成伤害。";
                if (!dashHasMana)
                    tooltip += "\n蓝量不足。";
                if (controller->RuntimeDashCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeDashCooldown) + " 秒";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_U"))
            {
                hoveredKey = "U";
                tooltip = "预留技能\nO，暂未开放。";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_I"))
            {
                hoveredKey = "I";
                tooltip = "支援技能\nH，消耗蓝量 " + FormatFloat(controller->AllySupportManaCost) + "。";
                if (!supportHasMana)
                    tooltip += "\n蓝量不足。";
                if (controller->RuntimeAllySupportCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeAllySupportCooldown) + " 秒";
            }
            else if (IsButtonHovered(scene, "SC_SkillIcon_L"))
            {
                hoveredKey = "L";
                tooltip = "断限\nL，消耗半条蓝剑，在首领霸体时清空黄条。";
                if (controller->RuntimeBreakLimitCooldown > 0.05f)
                    tooltip += "\n冷却 " + FormatCooldownSeconds(controller->RuntimeBreakLimitCooldown) + " 秒";
                else if (!breakLimitUnlocked)
                    tooltip += "\n尚未解锁。";
                else if (!breakLimitHasGauge)
                    tooltip += "\n蓝剑不足，需要 " + FormatFloat(breakLimitGaugeCost, 1) + "。";
                else if (!breakLimitComboReady)
                    tooltip += "\n连击不足，需要 " + std::to_string(tuning.AirCombo.BreakLimitMinCombo) + " 连击。";
                else if (!breakLimitBossReady)
                    tooltip += "\n等待首领进入霸体并保留黄条。";
            }
            ApplyCombatItemTooltip(scene, hoveredKey, tooltip, controller);
            UpdateSkillTooltip(scene, hoveredKey, tooltip);
        }
        else
        {
            std::string hoveredKey;
            std::string tooltip;
            ApplyCombatItemTooltip(scene, hoveredKey, tooltip, controller);
            UpdateSkillTooltip(scene, hoveredKey, tooltip);
        }
        std::string reward = level.RuntimeVictory
            ? (level.RuntimeResultSummary.empty()
                ? level.FirstClearRewardText
                : level.RuntimeResultSummary)
            : "主要掉落";
        if (level.RuntimeCollectedPickups > 0)
            reward += "  已吸收 " + std::to_string(level.RuntimeCollectedPickups);
        SetText(scene, level.RewardTextEntityName, reward);
    }

} // namespace Wheatear::SideCombatHudService
