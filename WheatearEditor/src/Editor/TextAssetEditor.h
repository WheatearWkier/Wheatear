#pragma once

#include "Editor/EditorLocale.h"
#include "Editor/AssetDocument.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::EditorUI {

    struct TextAssetEditorState
    {
        EditorDocuments::TextAssetDocument Document;
        std::string SourcePath;
        std::filesystem::path ResolvedPath;
        std::vector<char> Buffer;
        bool Loaded = false;
        bool Dirty = false;
        std::string Status;
    };

    inline size_t GetTextLength(const std::vector<char>& buffer)
    {
        const auto it = std::find(buffer.begin(), buffer.end(), '\0');
        return it == buffer.end()
            ? buffer.size()
            : static_cast<size_t>(std::distance(buffer.begin(), it));
    }

    inline void ResetBuffer(TextAssetEditorState& state, size_t capacity)
    {
        state.Buffer.assign(std::max<size_t>(capacity, 1), '\0');
    }

    inline void SyncTextAssetMetadata(TextAssetEditorState& state)
    {
        state.SourcePath = state.Document.GetSourcePath();
        state.ResolvedPath = state.Document.GetResolvedPath();
        state.Loaded = state.Document.IsLoaded();
        state.Dirty = state.Document.IsDirty();
        state.Status = state.Document.GetStatus();
    }

    inline void SyncTextAssetBufferFromDocument(TextAssetEditorState& state, size_t defaultCapacity)
    {
        constexpr size_t maxEditorCapacity = 2 * 1024 * 1024;
        const std::string& text = state.Document.GetText();
        const size_t targetCapacity = std::min(maxEditorCapacity, std::max(defaultCapacity, text.size() + 4096));
        ResetBuffer(state, targetCapacity);

        const size_t copyLength = std::min(text.size(), state.Buffer.size() - 1);
        if (copyLength > 0)
            std::memcpy(state.Buffer.data(), text.data(), copyLength);

        SyncTextAssetMetadata(state);
    }

    inline void MarkTextAssetDirty(TextAssetEditorState& state)
    {
        const size_t length = GetTextLength(state.Buffer);
        state.Document.SetText(std::string(state.Buffer.data(), state.Buffer.data() + length), true);
        SyncTextAssetMetadata(state);
    }

    inline TextAssetEditorState& GetTextAssetState(
        std::unordered_map<std::string, TextAssetEditorState>& cache,
        const std::string& sourcePath,
        size_t defaultCapacity)
    {
        auto& state = cache[sourcePath];
        if (state.SourcePath != sourcePath)
        {
            state = {};
            state.SourcePath = sourcePath;
            ResetBuffer(state, defaultCapacity);
        }
        return state;
    }

    inline bool LoadTextAsset(TextAssetEditorState& state, const std::string& sourcePath, size_t defaultCapacity)
    {
        constexpr size_t maxEditorCapacity = 2 * 1024 * 1024;
        state.Document.SetSourcePath(sourcePath);
        const bool loaded = state.Document.Load(maxEditorCapacity);
        SyncTextAssetBufferFromDocument(state, defaultCapacity);
        return loaded;
    }

    inline bool SaveTextAsset(TextAssetEditorState& state, const std::string& sourcePath)
    {
        const size_t length = GetTextLength(state.Buffer);
        state.Document.SetSourcePath(sourcePath);
        state.Document.SetText(std::string(state.Buffer.data(), state.Buffer.data() + length), true);
        const bool saved = state.Document.Save();
        SyncTextAssetMetadata(state);
        return saved;
    }

    inline void DrawTextAssetEditor(
        const char* title,
        const char* editorId,
        const std::string& sourcePath,
        std::unordered_map<std::string, TextAssetEditorState>& cache,
        size_t defaultCapacity = 256 * 1024,
        ImGuiInputTextFlags textFlags = ImGuiInputTextFlags_AllowTabInput)
    {
        ImGui::PushID(editorId);
        if (ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& state = GetTextAssetState(cache, sourcePath, defaultCapacity);
            if (!state.Loaded)
                LoadTextAsset(state, sourcePath, defaultCapacity);

            ImGui::TextDisabled(EditorLocale::Text("Resolved: %s", "解析路径: %s"), state.ResolvedPath.generic_string().c_str());

            if (ImGui::Button(EditorLocale::Text("Reload", "重载")))
                LoadTextAsset(state, sourcePath, defaultCapacity);

            if ((textFlags & ImGuiInputTextFlags_ReadOnly) == 0)
            {
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Save", "保存")))
                    SaveTextAsset(state, sourcePath);
            }

            if (state.Dirty)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.24f, 1.0f), "Modified");
            }

            if (!state.Status.empty())
                ImGui::TextWrapped("%s", state.Status.c_str());

            ImVec2 editorSize(-1.0f, 260.0f);
            if (ImGui::InputTextMultiline("##TextAssetBuffer", state.Buffer.data(), state.Buffer.size(), editorSize, textFlags))
                MarkTextAssetDirty(state);
        }
        ImGui::PopID();
    }

} // namespace Wheatear::EditorUI
