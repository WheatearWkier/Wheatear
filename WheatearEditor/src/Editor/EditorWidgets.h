#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <vector>

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

namespace Wheatear {

    class Texture2D;

} // namespace Wheatear

namespace Wheatear::EditorWidgets {

    enum class StatusKind
    {
        Neutral,
        Info,
        Success,
        Warning,
        Error
    };

    bool InputString(const char* label, std::string& value, size_t capacity = 512);
    bool InputMultilineString(const char* label,
        std::string& value,
        const ImVec2& size,
        size_t capacity = 1024,
        ImGuiInputTextFlags flags = 0);
    void HelpTooltip(const char* text);
    void PanelHeader(const char* title, const char* subtitle = nullptr);
    void SectionHeader(const char* label, const char* description = nullptr);
    void StatusBadge(const char* label, StatusKind kind = StatusKind::Neutral);
    void InlineStatus(const std::string& message, StatusKind kind = StatusKind::Info);
    void EmptyState(const char* title, const char* description = nullptr);
    std::string LabelWithId(const std::string& label, const std::string& id);
    bool SearchBar(const char* id, char* buffer, size_t bufferSize, const char* hint = "Search...");
    bool IconButton(const char* id,
        const Ref<Texture2D>& icon,
        const char* tooltip,
        const ImVec2& size = ImVec2(30.0f, 30.0f),
        bool enabled = true);
    bool DirtySaveBar(bool dirty,
        const std::string& status,
        const char* saveLabel = "Save",
        const char* cancelLabel = "Cancel",
        bool* cancelClicked = nullptr);

    enum class AssetReferenceKind
    {
        Any = 0,
        Texture,
        Audio,
        Font,
        Data,
        Scene,
        Script,
        Prefab,
        AnimationClip
    };

    std::vector<std::string> SplitList(const std::string& text);
    std::string JoinList(const std::vector<std::string>& values);

    bool ReadFileText(const std::filesystem::path& path, std::string& text);
    bool WriteFileText(const std::filesystem::path& path, const std::string& text);
    std::filesystem::path ResolveProjectAsset(const std::string& relativePath);
    // Write destination for game content: always lands under the project root,
    // never the engine root. Ensures edits to gameplay data (WAO recipes,
    // progression content, content manifest, tunings) stay in the project.
    std::filesystem::path ResolveWritableProjectAsset(const std::string& relativePath);
    bool ProjectAssetExists(const std::string& relativePath);
    void CopyProjectAssetPath(const std::string& relativePath);
    void OpenProjectAssetFolder(const std::string& relativePath);
    void DrawPathTools(const char* id, const std::string& relativePath);
    void DrawLabeledPathTools(const char* label, const std::string& relativePath);
    bool DrawAssetReferenceField(const char* label,
        std::string& reference,
        AssetReferenceKind kind = AssetReferenceKind::Any,
        size_t capacity = 512);

    template<typename T>
    T ReadScalar(const YAML::Node& node, const char* key, T fallback)
    {
        try
        {
            const YAML::Node value = node[key];
            return value ? value.as<T>(fallback) : fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

    std::string ReadString(const YAML::Node& node, const char* key, const std::string& fallback = {});
    YAML::Node EnsureMap(YAML::Node node, const char* key);
    std::vector<std::string> MapKeys(const YAML::Node& node);
    std::vector<std::string> ReadStringList(const YAML::Node& node);

    bool DrawFloat(YAML::Node map,
        const char* key,
        const char* label,
        float speed = 0.01f,
        float minValue = 0.0f,
        float maxValue = 0.0f,
        const char* format = "%.3f");
    bool DrawInt(YAML::Node map,
        const char* key,
        const char* label,
        int minValue = 0,
        int maxValue = 999);
    bool DrawBool(YAML::Node map, const char* key, const char* label);
    bool DrawString(YAML::Node map,
        const char* key,
        const char* label,
        size_t capacity = 512);
    bool DrawVec2(YAML::Node map,
        const char* key,
        const char* label,
        float speed = 0.01f);
    bool DrawStringList(YAML::Node map,
        const char* key,
        const char* label,
        size_t capacity = 512);
    bool DrawTeamCombo(int& team, const char* label = "Team");
    bool BeginSelector(const char* label,
        const std::vector<std::string>& keys,
        std::string& selected);

} // namespace Wheatear::EditorWidgets
