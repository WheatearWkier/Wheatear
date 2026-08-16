#include "wtpch.h"
#include "SideCombatHudService.h"

#include "SideCombatTuningService.h"
#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Gameplay/Services/GameplayUILayoutService.h"
#include "Wheatear/Gameplay/Services/GameplayVisualService.h"
#include "Wheatear/Gameplay/Services/GameplayTextService.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/InputBindingService.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_set>

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
        static std::string s_SkillPrefix = "SC_Skill";

        // Skill-slot entity names are built from a fixed prefix + slot key and
        // reconstructed every frame (~5 strings per slot × 7 slots). Cache the
        // composed names per key with transparent lookup so the hot path only
        // pays for the initial build.
        struct SkillSlotNames
        {
            std::string Slot;
            std::string Icon;
            std::string Cooldown;
            std::string CooldownText;
            std::string KeyText;
        };

        struct TransparentStringHash
        {
            using is_transparent = void;
            size_t operator()(const std::string& s) const { return std::hash<std::string>{}(s); }
            size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
        };

        static const SkillSlotNames& CachedSkillSlotNames(std::string_view key)
        {
            static std::unordered_map<std::string, SkillSlotNames, TransparentStringHash, std::equal_to<>> cache;
            auto it = cache.find(key);
            if (it == cache.end())
            {
                SkillSlotNames names;
                names.Slot = s_SkillPrefix + "Slot_" + std::string(key);
                names.Icon = s_SkillPrefix + "Icon_" + std::string(key);
                names.Cooldown = s_SkillPrefix + "Cooldown_" + std::string(key);
                names.CooldownText = s_SkillPrefix + "CooldownText_" + std::string(key);
                names.KeyText = s_SkillPrefix + "Key_" + std::string(key);
                it = cache.emplace(std::string(key), std::move(names)).first;
            }
            return it->second;
        }

        static void WarnMissingAuthoredHud(Scene* scene,
            const std::string& entityName,
            const char* missing)
        {
            if (!scene || entityName.empty() || !missing)
                return;

            static std::unordered_set<std::string> warned;
            std::ostringstream key;
            key << scene << ':' << entityName << ':' << missing;
            if (warned.insert(key.str()).second)
            {
                WT_CORE_WARN("SideCombatHudService: '{}' is missing {}. Add it to the HUD scene/preset asset; runtime HUD component creation is disabled.",
                    entityName,
                    missing);
            }
        }

        struct SheetUVRect
        {
            glm::vec2 Min = { 0.0f, 0.0f };
            glm::vec2 Max = { 1.0f, 1.0f };
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

        static SheetUVRect SheetRect(const glm::vec4& pixels)
        {
            return SheetRect(pixels.x, pixels.y, pixels.z, pixels.w);
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

        static std::string ResolveHudTexturePath(const std::string& pathOrAlias, const std::string& fallback)
        {
            if (pathOrAlias.empty())
                return fallback;
            return AssetAliasRegistry::Resolve(pathOrAlias);
        }

        static const SideCombatLevelComponent::SkillHudSlot* FindSkillHudSlot(
            const SideCombatLevelComponent& level,
            const std::string& key)
        {
            for (const auto& slot : level.SkillHudSlots)
            {
                if (slot.Key == key)
                    return &slot;
            }
            return nullptr;
        }

        static const SideCombatLevelComponent::CombatItemHudSlot* FindCombatItemHudSlot(
            const SideCombatLevelComponent& level,
            const std::string& key)
        {
            for (const auto& slot : level.CombatItemHudSlots)
            {
                if (slot.Key == key)
                    return &slot;
            }
            return nullptr;
        }

        static std::string GetSkillKeyLabel(const SideCombatLevelComponent& level,
            const std::string& key,
            const std::string& fallback)
        {
            const auto* slot = FindSkillHudSlot(level, key);
            if (!slot || slot->KeyLabel.empty())
                return fallback;
            return slot->KeyLabel;
        }

        static void ReplaceAll(std::string& value, const std::string& token, const std::string& replacement)
        {
            if (token.empty())
                return;

            size_t position = 0;
            while ((position = value.find(token, position)) != std::string::npos)
            {
                value.replace(position, token.length(), replacement);
                position += replacement.length();
            }
        }

        static std::string FormatHudTemplate(std::string value,
            const std::string& valueText = {},
            const std::string& manaCostText = {},
            const std::string& comboText = {})
        {
            ReplaceAll(value, "{value}", valueText);
            ReplaceAll(value, "{mana_cost}", manaCostText);
            ReplaceAll(value, "{combo}", comboText);
            return value;
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
            int sortOrder,
            bool visible = true,
            bool button = false)
        {
            if (!scene)
                return {};

            Entity entity = GameplayUILayoutService::EnsureUIWidget(scene,
                name,
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
                if (!entity.HasComponent<UIButtonComponent>())
                    WarnMissingAuthoredHud(scene, name, "UIButtonComponent");
            }

            return entity;
        }

        static Entity EnsureSheetImage(Scene* scene,
            const std::string& name,
            int sortOrder,
            const SheetUVRect& uv,
            glm::vec4 color = glm::vec4(1.0f),
            bool button = false,
            bool visible = true,
            bool forceImage = false)
        {
            Entity entity = EnsureTransparentWidget(scene, name, button);
            if (!entity)
                return {};

            const bool hadAuthoredImage = entity.HasComponent<UIImageComponent>() &&
                static_cast<bool>(entity.GetComponent<UIImageComponent>().Texture);
            if (!entity.HasComponent<UIImageComponent>())
            {
                WarnMissingAuthoredHud(scene, name, "UIImageComponent");
                return {};
            }

            auto& image = entity.GetComponent<UIImageComponent>();
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
            Entity entity = EnsureTransparentWidget(scene, name, false);
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

            if (!entity.HasComponent<UIImageComponent>())
            {
                WarnMissingAuthoredHud(scene, name, "UIImageComponent");
                return {};
            }

            auto& image = entity.GetComponent<UIImageComponent>();
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
            if (!scene)
                return {};

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
            case SideCombatState::HitStun: return "鍙楀嚮";
            case SideCombatState::Launched: return "娴┖";
            case SideCombatState::Knockdown: return "鍊掑湴";
            case SideCombatState::Recovery: return "纭洿";
            case SideCombatState::SuperArmor: return "闇镐綋";
            case SideCombatState::Broken: return "鐮撮槻";
            case SideCombatState::Dead: return "鎴樿触";
            case SideCombatState::Normal:
            default: return "姝ｅ父";
            }
        }

        static void SetSkillSlotVisible(Scene* scene, const std::string& key, bool visible)
        {
            const SkillSlotNames& names = CachedSkillSlotNames(key);
            SetWidgetVisible(scene, names.Slot, visible);
            SetWidgetVisible(scene, names.Icon, visible);
            SetWidgetVisible(scene, names.Cooldown, visible);
            SetWidgetVisible(scene, names.CooldownText, visible);
            SetWidgetVisible(scene, names.KeyText, visible);
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
            const std::string& unavailableText = "")
        {
            const SkillSlotNames& names = CachedSkillSlotNames(key);
            const std::string& slot = names.Slot;
            const std::string& icon = names.Icon;
            const std::string& overlay = names.Cooldown;
            const std::string& text = names.CooldownText;
            const std::string& keyText = names.KeyText;

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
                SetRadialProgress(scene, overlay, 0.0f);
                SetWidgetVisible(scene, overlay, true);
                SetText(scene, text, unavailableText);
                SetWidgetVisible(scene, text, true);
                return;
            }

            UpdateCooldownMask(scene, slot, overlay, text, cooldown, maxCooldown);
        }

        static void UpdateSkillTooltip(Scene* scene,
            const SideCombatLevelComponent& level,
            const std::string& key,
            const std::string& text)
        {
            const bool visible = !key.empty() && !text.empty();
            SetWidgetVisible(scene, level.SkillTooltipPanelEntityName, visible);
            SetWidgetVisible(scene, level.SkillTooltipTextEntityName, visible);
            if (!visible)
                return;

            glm::vec2 tooltipPosition = level.SkillTooltipLayout.Position;
            if (const auto* slot = FindSkillHudSlot(level, key))
            {
                tooltipPosition = slot->TooltipPosition;
            }
            else if (key.rfind("item:", 0) == 0)
            {
                if (const auto* itemSlot = FindCombatItemHudSlot(level, key.substr(5)))
                    tooltipPosition = itemSlot->TooltipPosition;
            }

            const glm::vec2 size = level.SkillTooltipLayout.Size;
            const glm::vec2 position = {
                std::clamp(tooltipPosition.x, 0.04f, 0.96f - size.x),
                tooltipPosition.y
            };
            SetWidgetTopLeft(scene, level.SkillTooltipPanelEntityName, position, size);
            SetWidgetTopLeft(scene, level.SkillTooltipTextEntityName,
                position + level.SkillTooltipPadding,
                size - level.SkillTooltipPadding * 2.0f);
            SetText(scene, level.SkillTooltipTextEntityName, text);
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
                const float remainingRatio = std::clamp(
                    cooldown / std::max(0.05f, maxCooldown),
                    0.0f,
                    1.0f);
                const float completionRatio = 1.0f - remainingRatio;
                SetWidgetTopLeft(scene,
                    overlayName,
                    sourceWidget.Position,
                    sourceWidget.Size);
                SetRadialProgress(scene, overlayName, completionRatio);
                SetWidgetVisible(scene, overlayName, remainingRatio > 0.001f);
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

        static void SetItemSlotVisible(Scene* scene, const std::string& prefix, bool visible)
        {
            SetWidgetVisible(scene, prefix + "_Frame", visible);
            SetWidgetVisible(scene, prefix + "_Icon", visible);
            SetWidgetVisible(scene, prefix + "_Button", visible);
            SetWidgetVisible(scene, prefix + "_Count", visible);
            SetWidgetVisible(scene, prefix + "_Cooldown", visible);
            SetWidgetVisible(scene, prefix + "_CooldownText", visible);
        }

        static void ConfigureCombatItemSlots(Scene* scene,
            const SideCombatLevelComponent& level)
        {
            const std::string itemSlotPrefix = level.ItemSlotPrefix.empty()
                ? "SC_ItemSlot_"
                : level.ItemSlotPrefix;

            for (const auto& slot : level.CombatItemHudSlots)
            {
                if (!slot.Enabled || slot.Key.empty())
                    continue;

                const std::string prefix = itemSlotPrefix + slot.Key;
                const glm::vec2 framePosition = slot.Position;
                const glm::vec2 iconPosition = framePosition + slot.IconInset;
                SetItemSlotVisible(scene, prefix, true);
                EnsureSheetImage(scene, prefix + "_Frame", 58, ItemFrameUV());
                if (slot.UseSheetIcon)
                {
                    EnsureSheetImage(scene, prefix + "_Icon", 60, SheetRect(slot.IconSheetPixels),
                        glm::vec4(1.0f), true);
                }
                else
                {
                    Entity icon = EnsureTransparentWidget(scene, prefix + "_Icon", 60, true, true);
                    if (icon)
                    {
                        if (icon.HasComponent<UIImageComponent>())
                        {
                            auto& image = icon.GetComponent<UIImageComponent>();
                            image.Color = glm::vec4(1.0f);
                            image.UVMin = { 0.0f, 0.0f };
                            image.UVMax = { 1.0f, 1.0f };
                        }
                        else
                        {
                            WarnMissingAuthoredHud(scene, prefix + "_Icon", "UIImageComponent");
                        }
                    }
                }
                EnsureTransparentWidget(scene, prefix + "_Button", 62, true, true);
                EnsureCooldownOverlay(scene, prefix + "_Cooldown", iconPosition, slot.IconSize, 61, slot.UseSheetIcon
                    ? SheetRect(slot.IconSheetPixels)
                    : SheetRect(glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f }));
                EnsureHudText(scene, prefix + "_CooldownText",
                    iconPosition + glm::vec2(0.003f, 0.026f),
                    { 0.034f, 0.020f },
                    64,
                    "",
                    11.0f,
                    { 0.95f, 0.98f, 1.0f, 1.0f },
                    false);
                const std::string command = slot.Command.empty()
                    ? "side:item:" + slot.Key
                    : slot.Command;
                GameplayUILayoutService::SetButtonCommand(scene, prefix + "_Icon", command);
                GameplayUILayoutService::SetButtonCommand(scene, prefix + "_Button", command);
                EnsureHudText(scene, prefix + "_Count",
                    framePosition + glm::vec2(0.005f, 0.004f),
                    { 0.018f, 0.018f },
                    63,
                    slot.Shortcut,
                    12.5f,
                    { 0.92f, 0.98f, 1.0f, 1.0f });
                SetImageTexture(scene, prefix + "_Icon", ResolveHudTexturePath(slot.IconTexturePath, SideUISheetPath()));
                SetImageTexture(scene, prefix + "_Cooldown", ResolveHudTexturePath(slot.IconTexturePath, SideUISheetPath()));
                SetImageColor(scene, prefix + "_Icon", glm::vec4(1.0f));
                SetText(scene, prefix + "_Count", slot.Shortcut);
            }
        }

        static void UpdateCombatItemSlots(Scene* scene,
            const SideCombatLevelComponent& level,
            const SidePlayerControllerComponent* controller)
        {
            const std::string itemSlotPrefix = level.ItemSlotPrefix.empty()
                ? "SC_ItemSlot_"
                : level.ItemSlotPrefix;

            for (const auto& slot : level.CombatItemHudSlots)
            {
                if (!slot.Enabled || slot.Key.empty())
                    continue;

                const std::string prefix = itemSlotPrefix + slot.Key;
                UpdateCooldownMask(scene,
                    prefix + "_Icon",
                    prefix + "_Cooldown",
                    prefix + "_CooldownText",
                    GetCombatItemCooldownRemaining(controller, slot.Key),
                    GetCombatItemCooldownDuration(controller, slot.Key));
            }
        }

        static void ApplyCombatItemTooltip(Scene* scene,
            const SideCombatLevelComponent& level,
            std::string& hoveredKey,
            std::string& tooltip,
            const SidePlayerControllerComponent* controller)
        {
            if (!tooltip.empty())
                return;

            const std::string itemSlotPrefix = level.ItemSlotPrefix.empty()
                ? "SC_ItemSlot_"
                : level.ItemSlotPrefix;
            for (const auto& slot : level.CombatItemHudSlots)
            {
                if (!slot.Enabled || slot.Key.empty())
                    continue;

                const std::string prefix = itemSlotPrefix + slot.Key;
                if (!IsButtonHovered(scene, prefix + "_Button") &&
                    !IsButtonHovered(scene, prefix + "_Icon"))
                {
                    continue;
                }

                hoveredKey = "item:" + slot.Key;
                std::ostringstream stream;
                stream << slot.Shortcut << "  " << slot.DisplayName << "\n";
                stream << slot.UsageText;
                const float cooldown = GetCombatItemCooldownRemaining(controller, slot.Key);
                if (cooldown > 0.05f)
                    stream << "\n" << level.HudCooldownPrefix << FormatCooldownSeconds(cooldown) << level.HudSecondsSuffix;
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
            if (!scene)
                return {};

            Entity entity = GameplayUILayoutService::EnsureUIWidget(scene,
                name,
                false);
            if (!entity)
                return {};

            ClearPanelVisual(entity);
            ClearProgressVisual(entity);
            if (!entity.HasComponent<UIRadialCooldownComponent>())
            {
                WarnMissingAuthoredHud(scene, name, "UIRadialCooldownComponent");
                return {};
            }
            if (!entity.HasComponent<UIImageComponent>())
            {
                WarnMissingAuthoredHud(scene, name, "UIImageComponent");
                return {};
            }

            auto& radial = entity.GetComponent<UIRadialCooldownComponent>();
            radial.Color = { 0.0f, 0.0f, 0.0f, 0.58f };
            radial.StartAngle = 1.57079632679f;
            radial.Thickness = 1.0f;
            radial.Fade = 0.005f;
            radial.Progress = 0.0f;
            auto& image = entity.GetComponent<UIImageComponent>();
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
            const std::string& prefix = s_SkillPrefix;
            EnsureTransparentWidget(scene, prefix + "Slot_" + key, 61, visible, true);
            EnsureSheetImage(scene, prefix + "Icon_" + key, 64, uv, iconColor, true, visible);
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
            const std::string& prefix = s_SkillPrefix;
            EnsureTransparentWidget(scene, prefix + "Slot_" + key, 61, visible, true);

            Entity icon = EnsureTransparentWidget(scene, prefix + "Icon_" + key, 64, visible, true);
            if (icon)
            {
                const bool hadAuthoredImage = icon.HasComponent<UIImageComponent>() &&
                    static_cast<bool>(icon.GetComponent<UIImageComponent>().Texture);
                if (!icon.HasComponent<UIImageComponent>())
                {
                    WarnMissingAuthoredHud(scene, prefix + "Icon_" + key, "UIImageComponent");
                    return;
                }

                auto& image = icon.GetComponent<UIImageComponent>();
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

            Entity overlay = EnsureTransparentWidget(scene, prefix + "Cooldown_" + key, 66, false, false);
            if (overlay)
            {
                if (!overlay.HasComponent<UIImageComponent>())
                {
                    WarnMissingAuthoredHud(scene, prefix + "Cooldown_" + key, "UIImageComponent");
                    return;
                }
                if (!overlay.HasComponent<UIRadialCooldownComponent>())
                {
                    WarnMissingAuthoredHud(scene, prefix + "Cooldown_" + key, "UIRadialCooldownComponent");
                    return;
                }

                auto& image = overlay.GetComponent<UIImageComponent>();
                image.Color.a = 0.0f;
                image.UVMin = { 0.0f, 0.0f };
                image.UVMax = { 1.0f, 1.0f };
                SetImageTexture(scene, prefix + "Cooldown_" + key, texturePath);
                auto& radial = overlay.GetComponent<UIRadialCooldownComponent>();
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

        static void UpdateStatusIcons(Scene* scene,
            const std::string& prefix,
            glm::vec2 buffStart,
            glm::vec2 debuffStart,
            const SideCombatLevelComponent::StatusIconLayout& layout,
            const SideCombatantComponent* combatant,
            const SidePlayerControllerComponent* controller,
            bool playerSide,
            bool visible)
        {
            const glm::vec2 size = layout.Size;
            const float gap = layout.Gap;
            const bool magicBuff = visible && playerSide && controller &&
                controller->RuntimeAttackBuffTimer > 0.0f;
            const bool shieldBuff = visible && combatant &&
                (combatant->RuntimeInvulnerableTimer > 0.0f ||
                    combatant->Invulnerable ||
                    combatant->RuntimeState == SideCombatState::SuperArmor ||
                    combatant->RuntimeProtection > 0.0f);
            const bool stateDebuff = visible && combatant && IsNegativeCombatState(combatant->RuntimeState);
            // "Broken" is the only true break state: protection == 0 is the
            // boss's normal starting condition (it grows on hits and refills
            // during protection recovery), so it must not be mistaken for a
            // broken guard.
            const bool breakDebuff = visible && combatant &&
                combatant->RuntimeState == SideCombatState::Broken;

            EnsureSheetImage(scene, prefix + "_Buff_0", 70, BuffAttackUV(),
                glm::vec4(1.0f), false, magicBuff);
            EnsureSheetImage(scene, prefix + "_Buff_1", 70, BuffShieldUV(),
                glm::vec4(1.0f), false, shieldBuff);
            EnsureSheetImage(scene, prefix + "_Debuff_0", 70, DebuffStateUV(),
                glm::vec4(1.0f), false, stateDebuff);
            EnsureSheetImage(scene, prefix + "_Debuff_1", 70, DebuffBreakUV(),
                glm::vec4(1.0f), false, breakDebuff);
        }

        // On-screen joystick drag state (module-wide so the input sampler can
        // consume it without touching the HUD internals).
        bool g_JoystickDragging = false;
        glm::vec2 g_JoystickDirection = { 0.0f, 0.0f };

        // Quantizes a raw stick vector onto the 8-way grid ({-1,0,1}^2).
        static glm::vec2 QuantizeEightWay(glm::vec2 direction)
        {
            const float length = glm::length(direction);
            if (length < 0.35f)
                return { 0.0f, 0.0f };
            direction /= length;
            return {
                direction.x > 0.35f ? 1.0f : (direction.x < -0.35f ? -1.0f : 0.0f),
                direction.y > 0.35f ? 1.0f : (direction.y < -0.35f ? -1.0f : 0.0f)
            };
        }

        static void UpdateJoystickVisual(Scene* scene, const SideCombatLevelComponent& level)
        {
            glm::vec2 basePosition = level.JoystickBaseLayout.Position;
            glm::vec2 baseSize = level.JoystickBaseLayout.Size;
            const glm::vec2 thumbSize = level.JoystickThumbSize;
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

            Entity base = EnsureSheetImage(scene, level.JoystickBaseEntityName, 58, JoystickUV(),
                { 1.0f, 1.0f, 1.0f, 0.86f });
            if (base && base.HasComponent<UIWidgetComponent>())
            {
                const auto& baseWidget = base.GetComponent<UIWidgetComponent>();
                basePosition = baseWidget.Position;
                baseSize = baseWidget.Size;
            }

            // Mouse drag: press inside the stick base to grab it, drag to
            // steer (8-way), release to let go. The stick direction is shared
            // with the movement sampler through GetJoystickInputDirection().
            Window& window = Application::Get().GetWindow();
            const glm::vec2 windowSize = {
                std::max(1.0f, static_cast<float>(window.GetWidth())),
                std::max(1.0f, static_cast<float>(window.GetHeight()))
            };
            const glm::vec2 baseCenterPx = (basePosition + baseSize * 0.5f) * windowSize;
            const glm::vec2 baseHalfPx = baseSize * windowSize * 0.5f;
            const glm::vec2 mousePx = { Input::GetMouseX(), Input::GetMouseY() };
            // IsMouseButtonPressed reports the held state (GLFW_PRESS), which
            // is exactly what a drag needs: grab while held inside the base,
            // keep steering when the cursor leaves it, release on unpress.
            const bool leftDown = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
            const float travelPx = std::max(1.0f, level.JoystickThumbTravel.x * windowSize.x);

            if (!g_JoystickDragging
                && leftDown
                && glm::abs(mousePx - baseCenterPx).x <= baseHalfPx.x
                && glm::abs(mousePx - baseCenterPx).y <= baseHalfPx.y)
            {
                g_JoystickDragging = true;
            }
            if (g_JoystickDragging && !leftDown)
            {
                g_JoystickDragging = false;
                g_JoystickDirection = { 0.0f, 0.0f };
            }
            if (g_JoystickDragging)
            {
                glm::vec2 raw = (mousePx - baseCenterPx) / travelPx;
                const float rawLength = glm::length(raw);
                if (rawLength > 1.0f)
                    raw /= rawLength;
                g_JoystickDirection = QuantizeEightWay(raw);
                if (g_JoystickDirection.x != 0.0f || g_JoystickDirection.y != 0.0f)
                    direction = g_JoystickDirection;
            }

            const glm::vec2 thumbCenter = basePosition + baseSize * 0.5f +
                direction * level.JoystickThumbTravel;
            Entity thumb = EnsureSheetImage(scene,
                level.JoystickThumbEntityName,
                59,
                JoystickUV(),
                lengthSq > 0.01f || g_JoystickDragging
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

        static void HideComboCounter(Scene* scene, const SideCombatLevelComponent& level)
        {
            SetWidgetVisible(scene, level.ComboFrameEntityName, false);
            SetWidgetVisible(scene, level.ComboLabelEntityName, false);
            SetWidgetVisible(scene, level.ComboMultiplyEntityName, false);
            for (int i = 0; i < 6; ++i)
                SetWidgetVisible(scene, level.ComboDigitPrefix + std::to_string(i), false);
        }

        static bool IsResultFadeActive(const SideCombatLevelComponent& level)
        {
            if (!level.RuntimeVictory && !level.RuntimeDefeat)
                return false;

            const bool victory = level.RuntimeVictory;
            const float delay = std::max(0.0f, victory ? level.VictoryReturnDelay : level.DefeatReturnDelay);
            const float fadeDuration = std::max(0.01f, level.ResultSceneFadeDuration);
            const float fadeStart = std::max(0.0f, delay - fadeDuration);
            return level.RuntimeResultTimer >= fadeStart;
        }

        static void HideResultFadeTextHud(Scene* scene, const SideCombatLevelComponent& level)
        {
            SetWidgetVisible(scene, level.MessageTextEntityName, false);
            SetWidgetVisible(scene, level.SkillTextEntityName, false);
            SetWidgetVisible(scene, level.RewardTextEntityName, false);
            SetWidgetVisible(scene, level.PlayerHealthTextEntityName, false);
            SetWidgetVisible(scene, level.BossHealthTextEntityName, false);
            SetWidgetVisible(scene, level.ComboTextEntityName, false);
            SetWidgetVisible(scene, level.SkillTooltipPanelEntityName, false);
            SetWidgetVisible(scene, level.SkillTooltipTextEntityName, false);
            HideComboCounter(scene, level);

            const std::string itemSlotPrefix = level.ItemSlotPrefix.empty()
                ? "SC_ItemSlot_"
                : level.ItemSlotPrefix;
            for (const auto& slot : level.CombatItemHudSlots)
            {
                if (!slot.Enabled || slot.Key.empty())
                    continue;

                const std::string prefix = itemSlotPrefix + slot.Key;
                SetWidgetVisible(scene, prefix + "_Cooldown", false);
                SetWidgetVisible(scene, prefix + "_Count", false);
                SetWidgetVisible(scene, prefix + "_CooldownText", false);
            }

            for (const auto& slot : level.SkillHudSlots)
            {
                if (!slot.Enabled || slot.Key.empty())
                    continue;

                SetWidgetVisible(scene, s_SkillPrefix + "Cooldown_" + slot.Key, false);
                SetWidgetVisible(scene, s_SkillPrefix + "Key_" + slot.Key, false);
                SetWidgetVisible(scene, s_SkillPrefix + "CooldownText_" + slot.Key, false);
            }
        }

        static void GetComboFrameLayout(Scene* scene, const std::string& frameEntityName, glm::vec2& position, glm::vec2& size)
        {
            Entity frame = FindEntityByName(scene, frameEntityName);
            if (!frame || !frame.HasComponent<UIWidgetComponent>())
                return;

            const auto& widget = frame.GetComponent<UIWidgetComponent>();
            if (widget.Size.x > 0.001f && widget.Size.y > 0.001f)
            {
                position = widget.Position;
                size = widget.Size;
            }
        }

        static void UpdateComboCounter(Scene* scene, const SideCombatLevelComponent& level, int comboCount)
        {
            if (!scene || comboCount <= 0)
            {
                HideComboCounter(scene, level);
                return;
            }

            glm::vec2 framePosition = level.ComboFrameLayout.Position;
            glm::vec2 frameSize = level.ComboFrameLayout.Size;
            GetComboFrameLayout(scene, level.ComboFrameEntityName, framePosition, frameSize);
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

            EnsureSheetImage(scene, level.ComboFrameEntityName, 38,
                ComboFrameUV(), { 1.0f, 1.0f, 1.0f, 0.94f }, false, true);

            const float labelWidth = label.Aspect * glyphHeight;
            EnsureComboFontImage(scene, level.ComboLabelEntityName, { x, top }, { labelWidth, glyphHeight }, 44, label);
            x += labelWidth + gap;

            const float multiplyWidth = multiply.Aspect * glyphHeight;
            EnsureComboFontImage(scene, level.ComboMultiplyEntityName, { x, top }, { multiplyWidth, glyphHeight }, 44, multiply);
            x += multiplyWidth + gap;

            for (int i = 0; i < 6; ++i)
            {
                if (i >= static_cast<int>(digits.size()))
                {
                    SetWidgetVisible(scene, level.ComboDigitPrefix + std::to_string(i), false);
                    continue;
                }

                const FontGlyph digitGlyph = ComboDigitGlyph(digits[static_cast<size_t>(i)]);
                const float digitWidth = digitGlyph.Aspect * glyphHeight;
                EnsureComboFontImage(scene,
                    level.ComboDigitPrefix + std::to_string(i),
                    { x, top },
                    { digitWidth, glyphHeight },
                    44,
                    digitGlyph);
                x += digitWidth + gap;
            }
        }

        static void ConfigureSideCombatHudLayout(Scene* scene,
            SideCombatLevelComponent& level,
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

            if (!level.RuntimeHudLayoutConfigured)
            {

                SetWidgetVisible(scene, level.SkillBarPanelEntityName, false);
                SetWidgetVisible(scene, level.MessageTextEntityName, false);
                SetWidgetVisible(scene, level.SkillTextEntityName, false);
                SetWidgetVisible(scene, level.RewardTextEntityName, false);

                EnsureSheetImage(scene, level.TopPanelEntityName, 18, PlayerFrameUV());
                EnsureSheetFill(scene, level.PlayerHealthBarEntityName, level.PlayerHealthLayout.Position, level.PlayerHealthLayout.Size, 24,
                    RedBarUV(), playerHealthRatio);
                SetWidgetVisible(scene, level.PlayerUltimateFillEntityName, false);
                EnsureHudText(scene, level.PlayerHealthTextEntityName, level.PlayerHealthTextLayout.Position, level.PlayerHealthTextLayout.Size, 30,
                    "", 13.0f, { 0.95f, 0.98f, 1.0f, 1.0f });

                EnsureSheetImage(scene, level.ComboPanelEntityName, 18,
                    BossFrameUV(), glm::vec4(1.0f), false, bossVisible);
                EnsureSheetFill(scene, level.BossHealthBarEntityName, level.BossHealthLayout.Position, level.BossHealthLayout.Size, 24,
                    CyanBarUV(), bossHealthRatio, glm::vec4(1.0f), bossVisible);
                EnsureHudText(scene, level.BossHealthTextEntityName, level.BossHealthTextLayout.Position, level.BossHealthTextLayout.Size, 30,
                    "", 13.0f, { 0.91f, 0.99f, 1.0f, 1.0f }, bossVisible);
                EnsureHudText(scene, level.ComboTextEntityName, level.ComboTextLayout.Position, level.ComboTextLayout.Size, 32,
                    "", 16.0f, { 0.98f, 0.92f, 0.68f, 1.0f }, false);

                auto configureSkillSlot = [&](const std::string& key, bool visible = true)
                {
                    const auto* slot = FindSkillHudSlot(level, key);
                    if (!slot)
                        return;

                    const SheetUVRect iconUv = SheetRect(slot->IconSheetPixels);
                    if (slot->UseSheetIcon)
                    {
                        ConfigureSkillButton(scene,
                            slot->Key,
                            slot->Position,
                            slot->Size,
                            iconUv,
                            slot->KeyLabel,
                            glm::vec4(1.0f),
                            visible && slot->Enabled);
                    }
                    else
                    {
                        ConfigureImageSkillButton(scene,
                            slot->Key,
                            slot->Position,
                            slot->Size,
                            ResolveHudTexturePath(slot->IconTexturePath, BreakLimitIconPath()),
                            slot->KeyLabel,
                            visible && slot->Enabled);
                    }

                    const std::string command = slot->Command.empty()
                        ? "side:skill:" + slot->Key
                        : slot->Command;
                    GameplayUILayoutService::SetButtonCommand(scene, s_SkillPrefix + "Slot_" + slot->Key, command);
                    GameplayUILayoutService::SetButtonCommand(scene, s_SkillPrefix + "Icon_" + slot->Key, command);
                };

                configureSkillSlot("J");
                configureSkillSlot("SJ");
                configureSkillSlot("S2");
                configureSkillSlot("S3");
                configureSkillSlot("U");
                configureSkillSlot("I");
                configureSkillSlot("L", breakLimitUiVisible);

                ConfigureCombatItemSlots(scene, level);
                level.RuntimeHudLayoutConfigured = true;
            }

            EnsureSheetFill(scene, level.PlayerManaEntityName, level.PlayerManaLayout.Position, level.PlayerManaLayout.Size, 24,
                BlueBarUV(), manaRatio);
            EnsureSheetFill(scene, level.PlayerUltimateMaskEntityName, level.PlayerUltimateLayout.Position, level.PlayerUltimateLayout.Size, 25,
                SwordMaskUV(), ultimateRatio, { 1.0f, 1.0f, 1.0f, 0.92f });
            EnsureSheetFill(scene, level.BossProtectionEntityName, level.BossProtectionLayout.Position, level.BossProtectionLayout.Size, 25,
                GoldBarUV(), bossProtectionRatio, glm::vec4(1.0f), bossVisible && bossProtectionRatio > 0.002f);
            SetWidgetVisible(scene, level.ComboPanelEntityName, bossVisible);

            UpdateJoystickVisual(scene, level);
            UpdateStatusIcons(scene, level.PlayerStatusPrefix, level.PlayerStatusLayout.BuffStart, level.PlayerStatusLayout.DebuffStart,
                level.PlayerStatusLayout,
                playerCombatant, controller, true, playerCombatant != nullptr);
            UpdateStatusIcons(scene, level.EnemyStatusPrefix, level.EnemyStatusLayout.BuffStart, level.EnemyStatusLayout.DebuffStart,
                level.EnemyStatusLayout,
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
            {
                // Wave-spawned enemies have no authored HUD entities; create
                // a plain colored sprite on demand. It is named after the
                // enemy (reused each frame, re-created after scene reloads;
                // stale ones simply stay hidden once the enemy is gone).
                entity = scene->CreateEntity(name);
                auto& created = entity.AddComponent<SpriteRendererComponent>();
                created.Texture = nullptr;
                created.Color = color;
                return entity;
            }

            if (!entity.HasComponent<SpriteRendererComponent>())
            {
                WarnMissingAuthoredHud(scene, name, "SpriteRendererComponent");
                return {};
            }

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

        static Entity EnsureWorldStatusIcon(Scene* scene,
            const std::string& name,
            const SheetUVRect& uv,
            bool visible)
        {
            if (!scene)
                return {};

            Entity entity = FindEntityByName(scene, name);
            if (!entity)
            {
                entity = scene->CreateEntity(name);
                entity.AddComponent<SpriteRendererComponent>();
            }
            if (!entity.HasComponent<SpriteRendererComponent>())
                return {};

            auto& sprite = entity.GetComponent<SpriteRendererComponent>();
            sprite.Texture = GameplayVisualService::LoadTextureCached(SideUISheetPath());
            sprite.UVMin = uv.Min;
            sprite.UVMax = uv.Max;
            sprite.Color.a = visible ? 1.0f : 0.0f;
            return entity;
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

                // Status icons follow the health bar: a row above it, sized
                // to the bar (each icon ~ bar height x 1.15), only taking
                // space while active.
                const bool shieldIcon = combatant.RuntimeInvulnerableTimer > 0.0f
                    || combatant.Invulnerable
                    || combatant.RuntimeState == SideCombatState::SuperArmor;
                const bool stateIcon = IsNegativeCombatState(combatant.RuntimeState);
                const bool breakIcon = combatant.RuntimeState == SideCombatState::Broken;
                const float iconSize = 0.105f;
                const float iconGap = 0.11f;
                const float iconY = baseY + 0.15f;
                float iconX = baseX - fullWidth * 0.5f;
                auto placeIcon = [&](const std::string& suffix,
                    const SheetUVRect& uv,
                    bool on)
                {
                    Entity icon = EnsureWorldStatusIcon(scene, tag + suffix, uv, on);
                    if (!icon || !icon.HasComponent<TransformComponent>())
                        return;
                    auto& transform = icon.GetComponent<TransformComponent>();
                    transform.Translation = { iconX + iconSize * 0.5f, iconY, z + 0.15f };
                    transform.Scale = { iconSize, iconSize * 1.1f, 1.0f };
                    if (on)
                        iconX += iconGap;
                };
                placeIcon("_Buff_0", BuffAttackUV(), false);
                placeIcon("_Buff_1", BuffShieldUV(), shieldIcon);
                placeIcon("_Debuff_0", DebuffStateUV(), stateIcon);
                placeIcon("_Debuff_1", DebuffBreakUV(), breakIcon);
            }
        }

    } // namespace

    void UpdateUI(Scene* scene,
        SideCombatLevelComponent& level,
        Entity player,
        Entity boss)
    {
        if (!scene)
            return;

        s_SkillPrefix = level.SkillPrefix.empty() ? "SC_Skill" : level.SkillPrefix;

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
            const float playerHealthRatio = std::clamp(
                playerCombatant->Health / std::max(1.0f, playerCombatant->MaxHealth),
                0.0f,
                1.0f);
            SetProgress(scene, level.PlayerHealthBarEntityName, playerCombatant->Health, playerCombatant->MaxHealth);
            EnsureSheetFill(scene, level.PlayerHealthBarEntityName, level.PlayerHealthLayout.Position, level.PlayerHealthLayout.Size, 24,
                RedBarUV(), playerHealthRatio);
            SetText(scene, level.PlayerHealthTextEntityName,
                level.HudPlayerHealthLabel + FormatFloat(playerCombatant->Health) + "/" + FormatFloat(playerCombatant->MaxHealth));
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
                const float bossHealthRatio = std::clamp(
                    bossCombatant->Health / std::max(1.0f, bossCombatant->MaxHealth),
                    0.0f,
                    1.0f);
                SetProgress(scene, level.BossHealthBarEntityName, bossCombatant->Health, bossCombatant->MaxHealth);
                EnsureSheetFill(scene, level.BossHealthBarEntityName, level.BossHealthLayout.Position, level.BossHealthLayout.Size, 24,
                    CyanBarUV(), bossHealthRatio, glm::vec4(1.0f), bossVisible);
                std::string bossText = level.HudBossHealthLabel + FormatFloat(bossCombatant->Health) + "/" + FormatFloat(bossCombatant->MaxHealth);
                if (SideCombatTuningService::ShouldShowCombatStateHud(level, tuning))
                    bossText += "  " + std::string(GetCombatStateLabel(bossCombatant->RuntimeState));
                if (SideCombatTuningService::ShouldShowBossProtectionHud(level, tuning) && bossCombatant->RuntimeProtectionMax > 0.0f)
                {
                    bossText += level.HudBossProtectionLabel + FormatFloat(bossCombatant->RuntimeProtection)
                        + "/" + FormatFloat(bossCombatant->RuntimeProtectionMax);
                }
                SetText(scene, level.BossHealthTextEntityName, bossText);
            }
        }

        UpdateEnemyHealthBars(scene, level, tuning);

        const bool showBreakLimitUi = SideCombatTuningService::ShouldShowBreakLimitUi(level, tuning);
        const bool breakLimitOfficial = SideCombatTuningService::IsBreakLimitOfficiallyAvailable(level, tuning);
        std::string message = level.HudDefaultMessage;
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "basic_attack") ||
            SideCombatTuningService::IsSkillUnlocked(level, tuning, "air_basic"))
        {
            message += level.HudAirBasicMessage;
        }
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "magic_bolt"))
            message += level.HudMagicMessage;
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "dash"))
            message += level.HudDashMessage;
        message += level.HudReservedSkillMessage;
        if (SideCombatTuningService::IsSkillUnlocked(level, tuning, "ally_support"))
            message += level.HudSupportMessage;
        if (showBreakLimitUi)
            message += breakLimitOfficial ? level.HudBreakLimitInputMessage : level.HudBreakLimitDebugInputMessage;
        if (level.RuntimeVictory)
            message = level.HudVictoryMessage;
        else if (level.RuntimeDefeat)
            message = level.HudDefeatMessage;
        else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
            playerCombatant->RuntimeAirHeight >= tuning.AirCombo.HighAirSafetyHeight)
        {
            message = level.HudHighAirMessage;
        }
        else if (playerCombatant && !playerCombatant->RuntimeOnGround &&
            playerCombatant->RuntimeAirHeight <= tuning.AirCombo.GroundThreatHeight)
        {
            message = level.HudLowAirMessage;
        }
        else if (level.RuntimeComboCount >= 6 && showBreakLimitUi)
        {
            message = level.HudBreakLimitHintMessage;
        }

        SetText(scene, level.MessageTextEntityName, message);

        SetWidgetVisible(scene, level.ComboTextEntityName, false);
        UpdateComboCounter(scene, level, level.RuntimeComboCount);
        UpdateCombatItemSlots(scene, level, controller);

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
            std::string breakLimitUnavailableText = level.HudConditionText;
            if (!breakLimitUnlocked)
                breakLimitUnavailableText = level.HudLockedText;
            else if (!breakLimitHasGauge)
                breakLimitUnavailableText = level.HudGaugeText;
            else if (!breakLimitComboReady)
                breakLimitUnavailableText = level.HudComboText;
            else if (!breakLimitBossReady)
                breakLimitUnavailableText = level.HudArmorText;

            UpdateSkillSlot(scene, "J", GetSkillKeyLabel(level, "J", "J"), true, controller->RuntimeBasicCooldown,
                controller->BasicCooldown);
            SetSkillSlotVisible(scene, "K", false);
            UpdateSkillSlot(scene, "SJ", GetSkillKeyLabel(level, "SJ", "S+J"), launcherUnlocked && launcherHasMana, controller->RuntimeLauncherCooldown,
                std::max(controller->LauncherCooldown, tuning.AirCombo.AirChaseCooldown),
                launcherUnlocked ? level.HudInsufficientManaText : level.HudLockedText);
            UpdateSkillSlot(scene, "S2", GetSkillKeyLabel(level, "S2", "U"), magicUnlocked && magicHasMana, controller->RuntimeMagicBoltCooldown,
                controller->MagicBoltCooldown,
                magicUnlocked ? level.HudInsufficientManaText : level.HudLockedText);
            UpdateSkillSlot(scene, "S3", GetSkillKeyLabel(level, "S3", "I"), dashUnlocked && dashHasMana, controller->RuntimeDashCooldown,
                controller->DashCooldown,
                dashUnlocked ? level.HudInsufficientManaText : level.HudLockedText);
            UpdateSkillSlot(scene, "U", GetSkillKeyLabel(level, "U", "O"), false, 0.0f, 1.0f, level.HudUnavailableText);
            UpdateSkillSlot(scene, "I", GetSkillKeyLabel(level, "I", "H"), supportUnlocked && supportHasMana, controller->RuntimeAllySupportCooldown,
                controller->AllySupportCooldown,
                supportUnlocked ? level.HudInsufficientManaText : level.HudLockedText);
            if (showBreakLimitUi)
            {
                if (controller->RuntimeBreakLimitCooldown > 0.05f)
                {
                    UpdateSkillSlot(scene, "L", GetSkillKeyLabel(level, "L", "L"), true, controller->RuntimeBreakLimitCooldown,
                        tuning.AirCombo.BreakLimitCooldown);
                }
                else
                {
                    UpdateSkillSlot(scene, "L", GetSkillKeyLabel(level, "L", "L"), breakLimitReady, 0.0f,
                        tuning.AirCombo.BreakLimitCooldown,
                        breakLimitUnavailableText);
                }
            }
            else
            {
                SetSkillSlotVisible(scene, "L", false);
            }
            SetText(scene, level.SkillTextEntityName,
                level.HudManaGaugeLabel + FormatFloat(controller->RuntimeMagicSwordGauge, 1)
                + "/" + FormatFloat(controller->RuntimeMagicSwordGaugeMax, 0)
                + level.HudAirActionsLabel + std::to_string(controller->RuntimeAirActionsRemaining));
            std::string hoveredKey;
            std::string tooltip;
            auto appendTooltipLine = [](std::string& value, const std::string& line)
            {
                if (line.empty())
                    return;
                if (!value.empty())
                    value += "\n";
                value += line;
            };
            auto appendCooldownLine = [&](std::string& value, float cooldown)
            {
                if (cooldown > 0.05f)
                    appendTooltipLine(value, level.HudCooldownPrefix + FormatCooldownSeconds(cooldown) + level.HudSecondsSuffix);
            };
            auto buildSkillTooltip = [&](const std::string& key, const std::string& manaCost = {})
            {
                const auto* slot = FindSkillHudSlot(level, key);
                if (!slot)
                    return std::string{};
                return FormatHudTemplate(slot->TooltipText, {}, manaCost);
            };

            if (IsButtonHovered(scene, s_SkillPrefix + "Icon_J"))
            {
                hoveredKey = "J";
                tooltip = buildSkillTooltip("J");
                appendCooldownLine(tooltip, controller->RuntimeBasicCooldown);
            }
            else if (IsButtonHovered(scene, s_SkillPrefix + "Icon_SJ"))
            {
                hoveredKey = "SJ";
                tooltip = buildSkillTooltip("SJ", FormatFloat(controller->LauncherManaCost));
                if (!launcherHasMana)
                    appendTooltipLine(tooltip, level.HudManaNotEnoughTooltip);
                appendCooldownLine(tooltip, controller->RuntimeLauncherCooldown);
            }
            else if (IsButtonHovered(scene, s_SkillPrefix + "Icon_S2"))
            {
                hoveredKey = "S2";
                tooltip = buildSkillTooltip("S2", FormatFloat(controller->MagicBoltManaCost));
                if (!magicHasMana)
                    appendTooltipLine(tooltip, level.HudManaNotEnoughTooltip);
                appendCooldownLine(tooltip, controller->RuntimeMagicBoltCooldown);
            }
            else if (IsButtonHovered(scene, s_SkillPrefix + "Icon_S3"))
            {
                hoveredKey = "S3";
                tooltip = buildSkillTooltip("S3", FormatFloat(controller->DashManaCost));
                if (!dashHasMana)
                    appendTooltipLine(tooltip, level.HudManaNotEnoughTooltip);
                appendCooldownLine(tooltip, controller->RuntimeDashCooldown);
            }
            else if (IsButtonHovered(scene, s_SkillPrefix + "Icon_U"))
            {
                hoveredKey = "U";
                tooltip = buildSkillTooltip("U");
            }
            else if (IsButtonHovered(scene, s_SkillPrefix + "Icon_I"))
            {
                hoveredKey = "I";
                tooltip = buildSkillTooltip("I", FormatFloat(controller->AllySupportManaCost));
                if (!supportHasMana)
                    appendTooltipLine(tooltip, level.HudManaNotEnoughTooltip);
                appendCooldownLine(tooltip, controller->RuntimeAllySupportCooldown);
            }
            else if (IsButtonHovered(scene, s_SkillPrefix + "Icon_L"))
            {
                hoveredKey = "L";
                tooltip = buildSkillTooltip("L");
                if (controller->RuntimeBreakLimitCooldown > 0.05f)
                    appendCooldownLine(tooltip, controller->RuntimeBreakLimitCooldown);
                else if (!breakLimitUnlocked)
                    appendTooltipLine(tooltip, level.HudNotUnlockedTooltip);
                else if (!breakLimitHasGauge)
                    appendTooltipLine(tooltip, FormatHudTemplate(level.BreakLimitGaugeNotEnoughTooltip, FormatFloat(breakLimitGaugeCost, 1)));
                else if (!breakLimitComboReady)
                    appendTooltipLine(tooltip, FormatHudTemplate(level.BreakLimitComboNotEnoughTooltip, std::to_string(tuning.AirCombo.BreakLimitMinCombo)));
                else if (!breakLimitBossReady)
                    appendTooltipLine(tooltip, level.BreakLimitBossNotReadyTooltip);
            }
            ApplyCombatItemTooltip(scene, level, hoveredKey, tooltip, controller);
            UpdateSkillTooltip(scene, level, hoveredKey, tooltip);
        }
        else
        {
            std::string hoveredKey;
            std::string tooltip;
            ApplyCombatItemTooltip(scene, level, hoveredKey, tooltip, controller);
            UpdateSkillTooltip(scene, level, hoveredKey, tooltip);
        }
        std::string reward = level.RuntimeVictory
            ? (level.RuntimeResultSummary.empty()
                ? level.FirstClearRewardText
                : level.RuntimeResultSummary)
            : level.HudRewardFallbackText;
        if (level.RuntimeCollectedPickups > 0)
            reward += level.HudCollectedPrefix + std::to_string(level.RuntimeCollectedPickups);
        SetText(scene, level.RewardTextEntityName, reward);

        if (IsResultFadeActive(level))
            HideResultFadeTextHud(scene, level);
    }

    glm::vec2 GetJoystickInputDirection()
    {
        return g_JoystickDragging ? g_JoystickDirection : glm::vec2(0.0f);
    }

} // namespace Wheatear::SideCombatHudService
