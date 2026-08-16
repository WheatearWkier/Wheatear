#pragma once

#include "Editor/EditorWidgets.h"
#include "Wheatear/Modules/Progression/ProgressionContent.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/EntityReference.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Wheatear::EditorContentPickers {

    enum class ProgressionIdKind
    {
        Material = 0,
        Equipment,
        EquipmentSlot,
        Dungeon,
        Relationship,
        Skill
    };

    inline void SortUnique(std::vector<std::string>& values)
    {
        values.erase(std::remove_if(values.begin(), values.end(), [](const std::string& value)
        {
            return value.empty();
        }), values.end());
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    }

    inline std::vector<std::string> SceneEntityNames(Entity context)
    {
        std::vector<std::string> names;
        Scene* scene = context ? context.GetScene() : nullptr;
        if (!scene)
            return names;

        auto view = scene->GetRegistry().view<TagComponent>();
        for (auto entityID : view)
        {
            const auto& tag = view.get<TagComponent>(entityID).Tag;
            if (!tag.empty())
                names.push_back(tag);
        }
        SortUnique(names);
        return names;
    }

    inline std::vector<std::string> ProgressionIds(ProgressionIdKind kind)
    {
        const auto& content = ProgressionContent::Get();
        std::vector<std::string> ids;
        switch (kind)
        {
        case ProgressionIdKind::Material:
            for (const auto& material : content.Materials)
                ids.push_back(material.ItemId);
            break;
        case ProgressionIdKind::Equipment:
            for (const auto& item : content.Equipment)
                ids.push_back(item.Id);
            break;
        case ProgressionIdKind::EquipmentSlot:
            for (const auto& slot : content.EquipmentSlots)
                ids.push_back(slot.Id);
            break;
        case ProgressionIdKind::Dungeon:
            for (const auto& dungeon : content.Dungeons)
                ids.push_back(dungeon.Id);
            break;
        case ProgressionIdKind::Relationship:
            for (const auto& relationship : content.Relationships)
                ids.push_back(relationship.CharacterId);
            break;
        case ProgressionIdKind::Skill:
            for (const auto& skill : content.SkillNodes)
                ids.push_back(skill.Id);
            break;
        }
        SortUnique(ids);
        return ids;
    }

    inline std::vector<std::string> StoryFlags()
    {
        const auto& content = ProgressionContent::Get();
        std::vector<std::string> flags = content.InitialStoryFlags;
        for (const auto& dungeon : content.Dungeons)
            flags.insert(flags.end(), dungeon.FlagsOnClear.begin(), dungeon.FlagsOnClear.end());
        SortUnique(flags);
        return flags;
    }

    inline bool DrawStringPicker(const char* label,
        std::string& value,
        const std::vector<std::string>& choices,
        size_t capacity = 256,
        const char* previewWhenEmpty = "None")
    {
        bool changed = false;
        ImGui::PushID(label);
        changed |= EditorWidgets::InputString(EditorWidgets::LabelWithId(label, "Value").c_str(), value, capacity);

        const bool hasChoices = !choices.empty();
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasChoices);
        const char* preview = value.empty() ? previewWhenEmpty : value.c_str();
        if (ImGui::BeginCombo("Pick", preview))
        {
            for (const std::string& choice : choices)
            {
                const bool selected = choice == value;
                if (ImGui::Selectable(choice.c_str(), selected))
                {
                    value = choice;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        if (!hasChoices)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("No choices");
        }
        else if (!value.empty() && std::find(choices.begin(), choices.end(), value) == choices.end())
        {
            ImGui::SameLine();
            EditorWidgets::StatusBadge("Missing", EditorWidgets::StatusKind::Warning);
        }

        ImGui::PopID();
        return changed;
    }

    // Entity binding field. Choosing an entity from the dropdown writes an
    // "@UUID" selector (so renames never break the binding), while the field
    // keeps showing the entity's current display name and flags stale
    // bindings with a "Missing" badge. Plain names stay supported for hand
    // authoring and legacy scenes (runtime resolves both forms).
    inline bool DrawSceneEntityField(const char* label,
        Entity context,
        std::string& value,
        size_t capacity = 256)
    {
        Scene* scene = context ? context.GetScene() : nullptr;
        bool changed = false;
        ImGui::PushID(label);

        changed |= EditorWidgets::InputString(
            EditorWidgets::LabelWithId(label, "Value").c_str(), value, capacity);

        // Resolve the current binding to a human-readable display name
        // ("@12345" -> entity tag); a binding that matches nothing is stale.
        std::string display = value;
        bool missing = false;
        if (!value.empty() && scene)
        {
            if (Entity bound = scene->GetEntityByName(value))
                display = bound.GetComponent<TagComponent>().Tag;
            else
                missing = true;
        }

        ImGui::SameLine();
        if (scene && ImGui::BeginCombo("Pick", display.empty() ? "None" : display.c_str()))
        {
            std::vector<std::pair<std::string, std::string>> entries;
            auto view = scene->GetRegistry().view<TagComponent, IDComponent>();
            for (auto entityID : view)
            {
                const auto& tag = view.get<TagComponent>(entityID).Tag;
                if (tag.empty())
                    continue;
                entries.emplace_back(tag,
                    EntityReferences::MakeUUIDSelector(view.get<IDComponent>(entityID).ID));
            }
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            std::string last;
            for (const auto& [name, selector] : entries)
            {
                if (name == last)
                    continue;
                last = name;
                const bool selected = (display == name);
                if (ImGui::Selectable(name.c_str(), selected))
                {
                    value = selector;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        else if (!scene)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("No scene");
        }

        if (missing)
        {
            ImGui::SameLine();
            EditorWidgets::StatusBadge("Missing", EditorWidgets::StatusKind::Warning);
        }

        ImGui::PopID();
        return changed;
    }

    inline bool DrawProgressionIdField(const char* label,
        std::string& value,
        ProgressionIdKind kind,
        size_t capacity = 256)
    {
        return DrawStringPicker(label, value, ProgressionIds(kind), capacity);
    }

    inline bool DrawStoryFlagField(const char* label,
        std::string& value,
        size_t capacity = 256)
    {
        return DrawStringPicker(label, value, StoryFlags(), capacity);
    }

    inline bool DrawAssetField(const char* label,
        std::string& value,
        EditorWidgets::AssetReferenceKind kind,
        size_t capacity = 512)
    {
        return EditorWidgets::DrawAssetReferenceField(label, value, kind, capacity);
    }

} // namespace Wheatear::EditorContentPickers
