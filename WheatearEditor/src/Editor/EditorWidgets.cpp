#include "wepch.h"
#include "EditorWidgets.h"

#include "Editor/EditorPlatform.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Renderer/Texture.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>

namespace Wheatear::EditorWidgets {

    namespace {

        struct AssetReferenceChoice
        {
            std::string Label;
            std::string Value;
            bool IsAlias = false;
        };

        static const char* AssetReferenceKindLabel(AssetReferenceKind kind)
        {
            switch (kind)
            {
            case AssetReferenceKind::Texture: return "Texture";
            case AssetReferenceKind::Audio: return "Audio";
            case AssetReferenceKind::Font: return "Font";
            case AssetReferenceKind::Data: return "Data";
            case AssetReferenceKind::Scene: return "Scene";
            case AssetReferenceKind::Script: return "Script";
            case AssetReferenceKind::Prefab: return "Prefab";
            case AssetReferenceKind::AnimationClip: return "Animation Clip";
            case AssetReferenceKind::Any:
            default: return "Asset";
            }
        }

        static bool IsAssetReferenceKindMatch(AssetReferenceKind kind, const std::filesystem::path& path)
        {
            const std::string extension = path.extension().generic_string();
            if (path.empty())
                return false;

            switch (kind)
            {
            case AssetReferenceKind::Texture:
                return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".bmp";
            case AssetReferenceKind::Audio:
                return extension == ".mp3" || extension == ".wav" || extension == ".ogg" || extension == ".flac";
            case AssetReferenceKind::Font:
                return extension == ".ttf" || extension == ".otf" || extension == ".ttc";
            case AssetReferenceKind::Scene:
                return extension == ".wt" || extension == ".yaml" || extension == ".yml";
            case AssetReferenceKind::Script:
                return extension == ".lua" || extension == ".cs" || extension == ".vn" || extension == ".wts";
            case AssetReferenceKind::Prefab:
                return extension == ".wtprefab";
            case AssetReferenceKind::AnimationClip:
                return extension == ".wtanim";
            case AssetReferenceKind::Data:
                return extension == ".yaml" || extension == ".yml" || extension == ".json" || extension == ".txt" || extension == ".wts" || extension == ".wtuit";
            case AssetReferenceKind::Any:
            default:
                return true;
            }
        }

        static std::string ToLowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        static std::vector<AssetReferenceChoice> BuildAssetReferenceChoices(AssetReferenceKind kind)
        {
            std::vector<AssetReferenceChoice> choices;
            std::set<std::string> seenValues;

            for (const auto& [alias, target] : AssetAliasRegistry::All())
            {
                const std::filesystem::path targetPath = AssetPath::Resolve(target);
                if (targetPath.empty() || !std::filesystem::exists(targetPath))
                    continue;
                if (!IsAssetReferenceKindMatch(kind, targetPath))
                    continue;

                AssetReferenceChoice choice;
                choice.Label = alias + " -> " + target;
                choice.Value = alias;
                choice.IsAlias = true;
                if (seenValues.insert(choice.Value).second)
                    choices.push_back(std::move(choice));
            }

            const std::filesystem::path assetRoot = AssetPath::GetAssetRoot();
            if (std::filesystem::exists(assetRoot))
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(assetRoot))
                {
                    if (!entry.is_regular_file())
                        continue;

                    const std::filesystem::path relative = AssetPath::ToProjectRelative(entry.path());
                    const std::string relativeText = relative.generic_string();
                    if (relative.empty() || relativeText.find("assets/.wheatear/") == 0)
                        continue;
                    if (!IsAssetReferenceKindMatch(kind, relative))
                        continue;

                    const std::string value = relativeText;
                    if (!seenValues.insert(value).second)
                        continue;

                    AssetReferenceChoice choice;
                    choice.Label = value;
                    choice.Value = value;
                    choices.push_back(std::move(choice));
                }
            }

            std::sort(choices.begin(), choices.end(), [](const AssetReferenceChoice& a, const AssetReferenceChoice& b)
            {
                if (a.IsAlias != b.IsAlias)
                    return a.IsAlias > b.IsAlias;
                return ToLowerCopy(a.Label) < ToLowerCopy(b.Label);
            });
            return choices;
        }

        ImVec4 StatusColor(StatusKind kind)
        {
            switch (kind)
            {
            case StatusKind::Info: return { 0.10f, 0.52f, 0.58f, 1.0f };
            case StatusKind::Success: return { 0.18f, 0.56f, 0.38f, 1.0f };
            case StatusKind::Warning: return { 0.70f, 0.45f, 0.12f, 1.0f };
            case StatusKind::Error: return { 0.78f, 0.24f, 0.18f, 1.0f };
            case StatusKind::Neutral:
            default: return { 0.30f, 0.40f, 0.42f, 1.0f };
            }
        }

        ImVec4 StatusBackground(StatusKind kind)
        {
            switch (kind)
            {
            case StatusKind::Info: return { 0.76f, 0.86f, 0.86f, 1.0f };
            case StatusKind::Success: return { 0.77f, 0.86f, 0.81f, 1.0f };
            case StatusKind::Warning: return { 0.90f, 0.83f, 0.66f, 1.0f };
            case StatusKind::Error: return { 0.91f, 0.73f, 0.69f, 1.0f };
            case StatusKind::Neutral:
            default: return { 0.80f, 0.84f, 0.83f, 1.0f };
            }
        }

        StatusKind StatusKindFromText(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            if (text.find("fail") != std::string::npos ||
                text.find("invalid") != std::string::npos ||
                text.find("blocked") != std::string::npos ||
                text.find("error") != std::string::npos)
                return StatusKind::Error;

            if (text.find("warning") != std::string::npos ||
                text.find("modified") != std::string::npos ||
                text.find("unsaved") != std::string::npos)
                return StatusKind::Warning;

            if (text.find("saved") != std::string::npos ||
                text.find("loaded") != std::string::npos ||
                text.find("written") != std::string::npos ||
                text.find("ready") != std::string::npos)
                return StatusKind::Success;

            return StatusKind::Info;
        }

    } // namespace

    bool InputString(const char* label, std::string& value, size_t capacity)
    {
        std::vector<char> buffer(std::max<size_t>(capacity, value.size() + 32), 0);
        strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
        if (ImGui::InputText(label, buffer.data(), buffer.size()))
        {
            value = buffer.data();
            return true;
        }
        return false;
    }

    bool InputMultilineString(const char* label,
        std::string& value,
        const ImVec2& size,
        size_t capacity,
        ImGuiInputTextFlags flags)
    {
        std::vector<char> buffer(std::max<size_t>(capacity, value.size() + 32), 0);
        strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
        if (ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), size, flags))
        {
            value = buffer.data();
            return true;
        }
        return false;
    }

    void HelpTooltip(const char* text)
    {
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", text);
    }

    void PanelHeader(const char* title, const char* subtitle)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.20f, 0.22f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 2.0f));
        ImGui::TextUnformatted(title ? title : "");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        if (subtitle && subtitle[0] != '\0')
            ImGui::TextDisabled("%s", subtitle);
        ImGui::Separator();
    }

    void SectionHeader(const char* label, const char* description)
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.12f, 0.45f, 0.48f, 1.0f), "%s", label ? label : "");
        if (description && description[0] != '\0')
            ImGui::TextDisabled("%s", description);
    }

    void StatusBadge(const char* label, StatusKind kind)
    {
        const ImVec4 bg = StatusBackground(kind);
        const ImVec4 fg = StatusColor(kind);
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg);
        ImGui::PushStyleColor(ImGuiCol_Text, fg);
        ImGui::PushItemFlag(ImGuiItemFlags_AllowDuplicateId, true);
        ImGui::SmallButton(label ? label : "");
        ImGui::PopItemFlag();
        ImGui::PopStyleColor(4);
    }

    void InlineStatus(const std::string& message, StatusKind kind)
    {
        if (message.empty())
            return;

        ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(kind));
        ImGui::TextWrapped("%s", message.c_str());
        ImGui::PopStyleColor();
    }

    void EmptyState(const char* title, const char* description)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", title ? title : "No content.");
        if (description && description[0] != '\0')
            ImGui::TextWrapped("%s", description);
    }

    std::string LabelWithId(const std::string& label, const std::string& id)
    {
        return label + "##" + id;
    }

    bool SearchBar(const char* id, char* buffer, size_t bufferSize, const char* hint)
    {
        return ImGui::InputTextWithHint(id, hint ? hint : "Search...", buffer, bufferSize);
    }

    bool IconButton(const char* id,
        const Ref<Texture2D>& icon,
        const char* tooltip,
        const ImVec2& size,
        bool enabled)
    {
        if (!icon)
            return false;

        bool clicked = false;
        if (!enabled)
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.35f);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));
        clicked = ImGui::ImageButton(
            id,
            static_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
            size,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
            ImVec4(1.0f, 1.0f, 1.0f, enabled ? 1.0f : 0.55f));
        ImGui::PopStyleVar();

        if (!enabled)
            ImGui::PopStyleVar();

        if (tooltip && tooltip[0] != '\0' && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);

        return clicked && enabled;
    }

    bool DirtySaveBar(bool dirty,
        const std::string& status,
        const char* saveLabel,
        const char* cancelLabel,
        bool* cancelClicked)
    {
        bool saveClicked = false;
        if (dirty)
        {
            StatusBadge("Unsaved", StatusKind::Warning);
            ImGui::SameLine();
        }
        else
        {
            StatusBadge("Clean", StatusKind::Success);
            ImGui::SameLine();
        }

        if (ImGui::Button(saveLabel ? saveLabel : "Save"))
            saveClicked = true;
        if (cancelClicked)
        {
            ImGui::SameLine();
            if (ImGui::Button(cancelLabel ? cancelLabel : "Cancel"))
                *cancelClicked = true;
        }
        if (!status.empty())
        {
            ImGui::SameLine();
            InlineStatus(status, StatusKindFromText(status));
        }
        return saveClicked;
    }

    std::vector<std::string> SplitList(const std::string& text)
    {
        std::vector<std::string> values;
        std::string current;
        auto flush = [&]()
        {
            const size_t start = current.find_first_not_of(" \t\r\n");
            const size_t end = current.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos)
                values.push_back(current.substr(start, end - start + 1));
            current.clear();
        };

        for (char c : text)
        {
            if (c == ',' || c == ';' || c == '|')
                flush();
            else
                current.push_back(c);
        }
        flush();
        return values;
    }

    std::string JoinList(const std::vector<std::string>& values)
    {
        std::ostringstream stream;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0)
                stream << ", ";
            stream << values[i];
        }
        return stream.str();
    }

    bool ReadFileText(const std::filesystem::path& path, std::string& text)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
            return false;

        text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        return true;
    }

    bool WriteFileText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
            return false;

        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return output.good();
    }

    std::filesystem::path ResolveProjectAsset(const std::string& relativePath)
    {
        if (relativePath.empty())
            return {};

        return AssetPath::GetProjectRoot() / std::filesystem::path(AssetAliasRegistry::Resolve(relativePath));
    }

    bool ProjectAssetExists(const std::string& relativePath)
    {
        const std::filesystem::path resolved = ResolveProjectAsset(relativePath);
        return !resolved.empty() && std::filesystem::exists(resolved);
    }

    void CopyProjectAssetPath(const std::string& relativePath)
    {
        const std::filesystem::path resolved = ResolveProjectAsset(relativePath);
        const std::string text = resolved.empty() ? relativePath : resolved.string();
        ImGui::SetClipboardText(text.c_str());
    }

    void OpenProjectAssetFolder(const std::string& relativePath)
    {
        const std::filesystem::path resolved = ResolveProjectAsset(relativePath);
        if (resolved.empty())
            return;

        const std::filesystem::path directory = std::filesystem::is_directory(resolved)
            ? resolved
            : resolved.parent_path();
        EditorPlatform::OpenDirectory(directory);
    }

    void DrawPathTools(const char* id, const std::string& relativePath)
    {
        if (relativePath.empty())
        {
            ImGui::TextDisabled("-");
            return;
        }

        const bool exists = ProjectAssetExists(relativePath);
        ImGui::TextWrapped("%s", relativePath.c_str());
        ImGui::SameLine();
        ImGui::PushID(id);
        if (exists)
        {
            if (ImGui::SmallButton("Open Folder"))
                OpenProjectAssetFolder(relativePath);
            ImGui::SameLine();
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Missing");
            ImGui::SameLine();
        }
        if (ImGui::SmallButton("Copy Path"))
            CopyProjectAssetPath(relativePath);
        ImGui::PopID();
    }

    void DrawLabeledPathTools(const char* label, const std::string& relativePath)
    {
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(150.0f);
        DrawPathTools(label, relativePath);
    }

    bool DrawAssetReferenceField(const char* label,
        std::string& reference,
        AssetReferenceKind kind,
        size_t capacity)
    {
        const std::string fieldId = std::string(label ? label : "Asset") + "##asset_ref_field";
        bool changed = false;

        ImGui::TextDisabled("%s", label ? label : "Asset");
        ImGui::SameLine(150.0f);
        ImGui::PushID(fieldId.c_str());

        std::vector<char> buffer(std::max<size_t>(capacity, reference.size() + 32), 0);
        strncpy_s(buffer.data(), buffer.size(), reference.c_str(), _TRUNCATE);
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::InputText("##reference", buffer.data(), buffer.size()))
        {
            reference = buffer.data();
            changed = true;
        }
        ImGui::SameLine();

        const std::string popupId = std::string("Select##") + fieldId;
        if (ImGui::SmallButton("Select"))
            ImGui::OpenPopup(popupId.c_str());
        if (ImGui::IsItemHovered())
        {
            const std::string hint = std::string("Pick a ") + AssetReferenceKindLabel(kind) + " from aliases or project assets.";
            ImGui::SetTooltip("%s", hint.c_str());
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            reference.clear();
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Open"))
            OpenProjectAssetFolder(reference);

        const bool exists = ProjectAssetExists(reference);
        ImGui::SameLine();
        if (reference.empty())
            ImGui::TextDisabled("(none)");
        else if (exists)
            ImGui::TextDisabled("%s", reference.c_str());
        else
            ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.45f, 1.0f), "%s", reference.c_str());

        if (ImGui::BeginPopup(popupId.c_str()))
        {
            static char searchBuffer[128] = {};
            SearchBar("##asset_reference_search", searchBuffer, sizeof(searchBuffer), "Search asset or alias...");
            ImGui::Separator();

            const std::vector<AssetReferenceChoice> choices = BuildAssetReferenceChoices(kind);
            const std::string filter = ToLowerCopy(searchBuffer);
            const size_t maxChoices = 180;
            size_t shown = 0;
            for (const AssetReferenceChoice& choice : choices)
            {
                if (!filter.empty())
                {
                    const std::string labelText = ToLowerCopy(choice.Label);
                    const std::string valueText = ToLowerCopy(choice.Value);
                    if (labelText.find(filter) == std::string::npos && valueText.find(filter) == std::string::npos)
                        continue;
                }

                const bool selected = reference == choice.Value;
                const std::string itemLabel = LabelWithId(choice.Label, "asset_ref_choice:" + choice.Value);
                if (ImGui::Selectable(itemLabel.c_str(), selected))
                {
                    reference = choice.Value;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();

                if (++shown >= maxChoices)
                {
                    ImGui::TextDisabled("More results omitted.");
                    break;
                }
            }

            if (choices.empty())
                EmptyState("No matching assets.", "Use Content Browser or add an alias to make this picker useful.");

            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    std::string ReadString(const YAML::Node& node, const char* key, const std::string& fallback)
    {
        return ReadScalar<std::string>(node, key, fallback);
    }

    YAML::Node EnsureMap(YAML::Node node, const char* key)
    {
        if (!node[key] || !node[key].IsMap())
            node[key] = YAML::Node(YAML::NodeType::Map);
        return node[key];
    }

    std::vector<std::string> MapKeys(const YAML::Node& node)
    {
        std::vector<std::string> keys;
        if (!node || !node.IsMap())
            return keys;

        for (auto it = node.begin(); it != node.end(); ++it)
        {
            if (it->first.IsScalar())
                keys.push_back(it->first.as<std::string>());
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    std::vector<std::string> ReadStringList(const YAML::Node& node)
    {
        std::vector<std::string> values;
        if (!node || !node.IsSequence())
            return values;

        for (std::size_t i = 0; i < node.size(); ++i)
        {
            const YAML::Node value = node[i];
            if (value.IsScalar())
                values.push_back(value.as<std::string>());
        }
        return values;
    }

    bool DrawFloat(YAML::Node map,
        const char* key,
        const char* label,
        float speed,
        float minValue,
        float maxValue,
        const char* format)
    {
        float value = ReadScalar<float>(map, key, 0.0f);
        const std::string itemLabel = LabelWithId(label ? label : key, key ? key : label);
        if (ImGui::DragFloat(itemLabel.c_str(), &value, speed, minValue, maxValue, format))
        {
            map[key] = value;
            return true;
        }
        return false;
    }

    bool DrawInt(YAML::Node map,
        const char* key,
        const char* label,
        int minValue,
        int maxValue)
    {
        int value = ReadScalar<int>(map, key, 0);
        const std::string itemLabel = LabelWithId(label ? label : key, key ? key : label);
        if (ImGui::DragInt(itemLabel.c_str(), &value, 1.0f, minValue, maxValue))
        {
            map[key] = value;
            return true;
        }
        return false;
    }

    bool DrawBool(YAML::Node map, const char* key, const char* label)
    {
        bool value = ReadScalar<bool>(map, key, false);
        const std::string itemLabel = LabelWithId(label ? label : key, key ? key : label);
        if (ImGui::Checkbox(itemLabel.c_str(), &value))
        {
            map[key] = value;
            return true;
        }
        return false;
    }

    bool DrawString(YAML::Node map,
        const char* key,
        const char* label,
        size_t capacity)
    {
        std::string value = ReadString(map, key);
        const std::string itemLabel = LabelWithId(label ? label : key, key ? key : label);
        if (InputString(itemLabel.c_str(), value, capacity))
        {
            map[key] = value;
            return true;
        }
        return false;
    }

    bool DrawVec2(YAML::Node map,
        const char* key,
        const char* label,
        float speed)
    {
        float values[2] = { 0.0f, 0.0f };
        const YAML::Node source = map[key];
        if (source && source.IsSequence() && source.size() >= 2)
        {
            values[0] = source[0].as<float>(0.0f);
            values[1] = source[1].as<float>(0.0f);
        }

        const std::string itemLabel = LabelWithId(label ? label : key, key ? key : label);
        if (ImGui::DragFloat2(itemLabel.c_str(), values, speed))
        {
            YAML::Node sequence(YAML::NodeType::Sequence);
            sequence.push_back(values[0]);
            sequence.push_back(values[1]);
            map[key] = sequence;
            return true;
        }
        return false;
    }

    bool DrawStringList(YAML::Node map,
        const char* key,
        const char* label,
        size_t capacity)
    {
        std::string value = JoinList(ReadStringList(map[key]));
        const std::string itemLabel = LabelWithId(label ? label : key, key ? key : label);
        if (InputString(itemLabel.c_str(), value, capacity))
        {
            YAML::Node sequence(YAML::NodeType::Sequence);
            for (const std::string& item : SplitList(value))
                sequence.push_back(item);
            map[key] = sequence;
            return true;
        }
        return false;
    }

    bool DrawTeamCombo(int& team, const char* label)
    {
        static const char* labels[] = { "Neutral", "Player", "Enemy" };
        int index = std::clamp(team, 0, 2);
        if (ImGui::Combo(label, &index, labels, 3))
        {
            team = index;
            return true;
        }
        return false;
    }

    bool BeginSelector(const char* label,
        const std::vector<std::string>& keys,
        std::string& selected)
    {
        if (selected.empty() && !keys.empty())
            selected = keys.front();

        if (ImGui::BeginCombo(label, selected.empty() ? "(none)" : selected.c_str()))
        {
            for (size_t i = 0; i < keys.size(); ++i)
            {
                const std::string& key = keys[i];
                const bool isSelected = selected == key;
                const std::string itemLabel = LabelWithId(key, "selector:" + std::to_string(i));
                if (ImGui::Selectable(itemLabel.c_str(), isSelected))
                    selected = key;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        return !selected.empty();
    }

} // namespace Wheatear::EditorWidgets
