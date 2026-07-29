#include "wtpch.h"
#include "ContentBrowserPanel.h"

#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cstring>

namespace Wheatear {

    std::filesystem::path GetEditorAssetPath()
    {
        return AssetPath::GetAssetRoot();
    }

    ContentBrowserPanel::ContentBrowserPanel()
        : m_CurrentDirectory(GetEditorAssetPath())
    {
        m_Icons[AssetType::Directory] = Texture2D::Create("Resources/Icons/ContentBrowser/DirectoryIcon.png");
        m_Icons[AssetType::Unknown]   = Texture2D::Create("Resources/Icons/ContentBrowser/FileIcon.png");
        m_Icons[AssetType::Scene]     = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Texture]   = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Shader]    = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Audio]     = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Script]    = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Prefab]    = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Material]  = m_Icons[AssetType::Unknown];

        NavigateTo(GetEditorAssetPath());
    }

    AssetType ContentBrowserPanel::GetAssetType(const std::filesystem::path& path) const
    {
        if (std::filesystem::is_directory(path))
            return AssetType::Directory;

        const std::string ext = path.extension().string();
        if (ext == AssetFileType::SceneExtension)
            return AssetType::Scene;
        if (ext == AssetFileType::PrefabExtension)
            return AssetType::Prefab;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
            return AssetType::Texture;
        if (ext == ".glsl" || ext == ".hlsl")
            return AssetType::Shader;
        if (ext == ".mp3" || ext == ".wav" || ext == ".ogg")
            return AssetType::Audio;
        if (ext == ".lua" || ext == ".cs" || ext == ".vn")
            return AssetType::Script;
        if (ext == AssetFileType::MaterialExtension)
            return AssetType::Material;

        return AssetType::Unknown;
    }

    Ref<Texture2D> ContentBrowserPanel::GetIconForType(AssetType type) const
    {
        auto it = m_Icons.find(type);
        if (it != m_Icons.end())
            return it->second;
        return m_Icons.at(AssetType::Unknown);
    }

    std::vector<std::filesystem::directory_entry> ContentBrowserPanel::GetFilteredEntries() const
    {
        std::vector<std::filesystem::directory_entry> result;
        const std::string filter = m_SearchBuffer;

        if (!std::filesystem::exists(m_CurrentDirectory))
            return result;

        for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
        {
            const std::string name = entry.path().filename().string();
            if (filter.empty() || name.find(filter) != std::string::npos)
                result.push_back(entry);
        }

        std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b)
            {
                return a.is_directory() > b.is_directory();
            });

        return result;
    }

    void ContentBrowserPanel::NavigateTo(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
            return;

        if (m_HistoryIndex >= 0)
            m_History.erase(m_History.begin() + m_HistoryIndex + 1, m_History.end());

        m_History.push_back(path);
        m_HistoryIndex = static_cast<int>(m_History.size()) - 1;
        m_CurrentDirectory = path;
        m_SelectedPath.clear();
        std::memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
    }

    void ContentBrowserPanel::NavigateBack()
    {
        if (m_HistoryIndex > 0)
        {
            m_HistoryIndex--;
            m_CurrentDirectory = m_History[m_HistoryIndex];
            m_SelectedPath.clear();
        }
    }

    void ContentBrowserPanel::NavigateForward()
    {
        if (m_HistoryIndex < static_cast<int>(m_History.size()) - 1)
        {
            m_HistoryIndex++;
            m_CurrentDirectory = m_History[m_HistoryIndex];
            m_SelectedPath.clear();
        }
    }

    void ContentBrowserPanel::DrawToolbar()
    {
        const bool canBack    = m_HistoryIndex > 0;
        const bool canForward = m_HistoryIndex < static_cast<int>(m_History.size()) - 1;

        if (!canBack) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        if (ImGui::Button("< ##back") && canBack)
            NavigateBack();
        if (!canBack) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");

        ImGui::SameLine();

        if (!canForward) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        if (ImGui::Button("> ##fwd") && canForward)
            NavigateForward();
        if (!canForward) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward");

        ImGui::SameLine();

        const bool canUp = (m_CurrentDirectory != GetEditorAssetPath());
        if (!canUp) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        if (ImGui::Button("^ ##up") && canUp)
            NavigateTo(m_CurrentDirectory.parent_path());
        if (!canUp) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Up one level");

        ImGui::SameLine();

        auto relative = std::filesystem::relative(m_CurrentDirectory, GetEditorAssetPath().parent_path());
        std::filesystem::path accumulated;
        bool first = true;
        for (const auto& part : relative)
        {
            accumulated /= part;
            if (!first)
                ImGui::SameLine(0, 2);
            first = false;

            if (ImGui::SmallButton(part.string().c_str()))
                NavigateTo(GetEditorAssetPath().parent_path() / accumulated);

            ImGui::SameLine(0, 2);
            ImGui::TextDisabled("/");
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200.0f);
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputTextWithHint("##search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));

        ImGui::SameLine();
        if (ImGui::Button(m_ShowSidebar ? "<<" : ">>"))
            m_ShowSidebar = !m_ShowSidebar;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Folder Tree");

        ImGui::Separator();
    }

    void ContentBrowserPanel::DrawSidebar()
    {
        if (!m_ShowSidebar)
            return;

        ImGui::BeginChild("##sidebar", ImVec2(150, 0), true);

        std::function<void(const std::filesystem::path&, int)> drawTree;
        drawTree = [&](const std::filesystem::path& dir, int depth)
        {
            if (!std::filesystem::exists(dir))
                return;

            for (const auto& entry : std::filesystem::directory_iterator(dir))
            {
                if (!entry.is_directory())
                    continue;

                const std::string name = entry.path().filename().string();
                const bool isCurrent = (entry.path() == m_CurrentDirectory);

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (isCurrent)
                    flags |= ImGuiTreeNodeFlags_Selected;
                if (depth == 0)
                    flags |= ImGuiTreeNodeFlags_DefaultOpen;

                const bool open = ImGui::TreeNodeEx(entry.path().string().c_str(), flags, "%s", name.c_str());
                if (ImGui::IsItemClicked())
                    NavigateTo(entry.path());

                if (open)
                {
                    drawTree(entry.path(), depth + 1);
                    ImGui::TreePop();
                }
            }
        };

        drawTree(GetEditorAssetPath(), 0);
        ImGui::EndChild();
    }

    void ContentBrowserPanel::DrawFileGrid()
    {
        ImGui::BeginChild("##filegrid", ImVec2(0, 0), false);

        const float cellSize    = m_ThumbnailSize + m_Padding;
        const float panelWidth  = ImGui::GetContentRegionAvail().x;
        const int   columnCount = std::max(1, static_cast<int>(panelWidth / cellSize));

        if (ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
        {
            // Reserved for future multi-selection.
        }

        ImGui::Columns(columnCount, nullptr, false);

        const auto entries = GetFilteredEntries();
        for (const auto& entry : entries)
        {
            const auto& path = entry.path();
            const auto relativePath = std::filesystem::relative(path, GetEditorAssetPath());
            const std::string filename = relativePath.filename().string();
            const bool isSelected = (path == m_SelectedPath);

            ImGui::PushID(path.string().c_str());

            const AssetType type = GetAssetType(path);
            const Ref<Texture2D>& icon = GetIconForType(type);

            if (isSelected)
            {
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 size = ImVec2(m_ThumbnailSize + m_Padding, m_ThumbnailSize + m_Padding + 20.0f);
                ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(70, 130, 180, 80), 4.0f);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.12f));
            ImGui::ImageButton(
                "##AssetIcon",
                static_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                { m_ThumbnailSize, m_ThumbnailSize },
                { 0, 1 }, { 1, 0 }
            );
            ImGui::PopStyleColor(3);

            if (ImGui::BeginDragDropSource())
            {
                const std::wstring itemPath = relativePath.wstring();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), (itemPath.size() + 1) * sizeof(wchar_t));
                ImGui::Image(
                    static_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                    { 32, 32 }, { 0, 1 }, { 1, 0 }
                );
                ImGui::SameLine();
                ImGui::Text("%s", filename.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemClicked())
                m_SelectedPath = path;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (entry.is_directory())
                    NavigateTo(path);
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Show in Explorer"))
                {
                    // Intentionally left as editor integration point.
                }

                if (!entry.is_directory())
                {
                    ImGui::Separator();
                    if (ImGui::MenuItem("Rename"))
                    {
                    }
                    if (ImGui::MenuItem("Delete"))
                    {
                    }
                }
                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", path.string().c_str());

            ImGui::TextWrapped("%s", filename.c_str());

            ImGui::NextColumn();
            ImGui::PopID();
        }

        if (ImGui::BeginPopupContextWindow("##DirCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("New Folder"))
            {
            }
            if (ImGui::MenuItem("Refresh"))
            {
            }
            ImGui::EndPopup();
        }

        ImGui::Columns(1);
        ImGui::EndChild();
    }

    void ContentBrowserPanel::DrawInspector()
    {
        if (!m_ShowInspector)
            return;

        ImGui::BeginChild("##inspector", ImVec2(180, 0), true);
        ImGui::TextDisabled("INSPECTOR");
        ImGui::Separator();

        if (!m_SelectedPath.empty() && std::filesystem::exists(m_SelectedPath))
        {
            const AssetType type = GetAssetType(m_SelectedPath);
            const Ref<Texture2D>& icon = GetIconForType(type);

            ImGui::Image(
                static_cast<ImTextureID>(static_cast<uintptr_t>(icon->GetRendererID())),
                ImVec2(160, 160), { 0, 1 }, { 1, 0 }
            );

            ImGui::Spacing();

            const std::string name = m_SelectedPath.filename().string();
            const std::string ext = m_SelectedPath.extension().string();

            ImGui::TextDisabled("Name");
            ImGui::TextWrapped("%s", name.c_str());

            ImGui::Spacing();
            ImGui::TextDisabled("Type");

            const char* typeStr = "Unknown";
            switch (type)
            {
                case AssetType::Directory: typeStr = "Folder"; break;
                case AssetType::Scene:     typeStr = "Scene"; break;
                case AssetType::Texture:   typeStr = "Texture"; break;
                case AssetType::Shader:    typeStr = "Shader"; break;
                case AssetType::Audio:     typeStr = "Audio"; break;
                case AssetType::Script:    typeStr = "Script"; break;
                case AssetType::Prefab:    typeStr = "Prefab"; break;
                case AssetType::Material:  typeStr = "Material"; break;
                default: break;
            }
            ImGui::Text("%s", typeStr);

            if (!ext.empty())
            {
                ImGui::Spacing();
                ImGui::TextDisabled("Extension");
                ImGui::Text("%s", ext.c_str());
            }

            if (!std::filesystem::is_directory(m_SelectedPath))
            {
                ImGui::Spacing();
                ImGui::TextDisabled("Size");
                const auto bytes = std::filesystem::file_size(m_SelectedPath);
                if (bytes < 1024)
                    ImGui::Text("%llu B", static_cast<unsigned long long>(bytes));
                else if (bytes < 1024 * 1024)
                    ImGui::Text("%.1f KB", bytes / 1024.0f);
                else
                    ImGui::Text("%.1f MB", bytes / (1024.0f * 1024.0f));
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Modified");
            ImGui::TextDisabled("(see OS)");
        }
        else
        {
            ImGui::TextDisabled("Nothing selected");
        }

        ImGui::EndChild();
    }

    void ContentBrowserPanel::DrawStatusBar()
    {
        ImGui::Separator();

        ImGui::SetNextItemWidth(80.0f);
        ImGui::SliderFloat("##thumb", &m_ThumbnailSize, 32.0f, 128.0f, "%.0f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Thumbnail size");
        ImGui::SameLine();
        ImGui::TextDisabled("size");

        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("%zu items", GetFilteredEntries().size());

        if (!m_SelectedPath.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("|  %s", m_SelectedPath.filename().string().c_str());
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200.0f);
        if (ImGui::SmallButton("i"))
            m_ShowInspector = !m_ShowInspector;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle Inspector");
    }

    void ContentBrowserPanel::OnImGuiRender()
    {
        ImGui::Begin("Content Browser");

        DrawToolbar();

        if (m_ShowSidebar)
        {
            DrawSidebar();
            ImGui::SameLine();
        }

        if (m_ShowInspector)
        {
            const float inspectorWidth = 190.0f;
            ImGui::BeginChild("##gridwrap", ImVec2(ImGui::GetContentRegionAvail().x - inspectorWidth, 0), false);
            DrawFileGrid();
            ImGui::EndChild();

            ImGui::SameLine();
            DrawInspector();
        }
        else
        {
            DrawFileGrid();
        }

        DrawStatusBar();

        ImGui::End();
    }

} // namespace Wheatear
