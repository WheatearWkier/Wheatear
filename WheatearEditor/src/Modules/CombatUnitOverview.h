#pragma once

// Shared helpers for the combat tuning panels' "scene unit overview" tab:
// a read-only summary of the module's unit components across every scene
// file in the project, complementing the typed tuning tabs.

#include "Editor/EditorLocale.h"
#include "Wheatear/Assets/AssetPath.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Wheatear::CombatUnitOverview {

    struct SceneFileUnitRow
    {
        std::string ScenePath;
        std::string EntityTag;
        std::string DisplayName;
        std::vector<std::pair<std::string, std::string>> Fields;  // label -> value
    };

    // Scans every assets/scenes/*.wt in the project and collects the
    // entities carrying `componentKey`, extracting the given field keys
    // (yaml key -> display label). Purely file-level: works for any module
    // component without touching C++ component types.
    inline std::vector<SceneFileUnitRow> ScanSceneFilesForComponent(
        const char* componentKey,
        const std::vector<std::pair<const char*, const char*>>& fieldKeys)
    {
        std::vector<SceneFileUnitRow> rows;

        const std::filesystem::path scenesDir =
            AssetPath::GetProjectRoot() / "assets" / "scenes";
        if (!std::filesystem::is_directory(scenesDir))
            return rows;

        for (const auto& entry : std::filesystem::directory_iterator(scenesDir))
        {
            if (!entry.is_regular_file())
                continue;
            const std::filesystem::path path = entry.path();
            if (path.extension() != ".wt")
                continue;

            YAML::Node root;
            try
            {
                root = YAML::LoadFile(path.string());
            }
            catch (...)
            {
                continue;
            }
            if (!root.IsMap())
                continue;

            const YAML::Node entities = root["Entities"];
            if (!entities.IsSequence())
                continue;

            const std::string relativePath =
                AssetPath::ToProjectRelative(path).generic_string();

            for (const YAML::Node& entity : entities)
            {
                if (!entity.IsMap())
                    continue;
                const YAML::Node component = entity[componentKey];
                if (!component.IsMap())
                    continue;

                SceneFileUnitRow row;
                row.ScenePath = relativePath;
                row.EntityTag = entity["TagComponent"]
                    ? entity["TagComponent"]["Tag"].as<std::string>("")
                    : "";
                if (row.EntityTag.empty())
                    row.EntityTag = entity["Entity"].as<std::string>("");
                row.DisplayName = component["DisplayName"].as<std::string>(row.EntityTag);

                for (const auto& [key, label] : fieldKeys)
                {
                    const YAML::Node value = component[key];
                    if (!value)
                    {
                        // Keep the row aligned with the field list even when
                        // this unit lacks the field.
                        row.Fields.emplace_back(label, "");
                        continue;
                    }
                    row.Fields.emplace_back(label,
                        value.IsSequence()
                            ? YAML::Dump(value)
                            : value.as<std::string>(""));
                }
                rows.push_back(std::move(row));
            }
        }

        return rows;
    }

    // Draws the read-only project-wide unit table for the given component.
    inline void DrawProjectUnitsTable(const char* componentKey,
        const std::vector<std::pair<const char*, const char*>>& fieldKeys,
        bool showEntityTag)
    {
        const std::vector<SceneFileUnitRow> rows =
            ScanSceneFilesForComponent(componentKey, fieldKeys);

        if (rows.empty())
        {
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "No units found in project scenes.",
                "项目场景中未找到单位。"));
            return;
        }

        ImGui::TextDisabled("%s", EditorLocale::Text(
            "Read-only overview across all scene files; edit the open scene above.",
            "全部场景文件的只读总览；编辑请使用上方当前场景列表。"));

        if (ImGui::BeginTable("##UnitOverview", 3 + static_cast<int>(fieldKeys.size()),
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(EditorLocale::Text("Scene", "场景"));
            if (showEntityTag)
                ImGui::TableSetupColumn(EditorLocale::Text("Entity", "实体"));
            ImGui::TableSetupColumn(EditorLocale::Text("Name", "名称"));
            for (const auto& [key, label] : fieldKeys)
                ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();

            for (const SceneFileUnitRow& row : rows)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(row.ScenePath.c_str());
                int column = 1;
                if (showEntityTag)
                {
                    ImGui::TableSetColumnIndex(column++);
                    ImGui::TextUnformatted(row.EntityTag.c_str());
                }
                ImGui::TableSetColumnIndex(column++);
                ImGui::TextUnformatted(row.DisplayName.c_str());
                for (size_t i = 0; i < fieldKeys.size(); ++i)
                {
                    ImGui::TableSetColumnIndex(column + static_cast<int>(i));
                    if (i < row.Fields.size() && !row.Fields[i].second.empty())
                        ImGui::TextUnformatted(row.Fields[i].second.c_str());
                }
            }
            ImGui::EndTable();
        }
    }

} // namespace Wheatear::CombatUnitOverview
