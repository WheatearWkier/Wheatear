#include "wepch.h"
#include "Wheatear/Utils/StringUtils.h"
#include "ContentBrowserPanel.h"

#include "Assets/AssetRegistry.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorPlatform.h"
#include "Editor/EditorWidgets.h"
#include "Panels/EventScriptGraphPanel.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/EngineInfo.h"
#include "Wheatear/Renderer/Framebuffer.h"
#include "Wheatear/Renderer/Mesh.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Renderer3D.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneSerializer.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace Wheatear {

    namespace {


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
                case AssetType::Mesh:          return "Mesh";
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
        m_Icons[AssetType::Directory] = Texture2D::Create("Resources/Icons/Editor/folder.png");
        m_Icons[AssetType::Unknown]   = Texture2D::Create("Resources/Icons/Editor/file_text.png");
        // Per-type Lucide icons so the grid reads at a glance.
        m_Icons[AssetType::Scene]     = Texture2D::Create("Resources/Icons/Editor/open_scene.png");
        m_Icons[AssetType::Texture]   = Texture2D::Create("Resources/Icons/Editor/sprite_sheet.png");
        m_Icons[AssetType::Shader]    = Texture2D::Create("Resources/Icons/Editor/code.png");
        m_Icons[AssetType::Audio]     = Texture2D::Create("Resources/Icons/Editor/audio.png");
        m_Icons[AssetType::Script]    = Texture2D::Create("Resources/Icons/Editor/script.png");
        m_Icons[AssetType::Prefab]    = Texture2D::Create("Resources/Icons/Editor/box.png");
        m_Icons[AssetType::UITemplate]= Texture2D::Create("Resources/Icons/Editor/template.png");
        m_Icons[AssetType::Material]  = Texture2D::Create("Resources/Icons/Editor/palette.png");
        m_Icons[AssetType::Data]      = Texture2D::Create("Resources/Icons/Editor/file_text.png");
        m_Icons[AssetType::Metadata]  = m_Icons[AssetType::Unknown];
        m_Icons[AssetType::AnimationClip] = Texture2D::Create("Resources/Icons/Editor/film.png");
        m_Icons[AssetType::Mesh] = Texture2D::Create("Resources/Icons/Editor/box.png");

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
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
            return AssetType::Texture;
        if (ext == ".obj" || ext == ".fbx")
            return AssetType::Mesh;
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
        const std::string filter = StringUtils::ToLower(m_SearchBuffer);

        if (!std::filesystem::exists(m_CurrentDirectory))
            return result;

        for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
        {
            const std::string name = entry.path().filename().string();
            const std::string searchable = StringUtils::ToLower(name);
            if (entry.path().extension() == AssetFileType::MetadataExtension)
                continue;
            if (entry.is_directory() && entry.path().filename() == ".wheatear")
                continue;

            if (filter.empty() || searchable.find(filter) != std::string::npos)
                result.push_back(entry);
        }

        // Folders first, then case-insensitive name order.
        std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b)
            {
                if (a.is_directory() != b.is_directory())
                    return a.is_directory() > b.is_directory();
                return StringUtils::ToLower(a.path().filename().string())
                    < StringUtils::ToLower(b.path().filename().string());
            });

        return result;
    }

    void ContentBrowserPanel::OpenEntry(const std::filesystem::path& path)
    {
        if (std::filesystem::is_directory(path))
        {
            NavigateTo(path);
            return;
        }

        const std::string ext = path.extension().string();
        const std::string relative = AssetPath::ToProjectRelative(path).generic_string();
        if (ext == ".vn")
            VisualNovelEditorRequests::RequestOpenScript(relative);
        else if (ext == ".wts")
            EventScriptGraphRequests::RequestOpenScript(relative);
        else if (ext == AssetFileType::SceneExtension)
        {
            if (m_OnOpenScene) m_OnOpenScene(path);
        }
        else if (ext == AssetFileType::PrefabExtension)
        {
            if (m_OnInstantiatePrefab) m_OnInstantiatePrefab(path);
        }
        else if (ext == AssetFileType::UITemplateExtension)
        {
            if (m_OnInstantiateUITemplate) m_OnInstantiateUITemplate(path);
        }
    }

    void ContentBrowserPanel::CommitRename(const std::filesystem::path& oldPath, const char* newName)
    {
        m_RenameTarget.clear();

        std::string name = StringUtils::Trim(newName);
        if (name.empty() || name == oldPath.filename().string())
            return;

        std::filesystem::path newPath = oldPath.parent_path() / name;
        std::error_code error;
        std::filesystem::rename(oldPath, newPath, error);
        if (error)
        {
            m_RegistryStatus = "Rename failed: " + error.message();
        }
        else
        {
            m_SelectedPath = newPath;
            m_RegistryStatus = "Renamed to: " + name;
        }
    }

    void ContentBrowserPanel::OnUpdate()
    {
        if (m_ThumbnailRenderQueue.empty())
            return;

        const std::string key = m_ThumbnailRenderQueue.front();
        m_ThumbnailRenderQueue.erase(m_ThumbnailRenderQueue.begin());

        const auto typeIt = m_ThumbnailQueueTypes.find(key);
        const AssetType type = (typeIt != m_ThumbnailQueueTypes.end())
            ? typeIt->second : AssetType::Unknown;

        if (!RenderThumbnail(key, type))
            m_ThumbnailCache[key] = nullptr;   // failed: don't retry this session
    }

    bool ContentBrowserPanel::RenderThumbnail(const std::string& key, AssetType type)
    {
        constexpr uint32_t thumbSize = 128;

        if (!m_ThumbnailFramebuffer)
        {
            FramebufferSpecification spec;
            spec.Attachments = {
                FramebufferTextureFormat::RGBA8,
                FramebufferTextureFormat::Depth
            };
            spec.Width = thumbSize;
            spec.Height = thumbSize;
            m_ThumbnailFramebuffer = Framebuffer::Create(spec);
        }

        const std::filesystem::path path(key);
        Ref<Scene> scene = CreateRef<Scene>();

        if (type == AssetType::Scene)
        {
            SceneSerializer serializer(scene);
            if (!serializer.DeserializeYaml(path))
                return false;
        }
        else if (type == AssetType::Prefab)
        {
            if (!SceneSerializer::DeserializePrefab(path, scene.get()))
                return false;
        }
        else if (type == AssetType::UITemplate)
        {
            Entity canvas = scene->CreateEntity("ThumbCanvas");
            canvas.AddComponent<UICanvasComponent>();
            canvas.AddComponent<UIWidgetComponent>();
            UITemplateFactory::CreateFromAsset(scene.get(), path, canvas.GetUUID());
        }
        else if (type == AssetType::Mesh)
        {
            Entity meshEntity = scene->CreateEntity("ThumbMesh");
            auto& meshComponent = meshEntity.AddComponent<MeshRendererComponent>();
            meshComponent.Mesh = Mesh::Create(path.string());
            if (!meshComponent.Mesh)
                return false;
            meshComponent.Material = Material::Create();
        }
        else
        {
            return false;
        }

        scene->OnEditorStart();
        scene->OnViewportResize(thumbSize, thumbSize);

        // Use the scene's primary camera, or place a default one looking at
        // the origin from +Z so prefabs/meshes without cameras preview well.
        Entity cameraEntity = scene->GetPrimaryCameraEntity();
        if (!cameraEntity)
        {
            cameraEntity = scene->CreateEntity("ThumbCam");
            cameraEntity.AddComponent<CameraComponent>();
            cameraEntity.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, 10.0f };
        }

        const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        const auto& transform = cameraEntity.GetComponent<TransformComponent>().GetTransform();
        const bool includeUI = (type == AssetType::UITemplate);

        const uint32_t previousFramebuffer = Framebuffer::GetBoundFramebufferID();

        m_ThumbnailFramebuffer->Bind();
        RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.10f, 1.0f });
        RenderCommand::Clear();
        scene->RenderWithSceneCamera(camera, transform, includeUI);

        std::vector<uint8_t> pixels(thumbSize * thumbSize * 4);
        m_ThumbnailFramebuffer->ReadPixelsRGBA(pixels.data());
        m_ThumbnailFramebuffer->Unbind();

        Framebuffer::BindFramebufferID(previousFramebuffer);

        scene->OnEditorStop();

        Ref<Texture2D> thumbnail = Texture2D::Create(thumbSize, thumbSize);
        thumbnail->SetData(pixels.data(), pixels.size());
        m_ThumbnailCache[key] = thumbnail;
        return true;
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
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(EditorLocale::Text("Back", "后退"));

        ImGui::SameLine();

        if (!canForward) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        if (ImGui::Button("> ##fwd") && canForward)
            NavigateForward();
        if (!canForward) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(EditorLocale::Text("Forward", "前进"));

        ImGui::SameLine();

        const bool canUp = (m_CurrentDirectory != GetEditorAssetPath());
        if (!canUp) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        if (ImGui::Button("^ ##up") && canUp)
            NavigateTo(m_CurrentDirectory.parent_path());
        if (!canUp) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(EditorLocale::Text("Up one level", "上一级"));

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
        if (ImGui::Button(EditorLocale::Text("Rescan", "重扫")))
        {
            AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
            AssetRegistry::Get().WriteRegistry();
            m_RegistryStatus = "Asset registry rescanned and written.";
        }

        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("Write Registry", "写入注册表")))
        {
            const bool saved = AssetRegistry::Get().WriteRegistry();
            m_RegistryStatus = saved ? "asset_registry.yaml written." : "Failed to write asset_registry.yaml.";
        }

        ImGui::SameLine();
        if (ImGui::Button(EditorLocale::Text("UI Templates", "UI 模板")))
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

        // Keyboard navigation for the file grid (guarded while typing).
        if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !m_SelectedPath.empty())
                OpenEntry(m_SelectedPath);
            else if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
                NavigateBack();
            else if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !m_SelectedPath.empty())
                m_ConfirmDeletePath = m_SelectedPath;
            else if (ImGui::IsKeyPressed(ImGuiKey_F2) && !m_SelectedPath.empty())
            {
                m_RenameTarget = m_SelectedPath;
                std::strncpy(m_RenameBuffer,
                    m_SelectedPath.filename().string().c_str(),
                    sizeof(m_RenameBuffer) - 1);
            }
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

            // Real thumbnail preview: image assets load their texture directly;
            // scenes/prefabs/UI templates/meshes are rendered offscreen through
            // a frame-by-frame queue (see OnUpdate/RenderThumbnail).
            Ref<Texture2D> displayIcon = icon;
            const bool needsRenderThumbnail =
                type == AssetType::Scene || type == AssetType::Prefab
                || type == AssetType::UITemplate || type == AssetType::Mesh;
            if (type == AssetType::Texture || needsRenderThumbnail)
            {
                const std::string thumbKey = path.string();
                auto thumbIt = m_ThumbnailCache.find(thumbKey);
                if (thumbIt == m_ThumbnailCache.end())
                {
                    if (type == AssetType::Texture)
                    {
                        Ref<Texture2D> loaded = Texture2D::Create(thumbKey);
                        m_ThumbnailCache[thumbKey] = (loaded && loaded->IsLoaded()) ? loaded : nullptr;
                        thumbIt = m_ThumbnailCache.find(thumbKey);
                    }
                    else if (m_ThumbnailQueued.insert(thumbKey).second)
                    {
                        m_ThumbnailQueueTypes[thumbKey] = type;
                        m_ThumbnailRenderQueue.push_back(thumbKey);
                    }
                }
                if (thumbIt != m_ThumbnailCache.end() && thumbIt->second)
                    displayIcon = thumbIt->second;
            }

            if (isSelected)
            {
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 size = ImVec2(m_ThumbnailSize + m_Padding, m_ThumbnailSize + m_Padding + 20.0f);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(59, 184, 204, 34), 6.0f);
                drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(59, 184, 204, 210), 6.0f, 0, 1.5f);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.72f, 0.80f, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.72f, 0.80f, 0.20f));
            ImGui::ImageButton(
                "##AssetIcon",
                static_cast<ImTextureID>(static_cast<uintptr_t>(displayIcon->GetRendererID())),
                { m_ThumbnailSize, m_ThumbnailSize },
                { 0, 1 }, { 1, 0 }
            );
            ImGui::PopStyleColor(3);

            if (ImGui::BeginDragDropSource())
            {
                const std::wstring itemPath = relativePath.wstring();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath.c_str(), (itemPath.size() + 1) * sizeof(wchar_t));
                ImGui::Image(
                    static_cast<ImTextureID>(static_cast<uintptr_t>(displayIcon->GetRendererID())),
                    { 32, 32 }, { 0, 1 }, { 1, 0 }
                );
                ImGui::SameLine();
                ImGui::Text("%s", filename.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemClicked())
                m_SelectedPath = path;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                OpenEntry(path);

            // Rich hover tooltip: name, type, size, full path.
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(filename.c_str());
                ImGui::Separator();
                ImGui::TextDisabled("%s", AssetTypeLabel(type));
                if (!entry.is_directory())
                {
                    std::error_code sizeError;
                    const uintmax_t bytes = std::filesystem::file_size(path, sizeError);
                    if (!sizeError)
                    {
                        if (bytes >= 1024 * 1024)
                            ImGui::TextDisabled("%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
                        else
                            ImGui::TextDisabled("%.1f KB", static_cast<double>(bytes) / 1024.0);
                    }
                }
                ImGui::TextDisabled("%s", path.string().c_str());
                ImGui::EndTooltip();
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem(EditorLocale::Text("Open", "打开")))
                    OpenEntry(path);
                if (ImGui::MenuItem(EditorLocale::Text("Show in Explorer", "在资源管理器中显示")))
                {
                    EditorPlatform::OpenDirectory(entry.is_directory() ? path : path.parent_path());
                }
                if (ImGui::MenuItem(EditorLocale::Text("Copy Path", "复制路径")))
                {
                    ImGui::SetClipboardText(path.string().c_str());
                }
                ImGui::Separator();
                if (ImGui::MenuItem(EditorLocale::Text("Rename", "重命名"), "F2"))
                {
                    m_RenameTarget = path;
                    std::strncpy(m_RenameBuffer, filename.c_str(), sizeof(m_RenameBuffer) - 1);
                }
                if (ImGui::MenuItem(EditorLocale::Text("Delete", "删除"), "Del"))
                    m_ConfirmDeletePath = path;
                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", path.string().c_str());

            // Inline rename field replaces the filename while renaming.
            if (path == m_RenameTarget)
            {
                ImGui::SetNextItemWidth(m_ThumbnailSize);
                ImGui::SetKeyboardFocusHere();
                const bool committed = ImGui::InputText("##rename", m_RenameBuffer,
                    sizeof(m_RenameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                const bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape);
                const bool deactivated = ImGui::IsItemDeactivated();
                if (committed)
                    CommitRename(m_RenameTarget, m_RenameBuffer);
                if (cancelled || (deactivated && !committed))
                    m_RenameTarget.clear();
            }
            else
            {
                // Single-line filename clipped to the cell width with an
                // ellipsis, centered under the thumbnail.
                const float nameMaxWidth = m_ThumbnailSize + m_Padding - 4.0f;
                std::string shortName = filename;
                if (ImGui::CalcTextSize(shortName.c_str()).x > nameMaxWidth)
                {
                    while (!shortName.empty()
                        && ImGui::CalcTextSize((shortName + "...").c_str()).x > nameMaxWidth)
                        shortName.pop_back();
                    shortName += "...";
                }
                const float nameTextWidth = ImGui::CalcTextSize(shortName.c_str()).x;
                ImGui::SetCursorPosX(
                    ImGui::GetCursorPosX() + std::max(0.0f, (m_ThumbnailSize + m_Padding - nameTextWidth) * 0.5f));
                ImGui::TextUnformatted(shortName.c_str());
            }

            ImGui::NextColumn();
            ImGui::PopID();
        }

        if (ImGui::BeginPopupContextWindow("##DirCtx", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem(EditorLocale::Text("New Folder", "新建文件夹")))
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
            if (ImGui::MenuItem(EditorLocale::Text("New Event Script (.wts)", "新建事件脚本 (.wts)")))
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

            ImGui::TextDisabled(EditorLocale::Text("Name", "名称"));
            ImGui::TextWrapped("%s", name.c_str());

            ImGui::Spacing();
            ImGui::TextDisabled(EditorLocale::Text("Type", "类型"));

            ImGui::Text("%s", AssetTypeLabel(type));

            const std::filesystem::path relativePath = AssetPath::ToProjectRelative(m_SelectedPath);
            EditorAssetMetadata* metadata = AssetRegistry::Get().FindMutableByPath(relativePath);
            if (metadata)
            {
                ImGui::Spacing();
                ImGui::TextDisabled(EditorLocale::Text("Asset UUID", "资源 UUID"));
                ImGui::TextWrapped("%llu", static_cast<unsigned long long>(static_cast<uint64_t>(metadata->ID)));

                ImGui::Spacing();
                ImGui::TextDisabled(EditorLocale::Text("Registry Type", "注册表类型"));
                ImGui::Text("%s", AssetRegistry::KindToString(metadata->Kind).c_str());

                bool changed = false;
                if (metadata->Kind == EditorAssetKind::Texture || metadata->Kind == EditorAssetKind::SpriteSheet)
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Texture Import");
                    const char* filters[] = { "Linear", "Nearest" };
                    int filterIndex = metadata->Texture.Filter == "Nearest" ? 1 : 0;
                    if (ImGui::Combo(EditorLocale::Text("Filter", "过滤"), &filterIndex, filters, IM_ARRAYSIZE(filters)))
                    {
                        metadata->Texture.Filter = filters[filterIndex];
                        changed = true;
                    }
                    changed |= ImGui::DragFloat(EditorLocale::Text("PPU", "PPU"), &metadata->Texture.PixelsPerUnit, 1.0f, 1.0f, 1000.0f, "%.0f");
                    changed |= ImGui::InputInt(EditorLocale::Text("Columns", "列数"), &metadata->Texture.Columns);
                    changed |= ImGui::InputInt(EditorLocale::Text("Rows", "行数"), &metadata->Texture.Rows);
                    changed |= ImGui::InputInt(EditorLocale::Text("Cell W", "单元格宽"), &metadata->Texture.CellWidth);
                    changed |= ImGui::InputInt(EditorLocale::Text("Cell H", "单元格高"), &metadata->Texture.CellHeight);
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
                    if (ImGui::Combo(EditorLocale::Text("Usage", "用途"), &usageIndex, usages, IM_ARRAYSIZE(usages)))
                    {
                        metadata->Audio.Usage = usages[usageIndex];
                        changed = true;
                    }
                    changed |= ImGui::SliderFloat(EditorLocale::Text("Default Volume", "默认音量"), &metadata->Audio.DefaultVolume, 0.0f, 1.0f, "%.2f");
                    changed |= ImGui::Checkbox(EditorLocale::Text("Loop", "循环"), &metadata->Audio.Loop);
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
                if (ImGui::Button(EditorLocale::Text("Copy UUID", "复制 UUID")))
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
                ImGui::TextDisabled(EditorLocale::Text("Extension", "扩展名"));
                ImGui::Text("%s", ext.c_str());
            }

            if (!std::filesystem::is_directory(m_SelectedPath))
            {
                ImGui::Spacing();
                ImGui::TextDisabled(EditorLocale::Text("Size", "大小"));
                const auto bytes = std::filesystem::file_size(m_SelectedPath);
                if (bytes < 1024)
                    ImGui::Text("%llu B", static_cast<unsigned long long>(bytes));
                else if (bytes < 1024 * 1024)
                    ImGui::Text("%.1f KB", bytes / 1024.0f);
                else
                    ImGui::Text("%.1f MB", bytes / (1024.0f * 1024.0f));
            }

            ImGui::Spacing();
            ImGui::TextDisabled(EditorLocale::Text("Modified", "修改时间"));
            ImGui::TextDisabled(EditorLocale::Text("(see OS)", "(见操作系统)"));
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
            ImGui::SetTooltip("%s", EditorLocale::Text("Thumbnail size", "缩略图大小"));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", EditorLocale::Text("size", "大小"));

        ImGui::SameLine(0, 20);
        const std::string countLabel = std::to_string(GetFilteredEntries().size())
            + EditorLocale::Text(" item(s)", " 项");
        EditorWidgets::StatusBadge(countLabel.c_str(), EditorWidgets::StatusKind::Neutral);

        if (!m_SelectedPath.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("|  %s", m_SelectedPath.filename().string().c_str());
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("i"))
            m_ShowInspector = !m_ShowInspector;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", EditorLocale::Text("Toggle Inspector", "切换检查器"));
    }

    void ContentBrowserPanel::OnImGuiRender()
    {
        if (m_DrawerMode)
        {
            // Fold the docked window to a tab bar and show the floating
            // drawer sliding up from the bottom (UE-style content drawer).
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_Always);
            ImGui::Begin("Content Browser");
            ImGui::End();

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float drawerHeight = 320.0f;
            ImGui::SetNextWindowPos(
                ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - drawerHeight),
                ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, drawerHeight), ImGuiCond_Always);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::Begin("##ContentDrawer", nullptr,
                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
            DrawPanelContent();
            ImGui::End();
            ImGui::PopStyleVar(2);

            // Clicking outside the drawer dismisses it.
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
                m_DrawerMode = false;
            return;
        }

        ImGui::Begin(EditorLocale::Text("Content Browser", "资源浏览器"));
        DrawPanelContent();
        ImGui::End();
    }

    void ContentBrowserPanel::DrawPanelContent()
    {
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

        // Confirmation before deleting an asset from disk.
        if (!m_ConfirmDeletePath.empty())
        {
            ImGui::OpenPopup(EditorLocale::Text("Delete Asset", "删除资源"));
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal(EditorLocale::Text("Delete Asset", "删除资源"),
                    nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("%s", EditorLocale::Text(
                    "Delete '%s' from disk? This cannot be undone.",
                    "确定从磁盘删除 '%s' 吗？此操作无法撤销。"),
                    m_ConfirmDeletePath.filename().string().c_str());
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::Button(EditorLocale::Text("Delete", "删除"), ImVec2(120.0f, 0.0f)))
                {
                    std::error_code error;
                    if (std::filesystem::is_directory(m_ConfirmDeletePath))
                        std::filesystem::remove_all(m_ConfirmDeletePath, error);
                    else
                        std::filesystem::remove(m_ConfirmDeletePath, error);
                    if (error)
                        m_RegistryStatus = "Delete failed: " + error.message();
                    else
                        m_RegistryStatus = "Deleted: " + m_ConfirmDeletePath.filename().string();
                    if (m_SelectedPath == m_ConfirmDeletePath)
                        m_SelectedPath.clear();
                    m_ConfirmDeletePath.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Cancel", "取消"), ImVec2(100.0f, 0.0f)))
                    m_ConfirmDeletePath.clear();
                ImGui::EndPopup();
            }
        }
    }

} // namespace Wheatear
