#include "wtpch.h"
#include "VisualNovelSystem.h"

#include "VisualNovelInputService.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Core/UserSettings.h"
#include "Wheatear/Modules/Common/GameplayAudioService.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Renderer/Texture.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRenderer.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <array>
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
        using UIRuntimeTools::IsButtonHovered;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetTopLeft;
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

        static int ParseInt(const std::string& value, int fallback)
        {
            try
            {
                return std::stoi(value);
            }
            catch (...)
            {
                return fallback;
            }
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

        static std::string VNUiAsset(const std::string& key, const std::string& fallback)
        {
            return AssetAliasRegistry::Path("vn.ui." + key, fallback);
        }

        struct VNUiAtlasRegion
        {
            float X = 0.0f;
            float Y = 0.0f;
            float Width = 1.0f;
            float Height = 1.0f;
        };

        static constexpr float kVNUiAtlasWidth = 3200.0f;
        static constexpr float kVNUiAtlasHeight = 1584.0f;

        static std::string VNUiAtlasAsset()
        {
            return VNUiAsset("atlas", "assets/vertical_slice/ui/atlases/vn_ui_atlas.png");
        }

        static glm::vec2 VNUiAtlasUVMin(const VNUiAtlasRegion& region)
        {
            return {
                region.X / kVNUiAtlasWidth,
                (kVNUiAtlasHeight - region.Y - region.Height) / kVNUiAtlasHeight
            };
        }

        static glm::vec2 VNUiAtlasUVMax(const VNUiAtlasRegion& region)
        {
            return {
                (region.X + region.Width) / kVNUiAtlasWidth,
                (kVNUiAtlasHeight - region.Y) / kVNUiAtlasHeight
            };
        }

        static VNUiAtlasRegion VNUiAtlasRegionFor(const std::string& key)
        {
            if (key == "textbox_panel") return { 0.0f, 0.0f, 3200.0f, 640.0f };
            if (key == "choice_panel") return { 0.0f, 1152.0f, 1800.0f, 240.0f };
            if (key == "nameplate") return { 0.0f, 1392.0f, 720.0f, 192.0f };
            if (key == "bgm_notice_panel") return { 720.0f, 1392.0f, 1120.0f, 192.0f };
            return { 0.0f, 0.0f, kVNUiAtlasWidth, kVNUiAtlasHeight };
        }

        static VNUiAtlasRegion VNCommandIconRegion(uint32_t atlasColumn, bool highlighted)
        {
            constexpr float iconSize = 256.0f;
            return {
                static_cast<float>(atlasColumn) * iconSize,
                640.0f + (highlighted ? iconSize : 0.0f),
                iconSize,
                iconSize
            };
        }

        static void ApplyUIImage(Scene* scene,
            const std::string& entityName,
            const std::string& texturePath,
            const glm::vec4& color,
            const glm::vec2& uvMin = { 0.0f, 0.0f },
            const glm::vec2& uvMax = { 1.0f, 1.0f })
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity)
                return;

            auto& image = entity.HasComponent<UIImageComponent>()
                ? entity.GetComponent<UIImageComponent>()
                : entity.AddComponent<UIImageComponent>();
            image.Color = color;
            image.UVMin = uvMin;
            image.UVMax = uvMax;
            UIRuntimeTools::SetImageTexture(scene, entityName, texturePath, false);
        }

        static void ApplyVNUiAtlasRegion(Scene* scene,
            const std::string& entityName,
            const std::string& regionKey,
            const glm::vec4& color)
        {
            const VNUiAtlasRegion region = VNUiAtlasRegionFor(regionKey);
            ApplyUIImage(scene,
                entityName,
                VNUiAtlasAsset(),
                color,
                VNUiAtlasUVMin(region),
                VNUiAtlasUVMax(region));
        }

        static void SetTransparentButtonChrome(Scene* scene, const std::string& entityName)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UIButtonComponent>())
                return;

            auto& button = entity.GetComponent<UIButtonComponent>();
            button.NormalColor = { 1.0f, 1.0f, 1.0f, 0.0f };
            button.HoverColor = { 1.0f, 1.0f, 1.0f, 0.0f };
            button.PressedColor = { 1.0f, 1.0f, 1.0f, 0.0f };
        }

        struct VNCommandButtonSpec
        {
            const char* EntityName;
            const char* Command;
            const char* Tooltip;
            uint32_t AtlasColumn;
        };

        static const std::array<VNCommandButtonSpec, 8>& VNCommandButtonSpecs()
        {
            static const std::array<VNCommandButtonSpec, 8> specs = {{
                { "VN_Command_Save", "vn:savemenu", "存档", 0 },
                { "VN_Command_Load", "vn:loadmenu", "读取", 1 },
                { "VN_Command_QuickSave", "vn:quicksave", "快速存档", 2 },
                { "VN_Command_QuickLoad", "vn:quickload", "快速读取", 3 },
                { "VN_Command_Settings", "vn:settings", "系统设置", 4 },
                { "VN_Command_History", "vn:history", "历史记录", 5 },
                { "VN_Command_Auto", "vn:auto", "自动播放", 6 },
                { "VN_Command_Skip", "vn:skip", "快进 / 跳过", 7 },
            }};
            return specs;
        }

        static glm::vec2 VNCommandIconUVMin(uint32_t atlasColumn, bool highlighted)
        {
            return VNUiAtlasUVMin(VNCommandIconRegion(atlasColumn, highlighted));
        }

        static glm::vec2 VNCommandIconUVMax(uint32_t atlasColumn, bool highlighted)
        {
            return VNUiAtlasUVMax(VNCommandIconRegion(atlasColumn, highlighted));
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
            Entity parent = parentTag.empty() ? Entity{} : FindEntityByName(scene, parentTag);
            widget.ParentEntity = parent ? parent.GetUUID() : UUID(0);

            auto& panel = entity.HasComponent<UIPanelComponent>()
                ? entity.GetComponent<UIPanelComponent>()
                : entity.AddComponent<UIPanelComponent>();
            panel.BackgroundColor = { 0.03f, 0.06f, 0.08f, 0.0f };
            panel.BorderColor = { 0.45f, 0.86f, 0.92f, 0.0f };
            panel.BorderThickness = 0.0f;
            panel.ClipChildren = true;
            ApplyVNUiAtlasRegion(scene,
                entityName,
                "bgm_notice_panel",
                { 1.0f, 1.0f, 1.0f, 0.92f });
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
            Entity parent = parentTag.empty() ? Entity{} : FindEntityByName(scene, parentTag);
            widget.ParentEntity = parent ? parent.GetUUID() : UUID(0);

            auto& text = entity.HasComponent<UITextComponent>()
                ? entity.GetComponent<UITextComponent>()
                : entity.AddComponent<UITextComponent>();
            text.FontSize = 24.0f;
            text.FontPath = AssetAliasRegistry::Path("font.ui_default", "assets/fonts/wqy-microhei.ttc");
            text.OutlineThickness = 0.85f;
            text.ShadowOffset = { 1.0f, 1.0f };
            return entity;
        }

        static Entity EnsureVNSettingsWidget(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            const glm::vec2& position,
            const glm::vec2& size,
            int sortOrder,
            bool visible)
        {
            if (!scene || entityName.empty())
                return {};

            Entity entity = FindEntityByName(scene, entityName);
            if (!entity)
                entity = scene->CreateEntity(entityName);

            auto& widget = entity.HasComponent<UIWidgetComponent>()
                ? entity.GetComponent<UIWidgetComponent>()
                : entity.AddComponent<UIWidgetComponent>();
            widget.Visible = visible;
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = position;
            widget.Size = size;
            widget.Rotation = 0.0f;
            widget.SortOrder = sortOrder;

            Entity parent = parentTag.empty() ? Entity{} : FindEntityByName(scene, parentTag);
            widget.ParentEntity = parent ? parent.GetUUID() : UUID(0);
            return entity;
        }

        static Entity EnsureVNSettingsText(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            const glm::vec2& position,
            const glm::vec2& size,
            int sortOrder,
            const std::string& value,
            float fontSize,
            const glm::vec4& color,
            bool visible)
        {
            Entity entity = EnsureVNSettingsWidget(scene, entityName, parentTag, position, size, sortOrder, visible);
            if (!entity)
                return {};

            auto& text = entity.HasComponent<UITextComponent>()
                ? entity.GetComponent<UITextComponent>()
                : entity.AddComponent<UITextComponent>();
            text.Text = value;
            text.FontSize = fontSize;
            text.Color = color;
            text.FontPath = AssetAliasRegistry::Path("font.ui_default", "assets/fonts/wqy-microhei.ttc");
            text.ShadowColor = { 0.02f, 0.025f, 0.030f, 0.78f };
            text.ShadowOffset = { 1.4f, 1.4f };
            text.OutlineColor = { 0.0f, 0.0f, 0.0f, 0.86f };
            text.OutlineThickness = 1.05f;
            UIRenderer::PreloadUIText(text);
            return entity;
        }

        static Entity EnsureVNSettingsButton(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            const glm::vec2& position,
            const glm::vec2& size,
            int sortOrder,
            const std::string& label,
            const std::string& command,
            bool visible)
        {
            Entity entity = EnsureVNSettingsText(scene,
                entityName,
                parentTag,
                position,
                size,
                sortOrder,
                label,
                18.0f,
                { 0.94f, 0.96f, 0.92f, 1.0f },
                visible);
            if (!entity)
                return {};

            auto& button = entity.HasComponent<UIButtonComponent>()
                ? entity.GetComponent<UIButtonComponent>()
                : entity.AddComponent<UIButtonComponent>();
            button.OnClickFunction = command;
            button.NormalColor = { 0.12f, 0.18f, 0.22f, 0.90f };
            button.HoverColor = { 0.25f, 0.42f, 0.46f, 0.96f };
            button.PressedColor = { 0.08f, 0.12f, 0.15f, 1.0f };
            return entity;
        }

        static Entity EnsureVNSettingsSlider(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            const glm::vec2& position,
            const glm::vec2& size,
            int sortOrder,
            const std::string& command,
            bool visible)
        {
            Entity entity = EnsureVNSettingsWidget(scene, entityName, parentTag, position, size, sortOrder, visible);
            if (!entity)
                return {};

            auto& slider = entity.HasComponent<UISliderComponent>()
                ? entity.GetComponent<UISliderComponent>()
                : entity.AddComponent<UISliderComponent>();
            slider.MinValue = 0.0f;
            slider.MaxValue = 100.0f;
            slider.TrackColor = { 0.08f, 0.10f, 0.12f, 0.92f };
            slider.FillColor = { 0.32f, 0.74f, 0.78f, 0.96f };
            slider.HandleColor = { 0.92f, 0.98f, 0.94f, 1.0f };
            slider.HoverColor = { 1.0f, 0.88f, 0.48f, 1.0f };
            slider.OnValueChangedFunction = command;
            return entity;
        }

        static void SetVNSettingsSliderValue(Scene* scene, const std::string& entityName, float value)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UISliderComponent>())
                return;

            auto& slider = entity.GetComponent<UISliderComponent>();
            slider.MinValue = 0.0f;
            slider.MaxValue = 100.0f;
            if (!slider.IsDragging)
                slider.Value = std::clamp(value, slider.MinValue, slider.MaxValue);
        }

        static void UpdateVNSettingsAudioControls(Scene* scene,
            const VisualNovelComponent& component,
            bool visible)
        {
            if (!scene || component.SettingsPanelEntityName.empty())
                return;

            Entity panel = FindEntityByName(scene, component.SettingsPanelEntityName);
            if (!panel)
                return;

            const std::string canvasTag = FindFirstCanvasTag(scene);
            SetWidgetTopLeft(scene, component.SettingsPanelEntityName, { 0.23f, 0.105f }, { 0.54f, 0.70f });
            SetWidgetTopLeft(scene, component.SettingsTextEntityName, { 0.29f, 0.155f }, { 0.42f, 0.205f });
            SetWidgetTopLeft(scene, "VN_Settings_TextMinus", { 0.34f, 0.602f }, { 0.15f, 0.048f });
            SetWidgetTopLeft(scene, "VN_Settings_TextPlus", { 0.51f, 0.602f }, { 0.15f, 0.048f });
            SetWidgetTopLeft(scene, "VN_Settings_AutoMinus", { 0.34f, 0.665f }, { 0.15f, 0.048f });
            SetWidgetTopLeft(scene, "VN_Settings_AutoPlus", { 0.51f, 0.665f }, { 0.15f, 0.048f });
            SetWidgetTopLeft(scene, "VN_SettingsClose", { 0.41f, 0.735f }, { 0.18f, 0.052f });

            const auto& settings = UserSettings::Get();
            const glm::vec4 labelColor = { 0.88f, 0.98f, 0.93f, 1.0f };
            const glm::vec2 labelSize = { 0.13f, 0.035f };
            const glm::vec2 sliderSize = { 0.20f, 0.030f };
            const glm::vec2 buttonSize = { 0.040f, 0.044f };

            EnsureVNSettingsText(scene,
                "VN_Settings_MasterVolumeLabel",
                canvasTag,
                { 0.30f, 0.382f },
                labelSize,
                88,
                "主音量 " + std::to_string(settings.MasterVolume) + "%",
                16.0f,
                labelColor,
                visible);
            EnsureVNSettingsSlider(scene,
                "VN_Settings_MasterVolumeSlider",
                canvasTag,
                { 0.43f, 0.389f },
                sliderSize,
                89,
                "progression:set_master_volume",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_MasterVolumeDown",
                canvasTag,
                { 0.645f, 0.377f },
                buttonSize,
                90,
                "-",
                "progression:master_volume_down",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_MasterVolumeUp",
                canvasTag,
                { 0.695f, 0.377f },
                buttonSize,
                90,
                "+",
                "progression:master_volume_up",
                visible);

            EnsureVNSettingsText(scene,
                "VN_Settings_BGMVolumeLabel",
                canvasTag,
                { 0.30f, 0.452f },
                labelSize,
                88,
                "音乐 " + std::to_string(settings.BGMVolume) + "%",
                16.0f,
                labelColor,
                visible);
            EnsureVNSettingsSlider(scene,
                "VN_Settings_BGMVolumeSlider",
                canvasTag,
                { 0.43f, 0.459f },
                sliderSize,
                89,
                "progression:set_bgm_volume",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_BGMVolumeDown",
                canvasTag,
                { 0.645f, 0.447f },
                buttonSize,
                90,
                "-",
                "progression:bgm_volume_down",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_BGMVolumeUp",
                canvasTag,
                { 0.695f, 0.447f },
                buttonSize,
                90,
                "+",
                "progression:bgm_volume_up",
                visible);

            EnsureVNSettingsText(scene,
                "VN_Settings_SFXVolumeLabel",
                canvasTag,
                { 0.30f, 0.522f },
                labelSize,
                88,
                "音效 " + std::to_string(settings.SFXVolume) + "%",
                16.0f,
                labelColor,
                visible);
            EnsureVNSettingsSlider(scene,
                "VN_Settings_SFXVolumeSlider",
                canvasTag,
                { 0.43f, 0.529f },
                sliderSize,
                89,
                "progression:set_sfx_volume",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_SFXVolumeDown",
                canvasTag,
                { 0.645f, 0.517f },
                buttonSize,
                90,
                "-",
                "progression:sfx_volume_down",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_SFXVolumeUp",
                canvasTag,
                { 0.695f, 0.517f },
                buttonSize,
                90,
                "+",
                "progression:sfx_volume_up",
                visible);

            SetVNSettingsSliderValue(scene, "VN_Settings_MasterVolumeSlider", static_cast<float>(settings.MasterVolume));
            SetVNSettingsSliderValue(scene, "VN_Settings_BGMVolumeSlider", static_cast<float>(settings.BGMVolume));
            SetVNSettingsSliderValue(scene, "VN_Settings_SFXVolumeSlider", static_cast<float>(settings.SFXVolume));
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

        static void ApplyVNStaticUISkin(Scene* scene, const VisualNovelComponent& component)
        {
            if (!scene)
                return;

            ApplyVNUiAtlasRegion(scene,
                "VN_DialoguePanel",
                "textbox_panel",
                { 1.0f, 1.0f, 1.0f, 0.96f });

            ApplyVNUiAtlasRegion(scene,
                component.SpeakerTextEntityName,
                "nameplate",
                { 1.0f, 1.0f, 1.0f, 0.94f });

            ApplyVNUiAtlasRegion(scene,
                component.HistoryPanelEntityName,
                "textbox_panel",
                { 1.0f, 1.0f, 1.0f, 0.93f });
            ApplyVNUiAtlasRegion(scene,
                component.SettingsPanelEntityName,
                "textbox_panel",
                { 1.0f, 1.0f, 1.0f, 0.93f });
            ApplyVNUiAtlasRegion(scene,
                component.SaveLoadPanelEntityName,
                "textbox_panel",
                { 1.0f, 1.0f, 1.0f, 0.93f });

            for (uint32_t i = 0; i < component.MaxVisibleChoices; ++i)
            {
                const std::string entityName = component.ChoiceEntityPrefix + std::to_string(i + 1);
                ApplyVNUiAtlasRegion(scene,
                    entityName,
                    "choice_panel",
                    { 1.0f, 1.0f, 1.0f, 0.95f });
                SetTransparentButtonChrome(scene, entityName);
            }
        }

        static bool IsVNCommandActive(const VNCommandButtonSpec& spec,
            bool autoPlay,
            bool showSettings,
            bool showHistory)
        {
            const std::string name = spec.EntityName;
            if (name == "VN_Command_Auto")
                return autoPlay;
            if (name == "VN_Command_Settings")
                return showSettings;
            if (name == "VN_Command_History")
                return showHistory;
            return false;
        }

        static void EnsureVNCommandBar(Scene* scene,
            const VisualNovelComponent& component,
            const std::string& canvasTag,
            bool visible)
        {
            Entity bar = EnsureVNSettingsWidget(scene,
                component.CommandBarEntityName,
                canvasTag,
                { 0.315f, 0.884f },
                { 0.370f, 0.086f },
                5050,
                visible);
            if (!bar)
                return;

            auto& image = bar.HasComponent<UIImageComponent>()
                ? bar.GetComponent<UIImageComponent>()
                : bar.AddComponent<UIImageComponent>();
            image.Color = { 1.0f, 1.0f, 1.0f, 0.0f };
            image.UVMin = { 0.0f, 0.0f };
            image.UVMax = { 1.0f, 1.0f };
        }

        static void ConfigureVNCommandButton(Scene* scene,
            const VisualNovelComponent& component,
            const VNCommandButtonSpec& spec,
            size_t index,
            bool visible,
            bool highlighted)
        {
            const float localGap = 0.035f;
            const float localWidth = 0.085f;
            const float localX = localGap + static_cast<float>(index) * (localWidth + localGap);
            Entity entity = EnsureVNSettingsWidget(scene,
                spec.EntityName,
                component.CommandBarEntityName,
                { localX, 0.140f },
                { localWidth, 0.720f },
                static_cast<int>(5060 + index),
                visible);
            if (!entity)
                return;

            auto& button = entity.HasComponent<UIButtonComponent>()
                ? entity.GetComponent<UIButtonComponent>()
                : entity.AddComponent<UIButtonComponent>();
            button.OnClickFunction = spec.Command;
            button.NormalColor = { 1.0f, 1.0f, 1.0f, 0.0f };
            button.HoverColor = { 1.0f, 1.0f, 1.0f, 0.0f };
            button.PressedColor = { 1.0f, 1.0f, 1.0f, 0.0f };

            if (entity.HasComponent<UIAnimatorComponent>())
                entity.RemoveComponent<UIAnimatorComponent>();

            if (!entity.HasComponent<UITextComponent>())
                entity.AddComponent<UITextComponent>();
            SetText(scene, spec.EntityName, "");

            const bool useHighlightedRow = highlighted || button.IsHovered || button.IsPressed;
            ApplyUIImage(scene,
                spec.EntityName,
                VNUiAtlasAsset(),
                { 1.0f, 1.0f, 1.0f, visible ? 1.0f : 0.0f },
                VNCommandIconUVMin(spec.AtlasColumn, useHighlightedRow),
                VNCommandIconUVMax(spec.AtlasColumn, useHighlightedRow));
        }

        static void UpdateVNCommandTooltip(Scene* scene, bool showStoryUi)
        {
            const VNCommandButtonSpec* hoveredSpec = nullptr;
            size_t hoveredIndex = 0;
            const auto& specs = VNCommandButtonSpecs();
            for (size_t i = 0; i < specs.size(); ++i)
            {
                if (IsButtonHovered(scene, specs[i].EntityName))
                {
                    hoveredSpec = &specs[i];
                    hoveredIndex = i;
                    break;
                }
            }

            const bool visible = showStoryUi && hoveredSpec != nullptr;
            const std::string canvasTag = FindFirstCanvasTag(scene);
            const float barLeft = 0.315f;
            const float barWidth = 0.370f;
            const float localGap = 0.035f;
            const float localWidth = 0.085f;
            const float tooltipWidth = 0.118f;
            const float tooltipHeight = 0.034f;
            const float localCenter = localGap + static_cast<float>(hoveredIndex) * (localWidth + localGap) + localWidth * 0.5f;
            const float tooltipX = std::clamp(barLeft + localCenter * barWidth - tooltipWidth * 0.5f,
                0.020f,
                0.980f - tooltipWidth);
            const float tooltipY = 0.962f;

            Entity panel = EnsureVNSettingsWidget(scene,
                "VN_Tooltip_CommandPanel",
                canvasTag,
                { tooltipX, tooltipY },
                { tooltipWidth, tooltipHeight },
                6200,
                visible);
            if (panel)
            {
                auto& panelStyle = panel.HasComponent<UIPanelComponent>()
                    ? panel.GetComponent<UIPanelComponent>()
                    : panel.AddComponent<UIPanelComponent>();
                panelStyle.BackgroundColor = { 0.0f, 0.0f, 0.0f, 0.0f };
                panelStyle.BorderColor = { 0.0f, 0.0f, 0.0f, 0.0f };
                panelStyle.BorderThickness = 0.0f;
                panelStyle.ClipChildren = true;
                ApplyVNUiAtlasRegion(scene,
                    "VN_Tooltip_CommandPanel",
                    "bgm_notice_panel",
                    { 1.0f, 1.0f, 1.0f, visible ? 0.92f : 0.0f });
            }

            EnsureVNSettingsText(scene,
                "VN_Tooltip_CommandText",
                canvasTag,
                { tooltipX + 0.012f, tooltipY + 0.005f },
                { tooltipWidth - 0.024f, tooltipHeight - 0.008f },
                6201,
                visible ? hoveredSpec->Tooltip : "",
                14.0f,
                { 0.96f, 0.98f, 0.93f, 1.0f },
                visible);
        }

        static void ApplyVNCommandBar(Scene* scene,
            const VisualNovelComponent& component,
            bool showStoryUi,
            bool autoPlay,
            bool showSettings,
            bool showHistory)
        {
            if (!scene)
                return;

            const std::string canvasTag = FindFirstCanvasTag(scene);
            EnsureVNCommandBar(scene, component, canvasTag, showStoryUi);

            const auto& specs = VNCommandButtonSpecs();
            for (size_t i = 0; i < specs.size(); ++i)
            {
                ConfigureVNCommandButton(scene,
                    component,
                    specs[i],
                    i,
                    showStoryUi,
                    IsVNCommandActive(specs[i], autoPlay, showSettings, showHistory));
            }

            SetWidgetVisible(scene, "VN_Command_Hide", false);
            SetText(scene, "VN_Command_Hide", "");
            SetButtonCommand(scene, "VN_Command_Hide", "vn:hide");
            UpdateVNCommandTooltip(scene, showStoryUi);
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

        static bool TryPlaySpriteAnimation(Scene* scene,
            const std::string& entityName,
            const std::string& clipName)
        {
            if (clipName.empty())
                return false;

            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<SpriteRendererComponent>() || !entity.HasComponent<SpriteAnimatorComponent>())
                return false;

            auto& animator = entity.GetComponent<SpriteAnimatorComponent>();
            if (animator.Clips.find(clipName) == animator.Clips.end())
                return false;

            if (animator.CurrentClipName != clipName)
            {
                animator.CurrentClipName.clear();
                animator.Play(clipName);
            }
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
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            return directory / ("slot" + std::to_string(safeSlot) + ".vnstate");
        }

        static int ParseVNSaveSlot(const std::string& value, int fallback = 1)
        {
            return std::clamp(ParseInt(value, fallback), 1, GameProgress::GetMaxSaveSlots());
        }

        static bool HasVNSaveSlot(const VisualNovelComponent& component, int slot)
        {
            return std::filesystem::exists(BuildSavePath(component, slot));
        }

        static bool HasAnySaveSlotData(const VisualNovelComponent& component, int slot)
        {
            return HasVNSaveSlot(component, slot) || GameProgress::IsSaveSlotOccupied(slot);
        }

        static void SetVNButtonPalette(Scene* scene,
            const std::string& entityName,
            const glm::vec4& normal,
            const glm::vec4& hover,
            const glm::vec4& pressed)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UIButtonComponent>())
                return;

            auto& button = entity.GetComponent<UIButtonComponent>();
            button.NormalColor = normal;
            button.HoverColor = hover;
            button.PressedColor = pressed;
        }

        static std::string BuildVNSaveSlotText(const VisualNovelComponent& component,
            int slot,
            bool saveMode)
        {
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            const bool occupied = HasAnySaveSlotData(component, safeSlot);
            std::ostringstream stream;
            stream << "槽位 ";
            if (safeSlot < 10)
                stream << "0";
            stream << safeSlot << "  " << (occupied ? "已有存档" : "空槽") << "\n";

            if (!occupied)
                stream << (saveMode ? "点击保存当前画面" : "没有可读取的存档");
            else if (saveMode)
                stream << "点击后确认是否覆盖";
            else
                stream << GameProgress::BuildSaveSlotSummary(safeSlot);

            return stream.str();
        }

        static std::string BuildSaveLoadText(const VisualNovelComponent& component,
            const VisualNovelRuntime& runtime,
            bool saveMode,
            int pendingOverwriteSlot)
        {
            std::ostringstream stream;
            stream << (saveMode ? "存档" : "读取") << "\n";
            if (saveMode)
            {
                stream << "选择一个槽位保存当前画面和成长进度。空槽直接保存；已有存档会先确认覆盖。";
                if (pendingOverwriteSlot > 0)
                    stream << "\n正在确认是否覆盖 " << pendingOverwriteSlot << " 号槽。";
            }
            else
            {
                stream << "选择已有槽位读取 VN 画面和成长进度。";
            }
            stream << "\n当前脚本: " << runtime.GetScript().GetSourcePath().filename().generic_string();
            return stream.str();
        }

        static void EnsureVNSaveLoadLayout(Scene* scene,
            const VisualNovelComponent& component,
            const VisualNovelRuntime& runtime,
            bool visible,
            bool saveMode,
            int pendingOverwriteSlot)
        {
            if (!scene)
                return;

            SetWidgetTopLeft(scene, component.SaveLoadPanelEntityName, { 0.17f, 0.10f }, { 0.66f, 0.76f });
            SetWidgetTopLeft(scene, "VN_SaveLoadIcon", { 0.21f, 0.145f }, { 0.060f, 0.082f });
            SetWidgetTopLeft(scene, "VN_SaveLoadTitle", { 0.29f, 0.145f }, { 0.34f, 0.060f });
            SetWidgetTopLeft(scene, component.SaveLoadTextEntityName, { 0.23f, 0.225f }, { 0.52f, 0.075f });
            SetWidgetTopLeft(scene, "VN_SaveLoad_Close", { 0.61f, 0.785f }, { 0.12f, 0.050f });

            SetWidgetVisible(scene, "VN_SaveLoad_SaveSlot1", false);
            SetWidgetVisible(scene, "VN_SaveLoad_LoadSlot1", false);

            SetText(scene, "VN_SaveLoadTitle", saveMode ? "保存" : "读取");
            SetTextVisible(scene,
                component.SaveLoadTextEntityName,
                BuildSaveLoadText(component, runtime, saveMode, pendingOverwriteSlot),
                visible);

            Entity scroll = EnsureVNSettingsWidget(scene,
                "VN_SaveLoadSlotScroll",
                component.SaveLoadPanelEntityName,
                { 0.075f, 0.285f },
                { 0.82f, 0.57f },
                100,
                visible);
            if (scroll)
            {
                auto& panel = scroll.HasComponent<UIPanelComponent>()
                    ? scroll.GetComponent<UIPanelComponent>()
                    : scroll.AddComponent<UIPanelComponent>();
                panel.BackgroundColor = { 0.012f, 0.014f, 0.018f, 0.34f };
                panel.BorderColor = { 0.64f, 0.52f, 0.38f, 0.34f };
                panel.BorderThickness = 1.0f;
                panel.ClipChildren = true;

                auto& scrollView = scroll.HasComponent<UIScrollViewComponent>()
                    ? scroll.GetComponent<UIScrollViewComponent>()
                    : scroll.AddComponent<UIScrollViewComponent>();
                constexpr float slotStep = 0.150f;
                scrollView.ContentHeight = 0.060f + static_cast<float>(GameProgress::GetMaxSaveSlots()) * slotStep;
                scrollView.WheelStep = 0.10f;
                scrollView.ScrollbarWidth = 0.018f;
                scrollView.EnableWheel = true;
                scrollView.ShowScrollbar = true;
                scrollView.DragScrollbar = true;
                scrollView.ClampToContent = true;
                scrollView.ClampOffset();
            }

            for (int slot = 1; slot <= GameProgress::GetMaxSaveSlots(); ++slot)
            {
                constexpr float slotStep = 0.150f;
                const std::string entityName = "VN_SaveLoad_Slot_" + std::to_string(slot);
                const bool occupied = HasAnySaveSlotData(component, slot);
                Entity slotEntity = EnsureVNSettingsButton(scene,
                    entityName,
                    "VN_SaveLoadSlotScroll",
                    { 0.035f, 0.030f + static_cast<float>(slot - 1) * slotStep },
                    { 0.89f, 0.125f },
                    120 + slot,
                    BuildVNSaveSlotText(component, slot, saveMode),
                    saveMode
                        ? "vn:saveslot:" + std::to_string(slot)
                        : "vn:loadslot:" + std::to_string(slot),
                    visible);
                if (!slotEntity)
                    continue;

                auto& text = slotEntity.GetComponent<UITextComponent>();
                text.FontSize = 16.0f;
                text.Color = occupied
                    ? glm::vec4{ 0.98f, 0.96f, 0.82f, 1.0f }
                    : glm::vec4{ 0.72f, 0.78f, 0.76f, 1.0f };
                UIRenderer::PreloadUIText(text);

                if (!occupied)
                {
                    SetVNButtonPalette(scene,
                        entityName,
                        saveMode ? glm::vec4{ 0.12f, 0.18f, 0.18f, 0.78f } : glm::vec4{ 0.08f, 0.09f, 0.10f, 0.58f },
                        saveMode ? glm::vec4{ 0.22f, 0.38f, 0.34f, 0.92f } : glm::vec4{ 0.12f, 0.14f, 0.16f, 0.70f },
                        { 0.08f, 0.11f, 0.12f, 0.96f });
                }
                else
                {
                    SetVNButtonPalette(scene,
                        entityName,
                        saveMode ? glm::vec4{ 0.26f, 0.18f, 0.10f, 0.90f } : glm::vec4{ 0.10f, 0.25f, 0.24f, 0.90f },
                        saveMode ? glm::vec4{ 0.50f, 0.34f, 0.16f, 0.98f } : glm::vec4{ 0.18f, 0.46f, 0.42f, 0.98f },
                        saveMode ? glm::vec4{ 0.18f, 0.11f, 0.06f, 1.0f } : glm::vec4{ 0.06f, 0.16f, 0.16f, 1.0f });
                }
            }

            const bool confirmVisible = visible && saveMode && pendingOverwriteSlot > 0;
            Entity confirmPanel = EnsureVNSettingsWidget(scene,
                "VN_SaveLoadConfirmPanel",
                component.SaveLoadPanelEntityName,
                { 0.20f, 0.37f },
                { 0.60f, 0.23f },
                260,
                confirmVisible);
            if (confirmPanel)
            {
                auto& panel = confirmPanel.HasComponent<UIPanelComponent>()
                    ? confirmPanel.GetComponent<UIPanelComponent>()
                    : confirmPanel.AddComponent<UIPanelComponent>();
                panel.BackgroundColor = { 0.018f, 0.020f, 0.024f, 0.96f };
                panel.BorderColor = { 0.86f, 0.66f, 0.34f, 0.96f };
                panel.BorderThickness = 2.0f;
                panel.ClipChildren = true;
            }

            EnsureVNSettingsText(scene,
                "VN_SaveLoadConfirmText",
                "VN_SaveLoadConfirmPanel",
                { 0.08f, 0.14f },
                { 0.84f, 0.32f },
                261,
                pendingOverwriteSlot > 0
                    ? "该槽位已有存档。\n是否覆盖 " + std::to_string(pendingOverwriteSlot) + " 号槽？"
                    : "",
                18.0f,
                { 0.98f, 0.94f, 0.82f, 1.0f },
                confirmVisible);
            EnsureVNSettingsButton(scene,
                "VN_SaveLoadConfirmYes",
                "VN_SaveLoadConfirmPanel",
                { 0.16f, 0.64f },
                { 0.25f, 0.22f },
                262,
                "覆盖",
                "vn:confirm_overwrite",
                confirmVisible);
            EnsureVNSettingsButton(scene,
                "VN_SaveLoadConfirmNo",
                "VN_SaveLoadConfirmPanel",
                { 0.58f, 0.64f },
                { 0.25f, 0.22f },
                262,
                "取消",
                "vn:cancel_overwrite",
                confirmVisible);
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

            const std::filesystem::path resolvedPath = AssetPath::ResolveRuntimeData(component.ScriptPath);
            if (!state.Loaded
                || state.LoadedPath != resolvedPath
                || state.LoadedAutoLoadSlot != component.AutoLoadSlot)
            {
                LoadRuntime(state, component);
            }

            component.CharactersPerSecond = static_cast<float>(UserSettings::Get().TextSpeed);
            state.Runtime.SetCharactersPerSecond(component.CharactersPerSecond);
            state.Runtime.SetAutoPlayDelay(component.AutoPlayDelay);
            const float deltaSeconds = ts.GetSeconds();
            state.SystemMessageTimer = std::max(0.0f, state.SystemMessageTimer - deltaSeconds);
            state.BGMNoticeTimer = std::max(0.0f, state.BGMNoticeTimer - deltaSeconds);

            for (const std::string& command : CommandBus::DrainGameplayCommands("vn:"))
                ExecuteCommand(scene, component, state, command);

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
                GameplayAudioService::SetBGMVolume(state.BGMHandle, 0.82f);
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

        state.BGMHandle = GameplayAudioService::PlayBGM(
            state.CurrentBGMPath,
            0.82f,
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
            panel.BackgroundColor = { 0.025f, 0.052f, 0.070f, 0.0f };
            panel.BorderColor = { 0.45f, 0.88f, 0.94f, 0.0f };
        }

        if (panelEntity && panelEntity.HasComponent<UIImageComponent>())
        {
            auto& image = panelEntity.GetComponent<UIImageComponent>();
            image.Color = { 1.0f, 1.0f, 1.0f, 0.92f * alpha };
        }

        if (textEntity && textEntity.HasComponent<UITextComponent>())
        {
            auto& text = textEntity.GetComponent<UITextComponent>();
            const std::string notice = "音乐  " + state.CurrentBGMTitle;
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
        state.LoadedPath = AssetPath::ResolveRuntimeData(component.ScriptPath);
        state.Runtime.SetCharactersPerSecond(component.CharactersPerSecond);
        state.Runtime.SetAutoPlayDelay(component.AutoPlayDelay);
        state.Loaded = state.Runtime.LoadScript(state.LoadedPath);
        state.LoadedAutoLoadSlot = component.AutoLoadSlot;
        state.ShowHistory = false;
        state.ShowSettings = false;
        state.ShowSaveLoad = false;
        state.SaveLoadSaveMode = true;
        state.PendingOverwriteSlot = 0;
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
        auto saveToSlot = [&](int slot, bool allowOverwrite)
        {
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            if (!allowOverwrite && HasAnySaveSlotData(component, safeSlot))
            {
                state.PendingOverwriteSlot = safeSlot;
                pushMessage("请确认是否覆盖 " + std::to_string(safeSlot) + " 号槽");
                return;
            }

            const std::filesystem::path savePath = BuildSavePath(component, safeSlot);
            const bool vnSaved = state.Runtime.SaveState(savePath);
            const bool progressSaved = GameProgress::SaveSlot(safeSlot);
            state.PendingOverwriteSlot = 0;

            if (vnSaved && progressSaved)
            {
                state.ShowSaveLoad = false;
                pushMessage("已保存到 " + std::to_string(safeSlot) + " 号槽");
            }
            else if (vnSaved)
            {
                pushMessage("VN 已保存，成长进度保存失败");
            }
            else
            {
                pushMessage("存档失败");
            }
        };
        auto loadFromSlot = [&](int slot)
        {
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            const std::filesystem::path savePath = BuildSavePath(component, safeSlot);
            if (state.Runtime.LoadState(savePath))
            {
                GameProgress::LoadSlot(safeSlot);
                state.ShowSaveLoad = false;
                state.PendingOverwriteSlot = 0;
                pushMessage("已读取 " + std::to_string(safeSlot) + " 号槽");
            }
            else
            {
                pushMessage(std::to_string(safeSlot) + " 号槽没有存档");
            }
        };

        if (action == "auto")
        {
            state.Runtime.ToggleAutoPlay();
            state.DialogueHidden = false;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            pushMessage(state.Runtime.IsAutoPlay() ? "自动播放已开启" : "自动播放已关闭");
            return true;
        }

        if (action == "history")
        {
            state.ShowHistory = !state.ShowHistory;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "settings")
        {
            state.ShowSettings = !state.ShowSettings;
            state.ShowHistory = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "close")
        {
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            return true;
        }

        if (action == "hide")
        {
            state.DialogueHidden = true;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            return true;
        }

        if (action == "savemenu" || action == "loadmenu")
        {
            state.ShowSaveLoad = true;
            state.SaveLoadSaveMode = action == "savemenu";
            state.PendingOverwriteSlot = 0;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "save" || action == "quicksave")
        {
            saveToSlot(1, true);
            return true;
        }

        if (action == "load" || action == "quickload")
        {
            loadFromSlot(1);
            return true;
        }

        if (action.rfind("saveslot:", 0) == 0)
        {
            state.SaveLoadSaveMode = true;
            saveToSlot(ParseVNSaveSlot(action.substr(9)), false);
            return true;
        }

        if (action.rfind("loadslot:", 0) == 0)
        {
            state.SaveLoadSaveMode = false;
            state.PendingOverwriteSlot = 0;
            loadFromSlot(ParseVNSaveSlot(action.substr(9)));
            return true;
        }

        if (action == "confirm_overwrite")
        {
            if (state.PendingOverwriteSlot > 0)
                saveToSlot(state.PendingOverwriteSlot, true);
            return true;
        }

        if (action == "cancel_overwrite")
        {
            state.PendingOverwriteSlot = 0;
            pushMessage("已取消覆盖");
            return true;
        }

        if (action == "textspeed+" || action == "speed+")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::min(180, settings.TextSpeed + 12);
            UserSettings::Save();
            UserSettings::ApplyToRuntime();
            component.CharactersPerSecond = static_cast<float>(settings.TextSpeed);
            pushMessage("文字速度提高");
            return true;
        }

        if (action == "textspeed-" || action == "speed-")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::max(12, settings.TextSpeed - 12);
            UserSettings::Save();
            UserSettings::ApplyToRuntime();
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

        const VisualNovelInputService::InputSnapshot input = VisualNovelInputService::Sample();
        const bool commandPressed = input.PrimaryMousePressed;
        state.PreviousCommandPressed = commandPressed;

        if (state.DialogueHidden)
        {
            const bool pressed = input.AdvancePressed;
            if (pressed && !state.PreviousAdvancePressed)
                state.DialogueHidden = false;
            state.PreviousAdvancePressed = pressed;
            return;
        }

        const bool autoPressed = input.AutoPressed;
        if (autoPressed && !state.PreviousAutoPressed)
            ExecuteCommand(scene, component, state, "vn:auto");
        state.PreviousAutoPressed = autoPressed;

        const bool historyPressed = input.HistoryPressed;
        if (historyPressed && !state.PreviousHistoryPressed)
            ExecuteCommand(scene, component, state, "vn:history");
        state.PreviousHistoryPressed = historyPressed;

        const bool savePressed = input.SavePressed;
        if (savePressed && !state.PreviousSavePressed)
            ExecuteCommand(scene, component, state, "vn:save");
        state.PreviousSavePressed = savePressed;

        const bool loadPressed = input.LoadPressed;
        if (loadPressed && !state.PreviousLoadPressed)
            ExecuteCommand(scene, component, state, "vn:load");
        state.PreviousLoadPressed = loadPressed;

        if (state.ShowHistory || state.ShowSettings || state.ShowSaveLoad)
        {
            state.PreviousAdvancePressed = input.AdvancePressed;
            return;
        }

        if (state.Runtime.IsWaitingForChoice())
        {
            const auto& choices = state.Runtime.GetCurrentChoices();
            const size_t maxChoices = std::min<size_t>(choices.size(), 9);

            for (size_t i = 0; i < maxChoices; ++i)
            {
                const bool pressed = input.ChoicePressed[i];
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

            const bool mousePressed = input.PrimaryMousePressed;
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

            state.PreviousAdvancePressed = input.AdvancePressed;
            return;
        }

        const bool pressed = input.AdvancePressed;
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

        ApplyVNStaticUISkin(scene, component);
        UpdateVNSettingsAudioControls(scene, component, state.ShowSettings);

        SetWidgetsWithPrefixVisible(scene, "VN_Command", showStoryUi);
        SetWidgetsWithPrefixVisible(scene, "VN_History", state.ShowHistory);
        SetWidgetsWithPrefixVisible(scene, "VN_Settings", state.ShowSettings);
        SetWidgetsWithPrefixVisible(scene, "VN_SaveLoad", state.ShowSaveLoad);

        SetWidgetVisible(scene, component.CommandBarEntityName, showStoryUi);
        SetWidgetVisible(scene, component.HistoryPanelEntityName, state.ShowHistory);
        SetWidgetVisible(scene, component.SettingsPanelEntityName, state.ShowSettings);
        SetWidgetVisible(scene, component.SaveLoadPanelEntityName, state.ShowSaveLoad);
        SetText(scene, "VN_Command_Auto", state.Runtime.IsAutoPlay() ? "自动中" : "自动");

        ApplyVNCommandBar(scene,
            component,
            showStoryUi,
            state.Runtime.IsAutoPlay(),
            state.ShowSettings,
            state.ShowHistory);

        SetTextVisible(scene, component.HistoryTextEntityName,
            BuildHistoryText(state.Runtime),
            state.ShowHistory);
        SetTextVisible(scene, component.SettingsTextEntityName,
            BuildSettingsText(component, state.Runtime),
            state.ShowSettings);
        EnsureVNSaveLoadLayout(scene,
            component,
            state.Runtime,
            state.ShowSaveLoad,
            state.SaveLoadSaveMode,
            state.PendingOverwriteSlot);

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
            if (TryPlaySpriteAnimation(scene, entityName, expression))
            {
                SetSpriteColor(scene, entityName, { 1.0f, 1.0f, 1.0f, alpha });
            }
            else if (!texturePath.empty() && TrySetSpriteTexture(scene, entityName, texturePath))
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
