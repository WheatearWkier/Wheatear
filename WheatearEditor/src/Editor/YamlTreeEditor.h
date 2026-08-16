#pragma once

// Generic editable YAML tree widget, shared by every content editor that
// needs a structured fallback for fields without a typed panel control
// (Progression content, SideCombat tuning, standalone .yaml data files...).
// Header-only so each translation unit compiles independently; the node is
// edited in place and `changed` reports whether the user modified anything.

#include "EditorContentPickers.h"
#include "EditorLocale.h"
#include "EditorWidgets.h"

#include <yaml-cpp/yaml.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::YamlTreeEditor {

    inline const char* NodeTypeLabel(const YAML::Node& node)
    {
        if (node.IsMap())
            return "map";
        if (node.IsSequence())
            return "list";
        if (node.IsScalar())
            return "value";
        if (node.IsNull())
            return "null";
        return "node";
    }

    inline std::string LastPathSegment(const std::string& path)
    {
        const size_t slash = path.find_last_of('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }

    inline std::string ToLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    inline bool ContainsInsensitive(const std::string& value, const std::string& token)
    {
        return ToLowerAscii(value).find(ToLowerAscii(token)) != std::string::npos;
    }

    inline bool MapContainsKey(const YAML::Node& node, const std::string& key)
    {
        if (!node || !node.IsMap())
            return false;

        for (auto it = node.begin(); it != node.end(); ++it)
        {
            if (it->first.IsScalar() && it->first.as<std::string>() == key)
                return true;
        }
        return false;
    }

    inline std::vector<std::string> MapKeysInOrder(const YAML::Node& node)
    {
        std::vector<std::string> keys;
        if (!node || !node.IsMap())
            return keys;

        for (auto it = node.begin(); it != node.end(); ++it)
        {
            if (it->first.IsScalar())
                keys.push_back(it->first.as<std::string>());
        }
        return keys;
    }

    inline std::string ScalarText(const YAML::Node& node)
    {
        if (!node || !node.IsDefined() || node.IsNull())
            return {};

        try
        {
            return node.as<std::string>("");
        }
        catch (...)
        {
            return {};
        }
    }

    // ---------------------------------------------------------------------
    //  Tree widgets
    // ---------------------------------------------------------------------

    // Rect editor: a `rect: [x, y, w, h]` sequence gets a mini canvas with a
    // draggable box (move by dragging inside, resize via the corner handle),
    // plus plain number fields. Generic — works for any atlas/UI layout data.
    struct RectDragState
    {
        bool Active = false;
        bool Resizing = false;
        int  Frame = 0;
        float GrabDX = 0.0f;
        float GrabDY = 0.0f;
        float StartX = 0.0f, StartY = 0.0f, StartW = 0.0f, StartH = 0.0f;
    };

    inline RectDragState& RectDragStateInstance()
    {
        static RectDragState state;
        return state;
    }

    inline bool DrawYamlRectEditor(YAML::Node node)
    {
        if (!node || !node.IsSequence() || node.size() < 4)
            return false;

        float values[4] = {};
        for (int i = 0; i < 4; ++i)
        {
            try { values[i] = node[i].as<float>(0.0f); }
            catch (...) { return false; }
        }

        bool changed = false;
        bool allInt = true;
        for (int i = 0; i < 4; ++i)
            allInt = allInt && (values[i] == std::floor(values[i]));

        const auto writeBack = [&](const float v[4])
        {
            for (int i = 0; i < 4; ++i)
            {
                if (allInt)
                    node[i] = static_cast<int>(std::lround(v[i]));
                else
                    node[i] = v[i];
            }
        };

        ImGui::PushID("RectEditor");
        const ImGuiStyle& style = ImGui::GetStyle();
        const float fieldWidth = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 3.0f) * 0.25f;
        ImGui::SetNextItemWidth(std::max(48.0f, fieldWidth));
        if (ImGui::DragFloat("X", &values[0], 1.0f)) changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(48.0f, fieldWidth));
        if (ImGui::DragFloat("Y", &values[1], 1.0f)) changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(48.0f, fieldWidth));
        if (ImGui::DragFloat("W", &values[2], 1.0f, 0.0f, 100000.0f)) changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(48.0f, fieldWidth));
        if (ImGui::DragFloat("H", &values[3], 1.0f, 0.0f, 100000.0f)) changed = true;

        // Mini canvas: normalized viewport, rect drawn to fit.
        const ImVec2 canvasSize(190.0f, 110.0f);
        ImGui::InvisibleButton("##RectCanvas", canvasSize);
        const bool canvasHovered = ImGui::IsItemHovered();
        const ImVec2 canvasMin = ImGui::GetItemRectMin();
        const ImVec2 canvasMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(30, 34, 38, 200), 4.0f);

        const float maxDim = std::max({ values[0] + values[2], values[1] + values[3], 1.0f });
        const float sx = canvasSize.x / maxDim;
        const float sy = canvasSize.y / maxDim;
        ImVec2 boxMin(canvasMin.x + values[0] * sx, canvasMin.y + values[1] * sy);
        ImVec2 boxMax(boxMin.x + values[2] * sx, boxMin.y + values[3] * sy);

        const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const int frame = ImGui::GetFrameCount();

        if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            // Corner handle wins over move.
            const bool nearCorner = std::abs(mouse.x - boxMax.x) < 8.0f
                && std::abs(mouse.y - boxMax.y) < 8.0f;
            if (nearCorner)
            {
                RectDragStateInstance() = { true, true, frame,
                    0.0f, 0.0f, values[0], values[1], values[2], values[3] };
            }
            else if (mouse.x >= boxMin.x && mouse.x <= boxMax.x
                && mouse.y >= boxMin.y && mouse.y <= boxMax.y)
            {
                RectDragStateInstance() = { true, false, frame,
                    mouse.x - boxMin.x, mouse.y - boxMin.y,
                    values[0], values[1], values[2], values[3] };
            }
        }

        if (RectDragStateInstance().Active && RectDragStateInstance().Frame == frame && mouseDown)
        {
            float x = values[0], y = values[1], w = values[2], h = values[3];
            if (RectDragStateInstance().Resizing)
            {
                w = std::max(1.0f, (mouse.x - canvasMin.x) / sx - RectDragStateInstance().StartX);
                h = std::max(1.0f, (mouse.y - canvasMin.y) / sy - RectDragStateInstance().StartY);
                x = RectDragStateInstance().StartX;
                y = RectDragStateInstance().StartY;
            }
            else
            {
                x = std::max(0.0f, (mouse.x - canvasMin.x - RectDragStateInstance().GrabDX) / sx);
                y = std::max(0.0f, (mouse.y - canvasMin.y - RectDragStateInstance().GrabDY) / sy);
                w = RectDragStateInstance().StartW;
                h = RectDragStateInstance().StartH;
            }
            const float next[4] = { x, y, w, h };
            writeBack(next);
            values[0] = x; values[1] = y; values[2] = w; values[3] = h;
            changed = true;
        }
        else if (RectDragStateInstance().Active && !mouseDown)
        {
            RectDragStateInstance().Active = false;
        }

        const float bx0 = std::min(boxMin.x, boxMax.x), by0 = std::min(boxMin.y, boxMax.y);
        const float bx1 = std::max(boxMin.x, boxMax.x), by1 = std::max(boxMin.y, boxMax.y);
        drawList->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1),
            IM_COL32(255, 170, 60, 235), 2.0f, 0, 1.5f);
        drawList->AddRectFilled(ImVec2(bx1 - 5.0f, by1 - 5.0f), ImVec2(bx1 + 1.0f, by1 + 1.0f),
            IM_COL32(255, 200, 90, 255));
        ImGui::PopID();

        if (changed)
            writeBack(values);
        return changed;
    }

    inline bool DrawYamlNode(YAML::Node node, const std::string& path, int depth,
        std::unordered_map<std::string, std::string>& newScalarValues,
        std::unordered_map<std::string, std::string>& newMapKeys);

    inline bool DrawYamlScalar(YAML::Node node, const std::string& path)
    {
        ImGui::PushID(path.c_str());
        std::string value = ScalarText(node);
        const std::string key = LastPathSegment(path);
        bool changed = false;

        // rect: [x, y, w, h] sequences get a draggable box editor instead of
        // a plain text field (atlas regions, UI layouts, ...).
        if (ContainsInsensitive(key, "rect")
            && node && node.IsSequence() && node.size() >= 4)
        {
            changed = DrawYamlRectEditor(node);
            ImGui::PopID();
            return changed;
        }

        if (ContainsInsensitive(key, "icon") || ContainsInsensitive(key, "texture") || ContainsInsensitive(key, "image"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Texture, 512);
        }
        else if (ContainsInsensitive(key, "audio") || ContainsInsensitive(key, "sound") || ContainsInsensitive(key, "sfx") || ContainsInsensitive(key, "bgm"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Audio, 512);
        }
        else if (ContainsInsensitive(key, "scene"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Scene, 512);
        }
        else if (ContainsInsensitive(key, "script") || ContainsInsensitive(key, "event"))
        {
            changed = EditorContentPickers::DrawAssetField(EditorLocale::Text("Value", "值"), value, EditorWidgets::AssetReferenceKind::Script, 512);
        }
        else if (ContainsInsensitive(key, "slot"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::EquipmentSlot, 256);
        }
        else if (ContainsInsensitive(key, "equipment"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Equipment, 256);
        }
        else if (ContainsInsensitive(key, "dungeon"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Dungeon, 256);
        }
        else if (ContainsInsensitive(key, "skill"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Skill, 256);
        }
        else if (ContainsInsensitive(key, "material") || key == "item" || ContainsInsensitive(key, "itemid"))
        {
            changed = EditorContentPickers::DrawProgressionIdField(EditorLocale::Text("Value", "值"), value, EditorContentPickers::ProgressionIdKind::Material, 256);
        }
        else if (ContainsInsensitive(key, "flag"))
        {
            changed = EditorContentPickers::DrawStoryFlagField(EditorLocale::Text("Value", "值"), value, 256);
        }
        else
        {
            changed = EditorWidgets::InputString(EditorLocale::Text("Value", "值"), value, 512);
        }
        if (changed)
            node = value;
        ImGui::PopID();
        return changed;
    }

    inline bool DrawYamlAddControls(YAML::Node node, const std::string& path, bool sequence,
        std::unordered_map<std::string, std::string>& newScalarValues,
        std::unordered_map<std::string, std::string>& newMapKeys)
    {
        bool changed = false;
        ImGui::PushID((path + "/add").c_str());
        ImGui::Separator();

        if (sequence)
        {
            std::string& value = newScalarValues[path];
            EditorWidgets::InputString("New Value", value, 256);
            if (ImGui::SmallButton("Add Value"))
            {
                node.push_back(value);
                value.clear();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add Map"))
            {
                node.push_back(YAML::Node(YAML::NodeType::Map));
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add List"))
            {
                node.push_back(YAML::Node(YAML::NodeType::Sequence));
                changed = true;
            }
            ImGui::SameLine();
            if (node.size() > 0 && ImGui::SmallButton("Duplicate Last"))
            {
                node.push_back(YAML::Clone(node[node.size() - 1]));
                changed = true;
            }
        }
        else
        {
            std::string& key = newMapKeys[path];
            EditorWidgets::InputString("New Key", key, 192);
            const bool canAdd = !key.empty() && !MapContainsKey(node, key);
            ImGui::BeginDisabled(!canAdd);
            if (ImGui::SmallButton("Add Value"))
            {
                node[key] = "";
                key.clear();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add Map"))
            {
                node[key] = YAML::Node(YAML::NodeType::Map);
                key.clear();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Add List"))
            {
                node[key] = YAML::Node(YAML::NodeType::Sequence);
                key.clear();
                changed = true;
            }
            ImGui::EndDisabled();
            if (!key.empty() && MapContainsKey(node, key))
                EditorWidgets::InlineStatus("Key already exists.", EditorWidgets::StatusKind::Warning);
        }

        ImGui::PopID();
        return changed;
    }

    inline bool DrawYamlMap(YAML::Node node, const std::string& path, int depth,
        std::unordered_map<std::string, std::string>& newScalarValues,
        std::unordered_map<std::string, std::string>& newMapKeys)
    {
        bool changed = false;
        const std::vector<std::string> keys = MapKeysInOrder(node);
        if (keys.empty())
            EditorWidgets::EmptyState("Empty map.");

        std::string removeKey;
        for (const std::string& key : keys)
        {
            YAML::Node child = node[key];
            const std::string childPath = path + "/" + key;
            ImGui::PushID(childPath.c_str());

            if (child.IsMap() || child.IsSequence())
            {
                const ImGuiTreeNodeFlags flags = depth < 1 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
                const bool open = ImGui::TreeNodeEx("##Node", flags, "%s (%s)", key.c_str(), NodeTypeLabel(child));
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeKey = key;

                if (open)
                {
                    changed |= DrawYamlNode(child, childPath, depth + 1, newScalarValues, newMapKeys);
                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextUnformatted(key.c_str());
                ImGui::SameLine(220.0f);
                changed |= DrawYamlScalar(child, childPath);
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeKey = key;
            }

            ImGui::PopID();
        }

        if (!removeKey.empty())
        {
            node.remove(removeKey);
            changed = true;
        }

        changed |= DrawYamlAddControls(node, path, false, newScalarValues, newMapKeys);
        return changed;
    }

    inline bool DrawYamlSequence(YAML::Node node, const std::string& path, int depth,
        std::unordered_map<std::string, std::string>& newScalarValues,
        std::unordered_map<std::string, std::string>& newMapKeys)
    {
        bool changed = false;
        if (node.size() == 0)
            EditorWidgets::EmptyState("Empty list.");

        int removeIndex = -1;
        for (size_t i = 0; i < node.size(); ++i)
        {
            YAML::Node child = node[i];
            const std::string childPath = path + "/" + std::to_string(i);
            ImGui::PushID(childPath.c_str());

            if (child.IsMap() || child.IsSequence())
            {
                const bool open = ImGui::TreeNodeEx("##Item", 0, "[%d] (%s)", static_cast<int>(i + 1), NodeTypeLabel(child));
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeIndex = static_cast<int>(i);

                if (open)
                {
                    changed |= DrawYamlNode(child, childPath, depth + 1, newScalarValues, newMapKeys);
                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::Text("[%d]", static_cast<int>(i + 1));
                ImGui::SameLine(80.0f);
                changed |= DrawYamlScalar(child, childPath);
                ImGui::SameLine();
                if (ImGui::SmallButton(EditorLocale::Text("Remove", "移除")))
                    removeIndex = static_cast<int>(i);
            }

            ImGui::PopID();
        }

        if (removeIndex >= 0)
        {
            node.remove(removeIndex);
            changed = true;
        }

        changed |= DrawYamlAddControls(node, path, true, newScalarValues, newMapKeys);
        return changed;
    }

    inline bool DrawYamlNode(YAML::Node node, const std::string& path, int depth,
        std::unordered_map<std::string, std::string>& newScalarValues,
        std::unordered_map<std::string, std::string>& newMapKeys)
    {
        if (!node || !node.IsDefined() || node.IsNull())
            return DrawYamlScalar(node, path);
        if (node.IsMap())
            return DrawYamlMap(node, path, depth, newScalarValues, newMapKeys);
        if (node.IsSequence())
            return DrawYamlSequence(node, path, depth, newScalarValues, newMapKeys);
        return DrawYamlScalar(node, path);
    }

    // Serializes a YAML node back to text (used by undo snapshots and save).
    inline std::string SerializeYaml(const YAML::Node& node)
    {
        YAML::Emitter out;
        out << node;
        return out.good() ? std::string(out.c_str()) : std::string{};
    }

} // namespace Wheatear::YamlTreeEditor
