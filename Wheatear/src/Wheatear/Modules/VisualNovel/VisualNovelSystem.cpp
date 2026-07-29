#include "wtpch.h"
#include "VisualNovelSystem.h"

#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/KeyCodes.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRenderer.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetVisible;

        static bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        static bool IsExternalChoiceCommand(const std::string& command)
        {
            return StartsWith(command, "scene:")
                || StartsWith(command, "newgame:")
                || StartsWith(command, "loadgame:")
                || StartsWith(command, "event:");
        }

        static std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        static void SetWidgetsWithPrefixVisible(Scene* scene, const std::string& prefix, bool visible)
        {
            if (!scene || prefix.empty())
                return;

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TagComponent, UIWidgetComponent>())
            {
                const auto& tag = registry.get<TagComponent>(e).Tag;
                if (StartsWith(tag, prefix))
                    registry.get<UIWidgetComponent>(e).Visible = visible;
            }
        }

        static std::string FindFirstCanvasTag(Scene* scene)
        {
            if (!scene)
                return {};

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TagComponent, UICanvasComponent>())
                return registry.get<TagComponent>(e).Tag;
            return {};
        }

        static Entity EnsureNoticePanel(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            const glm::vec2& position,
            const glm::vec2& size)
        {
            if (!scene || entityName.empty())
                return {};

            Entity entity = FindEntityByName(scene, entityName);
            if (!entity)
                entity = scene->CreateEntity(entityName);

            auto& widget = entity.HasComponent<UIWidgetComponent>()
                ? entity.GetComponent<UIWidgetComponent>()
                : entity.AddComponent<UIWidgetComponent>();
            widget.Visible = true;
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = position;
            widget.Size = size;
            widget.SortOrder = 5200;
            widget.ParentTag = parentTag;

            auto& panel = entity.HasComponent<UIPanelComponent>()
                ? entity.GetComponent<UIPanelComponent>()
                : entity.AddComponent<UIPanelComponent>();
            panel.BackgroundColor = { 0.03f, 0.06f, 0.08f, 0.82f };
            panel.BorderColor = { 0.45f, 0.86f, 0.92f, 0.82f };
            panel.BorderThickness = 1.5f;
            panel.ClipChildren = true;
            return entity;
        }

        static Entity EnsureNoticeText(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            const glm::vec2& position,
            const glm::vec2& size)
        {
            if (!scene || entityName.empty())
                return {};

            Entity entity = FindEntityByName(scene, entityName);
            if (!entity)
                entity = scene->CreateEntity(entityName);

            auto& widget = entity.HasComponent<UIWidgetComponent>()
                ? entity.GetComponent<UIWidgetComponent>()
                : entity.AddComponent<UIWidgetComponent>();
            widget.Visible = true;
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = position;
            widget.Size = size;
            widget.SortOrder = 5201;
            widget.ParentTag = parentTag;

            auto& text = entity.HasComponent<UITextComponent>()
                ? entity.GetComponent<UITextComponent>()
                : entity.AddComponent<UITextComponent>();
            text.FontSize = 24.0f;
            text.FontPath = "assets/fonts/wqy-microhei.ttc";
            text.OutlineThickness = 0.85f;
            text.ShadowOffset = { 1.0f, 1.0f };
            return entity;
        }

        static void PreloadTextForEntity(Scene* scene, const std::string& entityName, const std::string& value)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UITextComponent>() || value.empty())
                return;

            UITextComponent text = entity.GetComponent<UITextComponent>();
            text.Text = value;
            UIRenderer::PreloadUIText(text);
        }

        static void SetButtonCommand(Scene* scene, const std::string& entityName, const std::string& command)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<UIButtonComponent>())
                entity.GetComponent<UIButtonComponent>().OnClickFunction = command;
        }

        static void SetTextVisible(Scene* scene,
            const std::string& entityName,
            const std::string& value,
            bool visible)
        {
            SetText(scene, entityName, value);
            SetWidgetVisible(scene, entityName, visible);
        }

        static void SetSpriteColor(Scene* scene, const std::string& entityName, const glm::vec4& color)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<SpriteRendererComponent>())
                entity.GetComponent<SpriteRendererComponent>().Color = color;
        }

        static std::string NormalizeAssetPath(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }

        static float GetBGMVolume(float volume)
        {
            const auto& settings = GameProgress::GetState().Settings;
            const float master = AudioEngine::PercentToGain(static_cast<float>(settings.MasterVolume));
            const float bgm = AudioEngine::PercentToGain(static_cast<float>(settings.BGMVolume));
            return std::clamp(volume, 0.0f, 2.0f) * master * bgm;
        }

        static std::string ResolveMusicTitle(const std::string& musicPath, const std::string& explicitTitle)
        {
            if (!explicitTitle.empty())
                return explicitTitle;

            std::string stem = std::filesystem::path(musicPath).stem().generic_string();
            std::replace(stem.begin(), stem.end(), '_', ' ');
            return stem.empty() ? musicPath : stem;
        }

        static std::string ReplaceAll(std::string value,
            const std::string& token,
            const std::string& replacement)
        {
            if (token.empty())
                return value;

            size_t position = 0;
            while ((position = value.find(token, position)) != std::string::npos)
            {
                value.replace(position, token.size(), replacement);
                position += replacement.size();
            }
            return value;
        }

        static bool IsTextureReference(const std::string& value)
        {
            if (value.empty())
                return false;

            const std::string normalized = NormalizeAssetPath(value);
            const std::string lowered = ToLower(normalized);
            if (StartsWith(lowered, "procedural:") || StartsWith(lowered, "color:"))
                return false;

            const std::filesystem::path path(normalized);
            const std::string extension = ToLower(path.extension().string());
            return StartsWith(lowered, "assets/")
                || extension == ".png"
                || extension == ".jpg"
                || extension == ".jpeg"
                || extension == ".webp"
                || extension == ".bmp"
                || extension == ".tga";
        }

        static Ref<Texture2D> LoadSpriteTexture(const std::string& texturePath)
        {
            if (!IsTextureReference(texturePath))
                return nullptr;

            const std::string normalizedPath = NormalizeAssetPath(texturePath);
            static std::unordered_map<std::string, Ref<Texture2D>> textureCache;
            if (auto it = textureCache.find(normalizedPath); it != textureCache.end())
                return it->second;

            Ref<Texture2D> texture = Texture2D::Create(normalizedPath);
            if (!texture || !texture->IsLoaded())
                return nullptr;

            textureCache[normalizedPath] = texture;
            return texture;
        }

        static bool TrySetSpriteTexture(Scene* scene,
            const std::string& entityName,
            const std::string& texturePath)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<SpriteRendererComponent>())
                return false;

            Ref<Texture2D> texture = LoadSpriteTexture(texturePath);
            if (!texture)
                return false;

            entity.GetComponent<SpriteRendererComponent>().Texture = texture;
            return true;
        }

        static void ClearSpriteTexture(Scene* scene, const std::string& entityName)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<SpriteRendererComponent>())
                entity.GetComponent<SpriteRendererComponent>().Texture = nullptr;
        }

        static std::string ResolveCharacterTexturePath(
            const VisualNovelCharacter& character,
            const std::string& expression)
        {
            std::string style = character.Style;
            if (style.empty() || !IsTextureReference(style))
                return {};

            const std::string resolvedExpression = expression.empty() ? "neutral" : expression;
            style = ReplaceAll(style, "{expression}", resolvedExpression);
            style = ReplaceAll(style, "{name}", character.Name);
            style = ReplaceAll(style, "{character}", character.Name);

            return IsTextureReference(style) ? NormalizeAssetPath(style) : std::string{};
        }

        static glm::vec4 ResolveBackgroundColor(const std::string& background)
        {
            const std::string key = ToLower(background);
            if (key.find("sun") != std::string::npos)
                return { 0.96f, 0.51f, 0.31f, 1.0f };
            if (key.find("night") != std::string::npos)
                return { 0.06f, 0.12f, 0.21f, 1.0f };
            if (key.find("menu") != std::string::npos)
                return { 0.10f, 0.12f, 0.18f, 1.0f };
            return { 0.20f, 0.42f, 0.50f, 1.0f };
        }

        static glm::vec4 ResolveFloorColor(const std::string& background)
        {
            const std::string key = ToLower(background);
            if (key.find("sun") != std::string::npos)
                return { 0.23f, 0.21f, 0.25f, 1.0f };
            if (key.find("night") != std::string::npos)
                return { 0.08f, 0.09f, 0.12f, 1.0f };
            if (key.find("menu") != std::string::npos)
                return { 0.05f, 0.06f, 0.09f, 1.0f };
            return { 0.14f, 0.17f, 0.18f, 1.0f };
        }

        static glm::vec4 ResolveCharacterColor(
            const std::string& name,
            const std::string& expression,
            float alpha)
        {
            const std::string character = ToLower(name);
            const std::string mood = ToLower(expression);

            glm::vec4 color = character == "leo"
                ? glm::vec4{ 0.86f, 0.58f, 0.25f, alpha }
                : glm::vec4{ 0.74f, 0.32f, 0.50f, alpha };

            if (mood == "happy")
                color = character == "leo"
                    ? glm::vec4{ 0.98f, 0.72f, 0.30f, alpha }
                    : glm::vec4{ 0.90f, 0.42f, 0.62f, alpha };
            else if (mood == "serious")
                color = character == "leo"
                    ? glm::vec4{ 0.62f, 0.44f, 0.24f, alpha }
                    : glm::vec4{ 0.48f, 0.24f, 0.38f, alpha };
            else if (mood == "surprised")
                color = character == "leo"
                    ? glm::vec4{ 0.98f, 0.78f, 0.46f, alpha }
                    : glm::vec4{ 0.95f, 0.54f, 0.76f, alpha };
            else if (mood == "thinking")
                color = character == "leo"
                    ? glm::vec4{ 0.70f, 0.64f, 0.44f, alpha }
                    : glm::vec4{ 0.68f, 0.36f, 0.58f, alpha };

            return color;
        }

        static bool AdvancePressed()
        {
            return Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT)
                || Input::IsKeyPressed(WT_KEY_SPACE)
                || Input::IsKeyPressed(WT_KEY_ENTER)
                || Input::IsKeyPressed(WT_KEY_RIGHT);
        }

        static bool IsEntityHoveredButton(Scene* scene, const std::string& entityName)
        {
            Entity entity = FindEntityByName(scene, entityName);
            return entity
                && entity.HasComponent<UIWidgetComponent>()
                && entity.GetComponent<UIWidgetComponent>().Visible
                && entity.HasComponent<UIButtonComponent>()
                && entity.GetComponent<UIButtonComponent>().IsHovered;
        }

        static std::filesystem::path BuildSavePath(const VisualNovelComponent& component, int slot)
        {
            std::filesystem::path directory = AssetPath::Resolve(component.SaveDirectory);
            return directory / ("slot" + std::to_string(std::max(1, slot)) + ".vnstate");
        }

        static std::string ResolveSpeakerDisplayName(const VisualNovelRuntime& runtime, const std::string& speaker)
        {
            if (speaker.empty())
                return {};

            for (const auto& character : runtime.GetScript().GetCharacters())
            {
                if (character.Name == speaker)
                    return character.DisplayName.empty() ? character.Name : character.DisplayName;
            }

            if (speaker == "Choice")
                return "选择";

            return speaker;
        }

        static std::string BuildHistoryText(const VisualNovelRuntime& runtime)
        {
            const auto& history = runtime.GetHistory();
            if (history.empty())
                return "历史记录\n\n还没有读过的对白。";

            std::ostringstream stream;
            stream << "历史记录\n\n";
            const size_t maxLines = 10;
            const size_t start = history.size() > maxLines ? history.size() - maxLines : 0;
            for (size_t i = start; i < history.size(); ++i)
            {
                const auto& entry = history[i];
                if (entry.IsChoice)
                    stream << "> " << entry.Text;
                else if (entry.Speaker.empty())
                    stream << entry.Text;
                else
                    stream << ResolveSpeakerDisplayName(runtime, entry.Speaker) << ": " << entry.Text;

                if (i + 1 < history.size())
                    stream << "\n\n";
            }
            return stream.str();
        }

        static std::string BuildSettingsText(const VisualNovelComponent& component,
            const VisualNovelRuntime& runtime)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2);
            stream << "系统设置\n\n";
            stream << "文字速度: " << static_cast<int>(component.CharactersPerSecond) << " 字/秒\n";
            stream << "自动等待: " << runtime.GetAutoPlayDelay() << " 秒\n";
            stream << "自动播放: " << (runtime.IsAutoPlay() ? "开" : "关") << "\n";
            stream << "消息窗口: 开";
            return stream.str();
        }

        static std::string BuildSaveLoadText(const VisualNovelComponent& component,
            const VisualNovelRuntime& runtime)
        {
            const std::filesystem::path savePath = BuildSavePath(component, 1);
            std::ostringstream stream;
            stream << "存档 / 读取\n\n";
            stream << "槽位 1: " << (std::filesystem::exists(savePath) ? "已有 VN 存档" : "空槽") << "\n";
            stream << "当前脚本: " << runtime.GetScript().GetSourcePath().filename().generic_string() << "\n";
            stream << "进度槽: 与据点/战斗结算共用 1 号成长进度\n\n";
            stream << "保存会同时写入 VN 行进状态和成长进度。\n";
            stream << "读取会回到 VN 存档所在台词，并同步读取成长进度。";
            return stream.str();
        }

    } // namespace

    void VisualNovelSystem::OnRuntimeStart(Scene* scene)
    {
        for (auto e : scene->GetRegistry().view<VisualNovelComponent>())
        {
            Entity entity{ e, scene };
            auto& component = entity.GetComponent<VisualNovelComponent>();
            RuntimeState& state = GetState(entity.GetUUID());
            if (component.PlayOnStart)
                LoadRuntime(state, component);
        }
    }

    void VisualNovelSystem::OnRuntimeStop(Scene* scene)
    {
        for (auto& [id, state] : m_RuntimeStates)
            StopBGM(state);
        m_RuntimeStates.clear();
    }

    void VisualNovelSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        for (auto e : scene->GetRegistry().view<VisualNovelComponent>())
        {
            Entity entity{ e, scene };
            auto& component = entity.GetComponent<VisualNovelComponent>();
            RuntimeState& state = GetState(entity.GetUUID());

            const std::filesystem::path resolvedPath = AssetPath::Resolve(component.ScriptPath);
            if (!state.Loaded
                || state.LoadedPath != resolvedPath
                || state.LoadedAutoLoadSlot != component.AutoLoadSlot)
            {
                LoadRuntime(state, component);
            }

            component.CharactersPerSecond = static_cast<float>(GameProgress::GetState().Settings.TextSpeed);
            state.Runtime.SetCharactersPerSecond(component.CharactersPerSecond);
            state.Runtime.SetAutoPlayDelay(component.AutoPlayDelay);
            const float deltaSeconds = ts.GetSeconds();
            state.SystemMessageTimer = std::max(0.0f, state.SystemMessageTimer - deltaSeconds);
            state.BGMNoticeTimer = std::max(0.0f, state.BGMNoticeTimer - deltaSeconds);

            UpdateInput(scene, component, state);

            const bool uiBlocksStory = state.ShowHistory || state.ShowSettings || state.ShowSaveLoad || state.DialogueHidden;
            if (!uiBlocksStory)
                state.Runtime.Update(deltaSeconds);

            UpdateBGM(scene, component, state);
            UpdateSceneBindings(scene, component, state);
            UpdateMusicNotice(scene, component, state, deltaSeconds);
        }
    }

    VisualNovelSystem::RuntimeState& VisualNovelSystem::GetState(UUID id)
    {
        return m_RuntimeStates[id];
    }

    void VisualNovelSystem::StopBGM(RuntimeState& state)
    {
        if (state.BGMHandle != 0)
            AudioEngine::StopSound(state.BGMHandle);

        state.BGMHandle = 0;
        state.CurrentBGMPath.clear();
        state.CurrentBGMTitle.clear();
        state.BGMNoticeTimer = 0.0f;
    }

    void VisualNovelSystem::UpdateBGM(Scene* scene,
        const VisualNovelComponent& component,
        RuntimeState& state)
    {
        if (!state.Loaded)
            return;

        const std::string desiredPath = NormalizeAssetPath(state.Runtime.GetCurrentMusic());
        const std::string desiredTitle = ResolveMusicTitle(
            desiredPath,
            state.Runtime.GetCurrentMusicTitle());

        if (desiredPath == state.CurrentBGMPath)
        {
            if (state.BGMHandle != 0)
                AudioEngine::SetVolume(state.BGMHandle, GetBGMVolume(0.82f));
            return;
        }

        if (state.BGMHandle != 0)
        {
            AudioEngine::StopSound(state.BGMHandle);
            state.BGMHandle = 0;
        }

        state.CurrentBGMPath = desiredPath;
        state.CurrentBGMTitle = desiredTitle;

        if (state.CurrentBGMPath.empty())
        {
            state.CurrentBGMTitle.clear();
            state.BGMNoticeTimer = 0.0f;
            return;
        }

        state.BGMHandle = AudioEngine::PlaySoundWithHandle(
            state.CurrentBGMPath,
            GetBGMVolume(0.82f),
            true);

        if (state.BGMHandle != 0)
            state.BGMNoticeTimer = state.BGMNoticeDuration;
        else
            state.BGMNoticeTimer = 0.0f;
    }

    void VisualNovelSystem::UpdateMusicNotice(Scene* scene,
        const VisualNovelComponent& component,
        RuntimeState& state,
        float deltaSeconds)
    {
        const bool visible = state.BGMNoticeTimer > 0.0f && !state.CurrentBGMTitle.empty();
        if (!visible)
        {
            SetWidgetVisible(scene, component.MusicNoticePanelEntityName, false);
            SetWidgetVisible(scene, component.MusicNoticeTextEntityName, false);
            return;
        }

        const float duration = std::max(0.01f, state.BGMNoticeDuration);
        const float elapsed = std::clamp(duration - state.BGMNoticeTimer, 0.0f, duration);
        const float normalized = elapsed / duration;
        float alpha = 1.0f;
        if (normalized < 0.14f)
            alpha = normalized / 0.14f;
        else if (normalized > 0.76f)
            alpha = std::max(0.0f, (1.0f - normalized) / 0.24f);

        const float slide = (1.0f - alpha) * 0.045f;
        const glm::vec2 panelPosition = { 0.030f - slide, 0.047f };
        const glm::vec2 panelSize = { 0.300f, 0.058f };
        const std::string parentTag = FindFirstCanvasTag(scene);

        Entity panelEntity = EnsureNoticePanel(
            scene,
            component.MusicNoticePanelEntityName,
            parentTag,
            panelPosition,
            panelSize);
        Entity textEntity = EnsureNoticeText(
            scene,
            component.MusicNoticeTextEntityName,
            parentTag,
            { panelPosition.x + 0.014f, panelPosition.y + 0.012f },
            { panelSize.x - 0.026f, panelSize.y - 0.016f });

        if (panelEntity && panelEntity.HasComponent<UIPanelComponent>())
        {
            auto& panel = panelEntity.GetComponent<UIPanelComponent>();
            panel.BackgroundColor = { 0.025f, 0.052f, 0.070f, 0.80f * alpha };
            panel.BorderColor = { 0.45f, 0.88f, 0.94f, 0.86f * alpha };
        }

        if (textEntity && textEntity.HasComponent<UITextComponent>())
        {
            auto& text = textEntity.GetComponent<UITextComponent>();
            const std::string notice = "BGM  " + state.CurrentBGMTitle;
            if (text.Text != notice)
            {
                text.Text = notice;
                UIRenderer::PreloadUIText(text);
            }
            text.Color = { 0.84f, 0.96f, 1.0f, alpha };
            text.OutlineColor = { 0.01f, 0.02f, 0.025f, 0.88f * alpha };
            text.ShadowColor = { 0.0f, 0.0f, 0.0f, 0.62f * alpha };
        }

        SetWidgetVisible(scene, component.MusicNoticePanelEntityName, alpha > 0.02f);
        SetWidgetVisible(scene, component.MusicNoticeTextEntityName, alpha > 0.02f);
    }

    bool VisualNovelSystem::LoadRuntime(RuntimeState& state, const VisualNovelComponent& component)
    {
        StopBGM(state);
        state.LoadedPath = AssetPath::Resolve(component.ScriptPath);
        state.Runtime.SetCharactersPerSecond(component.CharactersPerSecond);
        state.Runtime.SetAutoPlayDelay(component.AutoPlayDelay);
        state.Loaded = state.Runtime.LoadScript(state.LoadedPath);
        state.LoadedAutoLoadSlot = component.AutoLoadSlot;
        state.ShowHistory = false;
        state.ShowSettings = false;
        state.ShowSaveLoad = false;
        state.DialogueHidden = false;
        state.PreviousChoicePressed.assign(9, false);

        if (!state.Loaded)
        {
            WT_CORE_WARN("VisualNovelSystem: failed to load script '{}'", state.LoadedPath.string());
            return false;
        }

        state.Runtime.SetAutoPlay(component.AutoPlayOnStart);

        if (component.AutoLoadSlot > 0)
        {
            const std::filesystem::path savePath = BuildSavePath(component, component.AutoLoadSlot);
            state.Runtime.LoadState(savePath);
        }

        return true;
    }

    bool VisualNovelSystem::ExecuteHoveredCommand(Scene* scene,
        VisualNovelComponent& component,
        RuntimeState& state)
    {
        if (!scene)
            return false;

        int bestOrder = std::numeric_limits<int>::min();
        std::string bestCommand;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TagComponent, UIWidgetComponent, UIButtonComponent>())
        {
            auto& widget = registry.get<UIWidgetComponent>(e);
            auto& button = registry.get<UIButtonComponent>(e);
            if (!widget.Visible || !button.IsHovered || !StartsWith(button.OnClickFunction, "vn:"))
                continue;

            if (widget.SortOrder >= bestOrder)
            {
                bestOrder = widget.SortOrder;
                bestCommand = button.OnClickFunction;
            }
        }

        return ExecuteCommand(scene, component, state, bestCommand);
    }

    bool VisualNovelSystem::ExecuteCommand(Scene* scene,
        VisualNovelComponent& component,
        RuntimeState& state,
        const std::string& command)
    {
        if (!StartsWith(command, "vn:"))
            return false;

        const std::string action = ToLower(command.substr(3));
        auto pushMessage = [&](const std::string& message)
        {
            state.SystemMessage = message;
            state.SystemMessageTimer = 2.0f;
        };

        if (action == "auto")
        {
            state.Runtime.ToggleAutoPlay();
            state.DialogueHidden = false;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            pushMessage(state.Runtime.IsAutoPlay() ? "自动播放已开启" : "自动播放已关闭");
            return true;
        }

        if (action == "history")
        {
            state.ShowHistory = !state.ShowHistory;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "settings")
        {
            state.ShowSettings = !state.ShowSettings;
            state.ShowHistory = false;
            state.ShowSaveLoad = false;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "close")
        {
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            return true;
        }

        if (action == "hide")
        {
            state.DialogueHidden = true;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            return true;
        }

        if (action == "savemenu" || action == "loadmenu")
        {
            state.ShowSaveLoad = true;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "save" || action == "quicksave")
        {
            const std::filesystem::path savePath = BuildSavePath(component, 1);
            const bool vnSaved = state.Runtime.SaveState(savePath);
            const bool progressSaved = GameProgress::SaveSlot(1);
            if (vnSaved && progressSaved)
                pushMessage("已保存到 1 号槽");
            else if (vnSaved)
                pushMessage("VN 已保存，成长进度保存失败");
            return true;
        }

        if (action == "load" || action == "quickload")
        {
            const std::filesystem::path savePath = BuildSavePath(component, 1);
            if (state.Runtime.LoadState(savePath))
            {
                GameProgress::LoadSlot(1);
                pushMessage("已读取 1 号槽");
            }
            else
                pushMessage("1 号槽没有存档");
            return true;
        }

        if (action == "textspeed+" || action == "speed+")
        {
            auto& settings = GameProgress::GetState().Settings;
            settings.TextSpeed = std::min(180, settings.TextSpeed + 12);
            component.CharactersPerSecond = static_cast<float>(settings.TextSpeed);
            pushMessage("文字速度提高");
            return true;
        }

        if (action == "textspeed-" || action == "speed-")
        {
            auto& settings = GameProgress::GetState().Settings;
            settings.TextSpeed = std::max(12, settings.TextSpeed - 12);
            component.CharactersPerSecond = static_cast<float>(settings.TextSpeed);
            pushMessage("文字速度降低");
            return true;
        }

        if (action == "autodelay+")
        {
            component.AutoPlayDelay = std::min(6.0f, component.AutoPlayDelay + 0.25f);
            pushMessage("自动等待变长");
            return true;
        }

        if (action == "autodelay-")
        {
            component.AutoPlayDelay = std::max(0.4f, component.AutoPlayDelay - 0.25f);
            pushMessage("自动等待变短");
            return true;
        }

        if (action == "advance" || action == "skip")
        {
            if (component.RestartOnFinish || !state.Runtime.IsFinished())
                state.Runtime.Advance();
            return true;
        }

        return false;
    }

    void VisualNovelSystem::UpdateInput(Scene* scene,
        VisualNovelComponent& component,
        RuntimeState& state)
    {
        if (!state.Loaded)
            return;

        if (state.PreviousChoicePressed.size() < 9)
            state.PreviousChoicePressed.assign(9, false);

        const bool commandPressed = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
        if (commandPressed && !state.PreviousCommandPressed)
        {
            if (ExecuteHoveredCommand(scene, component, state))
            {
                state.PreviousCommandPressed = commandPressed;
                state.PreviousAdvancePressed = AdvancePressed();
                return;
            }
        }
        state.PreviousCommandPressed = commandPressed;

        if (state.DialogueHidden)
        {
            const bool pressed = AdvancePressed();
            if (pressed && !state.PreviousAdvancePressed)
                state.DialogueHidden = false;
            state.PreviousAdvancePressed = pressed;
            return;
        }

        const bool autoPressed = Input::IsKeyPressed(WT_KEY_A);
        if (autoPressed && !state.PreviousAutoPressed)
            ExecuteCommand(scene, component, state, "vn:auto");
        state.PreviousAutoPressed = autoPressed;

        const bool historyPressed = Input::IsKeyPressed(WT_KEY_H);
        if (historyPressed && !state.PreviousHistoryPressed)
            ExecuteCommand(scene, component, state, "vn:history");
        state.PreviousHistoryPressed = historyPressed;

        const bool savePressed = Input::IsKeyPressed(WT_KEY_F5);
        if (savePressed && !state.PreviousSavePressed)
            ExecuteCommand(scene, component, state, "vn:save");
        state.PreviousSavePressed = savePressed;

        const bool loadPressed = Input::IsKeyPressed(WT_KEY_F9);
        if (loadPressed && !state.PreviousLoadPressed)
            ExecuteCommand(scene, component, state, "vn:load");
        state.PreviousLoadPressed = loadPressed;

        if (state.ShowHistory || state.ShowSettings || state.ShowSaveLoad)
        {
            state.PreviousAdvancePressed = AdvancePressed();
            return;
        }

        if (state.Runtime.IsWaitingForChoice())
        {
            const auto& choices = state.Runtime.GetCurrentChoices();
            const size_t maxChoices = std::min<size_t>(choices.size(), 9);

            for (size_t i = 0; i < maxChoices; ++i)
            {
                const bool pressed = Input::IsKeyPressed(WT_KEY_1 + static_cast<int>(i));
                if (pressed && !state.PreviousChoicePressed[i])
                {
                    if (IsExternalChoiceCommand(choices[i].TargetLabel))
                        component.RuntimeRequestedCommand = choices[i].TargetLabel;
                    else
                        state.Runtime.Choose(i);
                    break;
                }
                state.PreviousChoicePressed[i] = pressed;
            }

            const bool mousePressed = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
            if (mousePressed && !state.PreviousAdvancePressed)
            {
                for (size_t i = 0; i < maxChoices; ++i)
                {
                    if (IsEntityHoveredButton(scene, component.ChoiceEntityPrefix + std::to_string(i + 1)))
                    {
                        if (IsExternalChoiceCommand(choices[i].TargetLabel))
                            component.RuntimeRequestedCommand = choices[i].TargetLabel;
                        else
                            state.Runtime.Choose(i);
                        break;
                    }
                }
            }

            state.PreviousAdvancePressed = AdvancePressed();
            return;
        }

        const bool pressed = AdvancePressed();
        if (pressed && !state.PreviousAdvancePressed)
        {
            if (component.RestartOnFinish || !state.Runtime.IsFinished())
                state.Runtime.Advance();
        }
        state.PreviousAdvancePressed = pressed;
    }

    void VisualNovelSystem::UpdateSceneBindings(Scene* scene,
        const VisualNovelComponent& component,
        RuntimeState& state)
    {
        const VisualNovelLine* line = state.Runtime.GetCurrentLine();
        const bool showStoryUi = !state.DialogueHidden && !state.ShowHistory && !state.ShowSettings && !state.ShowSaveLoad;
        const bool waitingForChoice = showStoryUi && state.Runtime.IsWaitingForChoice();

        SetWidgetsWithPrefixVisible(scene, "VN_Command", showStoryUi);
        SetWidgetsWithPrefixVisible(scene, "VN_History", state.ShowHistory);
        SetWidgetsWithPrefixVisible(scene, "VN_Settings", state.ShowSettings);
        SetWidgetsWithPrefixVisible(scene, "VN_SaveLoad", state.ShowSaveLoad);

        SetWidgetVisible(scene, component.CommandBarEntityName, showStoryUi);
        SetWidgetVisible(scene, component.HistoryPanelEntityName, state.ShowHistory);
        SetWidgetVisible(scene, component.SettingsPanelEntityName, state.ShowSettings);
        SetWidgetVisible(scene, component.SaveLoadPanelEntityName, state.ShowSaveLoad);
        SetText(scene, "VN_Command_Auto", state.Runtime.IsAutoPlay() ? "自动中" : "自动");

        SetTextVisible(scene, component.HistoryTextEntityName,
            BuildHistoryText(state.Runtime),
            state.ShowHistory);
        SetTextVisible(scene, component.SettingsTextEntityName,
            BuildSettingsText(component, state.Runtime),
            state.ShowSettings);
        SetTextVisible(scene, component.SaveLoadTextEntityName,
            BuildSaveLoadText(component, state.Runtime),
            state.ShowSaveLoad);

        SetTextVisible(scene,
            component.SystemMessageEntityName,
            state.SystemMessage,
            state.SystemMessageTimer > 0.0f && !state.DialogueHidden);

        if (!line)
        {
            SetWidgetVisible(scene, "VN_DialoguePanel", showStoryUi);
            SetText(scene, component.SpeakerTextEntityName, "");
            SetText(scene, component.BodyTextEntityName, "");
            SetText(scene, component.AdvanceHintEntityName, "");
            for (uint32_t i = 0; i < component.MaxVisibleChoices; ++i)
                SetWidgetVisible(scene, component.ChoiceEntityPrefix + std::to_string(i + 1), false);
            SetTextVisible(scene, component.AutoPlayIndicatorEntityName, "", false);
            return;
        }

        SetWidgetVisible(scene, "VN_DialoguePanel", showStoryUi);
        SetWidgetVisible(scene, component.SpeakerTextEntityName, showStoryUi && !waitingForChoice);
        SetWidgetVisible(scene, component.BodyTextEntityName, showStoryUi);
        SetWidgetVisible(scene, component.AdvanceHintEntityName, showStoryUi);

        const std::string speakerText = waitingForChoice ? "" : ResolveSpeakerDisplayName(state.Runtime, line->Speaker);
        PreloadTextForEntity(scene, component.SpeakerTextEntityName, speakerText);
        PreloadTextForEntity(scene, component.BodyTextEntityName, line->Text);
        SetText(scene, component.SpeakerTextEntityName, speakerText);
        SetText(scene, component.BodyTextEntityName, state.Runtime.GetVisibleText());

        std::string hint;
        if (waitingForChoice)
            hint = "请选择";
        else if (state.Runtime.IsLineComplete())
            hint = "点击 / 空格";
        SetText(scene, component.AdvanceHintEntityName, hint);

        const auto& choices = state.Runtime.GetCurrentChoices();
        const uint32_t maxVisibleChoices = std::min<uint32_t>(
            component.MaxVisibleChoices,
            static_cast<uint32_t>(choices.size()));

        for (uint32_t i = 0; i < component.MaxVisibleChoices; ++i)
        {
            const std::string entityName = component.ChoiceEntityPrefix + std::to_string(i + 1);
            const bool visible = waitingForChoice && i < maxVisibleChoices;
            SetWidgetVisible(scene, entityName, visible);
            if (visible)
            {
                const std::string choiceText = std::to_string(i + 1) + ". " + choices[i].Text;
                PreloadTextForEntity(scene, entityName, choiceText);
                SetText(scene, entityName, choiceText);
                SetButtonCommand(scene, entityName,
                    IsExternalChoiceCommand(choices[i].TargetLabel) ? choices[i].TargetLabel : "");
            }
            else
            {
                SetButtonCommand(scene, entityName, "");
            }
        }

        SetTextVisible(scene,
            component.AutoPlayIndicatorEntityName,
            state.Runtime.IsAutoPlay() ? "自动" : "",
            state.Runtime.IsAutoPlay() && showStoryUi);

        const std::string& background = state.Runtime.GetCurrentBackground();
        const bool backgroundHasTexture = TrySetSpriteTexture(scene, component.BackgroundEntityName, background);
        if (backgroundHasTexture)
        {
            SetSpriteColor(scene, component.BackgroundEntityName, { 1.0f, 1.0f, 1.0f, 1.0f });
            SetSpriteColor(scene, component.FloorEntityName, { 1.0f, 1.0f, 1.0f, 0.0f });
        }
        else
        {
            ClearSpriteTexture(scene, component.BackgroundEntityName);
            SetSpriteColor(scene, component.BackgroundEntityName, ResolveBackgroundColor(background));
            SetSpriteColor(scene, component.FloorEntityName, ResolveFloorColor(background));
        }

        std::unordered_set<std::string> visible;
        for (const std::string& name : state.Runtime.GetCurrentVisibleCharacters())
            visible.insert(name);

        const auto& expressions = state.Runtime.GetCurrentCharacterExpressions();
        for (const auto& character : state.Runtime.GetScript().GetCharacters())
        {
            const bool isVisible = visible.count(character.Name) > 0;
            const bool isSpeaker = line->Speaker == character.Name;
            const float alpha = isVisible ? (isSpeaker ? 1.0f : 0.58f) : 0.0f;

            auto expressionIt = expressions.find(character.Name);
            const std::string expression = expressionIt == expressions.end()
                ? "neutral"
                : expressionIt->second;

            const std::string entityName = component.CharacterEntityPrefix + character.Name;
            const std::string texturePath = ResolveCharacterTexturePath(character, expression);
            if (!texturePath.empty() && TrySetSpriteTexture(scene, entityName, texturePath))
            {
                SetSpriteColor(scene, entityName, { 1.0f, 1.0f, 1.0f, alpha });
            }
            else
            {
                ClearSpriteTexture(scene, entityName);
                SetSpriteColor(scene, entityName,
                    ResolveCharacterColor(character.Name, expression, alpha));
            }
        }
    }

} // namespace Wheatear
