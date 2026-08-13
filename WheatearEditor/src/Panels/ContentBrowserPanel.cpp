#include "wtpch.h"
#include "ContentBrowserPanel.h"

#include "Assets/AssetRegistry.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/EditorPlatform.h"
#include "Editor/EditorWidgets.h"
#include "Editor/EventScriptGraphPanel.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace Wheatear {

    namespace {

        static std::string ToLowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        static const char* AssetTypeLabel(AssetType type)
        {
            switch (type)
            {
                case AssetType::Directory:  return "Folder";
                case AssetType::Scene:      return "Scene";
                case AssetType::Texture:    return "Texture";
                case AssetType::Shader:     return "Shader";
                case AssetType::Audio:      return "Audio";
                case AssetType::Script:     return "Script";
                case AssetType::Prefab:     return "Prefab";
                case AssetType::UITemplate: return "UI Template";
                case AssetType::Material:   return "Material";
                case AssetType::Data:       return "Data";
                case AssetType::Metadata:   return "Metadata";
                case AssetType::AnimationClip: return "Animation Clip";
                default:                    return "Unknown";
            }
        }

    } // namespace

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
        m_Icons[AssetType::UITemplate]= m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Material]  = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Data]      = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::Metadata]  = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::AnimationClip] = m_Icons[AssetType::Unknown];

        AssetRegistry::Get().LoadCache(AssetPath::GetProjectRoot());
        m_RegistryStatus = "Loaded asset registry cache. Use Rescan Assets after adding or replacing resources.";
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
        if (ext == AssetFileType::UITemplateExtension)
            return AssetType::UITemplate;
        if (ext == AssetFileType::MetadataExtension)
            return AssetType::Metadata;
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
        if (ext == AssetFileType::AnimationClipExtension)
            return AssetType::AnimationClip;
        if (ext == ".yaml" || ext == ".yml" || ext == ".json" || ext == ".txt")
            return AssetType::Data;

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
        const std::string filter = ToLowerCopy(m_SearchBuffer);

        if (!std::filesystem::exists(m_CurrentDirectory))
            return result;

        for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
        {
            const std::string name = entry.path().filename().string();
            const std::string searchable = ToLowerCopy(name);
            if (entry.path().extension() == AssetFileType::MetadataExtension)
                continue;
            if (entry.is_directory() && entry.path().filename() == ".wheatear")
                continue;

            if (filter.empty() || searchable.find(filter) != std::string::npos)
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
        const auto entries = GetFilteredEntries();

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

            const std::string breadcrumbLabel = EditorWidgets::LabelWithId(
                part.string(),
                "breadcrumb:" + accumulated.generic_string());
            if (ImGui::SmallButton(breadcrumbLabel.c_str()))
                NavigateTo(GetEditorAssetPath().parent_path() / accumulated);

            ImGui::SameLine(0, 2);
            ImGui::TextDisabled("/");
        }

        ImGui::Spacing();
        EditorWidgets::StatusBadge((std::to_string(entries.size()) + " visible item(s)").c_str(), EditorWidgets::StatusKind::Info);
        if (m_SearchBuffer[0] != '\0')
        {
            ImGui::SameLine();
            EditorWidgets::StatusBadge("Filtered", EditorWidgets::StatusKind::Warning);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        EditorWidgets::SearchBar("##search", m_SearchBuffer, sizeof(m_SearchBuffer), "Search current folder...");

        ImGui::SameLine();
        if (ImGui::Button("Rescan"))
        {
            AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
            AssetRegistry::Get().WriteRegistry();
            m_RegistryStatus = "Asset registry rescanned and written.";
        }

        ImGui::SameLine();
        if (ImGui::Button("Write Registry"))
        {
            const bool saved = AssetRegistry::Get().WriteRegistry();
            m_RegistryStatus = saved ? "asset_registry.yaml written." : "Failed to write asset_registry.yaml.";
        }

        ImGui::SameLine();
        if (ImGui::Button("UI Templates"))
        {
            UITemplateFactory::WriteBuiltinTemplateAssets(AssetPath::GetProjectRoot());
            AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
            AssetRegistry::Get().WriteRegistry();
            m_RegistryStatus = "Builtin .wtuit UI template assets generated.";
        }

        ImGui::SameLine();
        if (ImGui::Button(m_ShowSidebar ? "<<" : ">>"))
            m_ShowSidebar = !m_ShowSidebar;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Folder Tree");

        if (!m_RegistryStatus.empty())
            EditorWidgets::InlineStatus(m_RegistryStatus,
                m_RegistryStatus.find("Failed") != std::string::npos ? EditorWidgets::StatusKind::Error : EditorWidgets::StatusKind::Info);

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

        const auto entries = GetFilteredEntries();
        if (entries.empty())
        {
            EditorWidgets::EmptyState("No assets found.",
                m_SearchBuffer[0] == '\0' ? "This folder is empty." : "No asset matches the current search filter.");
            ImGui::EndChild();
            return;
        }

        const float cellSize    = m_ThumbnailSize + m_Padding;
        const float panelWidth  = ImGui::GetContentRegionAvail().x;
        const int   columnCount = std::max(1, static_cast<int>(panelWidth / cellSize));

        if (ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
        {
            // Reserved for future multi-selection.
        }

        ImGui::Columns(columnCount, nullptr, false);

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
                    EditorPlatform::OpenDirectory(entry.is_directory() ? path : path.parent_path());
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
                std::filesystem::path candidate = m_CurrentDirectory / "New Folder";
                for (int i = 2; std::filesystem::exists(candidate); ++i)
                    candidate = m_CurrentDirectory / ("New Folder " + std::to_string(i));

                std::error_code error;
                if (std::filesystem::create_directory(candidate, error))
                {
                    m_SelectedPath = candidate;
                    m_RegistryStatus = "Created folder: " + candidate.filename().string();
                }
                else
                {
                    m_RegistryStatus = "Failed to create folder.";
                }
            }
            if (ImGui::MenuItem("New Event Script (.wts)"))
            {
                std::filesystem::path candidate = m_CurrentDirectory / "new_event.wts";
                for (int i = 2; std::filesystem::exists(candidate); ++i)
                    candidate = m_CurrentDirectory / ("new_event_" + std::to_string(i) + ".wts");

                if (EditorWidgets::WriteFileText(candidate, "# Generated by Wheatear Event Script Editor.\n"))
                {
                    m_SelectedPath = candidate;
                    m_RegistryStatus = "Created event script: " + candidate.filename().string();
                    // Open it in the Event Script Editor right away so the
                    // designer never has to type the path manually.
                    EventScriptGraphRequests::RequestOpenScript(
                        AssetPath::ToProjectRelative(candidate).generic_string());
                }
                else
                {
                    m_RegistryStatus = "Failed to create event script.";
                }
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
        EditorWidgets::SectionHeader("Inspector", "Selected asset metadata and import settings.");

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

            ImGui::Text("%s", AssetTypeLabel(type));

            const std::filesystem::path relativePath = AssetPath::ToProjectRelative(m_SelectedPath);
            EditorAssetMetadata* metadata = AssetRegistry::Get().FindMutableByPath(relativePath);
            if (metadata)
            {
                ImGui::Spacing();
                ImGui::TextDisabled("Asset UUID");
                ImGui::TextWrapped("%llu", static_cast<unsigned long long>(static_cast<uint64_t>(metadata->ID)));

                ImGui::Spacing();
                ImGui::TextDisabled("Registry Type");
                ImGui::Text("%s", AssetRegistry::KindToString(metadata->Kind).c_str());

                bool changed = false;
                if (metadata->Kind == EditorAssetKind::Texture || metadata->Kind == EditorAssetKind::SpriteSheet)
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Texture Import");
                    const char* filters[] = { "Linear", "Nearest" };
                    int filterIndex = metadata->Texture.Filter == "Nearest" ? 1 : 0;
                    if (ImGui::Combo("Filter", &filterIndex, filters, IM_ARRAYSIZE(filters)))
                    {
                        metadata->Texture.Filter = filters[filterIndex];
                        changed = true;
                    }
                    changed |= ImGui::DragFloat("PPU", &metadata->Texture.PixelsPerUnit, 1.0f, 1.0f, 1000.0f, "%.0f");
                    changed |= ImGui::InputInt("Columns", &metadata->Texture.Columns);
                    changed |= ImGui::InputInt("Rows", &metadata->Texture.Rows);
                    changed |= ImGui::InputInt("Cell W", &metadata->Texture.CellWidth);
                    changed |= ImGui::InputInt("Cell H", &metadata->Texture.CellHeight);
                    metadata->Texture.Columns = std::max(metadata->Texture.Columns, 1);
                    metadata->Texture.Rows = std::max(metadata->Texture.Rows, 1);
                    metadata->Texture.CellWidth = std::max(metadata->Texture.CellWidth, 0);
                    metadata->Texture.CellHeight = std::max(metadata->Texture.CellHeight, 0);
                }
                else if (metadata->Kind == EditorAssetKind::Audio)
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Audio Import");
                    const char* usages[] = { "SFX", "BGM", "UI", "Voice" };
                    int usageIndex = 0;
                    for (int i = 0; i < IM_ARRAYSIZE(usages); ++i)
                    {
                        if (metadata->Audio.Usage == usages[i])
                            usageIndex = i;
                    }
                    if (ImGui::Combo("Usage", &usageIndex, usages, IM_ARRAYSIZE(usages)))
                    {
                        metadata->Audio.Usage = usages[usageIndex];
                        changed = true;
                    }
                    changed |= ImGui::SliderFloat("Default Volume", &metadata->Audio.DefaultVolume, 0.0f, 1.0f, "%.2f");
                    changed |= ImGui::Checkbox("Loop", &metadata->Audio.Loop);
                }
                else if (metadata->Kind == EditorAssetKind::Prefab || metadata->Kind == EditorAssetKind::UITemplate)
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Template / Prefab");
                    changed |= EditorWidgets::InputString("Category", metadata->Prefab.Category, 128);
                    changed |= EditorWidgets::InputString("Template Kind", metadata->Prefab.TemplateKind, 128);
                    changed |= EditorWidgets::InputMultilineString("Description",
                        metadata->Prefab.Description,
                        ImVec2(-1.0f, 64.0f),
                        512);
                }

                if (changed)
                    metadata->Dirty = true;

                ImGui::Spacing();
                if (EditorWidgets::DirtySaveBar(metadata->Dirty, m_RegistryStatus, "Save Registry", nullptr, nullptr))
                {
                    const bool saved = AssetRegistry::Get().WriteRegistry();
                    if (saved)
                    {
                        metadata->Dirty = false;
                        m_RegistryStatus = "asset_registry.yaml saved.";
                    }
                    else
                    {
                        m_RegistryStatus = "Failed to save asset_registry.yaml.";
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Copy UUID"))
                    ImGui::SetClipboardText(std::to_string(static_cast<uint64_t>(metadata->ID)).c_str());

                ImGui::Spacing();
                EditorWidgets::SectionHeader("References");
                if (metadata->References.empty())
                    EditorWidgets::EmptyState("No outgoing references.", "This asset does not declare dependencies in the registry.");
                else
                {
                    ImGui::BeginChild("##refs", ImVec2(0, 70), true);
                    for (const std::string& reference : metadata->References)
                        ImGui::BulletText("%s", reference.c_str());
                    ImGui::EndChild();
                }

                EditorWidgets::SectionHeader("Referenced By");
                if (metadata->ReferencedBy.empty())
                    EditorWidgets::EmptyState("No incoming references.", "No registered asset currently depends on this item.");
                else
                {
                    ImGui::BeginChild("##refby", ImVec2(0, 70), true);
                    for (const std::string& reference : metadata->ReferencedBy)
                        ImGui::BulletText("%s", reference.c_str());
                    ImGui::EndChild();
                }
            }
            else if (!std::filesystem::is_directory(m_SelectedPath))
            {
                ImGui::Spacing();
                EditorWidgets::InlineStatus("This asset is not in the registry yet.", EditorWidgets::StatusKind::Warning);
                if (ImGui::Button("Rescan Registry"))
                {
                    AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
                    m_RegistryStatus = "Asset registry rescanned.";
                }
            }

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
            EditorWidgets::EmptyState("Nothing selected.", "Select an asset to inspect metadata, import settings, and registry references.");
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
        EditorWidgets::StatusBadge((std::to_string(GetFilteredEntries().size()) + " item(s)").c_str(), EditorWidgets::StatusKind::Neutral);

        if (!m_SelectedPath.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("|  %s", m_SelectedPath.filename().string().c_str());
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("i"))
            m_ShowInspector = !m_ShowInspector;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle Inspector");
    }

    void ContentBrowserPanel::OnImGuiRender()
    {
        ImGui::Begin("Content Browser");

        EditorWidgets::PanelHeader("Content Browser", "Project asset workspace rooted at WheatearEditor/assets.");
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
