#include "wtpch.h"
#include "SideCombatHudPreset.h"

#include "Wheatear/Assets/AssetAliasRegistry.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace YAML {

    template<> struct convert<glm::vec2> {
        static Node encode(const glm::vec2& value) {
            Node node; node.push_back(value.x); node.push_back(value.y); return node;
        }
        static bool decode(const Node& node, glm::vec2& value) {
            if (!node.IsSequence() || node.size() != 2) return false;
            value = { node[0].as<float>(), node[1].as<float>() }; return true;
        }
    };

    template<> struct convert<glm::vec4> {
        static Node encode(const glm::vec4& value) {
            Node node; node.push_back(value.x); node.push_back(value.y); node.push_back(value.z); node.push_back(value.w); return node;
        }
        static bool decode(const Node& node, glm::vec4& value) {
            if (!node.IsSequence() || node.size() != 4) return false;
            value = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() }; return true;
        }
    };

} // namespace YAML

namespace Wheatear {


    namespace {
        static bool CaptureWidgetRect(Scene* scene,
            const std::string& name,
            glm::vec2& position,
            glm::vec2& size)
        {
            Entity entity = SceneQueries::FindEntityByName(scene, name);
            if (!entity || !entity.HasComponent<UIWidgetComponent>())
                return false;

            const auto& widget = entity.GetComponent<UIWidgetComponent>();
            position = widget.Position;
            size = widget.Size;
            return true;
        }

        static bool CaptureWidgetRect(Scene* scene,
            const std::string& name,
            SideCombatLevelComponent::HudRect& rect)
        {
            return CaptureWidgetRect(scene, name, rect.Position, rect.Size);
        }

        static int CaptureStatusBadgeLayout(Scene* scene,
            const std::string& prefix,
            SideCombatLevelComponent::StatusBadgeLayout& layout)
        {
            if (prefix.empty())
                return 0;

            int captured = 0;
            glm::vec2 position = layout.BuffStart;
            glm::vec2 size = layout.Size;
            if (CaptureWidgetRect(scene, prefix + "_Buff_0", position, size))
            {
                layout.BuffStart = position;
                layout.Size = size;
                captured++;
            }

            position = layout.DebuffStart;
            size = layout.Size;
            if (CaptureWidgetRect(scene, prefix + "_Debuff_0", position, size))
            {
                layout.DebuffStart = position;
                layout.Size = size;
                captured++;
            }

            glm::vec2 secondPosition = {};
            glm::vec2 secondSize = {};
            if (CaptureWidgetRect(scene, prefix + "_Buff_1", secondPosition, secondSize))
            {
                layout.Gap = secondPosition.x - layout.BuffStart.x;
                captured++;
            }

            return captured;
        }
    } // namespace

    namespace SideCombatHudPreset {

    int CaptureSceneLayout(SideCombatLevelComponent& level,
        Scene* scene)
    {
        if (!scene)
            return 0;

        int captured = 0;
        captured += CaptureWidgetRect(scene, level.TopPanelEntityName, level.TopPanelLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerHealthBarEntityName, level.PlayerHealthLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerManaEntityName, level.PlayerManaLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerUltimateMaskEntityName, level.PlayerUltimateLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.PlayerHealthTextEntityName, level.PlayerHealthTextLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.ComboPanelEntityName, level.BossPanelLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.BossHealthBarEntityName, level.BossHealthLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.BossProtectionEntityName, level.BossProtectionLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.BossHealthTextEntityName, level.BossHealthTextLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.ComboTextEntityName, level.ComboTextLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.ComboFrameEntityName, level.ComboFrameLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.SkillTooltipPanelEntityName, level.SkillTooltipLayout) ? 1 : 0;
        captured += CaptureWidgetRect(scene, level.JoystickBaseEntityName, level.JoystickBaseLayout) ? 1 : 0;

        glm::vec2 thumbPosition = {};
        glm::vec2 thumbSize = {};
        if (CaptureWidgetRect(scene, level.JoystickThumbEntityName, thumbPosition, thumbSize))
        {
            level.JoystickThumbSize = thumbSize;
            captured++;
        }

        const std::string skillPrefix = level.SkillPrefix.empty() ? "SC_Skill" : level.SkillPrefix;
        for (auto& slot : level.SkillHudSlots)
        {
            if (slot.Key.empty())
                continue;

            if (CaptureWidgetRect(scene, skillPrefix + "Icon_" + slot.Key, slot.Position, slot.Size)
                || CaptureWidgetRect(scene, skillPrefix + "Slot_" + slot.Key, slot.Position, slot.Size))
            {
                captured++;
            }
        }

        const std::string itemSlotPrefix = level.ItemSlotPrefix.empty() ? "SC_ItemSlot_" : level.ItemSlotPrefix;
        for (auto& slot : level.CombatItemHudSlots)
        {
            if (slot.Key.empty())
                continue;

            const std::string prefix = itemSlotPrefix + slot.Key;
            glm::vec2 framePosition = slot.Position;
            glm::vec2 frameSize = slot.FrameSize;
            if (CaptureWidgetRect(scene, prefix + "_Frame", framePosition, frameSize)
                || CaptureWidgetRect(scene, prefix + "_Button", framePosition, frameSize))
            {
                slot.Position = framePosition;
                slot.FrameSize = frameSize;
                captured++;
            }

            glm::vec2 iconPosition = slot.Position + slot.IconInset;
            glm::vec2 iconSize = slot.IconSize;
            if (CaptureWidgetRect(scene, prefix + "_Icon", iconPosition, iconSize))
            {
                slot.IconInset = iconPosition - slot.Position;
                slot.IconSize = iconSize;
                captured++;
            }
        }

        captured += CaptureStatusBadgeLayout(scene, level.PlayerStatusPrefix, level.PlayerStatusLayout);
        captured += CaptureStatusBadgeLayout(scene, level.EnemyStatusPrefix, level.EnemyStatusLayout);

        return captured;
    }

    } // namespace Wheatear::SideCombatHudPreset
} // namespace Wheatear
