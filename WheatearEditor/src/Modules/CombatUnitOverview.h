#pragma once

// Shared helpers for the combat tuning panels' "scene unit overview" tab:
// a read-only summary of the module's unit components across every scene
// file in the project, complementing the typed tuning tabs.

#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Assets/AssetPath.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
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
        std::vector<std::string> EditBuffers;                     // live edit text
    };

    // Best-effort scalar parse: bool -> int -> float -> string.
    inline YAML::Node ParseScalarValue(const std::string& text)
    {
        if (text == "true" || text == "false")
            return YAML::Node(text == "true");
        try
        {
            size_t pos = 0;
            const int value = std::stoi(text, &pos);
            if (pos == text.size())
                return YAML::Node(value);
        }
        catch (...) {}
        try
        {
            size_t pos = 0;
            const float value = std::stof(text, &pos);
            if (pos == text.size())
                return YAML::Node(value);
        }
        catch (...) {}
        return YAML::Node(text);
    }

    // Rewrites one scalar field of the entity with `entityTag` inside the
    // scene file. Scene files are editor-serialized (no hand-written
    // comments), so a full YAML rewrite is lossless here.
    inline bool WriteSceneField(const std::string& scenePath,
        const std::string& entityTag,
        const std::string& componentKey,
        const std::string& fieldKey,
        const std::string& newValue,
        std::string* errorMessage)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(scenePath);
        YAML::Node root;
        try
        {
            root = YAML::LoadFile(resolvedPath.string());
        }
        catch (const std::exception& e)
        {
            if (errorMessage)
                *errorMessage = std::string("无法解析场景: ") + e.what();
            return false;
        }
        if (!root.IsMap())
        {
            if (errorMessage)
                *errorMessage = "场景根节点不是映射。";
            return false;
        }

        YAML::Node entities = root["Entities"];
        if (!entities.IsSequence())
        {
            if (errorMessage)
                *errorMessage = "场景没有 Entities 序列。";
            return false;
        }

        // Node copies share the underlying yaml data, so mutation propagates
        // back into `root` when saving.
        for (YAML::Node entity : entities)
        {
            if (!entity.IsMap())
                continue;
            const YAML::Node tag = entity["TagComponent"];
            const std::string tagName = tag && tag["Tag"]
                ? tag["Tag"].as<std::string>("") : "";
            if (tagName != entityTag)
                continue;

            const YAML::Node probe = entity[componentKey];
            if (!probe || !probe.IsMap())
            {
                if (errorMessage)
                    *errorMessage = "实体缺少组件: " + componentKey;
                return false;
            }
            YAML::Node component = entity[componentKey];
            component[fieldKey] = ParseScalarValue(newValue);

            YAML::Emitter out;
            out.SetIndent(2);
            out << root;
            if (!EditorWidgets::WriteFileText(resolvedPath, out.c_str()))
            {
                if (errorMessage)
                    *errorMessage = "写入场景文件失败: " + resolvedPath.string();
                return false;
            }
            return true;
        }

        if (errorMessage)
            *errorMessage = "场景中未找到实体: " + entityTag;
        return false;
    }

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
                if (!component || !component.IsMap())
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

    // Draws the project-wide unit table for the given component. Numeric /
    // string cells are editable and write back to the scene file on commit,
    // so designers can batch-tune units without opening each scene.
    inline void DrawProjectUnitsTable(const char* componentKey,
        const std::vector<std::pair<const char*, const char*>>& fieldKeys,
        bool showEntityTag)
    {
        std::vector<SceneFileUnitRow> rows =
            ScanSceneFilesForComponent(componentKey, fieldKeys);
        for (SceneFileUnitRow& row : rows)
        {
            row.EditBuffers.resize(fieldKeys.size());
            for (size_t i = 0; i < fieldKeys.size(); ++i)
            {
                if (row.EditBuffers[i].empty() && i < row.Fields.size())
                    row.EditBuffers[i] = row.Fields[i].second;
            }
        }

        static std::string s_EditStatus;
        if (rows.empty())
        {
            ImGui::TextDisabled("%s", EditorLocale::Text(
                "No units found in project scenes.",
                "项目场景中未找到单位。"));
            return;
        }

        ImGui::TextDisabled("%s", EditorLocale::Text(
            "Project-wide unit table; edit a cell and press Enter to write it "
            "back to the scene file.",
            "全项目单位表；编辑单元格后回车写回场景文件。"));
        if (!s_EditStatus.empty())
        {
            ImGui::SameLine();
            EditorWidgets::InlineStatus(s_EditStatus,
                s_EditStatus.find("失败") != std::string::npos
                    || s_EditStatus.find("Failed") != std::string::npos
                    || s_EditStatus.find("找不到") != std::string::npos
                    ? EditorWidgets::StatusKind::Error : EditorWidgets::StatusKind::Info);
        }

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

            for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
            {
                SceneFileUnitRow& row = rows[rowIndex];
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
                    const std::string id = "##unit_" + row.ScenePath + "_"
                        + row.EntityTag + "_" + fieldKeys[i].first;
                    if (i >= row.EditBuffers.size())
                        row.EditBuffers.resize(fieldKeys.size());
                    ImGui::PushItemWidth(-1.0f);
                    if (ImGui::InputText(id.c_str(),
                            &row.EditBuffers[i],
                            ImGuiInputTextFlags_AutoSelectAll)
                        && ImGui::IsItemDeactivatedAfterEdit())
                    {
                        std::string error;
                        if (WriteSceneField(row.ScenePath, row.EntityTag,
                                componentKey, fieldKeys[i].first,
                                row.EditBuffers[i], &error))
                        {
                            s_EditStatus = "已保存: " + row.EntityTag + "." + fieldKeys[i].first
                                + " = " + row.EditBuffers[i];
                        }
                        else
                        {
                            s_EditStatus = "保存失败: " + error;
                        }
                    }
                    ImGui::PopItemWidth();
                }
            }
            ImGui::EndTable();
        }
    }

} // namespace Wheatear::CombatUnitOverview
