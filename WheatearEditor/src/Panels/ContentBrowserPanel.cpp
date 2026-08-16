#include "wepch.h"
#include "Wheatear/Utils/StringUtils.h"
#include "ContentBrowserPanel.h"
#include "ContentBrowserRequests.h"

#include "Assets/AssetRegistry.h"
#include "Assets/UITemplateFactory.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorPlatform.h"
#include "Editor/EditorWidgets.h"
#include "Panels/EventScriptGraphPanel.h"
#include "Panels/DataFileEditorPanel.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Animation/AnimationClipSerializer.h"
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
                case AssetType::SpriteSheet:   return "Sprite Sheet";
                default:                    return "Unknown";
            }
        }

    } // namespace

    namespace {

        // Lightweight WAV peak extractor for the audio preview waveform
        // (uncompressed PCM WAV only; other formats show no waveform).
        struct WavPreview
        {
            float DurationSeconds = 0.0f;
            std::vector<float> Peaks;   // 64 bars in 0..1
        };

        static bool ParseWavPreview(const std::string& path, WavPreview& out)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            char riff[4];
            input.read(riff, 4);
            if (std::memcmp(riff, "RIFF", 4) != 0)
                return false;

            uint32_t sampleRate = 0;
            uint16_t channels = 0;
            uint16_t bitsPerSample = 0;
            uint32_t dataSize = 0;

            input.seekg(4, std::ios::cur); // chunk size
            char wave[4];
            input.read(wave, 4);
            if (std::memcmp(wave, "WAVE", 4) != 0)
                return false;

            while (input.good())
            {
                char chunkId[4];
                uint32_t chunkSize = 0;
                input.read(chunkId, 4);
                input.read(reinterpret_cast<char*>(&chunkSize), 4);
                if (std::memcmp(chunkId, "fmt ", 4) == 0)
                {
                    uint16_t audioFormat = 0;
                    input.read(reinterpret_cast<char*>(&audioFormat), 2);
                    input.read(reinterpret_cast<char*>(&channels), 2);
                    input.read(reinterpret_cast<char*>(&sampleRate), 4);
                    input.seekg(6, std::ios::cur); // byte rate + block align
                    input.read(reinterpret_cast<char*>(&bitsPerSample), 2);
                    input.seekg(chunkSize - 16, std::ios::cur);
                }
                else if (std::memcmp(chunkId, "data", 4) == 0)
                {
                    dataSize = chunkSize;
                    break;
                }
                else
                {
                    input.seekg(chunkSize, std::ios::cur);
                    if (chunkSize % 2 != 0)
                        input.seekg(1, std::ios::cur);
                }
            }

            if (channels == 0 || sampleRate == 0 || bitsPerSample == 0 || dataSize == 0)
                return false;

            constexpr size_t kBarCount = 64;
            const size_t sampleCount = dataSize / (channels * bitsPerSample / 8);
            out.DurationSeconds = static_cast<float>(sampleCount) / sampleRate;
            out.Peaks.assign(kBarCount, 0.0f);

            if (bitsPerSample == 16)
            {
                const size_t samplesPerBar = std::max<size_t>(1, sampleCount / kBarCount);
                std::vector<int16_t> buffer(samplesPerBar * channels);
                for (size_t bar = 0; bar < kBarCount && input.good(); ++bar)
                {
                    input.read(reinterpret_cast<char*>(buffer.data()),
                        static_cast<std::streamsize>(buffer.size() * sizeof(int16_t)));
                    const size_t read = static_cast<size_t>(input.gcount()) / sizeof(int16_t);
                    float peak = 0.0f;
                    for (size_t i = 0; i < read; ++i)
                        peak = std::max(peak, std::abs(buffer[i]) / 32768.0f);
                    out.Peaks[bar] = peak;
                }
            }
            else if (bitsPerSample == 8)
            {
                const size_t samplesPerBar = std::max<size_t>(1, sampleCount / kBarCount);
                std::vector<uint8_t> buffer(samplesPerBar * channels);
                for (size_t bar = 0; bar < kBarCount && input.good(); ++bar)
                {
                    input.read(reinterpret_cast<char*>(buffer.data()),
                        static_cast<std::streamsize>(buffer.size()));
                    const size_t read = static_cast<size_t>(input.gcount());
                    float peak = 0.0f;
                    for (size_t i = 0; i < read; ++i)
                        peak = std::max(peak, std::abs(static_cast<int>(buffer[i]) - 128) / 128.0f);
                    out.Peaks[bar] = peak;
                }
            }

            return true;
        }

    } // namespace

    std::filesystem::path GetEditorAssetPath()
    {
        return AssetPath::GetAssetRoot();
    }

    namespace {

        // UE-style drag splitter: visible grip, resize cursor, live width
        // clamp. `dragInverts` flips the delta so the drag direction feels
        // natural on both sides of the grid.
        void DrawVerticalSplitter(const char* id,
            float& size,
            float minSize,
            float maxSize,
            bool dragInverts)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.13f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.72f, 0.80f, 0.30f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.72f, 0.80f, 0.45f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 2.0f));
            ImGui::InvisibleButton(id, ImVec2(16.0f, -1.0f));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            if (ImGui::IsItemActive())
            {
                const float delta = ImGui::GetIO().MouseDelta.x;
                size = std::clamp(size + (dragInverts ? -delta : delta), minSize, maxSize);
            }

            // Grip line: always visible (dim), brightens on hover/drag so the
            // handle is easy to find on dark themes.
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float x = (min.x + max.x) * 0.5f;
            const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(x - 1.0f, min.y + 3.0f),
                ImVec2(x - 1.0f, max.y - 3.0f),
                hot ? IM_COL32(90, 210, 230, 255) : IM_COL32(120, 140, 150, 70),
                hot ? 2.0f : 1.0f);
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(x + 1.0f, min.y + 3.0f),
                ImVec2(x + 1.0f, max.y - 3.0f),
                hot ? IM_COL32(90, 210, 230, 255) : IM_COL32(120, 140, 150, 70),
                hot ? 2.0f : 1.0f);
        }

    } // namespace

    namespace ContentBrowserRequests {

        static bool s_HasPendingReveal = false;
        static std::string s_PendingRevealPath;

        void RequestReveal(const std::string& projectRelativePath)
        {
            s_PendingRevealPath = projectRelativePath;
            s_HasPendingReveal = true;
        }

        bool ConsumeRevealRequest(std::string& outPath)
        {
            if (!s_HasPendingReveal)
                return false;

            outPath = s_PendingRevealPath;
            s_PendingRevealPath.clear();
            s_HasPendingReveal = false;
            return true;
        }

    } // namespace ContentBrowserRequests

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
        m_Icons[AssetType::SpriteSheet] = Texture2D::Create("Resources/Icons/Editor/sprite_sheet.png");

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
        if (ext == AssetFileType::SheetExtension)
            return AssetType::SpriteSheet;
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

        // Cache the scan per (directory, filter, type) so the grid does not
        // hit the file system on every frame for large folders; a
        // quarter-second staleness is imperceptible for asset browsing.
        const std::string cacheKey = filter + "#"
            + std::to_string(m_TypeFilterMask);
        constexpr auto kEntryScanInterval = std::chrono::milliseconds(250);
        const auto now = std::chrono::steady_clock::now();
        if (m_EntryCacheDir == m_CurrentDirectory
            && m_EntryCacheFilter == cacheKey
            && (now - m_LastEntryScan) < kEntryScanInterval)
        {
            return m_EntryCache;
        }

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
            {
                if (m_TypeFilterMask != 0
                    && !entry.is_directory()
                    && (m_TypeFilterMask & (1u << static_cast<int>(GetAssetType(entry.path())))) == 0)
                {
                    continue;
                }
                result.push_back(entry);
            }
        }

        // Folders first, then case-insensitive name order.
        std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b)
            {
                if (a.is_directory() != b.is_directory())
                    return a.is_directory() > b.is_directory();
                return StringUtils::ToLower(a.path().filename().string())
                    < StringUtils::ToLower(b.path().filename().string());
            });

        m_EntryCache = result;
        m_EntryCacheDir = m_CurrentDirectory;
        m_EntryCacheFilter = cacheKey;
        m_LastEntryScan = std::chrono::steady_clock::now();
        return m_EntryCache;
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
        else if (ext == AssetFileType::SheetExtension)
        {
            if (m_OnOpenSpriteSheet) m_OnOpenSpriteSheet(path);
        }
        else if (ext == ".yaml" || ext == ".yml" || ext == ".json"
            || ext == ".wtsettings" || ext == AssetFileType::AnimationClipExtension)
        {
            // Generic data files open in the Data File Editor (structured tree
            // for YAML-ish files, validated raw text for JSON/others) so no
            // designer ever has to hand-edit them outside the editor.
            DataFileEditorRequests::RequestOpen(relative);
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
        if (m_RevealHighlightTimer > 0.0f)
        {
            m_RevealHighlightTimer -= 1.0f / 60.0f;
            if (m_RevealHighlightTimer <= 0.0f)
                m_RevealHighlightPath.clear();
        }

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
        else if (type == AssetType::AnimationClip)
        {
            // Preview the clip's first frame (sheet-linked frames resolve
            // through the shared sheet cache).
            Ref<AnimationClip> clip = AnimationClipSerializer::Load(path);
            if (!clip || clip->GetFrameCount() == 0)
                return false;

            const AnimationFrame& frame = clip->GetFrames()[0];
            Entity spriteEntity = scene->CreateEntity(
                clip->GetName().empty() ? "ThumbAnim" : clip->GetName());
            auto& sr = spriteEntity.AddComponent<SpriteRendererComponent>();
            if (!frame.SpriteSheet.empty() && frame.CellIndex >= 0)
            {
                SpriteSheetAsset::ResolvedCell resolved;
                if (!SpriteSheetAsset::ResolveCell(frame.SpriteSheet, frame.CellIndex, resolved))
                    return false;
                sr.Texture = resolved.Texture;
                sr.UVMin = resolved.UVMin;
                sr.UVMax = resolved.UVMax;
            }
            else
            {
                sr.Texture = frame.Texture;
                sr.UVMin = frame.TexCoordMin;
                sr.UVMax = frame.TexCoordMax;
                if (!sr.Texture)
                    return false;
            }
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
        thumbnail->SetData(pixels.data(), static_cast<uint32_t>(pixels.size()));
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

        // --- Row 1: navigation + breadcrumbs + sidebar toggle ----------------
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

        ImGui::SameLine(0, 8.0f);

        // Breadcrumbs: elide leading segments when the path is too wide so
        // long paths never push the rest of the toolbar off-screen.
        auto relative = std::filesystem::relative(m_CurrentDirectory, GetEditorAssetPath().parent_path());
        struct Crumb { std::string Label; std::filesystem::path Path; float Width; };
        std::vector<Crumb> crumbs;
        std::filesystem::path accumulated;
        for (const auto& part : relative)
        {
            accumulated /= part;
            crumbs.push_back({ part.string(), accumulated,
                ImGui::CalcTextSize(part.string().c_str()).x + 16.0f });
        }

        const float row1Avail = ImGui::GetContentRegionAvail().x;
        constexpr float kElideWidth = 30.0f;
        float totalWidth = 0.0f;
        for (const auto& crumb : crumbs)
            totalWidth += crumb.Width;

        int firstShown = 0;
        if (totalWidth > row1Avail)
        {
            // Keep the tail that fits, elide the head with a "..." button.
            float tailWidth = 0.0f;
            int keep = 0;
            for (int i = static_cast<int>(crumbs.size()) - 1; i >= 0; --i)
            {
                if (tailWidth + crumbs[i].Width > row1Avail - kElideWidth)
                    break;
                tailWidth += crumbs[i].Width;
                ++keep;
            }
            firstShown = std::max(0, static_cast<int>(crumbs.size()) - keep);
        }

        bool firstCrumb = true;
        if (firstShown > 0)
        {
            if (ImGui::SmallButton("...##elide"))
                NavigateTo(GetEditorAssetPath().parent_path() / crumbs[firstShown].Path);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", crumbs[firstShown].Path.generic_string().c_str());
            firstCrumb = false;
        }
        for (int i = firstShown; i < static_cast<int>(crumbs.size()); ++i)
        {
            if (!firstCrumb)
            {
                ImGui::SameLine(0, 2);
                ImGui::TextDisabled("/");
            }
            firstCrumb = false;
            ImGui::SameLine(0, 2);
            const std::string label = EditorWidgets::LabelWithId(
                crumbs[i].Label, "breadcrumb:" + crumbs[i].Path.generic_string());
            if (ImGui::SmallButton(label.c_str()))
                NavigateTo(GetEditorAssetPath().parent_path() / crumbs[i].Path);
        }

        ImGui::SameLine(0, 10.0f);
        if (ImGui::Button(m_ShowSidebar ? "<<" : ">>"))
            m_ShowSidebar = !m_ShowSidebar;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(EditorLocale::Text("Toggle Folder Tree", "切换文件夹树"));

        // --- Row 2: search + item count + management menu --------------------
        ImGui::Spacing();

        ImGui::SetNextItemWidth(260.0f);
        EditorWidgets::SearchBar("##search", m_SearchBuffer, sizeof(m_SearchBuffer),
            EditorLocale::Text("Search current folder...", "搜索当前文件夹..."));
        ImGui::SameLine();
        const std::string countLabel = std::to_string(entries.size())
            + EditorLocale::Text(" visible item(s)", " 项可见");
        EditorWidgets::StatusBadge(countLabel.c_str(), EditorWidgets::StatusKind::Info);
        if (m_SearchBuffer[0] != '\0')
        {
            ImGui::SameLine();
            EditorWidgets::StatusBadge(EditorLocale::Text("Filtered", "已过滤"), EditorWidgets::StatusKind::Warning);
        }

        // UE-style type filter: one button opens a checkbox menu. Folders are
        // never filtered; an empty mask shows everything.
        ImGui::SameLine(0, 14.0f);
        {
            int activeCount = 0;
            for (int t = 0; t < static_cast<int>(AssetType::SpriteSheet); ++t)
            {
                if (m_TypeFilterMask & (1u << t))
                    ++activeCount;
            }

            char filterLabel[64];
            if (activeCount > 0)
                std::snprintf(filterLabel, sizeof(filterLabel), "%s (%d)",
                    EditorLocale::Text("Filter", "筛选"), activeCount);
            else
                std::snprintf(filterLabel, sizeof(filterLabel), "%s",
                    EditorLocale::Text("Filter", "筛选"));

            if (ImGui::Button(filterLabel))
                ImGui::OpenPopup("##type_filter_menu");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", EditorLocale::Text(
                    "Show only the checked asset types.",
                    "只显示勾选的资产类型。"));

            if (ImGui::BeginPopup("##type_filter_menu"))
            {
                const struct { AssetType Type; const char* Label; } kFilterTypes[] = {
                    { AssetType::Texture, EditorLocale::Text("Textures", "纹理") },
                    { AssetType::Audio, EditorLocale::Text("Audio", "音频") },
                    { AssetType::Scene, EditorLocale::Text("Scenes", "场景") },
                    { AssetType::Script, EditorLocale::Text("Scripts", "脚本") },
                    { AssetType::Prefab, EditorLocale::Text("Prefabs", "预制体") },
                    { AssetType::UITemplate, EditorLocale::Text("UI Templates", "UI 模板") },
                    { AssetType::Material, EditorLocale::Text("Materials", "材质") },
                    { AssetType::AnimationClip, EditorLocale::Text("Animations", "动画") },
                    { AssetType::SpriteSheet, EditorLocale::Text("Sprite Sheets", "图集") },
                    { AssetType::Mesh, EditorLocale::Text("Meshes", "网格") },
                    { AssetType::Data, EditorLocale::Text("Data", "数据") },
                    { AssetType::Shader, EditorLocale::Text("Shaders", "着色器") },
                };

                const bool allShown = (m_TypeFilterMask == 0);
                if (ImGui::MenuItem(EditorLocale::Text("All", "全部"), nullptr, allShown))
                    m_TypeFilterMask = 0;
                ImGui::Separator();

                for (const auto& entry : kFilterTypes)
                {
                    const uint32_t bit = 1u << static_cast<int>(entry.Type);
                    const bool checked = (m_TypeFilterMask & bit) != 0;
                    if (ImGui::MenuItem(entry.Label, nullptr, checked))
                        m_TypeFilterMask ^= bit;
                }

                ImGui::Separator();
                if (ImGui::MenuItem(EditorLocale::Text("Clear Filter", "清除筛选")))
                    m_TypeFilterMask = 0;
                ImGui::EndPopup();
            }
        }

        ImGui::SameLine(0, 12.0f);
        if (ImGui::Button(EditorLocale::Text("Tools...", "工具...")))
            ImGui::OpenPopup("##browser_tools");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(EditorLocale::Text("Asset management actions", "资产管理操作"));
        if (ImGui::BeginPopup("##browser_tools"))
        {
            if (ImGui::MenuItem(EditorLocale::Text("Rescan Asset Registry", "重扫资源注册表")))
            {
                AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
                AssetRegistry::Get().WriteRegistry();
                m_RegistryStatus = "Asset registry rescanned and written.";
            }
            if (ImGui::MenuItem(EditorLocale::Text("Write Registry", "写入注册表")))
            {
                const bool saved = AssetRegistry::Get().WriteRegistry();
                m_RegistryStatus = saved ? "asset_registry.yaml written." : "Failed to write asset_registry.yaml.";
            }
            if (ImGui::MenuItem(EditorLocale::Text("Generate UI Templates", "生成内置 UI 模板")))
            {
                UITemplateFactory::WriteBuiltinTemplateAssets(AssetPath::GetProjectRoot());
                AssetRegistry::Get().Scan(AssetPath::GetProjectRoot());
                AssetRegistry::Get().WriteRegistry();
                m_RegistryStatus = "Builtin .wtuit UI template assets generated.";
            }
            ImGui::EndPopup();
        }

        if (!m_RegistryStatus.empty())
        {
            ImGui::SameLine();
            EditorWidgets::InlineStatus(m_RegistryStatus,
                m_RegistryStatus.find("Failed") != std::string::npos ? EditorWidgets::StatusKind::Error : EditorWidgets::StatusKind::Info);
        }

        ImGui::Separator();
    }

    void ContentBrowserPanel::DrawSidebar()
    {
        if (!m_ShowSidebar)
            return;

        ImGui::BeginChild("##sidebar", ImVec2(m_SidebarWidth, 0), true);

        // UE-style Sources panel: a root entry plus the folder tree.
        EditorWidgets::SectionHeader(EditorLocale::Text("Sources", "内容"));

        const std::filesystem::path assetRoot = GetEditorAssetPath();
        const bool rootSelected = (m_CurrentDirectory == assetRoot);
        if (ImGui::Selectable(EditorLocale::Text("All Assets", "全部资源"), rootSelected))
            NavigateTo(assetRoot);

        ImGui::Separator();

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

        drawTree(assetRoot, 0);
        ImGui::EndChild();
    }

    void ContentBrowserPanel::DrawFileGrid()
    {
        // Ctrl+wheel zooms the thumbnail size (UE-style). Consume the wheel
        // before the child window's scroll handling reads it, so zoom never
        // also scrolls the grid.
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            m_ThumbnailSize = std::clamp(
                m_ThumbnailSize * (1.0f + io.MouseWheel * 0.06f), 32.0f, 160.0f);
            io.MouseWheel = 0.0f;
        }

        ImGui::BeginChild("##filegrid", ImVec2(0, 0), false);

        const auto entries = GetFilteredEntries();
        if (entries.empty())
        {
            EditorWidgets::EmptyState(
                EditorLocale::Text("No assets found.", "未找到资源。"),
                m_SearchBuffer[0] == '\0'
                    ? EditorLocale::Text("This folder is empty.", "此文件夹为空。")
                    : EditorLocale::Text("No asset matches the current search filter.", "没有资产匹配当前搜索条件。"));
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

        // Manual grid layout (instead of ImGui::Columns) so an expanded
        // .wtsheet item can reserve vertical space below its row for the
        // inline cell strip. Pass 1 measures strip heights per row.
        const int   rowCount  = static_cast<int>((entries.size() + columnCount - 1) / columnCount);
        const float rowHeight = m_ThumbnailSize + m_Padding + 24.0f;
        std::vector<float> rowStripHeight(rowCount, 0.0f);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            const auto& entry = entries[i];
            if (entry.is_directory() || GetAssetType(entry.path()) != AssetType::SpriteSheet)
                continue;
            if (entry.path().string() != m_ExpandedSheetPath)
                continue;
            const int row = i / columnCount;
            rowStripHeight[row] = std::max(rowStripHeight[row],
                static_cast<float>(ComputeSheetStripHeight(entry.path())));
        }
        std::vector<float> rowOffset(rowCount + 1, 0.0f);
        for (int r = 1; r <= rowCount; ++r)
            rowOffset[r] = rowOffset[r - 1] + rowStripHeight[r - 1];

        // Plain wheel also zooms when the whole grid fits without scrolling
        // (nothing to scroll anyway); with overflow the wheel keeps scrolling.
        if (!io.KeyCtrl && io.MouseWheel != 0.0f && ImGui::IsWindowHovered())
        {
            const float totalHeight = rowOffset[rowCount] + rowCount * rowHeight;
            if (totalHeight <= ImGui::GetContentRegionAvail().y + 2.0f)
            {
                m_ThumbnailSize = std::clamp(
                    m_ThumbnailSize * (1.0f + io.MouseWheel * 0.06f), 32.0f, 160.0f);
                io.MouseWheel = 0.0f;
            }
        }

        const float gridStartX = ImGui::GetCursorScreenPos().x;
        const float gridStartY = ImGui::GetCursorScreenPos().y;

        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            const auto& entry = entries[i];
            const auto& path = entry.path();
            const int row = i / columnCount;
            const int col = i % columnCount;
            ImGui::SetCursorScreenPos(ImVec2(gridStartX + col * cellSize,
                gridStartY + row * rowHeight + rowOffset[row]));

            const auto relativePath = std::filesystem::relative(path, GetEditorAssetPath());
            const std::string filename = relativePath.filename().string();
            const bool isSelected = (path == m_SelectedPath);

            ImGui::PushID(path.string().c_str());

            const AssetType type = GetAssetType(path);
            const Ref<Texture2D>& icon = GetIconForType(type);

            // Real thumbnail preview: image assets load their texture directly;
            // scenes/prefabs/UI templates/meshes/animations are rendered
            // offscreen through a frame-by-frame queue (see OnUpdate/RenderThumbnail).
            Ref<Texture2D> displayIcon = icon;
            const bool needsRenderThumbnail =
                type == AssetType::Scene || type == AssetType::Prefab
                || type == AssetType::UITemplate || type == AssetType::Mesh
                || type == AssetType::AnimationClip;
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
                const ImVec2 size = ImVec2(m_ThumbnailSize + m_Padding, m_ThumbnailSize + m_Padding + 24.0f);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(59, 184, 204, 34), 6.0f);
                drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(59, 184, 204, 210), 6.0f, 0, 1.5f);
            }

            // Reveal highlight: orange flash on the asset requested by a
            // reference field's locate button.
            if (m_RevealHighlightTimer > 0.0f && path == m_RevealHighlightPath)
            {
                const float alpha = std::min(1.0f, m_RevealHighlightTimer);
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 size = ImVec2(m_ThumbnailSize + m_Padding, m_ThumbnailSize + m_Padding + 24.0f);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(255, 170, 60, static_cast<int>(55.0f * alpha)), 6.0f);
                drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(255, 170, 60, static_cast<int>(230.0f * alpha)), 6.0f, 0, 2.5f);
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
                if (type == AssetType::SpriteSheet && m_OnOpenSpriteSheet)
                {
                    if (ImGui::MenuItem(EditorLocale::Text("Open in Sheet Splitter", "打开分割器")))
                        m_OnOpenSpriteSheet(path);
                }
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
                // Filename under the thumbnail, drawn directly on the draw
                // list at a fixed position so it is always visible (never
                // dependent on cursor state), clipped to the cell width with
                // an ellipsis and centered.
                const ImVec2 thumbMin = ImGui::GetItemRectMin();
                const ImVec2 thumbMax = ImGui::GetItemRectMax();
                const float nameMaxWidth = m_ThumbnailSize + m_Padding - 6.0f;
                std::string shortName = filename;
                while (!shortName.empty()
                    && ImGui::CalcTextSize((shortName + "...").c_str()).x > nameMaxWidth)
                {
                    shortName.pop_back();
                }
                if (shortName != filename)
                    shortName += "...";

                const float nameWidth = ImGui::CalcTextSize(shortName.c_str()).x;
                const float nameX = thumbMin.x
                    + std::max(0.0f, (m_ThumbnailSize + m_Padding - nameWidth) * 0.5f);
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(nameX, thumbMax.y + 3.0f),
                    ImGui::GetColorU32(ImGuiCol_Text),
                    shortName.c_str());
            }

            // Unity-style expand arrow for .wtsheet items, overlaid on the
            // thumbnail's top-right corner. Drawn last so the thumbnail stays
            // the "last item" for click/drag/tooltip handling above.
            if (type == AssetType::SpriteSheet)
            {
                const ImVec2 thumbMin = ImGui::GetItemRectMin();
                const ImVec2 thumbMax = ImGui::GetItemRectMax();
                const bool expanded = (path.string() == m_ExpandedSheetPath);
                ImGui::SetCursorScreenPos(ImVec2(thumbMax.x - 20.0f, thumbMin.y + 3.0f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.72f, 0.80f, 0.25f));
                if (ImGui::SmallButton(expanded ? "▾" : "▸"))
                {
                    if (expanded)
                        m_ExpandedSheetPath.clear();
                    else
                    {
                        m_ExpandedSheetPath = path.string();
                        m_SelectedPath = path;
                    }
                }
                ImGui::PopStyleColor(2);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(expanded
                        ? EditorLocale::Text("Collapse sheet cells", "收起格子")
                        : EditorLocale::Text("Expand sheet cells", "展开格子"));
            }

            // Inline cell strip below the expanded .wtsheet row (space was
            // reserved for it in the layout pass above).
            if (type == AssetType::SpriteSheet && path.string() == m_ExpandedSheetPath)
            {
                const float stripY = gridStartY + (row + 1) * rowHeight + rowOffset[row] + 6.0f;
                ImGui::SetCursorScreenPos(ImVec2(gridStartX, stripY));
                DrawSheetCellStrip(path);
            }

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
            if (ImGui::MenuItem(EditorLocale::Text("New VN Script (.vn)", "新建视觉小说脚本 (.vn)")))
            {
                std::filesystem::path candidate = m_CurrentDirectory / "new_script.vn";
                for (int i = 2; std::filesystem::exists(candidate); ++i)
                    candidate = m_CurrentDirectory / ("new_script_" + std::to_string(i) + ".vn");

                const std::string templateText =
                    "# Generated by Wheatear Visual Novel Editor.\n"
                    "# 行类型：@label 标签 / @character 注册角色 / @show 立绘 /\n"
                    "# @background 背景 / @music 音乐 / @expression 表情 / @choice 分支选项\n"
                    "@label start\n"
                    "角色名: 在这里写台词。\n";

                if (EditorWidgets::WriteFileText(candidate, templateText))
                {
                    m_SelectedPath = candidate;
                    m_RegistryStatus = "Created VN script: " + candidate.filename().string();
                    VisualNovelEditorRequests::RequestOpenScript(
                        AssetPath::ToProjectRelative(candidate).generic_string());
                }
                else
                {
                    m_RegistryStatus = "Failed to create VN script.";
                }
            }
            if (ImGui::MenuItem(EditorLocale::Text("New Animation Clip (.wtanim)", "新建动画片段 (.wtanim)")))
            {
                std::filesystem::path candidate = m_CurrentDirectory / "new_clip.wtanim";
                for (int i = 2; std::filesystem::exists(candidate); ++i)
                    candidate = m_CurrentDirectory / ("new_clip_" + std::to_string(i) + ".wtanim");

                const std::string templateText =
                    "AnimationClip:\n"
                    "  Name: new_clip\n"
                    "  Looping: true\n"
                    "  Frames: []\n"
                    "  Events: []\n"
                    "  PropertyTracks: []\n";

                if (EditorWidgets::WriteFileText(candidate, templateText))
                {
                    m_SelectedPath = candidate;
                    m_RegistryStatus = "Created animation clip: " + candidate.filename().string();
                    // Open in the generic Data File Editor (tree/raw); the
                    // Animation Editor can load it via a SpriteAnimator's
                    // "Load .wtanim" binding.
                    DataFileEditorRequests::RequestOpen(
                        AssetPath::ToProjectRelative(candidate).generic_string());
                }
                else
                {
                    m_RegistryStatus = "Failed to create animation clip.";
                }
            }
            ImGui::EndPopup();
        }

        ImGui::EndChild();
    }

    namespace {

        // Sheet cell strip layout (inline expansion below a .wtsheet item).
        constexpr int   kSheetCellsPerStripRow = 8;
        constexpr float kSheetCellThumb        = 44.0f;
        constexpr float kSheetCellRowHeight    = 72.0f; // thumb + index label + spacing
        constexpr float kSheetStripPadding     = 10.0f;

    } // namespace

    int ContentBrowserPanel::ComputeSheetStripHeight(const std::filesystem::path& path)
    {
        const std::string key = path.string();
        auto& cached = m_SheetCellCache[key];
        // Refresh the definition every time so re-gridded sheets show up
        // live; the texture itself only re-loads when missing.
        cached.Data = SpriteSheetAsset::Load(key);
        if (!cached.Texture && !cached.Data.TexturePath.empty())
            cached.Texture = Texture2D::Create(cached.Data.TexturePath);
        if (!cached.Texture || !cached.Texture->IsLoaded())
            return 0;

        const int cellCount = SpriteSheetAsset::CellCount(cached.Data);
        const int cellRows  = (cellCount + kSheetCellsPerStripRow - 1) / kSheetCellsPerStripRow;
        const int rectRows  = (static_cast<int>(cached.Data.Rects.size()) + kSheetCellsPerStripRow - 1) / kSheetCellsPerStripRow;
        return static_cast<int>((cellRows + rectRows) * kSheetCellRowHeight + kSheetStripPadding);
    }

    void ContentBrowserPanel::DrawSheetCellStrip(const std::filesystem::path& path)
    {
        const std::string key = path.string();
        auto& cached = m_SheetCellCache[key];
        if (!cached.Texture || !cached.Texture->IsLoaded())
        {
            ImGui::TextDisabled(EditorLocale::Text("Failed to load sheet cells.", "无法加载格子。"));
            return;
        }

        const int cellCount = SpriteSheetAsset::CellCount(cached.Data);
        const int rectCount = static_cast<int>(cached.Data.Rects.size());
        const int cellRows  = (cellCount + kSheetCellsPerStripRow - 1) / kSheetCellsPerStripRow;
        const int rectRows  = (rectCount + kSheetCellsPerStripRow - 1) / kSheetCellsPerStripRow;
        const float stripWidth  = ImGui::GetContentRegionAvail().x;
        const float stripHeight = (cellRows + rectRows) * kSheetCellRowHeight + kSheetStripPadding;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.06f, 0.08f, 0.85f));
        ImGui::BeginChild("##sheetcells", ImVec2(stripWidth, stripHeight), true);

        const ImTextureID texId =
            static_cast<ImTextureID>(static_cast<uintptr_t>(cached.Texture->GetRendererID()));

        // Named sub-rects (irregular atlases) render first; each shows its
        // name and is draggable into the viewport.
        for (int r = 0; r < rectCount; ++r)
        {
            ImGui::PushID(r);
            const auto& rect = cached.Data.Rects[r];

            glm::vec2 uvMin{ 0.0f }, uvMax{ 1.0f };
            SpriteSheetAsset::ResolvedCell resolved;
            if (SpriteSheetAsset::ResolveCell(key, rect.Name, resolved))
            {
                uvMin = resolved.UVMin;
                uvMax = resolved.UVMax;
            }
            // Sheet UVs follow the renderer convention (v=0 bottom), which
            // matches ImGui's texture orientation {0,1}..{1,0}.
            const ImVec2 imgUV0(uvMin.x, uvMax.y);
            const ImVec2 imgUV1(uvMax.x, uvMin.y);

            ImGui::ImageButton("##rect", texId, ImVec2(kSheetCellThumb, kSheetCellThumb), imgUV0, imgUV1);

            if (ImGui::BeginDragDropSource())
            {
                // Payload: project-relative sheet path + '\n' + rect:<name>.
                std::wstring payload = AssetPath::ToProjectRelative(path).wstring();
                payload += L"\nrect:" + std::wstring(rect.Name.begin(), rect.Name.end());
                ImGui::SetDragDropPayload("SPRITE_SHEET_CELL", payload.c_str(),
                    (payload.size() + 1) * sizeof(wchar_t));
                ImGui::Image(texId, ImVec2(32, 32), imgUV0, imgUV1);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", rect.Name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", EditorLocale::Text(
                    "Rect '%s' — drag into the viewport to spawn a sprite",
                    "子图 '%s' — 拖到视口创建精灵"), rect.Name.c_str());

            ImGui::TextUnformatted(rect.Name.c_str());
            if ((r + 1) % kSheetCellsPerStripRow != 0 && r + 1 < rectCount)
                ImGui::SameLine();
            ImGui::PopID();
        }

        if (rectCount > 0 && cellCount > 0)
            ImGui::Spacing();

        for (int c = 0; c < cellCount; ++c)
        {
            ImGui::PushID(c);

            // Resolve through the shared cache so trimmed cells preview their
            // actual content; full-cell UVs fall back when resolution fails.
            glm::vec2 uvMin, uvMax;
            SpriteSheetAsset::ResolvedCell resolved;
            if (SpriteSheetAsset::ResolveCell(key, c, resolved))
            {
                uvMin = resolved.UVMin;
                uvMax = resolved.UVMax;
            }
            else
            {
                uvMin = SpriteSheetAsset::CellUVMin(cached.Data, c);
                uvMax = SpriteSheetAsset::CellUVMax(cached.Data, c);
            }
            // Sheet UVs follow the renderer convention (v=0 bottom), which
            // matches ImGui's texture orientation {0,1}..{1,0}.
            const ImVec2 imgUV0(uvMin.x, uvMax.y);
            const ImVec2 imgUV1(uvMax.x, uvMin.y);

            ImGui::ImageButton("##cell", texId, ImVec2(kSheetCellThumb, kSheetCellThumb), imgUV0, imgUV1);

            if (ImGui::BeginDragDropSource())
            {
                // Payload: project-relative sheet path + '\n' + cell:<index>.
                std::wstring payload = AssetPath::ToProjectRelative(path).wstring();
                payload += L"\ncell:" + std::to_wstring(c);
                ImGui::SetDragDropPayload("SPRITE_SHEET_CELL", payload.c_str(),
                    (payload.size() + 1) * sizeof(wchar_t));
                ImGui::Image(texId, ImVec2(32, 32), imgUV0, imgUV1);
                ImGui::SameLine();
                ImGui::TextDisabled("Cell %d", c);
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(EditorLocale::Text(
                    "Cell %d — drag into the viewport to spawn a sprite",
                    "格子 %d — 拖到视口创建精灵"), c);

            const std::string label = std::to_string(c);
            const float labelWidth = ImGui::CalcTextSize(label.c_str()).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (kSheetCellThumb - labelWidth) * 0.5f);
            ImGui::TextUnformatted(label.c_str());

            if ((c + 1) % kSheetCellsPerStripRow != 0 && c + 1 < cellCount)
                ImGui::SameLine();
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void ContentBrowserPanel::DrawInspector()
    {
        if (!m_ShowInspector)
            return;

        ImGui::BeginChild("##inspector", ImVec2(180, 0), true);
        EditorWidgets::SectionHeader(
            EditorLocale::Text("Inspector", "检查器"),
            EditorLocale::Text("Selected asset metadata and import settings.",
                "选中资产的元数据与导入设置。"));

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

                    // Audio preview: waveform + duration + play/stop.
                    ImGui::Spacing();
                    const bool isPlaying = (m_AudioPreviewHandle != 0 && m_AudioPreviewPath == m_SelectedPath.string());
                    if (isPlaying)
                    {
                        if (ImGui::Button(EditorLocale::Text("Stop", "停止")))
                        {
                            AudioEngine::StopSound(m_AudioPreviewHandle);
                            m_AudioPreviewHandle = 0;
                            m_AudioPreviewPath.clear();
                        }
                    }
                    else
                    {
                        if (ImGui::Button(EditorLocale::Text("Play", "播放")))
                        {
                            m_AudioPreviewHandle = AudioEngine::PlaySoundWithHandle(m_SelectedPath.string(), 0.8f);
                            m_AudioPreviewPath = m_SelectedPath.string();
                        }
                    }

                    WavPreview wav;
                    if (ParseWavPreview(m_SelectedPath.string(), wav) && !wav.Peaks.empty())
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("%.1f s", wav.DurationSeconds);

                        const ImVec2 cursor = ImGui::GetCursorScreenPos();
                        const ImVec2 area(ImGui::GetContentRegionAvail().x, 36.0f);
                        ImGui::Dummy(area);
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        const float barWidth = area.x / static_cast<float>(wav.Peaks.size());
                        const ImU32 barColor = ImGui::ColorConvertFloat4ToU32(
                            ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
                        for (size_t i = 0; i < wav.Peaks.size(); ++i)
                        {
                            const float barHeight = std::max(2.0f, wav.Peaks[i] * area.y);
                            const float x = cursor.x + static_cast<float>(i) * barWidth;
                            const float y = cursor.y + (area.y - barHeight) * 0.5f;
                            drawList->AddRectFilled(ImVec2(x + 1.0f, y),
                                ImVec2(x + barWidth - 1.0f, y + barHeight), barColor);
                        }
                    }
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
        // Reveal requests from asset reference fields: navigate to the
        // asset's folder, select it, and flash a highlight.
        std::string revealPath;
        if (ContentBrowserRequests::ConsumeRevealRequest(revealPath))
        {
            const std::filesystem::path resolved = AssetPath::Resolve(revealPath);
            const std::filesystem::path parent = resolved.parent_path();
            if (!parent.empty() && m_CurrentDirectory != parent)
                NavigateTo(parent);
            m_SelectedPath = resolved;
            m_RevealHighlightPath = resolved;
            m_RevealHighlightTimer = 2.0f;
        }

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
        EditorWidgets::PanelHeader(EditorLocale::Text("Content Browser", "资源浏览器"));
        DrawToolbar();

        if (m_ShowSidebar)
        {
            DrawSidebar();
            ImGui::SameLine();
            DrawVerticalSplitter("##sidebar_split", m_SidebarWidth, 90.0f, 420.0f, false);
            ImGui::SameLine();
        }

        if (m_ShowInspector)
        {
            const float gridWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x - m_InspectorWidth);
            ImGui::BeginChild("##gridwrap", ImVec2(gridWidth, 0), false);
            DrawFileGrid();
            ImGui::EndChild();

            ImGui::SameLine();
            DrawVerticalSplitter("##inspector_split", m_InspectorWidth, 140.0f, 420.0f, true);
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
