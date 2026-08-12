#pragma once

#include "Editor/EditorWidgets.h"
#include "Editor/EditorLocale.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace Wheatear::EditorGameplayShell {

    enum class DocumentKind
    {
        Asset = 0,
        Scene,
        RuntimeDebug,
        ReadOnly
    };

    struct DocumentStatus
    {
        DocumentKind Kind = DocumentKind::Asset;
        bool Dirty = false;
        bool Valid = true;
        std::string SourcePath;
        std::string Status;
    };

    inline const char* DocumentKindLabel(DocumentKind kind)
    {
        switch (kind)
        {
        case DocumentKind::Asset: return EditorLocale::Text("Edits Asset", "编辑资产");
        case DocumentKind::Scene: return EditorLocale::Text("Edits Scene", "编辑场景");
        case DocumentKind::RuntimeDebug: return EditorLocale::Text("Runtime Debug", "运行时调试");
        case DocumentKind::ReadOnly: return EditorLocale::Text("Read-only View", "只读视图");
        }
        return EditorLocale::Text("Edits Asset", "编辑资产");
    }

    inline EditorWidgets::StatusKind DocumentKindBadge(DocumentKind kind)
    {
        switch (kind)
        {
        case DocumentKind::Asset: return EditorWidgets::StatusKind::Info;
        case DocumentKind::Scene: return EditorWidgets::StatusKind::Success;
        case DocumentKind::RuntimeDebug: return EditorWidgets::StatusKind::Warning;
        case DocumentKind::ReadOnly: return EditorWidgets::StatusKind::Neutral;
        }
        return EditorWidgets::StatusKind::Info;
    }

    inline EditorWidgets::StatusKind StatusBadgeKind(const std::string& status)
    {
        std::string lower = status;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

        if (lower.find("fail") != std::string::npos ||
            lower.find("error") != std::string::npos ||
            lower.find("invalid") != std::string::npos ||
            lower.find("blocked") != std::string::npos)
            return EditorWidgets::StatusKind::Error;

        if (lower.find("modified") != std::string::npos ||
            lower.find("unsaved") != std::string::npos)
            return EditorWidgets::StatusKind::Warning;

        if (lower.find("saved") != std::string::npos ||
            lower.find("loaded") != std::string::npos ||
            lower.find("reloaded") != std::string::npos ||
            lower.find("clean") != std::string::npos)
            return EditorWidgets::StatusKind::Success;

        return EditorWidgets::StatusKind::Info;
    }

    inline void DrawDocumentStatus(const DocumentStatus& status)
    {
        EditorWidgets::StatusBadge(DocumentKindLabel(status.Kind), DocumentKindBadge(status.Kind));
        ImGui::SameLine();
        EditorWidgets::StatusBadge(status.Valid
            ? EditorLocale::Text("Valid", "有效")
            : EditorLocale::Text("Invalid", "无效"),
            status.Valid ? EditorWidgets::StatusKind::Success : EditorWidgets::StatusKind::Error);
        ImGui::SameLine();
        EditorWidgets::StatusBadge(status.Dirty
            ? EditorLocale::Text("Unsaved", "未保存")
            : EditorLocale::Text("Clean", "干净"),
            status.Dirty ? EditorWidgets::StatusKind::Warning : EditorWidgets::StatusKind::Success);

        if (!status.SourcePath.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", status.SourcePath.c_str());
        }

        if (!status.Status.empty())
            EditorWidgets::InlineStatus(status.Status, StatusBadgeKind(status.Status));
    }

    inline bool DrawAssetToolbar(const DocumentStatus& status,
        const char* saveLabel,
        const char* reloadLabel,
        bool* reloadClicked)
    {
        DrawDocumentStatus(status);
        return EditorWidgets::DirtySaveBar(status.Dirty,
            status.Status,
            saveLabel ? saveLabel : EditorLocale::Text("Save Asset", "保存资产"),
            reloadLabel ? reloadLabel : EditorLocale::Text("Reload Asset", "重载资产"),
            reloadClicked);
    }

    inline bool BeginAdvancedTab(const char* label = "Advanced")
    {
        return ImGui::BeginTabItem(label ? label : "Advanced");
    }

    inline bool BeginRawPreviewTab(const char* label = "Advanced Raw")
    {
        return ImGui::BeginTabItem(label ? label : "Advanced Raw");
    }

    inline void DrawRawPreview(const std::string& text, const char* id)
    {
        std::string preview = text;
        EditorWidgets::InputMultilineString(id ? id : "##GameplayRawPreview",
            preview,
            ImVec2(-1.0f, -1.0f),
            std::max<size_t>(text.size() + 1, 4096),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);
    }

    inline bool AdvancedModeToggle(bool& enabled, const char* label = "Advanced")
    {
        return ImGui::Checkbox(label ? label : "Advanced", &enabled);
    }

} // namespace Wheatear::EditorGameplayShell
