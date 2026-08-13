#include "wtpch.h"
#include "VisualNovelSystem.h"

#include "VisualNovelInputService.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"
#include "Wheatear/Core/Input.h"
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

#include <yaml-cpp/yaml.h>

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
        using UIRuntimeTools::SetWidgetVisible;

        static constexpr float kVNCommandBarLeft = 0.295f;
        static constexpr float kVNCommandBarTop = 0.884f;
        static constexpr float kVNCommandBarWidth = 0.410f;
        static constexpr float kVNCommandBarHeight = 0.086f;
        static constexpr float kVNSkipStepInterval = 0.020f;
        static constexpr int kVNMaxSkipStepsPerFrame = 16;

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

        static void WarnMissingAuthoredVNUI(Scene* scene,
            const std::string& entityName,
            const char* missing)
        {
            if (!scene || entityName.empty() || !missing)
                return;

            static std::unordered_set<std::string> warned;
            std::ostringstream key;
            key << scene << ':' << entityName << ':' << missing;
            if (warned.insert(key.str()).second)
            {
                WT_CORE_WARN("VisualNovelSystem: '{}' is missing {}. Add it to the scene asset; runtime UI creation is disabled.",
                    entityName,
                    missing);
            }
        }

        static Entity FindAuthoredVNUIWidget(Scene* scene, const std::string& entityName)
        {
            if (!scene || entityName.empty())
                return {};

            Entity entity = FindEntityByName(scene, entityName);
            if (!entity)
            {
                WarnMissingAuthoredVNUI(scene, entityName, "entity");
                return {};
            }
            if (!entity.HasComponent<UIWidgetComponent>())
            {
                WarnMissingAuthoredVNUI(scene, entityName, "UIWidgetComponent");
                return {};
            }
            return entity;
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

        static constexpr float kVNUiAtlasWidth = 3200.0f;
        static constexpr float kVNUiAtlasHeight = 1584.0f;

        struct VNUiAtlasRegion
        {
            float X = 0.0f;
            float Y = 0.0f;
            float Width = 1.0f;
            float Height = 1.0f;
            float AtlasWidth = kVNUiAtlasWidth;
            float AtlasHeight = kVNUiAtlasHeight;
            float CellWidth = 0.0f;
            float CellHeight = 0.0f;
        };

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

        static bool TryReadVNUiAtlasRegion(const YAML::Node& node,
            float atlasWidth,
            float atlasHeight,
            VNUiAtlasRegion* outRegion)
        {
            if (!node || !outRegion)
                return false;

            const YAML::Node rect = node["rect"];
            if (!rect || !rect.IsSequence() || rect.size() < 4)
                return false;

            VNUiAtlasRegion region;
            region.X = rect[0].as<float>(0.0f);
            region.Y = rect[1].as<float>(0.0f);
            region.Width = rect[2].as<float>(1.0f);
            region.Height = rect[3].as<float>(1.0f);
            region.AtlasWidth = atlasWidth;
            region.AtlasHeight = atlasHeight;
            region.CellWidth = node["cellWidth"].as<float>(0.0f);
            region.CellHeight = node["cellHeight"].as<float>(0.0f);
            *outRegion = region;
            return true;
        }

        static const std::unordered_map<std::string, VNUiAtlasRegion>& VNUiAtlasRegions()
        {
            static const std::unordered_map<std::string, VNUiAtlasRegion> regions = []
            {
                std::unordered_map<std::string, VNUiAtlasRegion> loadedRegions;
                const std::string paramsPath = VNUiAsset("atlas_params",
                    "assets/vertical_slice/ui/atlases/ui_atlas_params.yaml");
                const std::filesystem::path resolvedPath = AssetPath::ResolveRuntimeData(paramsPath);
                if (!std::filesystem::is_regular_file(resolvedPath))
                    return loadedRegions;

                try
                {
                    const YAML::Node root = YAML::LoadFile(resolvedPath.string());
                    const YAML::Node atlas = root["vn_ui_atlas"] ? root["vn_ui_atlas"] : root;
                    const float atlasWidth = atlas["width"].as<float>(kVNUiAtlasWidth);
                    const float atlasHeight = atlas["height"].as<float>(kVNUiAtlasHeight);
                    const YAML::Node regionsNode = atlas["regions"];
                    if (!regionsNode || !regionsNode.IsMap())
                        return loadedRegions;

                    for (const auto& entry : regionsNode)
                    {
                        if (!entry.first.IsScalar())
                            continue;

                        VNUiAtlasRegion region;
                        if (TryReadVNUiAtlasRegion(entry.second, atlasWidth, atlasHeight, &region))
                            loadedRegions[entry.first.as<std::string>()] = region;
                    }
                }
                catch (const YAML::Exception& exception)
                {
                    WT_CORE_WARN("VisualNovelSystem: failed to load VN UI atlas params '{}': {}",
                        resolvedPath.string(),
                        exception.what());
                }
                return loadedRegions;
            }();
            return regions;
        }

        static VNUiAtlasRegion VNUiAtlasRegionFor(const std::string& key)
        {
            const auto& regions = VNUiAtlasRegions();
            if (auto it = regions.find(key); it != regions.end())
                return it->second;

            if (key == "textbox_panel") return { 352.0f, 82.0f, 2495.0f, 474.0f };
            if (key == "command_icons") return { 0.0f, 640.0f, 2048.0f, 512.0f, kVNUiAtlasWidth, kVNUiAtlasHeight, 256.0f, 256.0f };
            if (key == "choice_panel") return { 267.0f, 1177.0f, 1266.0f, 190.0f };
            if (key == "nameplate") return { 0.0f, 1392.0f, 720.0f, 192.0f };
            if (key == "bgm_notice_panel") return { 720.0f, 1392.0f, 1120.0f, 192.0f };
            return { 0.0f, 0.0f, kVNUiAtlasWidth, kVNUiAtlasHeight };
        }

        static VNUiAtlasRegion VNCommandIconRegion(uint32_t atlasColumn, bool highlighted)
        {
            const VNUiAtlasRegion commandIcons = VNUiAtlasRegionFor("command_icons");
            const float iconWidth = commandIcons.CellWidth > 0.0f ? commandIcons.CellWidth : 256.0f;
            const float iconHeight = commandIcons.CellHeight > 0.0f ? commandIcons.CellHeight : 256.0f;
            return {
                commandIcons.X + static_cast<float>(atlasColumn) * iconWidth,
                commandIcons.Y + (highlighted ? iconHeight : 0.0f),
                iconWidth,
                iconHeight,
                commandIcons.AtlasWidth,
                commandIcons.AtlasHeight
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
            if (!entity.HasComponent<UIImageComponent>())
            {
                WarnMissingAuthoredVNUI(scene, entityName, "UIImageComponent");
                return;
            }

            auto& image = entity.GetComponent<UIImageComponent>();
            image.Color = color;
            image.UVMin = uvMin;
            image.UVMax = uvMax;
            UIRuntimeTools::SetImageTexture(scene, entityName, texturePath, false);
        }

        static void ApplyVNUiAtlasRegion(Scene* scene,
            const std::string& entityName,
            const std::string& regionKey,
            const glm::vec4& color,
            bool onlyIfMissing = false)
        {
            if (onlyIfMissing)
            {
                Entity entity = FindEntityByName(scene, entityName);
                if (entity && entity.HasComponent<UIImageComponent>() &&
                    entity.GetComponent<UIImageComponent>().Texture)
                {
                    return;
                }
            }

            const VNUiAtlasRegion region = VNUiAtlasRegionFor(regionKey);
            ApplyUIImage(scene,
                entityName,
                VNUiAtlasAsset(),
                color,
                VNUiAtlasUVMin(region),
                VNUiAtlasUVMax(region));
        }

        struct VNCommandButtonSpec
        {
            const char* EntityName;
            uint32_t AtlasColumn;
        };

        static const std::array<VNCommandButtonSpec, 8>& VNCommandButtonSpecs()
        {
            static const std::array<VNCommandButtonSpec, 8> specs = {{
                { "VN_Command_Save", 0 },
                { "VN_Command_Load", 1 },
                { "VN_Command_QuickSave", 2 },
                { "VN_Command_QuickLoad", 3 },
                { "VN_Command_Settings", 4 },
                { "VN_Command_History", 5 },
                { "VN_Command_Auto", 6 },
                { "VN_Command_Skip", 7 },
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
            (void)parentTag;
            (void)position;
            (void)size;

            Entity entity = FindAuthoredVNUIWidget(scene, entityName);
            if (!entity)
                return {};

            auto& widget = entity.GetComponent<UIWidgetComponent>();
            widget.Visible = true;

            if (entity.HasComponent<UIPanelComponent>())
            {
                auto& panel = entity.GetComponent<UIPanelComponent>();
                panel.BackgroundColor = { 0.03f, 0.06f, 0.08f, 0.0f };
                panel.BorderColor = { 0.45f, 0.86f, 0.92f, 0.0f };
            }
            ApplyVNUiAtlasRegion(scene,
                entityName,
                "bgm_notice_panel",
                { 1.0f, 1.0f, 1.0f, 0.92f },
                true);
            return entity;
        }

        static Entity EnsureNoticeText(Scene* scene,
            const std::string& entityName,
            const std::string& parentTag,
            const glm::vec2& position,
            const glm::vec2& size)
        {
            (void)parentTag;
            (void)position;
            (void)size;

            Entity entity = FindAuthoredVNUIWidget(scene, entityName);
            if (!entity)
                return {};

            auto& widget = entity.GetComponent<UIWidgetComponent>();
            widget.Visible = true;
            if (!entity.HasComponent<UITextComponent>())
            {
                WarnMissingAuthoredVNUI(scene, entityName, "UITextComponent");
                return {};
            }

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
            (void)parentTag;
            (void)position;
            (void)size;
            (void)sortOrder;

            Entity entity = FindAuthoredVNUIWidget(scene, entityName);
            if (!entity)
                return {};

            auto& widget = entity.GetComponent<UIWidgetComponent>();
            widget.Visible = visible;
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
            (void)fontSize;
            (void)color;

            Entity entity = EnsureVNSettingsWidget(scene, entityName, parentTag, position, size, sortOrder, visible);
            if (!entity)
                return {};

            if (!entity.HasComponent<UITextComponent>())
            {
                WarnMissingAuthoredVNUI(scene, entityName, "UITextComponent");
                return {};
            }

            auto& text = entity.GetComponent<UITextComponent>();
            text.Text = value;
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
                24.0f,
                { 0.94f, 0.96f, 0.92f, 1.0f },
                visible);
            if (!entity)
                return {};

            if (!entity.HasComponent<UIButtonComponent>())
            {
                WarnMissingAuthoredVNUI(scene, entityName, "UIButtonComponent");
                return entity;
            }

            auto& button = entity.GetComponent<UIButtonComponent>();
            button.OnClickFunction = command;
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

            if (!entity.HasComponent<UISliderComponent>())
            {
                WarnMissingAuthoredVNUI(scene, entityName, "UISliderComponent");
                return entity;
            }

            auto& slider = entity.GetComponent<UISliderComponent>();
            slider.OnValueChangedFunction = command;
            return entity;
        }

        static void SetVNSettingsSliderValue(Scene* scene, const std::string& entityName, float value)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UISliderComponent>())
                return;

            auto& slider = entity.GetComponent<UISliderComponent>();
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
            const auto& settings = UserSettings::Get();
            const glm::vec4 labelColor = { 0.88f, 0.98f, 0.93f, 1.0f };
            const glm::vec2 labelSize = { 0.20f, 0.060f };
            const glm::vec2 sliderSize = { 0.20f, 0.030f };
            const glm::vec2 buttonSize = { 0.055f, 0.055f };

            EnsureVNSettingsText(scene,
                "VN_Settings_MasterVolumeLabel",
                canvasTag,
                { 0.245f, 0.382f },
                labelSize,
                88,
                "总音量 " + std::to_string(settings.MasterVolume) + "%",
                30.0f,
                labelColor,
                visible);
            EnsureVNSettingsSlider(scene,
                "VN_Settings_MasterVolumeSlider",
                canvasTag,
                { 0.470f, 0.389f },
                sliderSize,
                89,
                "progression:set_master_volume",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_MasterVolumeDown",
                canvasTag,
                { 0.690f, 0.377f },
                buttonSize,
                90,
                "-",
                "progression:master_volume_down",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_MasterVolumeUp",
                canvasTag,
                { 0.750f, 0.377f },
                buttonSize,
                90,
                "+",
                "progression:master_volume_up",
                visible);

            EnsureVNSettingsText(scene,
                "VN_Settings_BGMVolumeLabel",
                canvasTag,
                { 0.245f, 0.452f },
                labelSize,
                88,
                "音乐 " + std::to_string(settings.BGMVolume) + "%",
                30.0f,
                labelColor,
                visible);
            EnsureVNSettingsSlider(scene,
                "VN_Settings_BGMVolumeSlider",
                canvasTag,
                { 0.470f, 0.459f },
                sliderSize,
                89,
                "progression:set_bgm_volume",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_BGMVolumeDown",
                canvasTag,
                { 0.690f, 0.447f },
                buttonSize,
                90,
                "-",
                "progression:bgm_volume_down",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_BGMVolumeUp",
                canvasTag,
                { 0.750f, 0.447f },
                buttonSize,
                90,
                "+",
                "progression:bgm_volume_up",
                visible);

            EnsureVNSettingsText(scene,
                "VN_Settings_SFXVolumeLabel",
                canvasTag,
                { 0.245f, 0.522f },
                labelSize,
                88,
                "音效 " + std::to_string(settings.SFXVolume) + "%",
                30.0f,
                labelColor,
                visible);
            EnsureVNSettingsSlider(scene,
                "VN_Settings_SFXVolumeSlider",
                canvasTag,
                { 0.470f, 0.529f },
                sliderSize,
                89,
                "progression:set_sfx_volume",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_SFXVolumeDown",
                canvasTag,
                { 0.690f, 0.517f },
                buttonSize,
                90,
                "-",
                "progression:sfx_volume_down",
                visible);
            EnsureVNSettingsButton(scene,
                "VN_Settings_SFXVolumeUp",
                canvasTag,
                { 0.750f, 0.517f },
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

        static bool IsVNCommandActive(const VNCommandButtonSpec& spec,
            bool autoPlay,
            bool skipMode,
            bool showSettings,
            bool showHistory)
        {
            const std::string name = spec.EntityName;
            if (name == "VN_Command_Auto")
                return autoPlay;
            if (name == "VN_Command_Skip")
                return skipMode;
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
                { kVNCommandBarLeft, kVNCommandBarTop },
                { kVNCommandBarWidth, kVNCommandBarHeight },
                5050,
                visible);
            if (!bar)
                return;
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

            if (!entity.HasComponent<UIButtonComponent>())
            {
                WarnMissingAuthoredVNUI(scene, spec.EntityName, "UIButtonComponent");
                return;
            }

            auto& button = entity.GetComponent<UIButtonComponent>();
            if (entity.HasComponent<UITextComponent>())
                SetText(scene, spec.EntityName, "");

            const bool useHighlightedRow = highlighted || button.IsHovered || button.IsPressed;
            ApplyUIImage(scene,
                spec.EntityName,
                VNUiAtlasAsset(),
                { 1.0f, 1.0f, 1.0f, visible ? 1.0f : 0.0f },
                VNCommandIconUVMin(spec.AtlasColumn, useHighlightedRow),
                VNCommandIconUVMax(spec.AtlasColumn, useHighlightedRow));
        }

        static void ApplyVNCommandBar(Scene* scene,
            const VisualNovelComponent& component,
            bool showStoryUi,
            bool autoPlay,
            bool skipMode,
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
                    IsVNCommandActive(specs[i], autoPlay, skipMode, showSettings, showHistory));
            }

            SetWidgetVisible(scene, "VN_Command_Hide", false);
            SetText(scene, "VN_Command_Hide", "");
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

        static bool TryGetHoveredButtonTooltip(Scene* scene, const std::string& entityName, std::string& tooltipText)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity
                || !entity.HasComponent<UIWidgetComponent>()
                || !entity.GetComponent<UIWidgetComponent>().Visible
                || !entity.HasComponent<UIButtonComponent>())
                return false;

            const auto& button = entity.GetComponent<UIButtonComponent>();
            if (!button.IsHovered || button.TooltipText.empty())
                return false;

            tooltipText = button.TooltipText;
            return true;
        }

        static bool TryGetNormalizedMousePosition(Scene* scene, glm::vec2& position)
        {
            if (!scene || scene->GetViewportWidth() == 0 || scene->GetViewportHeight() == 0)
                return false;

            const auto [mouseX, mouseY] = Input::GetMousePosition();
            const glm::vec2 viewportOffset = scene->GetViewportOffset();
            position = {
                (mouseX - viewportOffset.x) / static_cast<float>(scene->GetViewportWidth()),
                (mouseY - viewportOffset.y) / static_cast<float>(scene->GetViewportHeight())
            };
            return true;
        }

        static float ClampTooltipAxis(float value, float minValue, float maxValue)
        {
            if (maxValue < minValue)
                return (minValue + maxValue) * 0.5f;
            return std::clamp(value, minValue, maxValue);
        }

        static glm::vec2 ClampTooltipPosition(const UIWidgetComponent& widget, glm::vec2 position)
        {
            const float width = std::max(widget.Size.x, 0.0f);
            const float height = std::max(widget.Size.y, 0.0f);
            const float halfWidth = width * 0.5f;
            const float halfHeight = height * 0.5f;

            float minX = 0.0f;
            float maxX = 1.0f;
            float minY = 0.0f;
            float maxY = 1.0f;

            switch (widget.Anchor)
            {
            case UIAnchor::TopLeft:
                maxX = 1.0f - width;
                maxY = 1.0f - height;
                break;
            case UIAnchor::TopCenter:
                minX = halfWidth;
                maxX = 1.0f - halfWidth;
                maxY = 1.0f - height;
                break;
            case UIAnchor::TopRight:
                minX = width;
                maxY = 1.0f - height;
                break;
            case UIAnchor::MiddleLeft:
                maxX = 1.0f - width;
                minY = halfHeight;
                maxY = 1.0f - halfHeight;
                break;
            case UIAnchor::MiddleCenter:
                minX = halfWidth;
                maxX = 1.0f - halfWidth;
                minY = halfHeight;
                maxY = 1.0f - halfHeight;
                break;
            case UIAnchor::MiddleRight:
                minX = width;
                minY = halfHeight;
                maxY = 1.0f - halfHeight;
                break;
            case UIAnchor::BottomLeft:
                maxX = 1.0f - width;
                minY = height;
                break;
            case UIAnchor::BottomCenter:
                minX = halfWidth;
                maxX = 1.0f - halfWidth;
                minY = height;
                break;
            case UIAnchor::BottomRight:
                minX = width;
                minY = height;
                break;
            }

            position.x = ClampTooltipAxis(position.x, minX, maxX);
            position.y = ClampTooltipAxis(position.y, minY, maxY);
            return position;
        }

        static void MoveVNCommandTooltipToMouse(Scene* scene, const VisualNovelComponent& component)
        {
            if (!scene || component.CommandTooltipEntityName.empty())
                return;

            Entity tooltip = FindEntityByName(scene, component.CommandTooltipEntityName);
            if (!tooltip || !tooltip.HasComponent<UIWidgetComponent>())
                return;

            glm::vec2 mousePosition;
            if (!TryGetNormalizedMousePosition(scene, mousePosition))
                return;

            auto& widget = tooltip.GetComponent<UIWidgetComponent>();
            widget.Position = ClampTooltipPosition(widget, mousePosition + component.CommandTooltipMouseOffset);
        }

        static bool IsAnyVNCommandButtonHovered(Scene* scene)
        {
            for (const auto& spec : VNCommandButtonSpecs())
            {
                if (IsEntityHoveredButton(scene, spec.EntityName))
                    return true;
            }

            return IsEntityHoveredButton(scene, "VN_Command_Hide");
        }

        static void UpdateVNCommandTooltip(Scene* scene,
            const VisualNovelComponent& component,
            bool showStoryUi)
        {
            if (!scene || component.CommandTooltipEntityName.empty())
                return;

            std::string tooltipText;
            if (showStoryUi)
            {
                for (const auto& spec : VNCommandButtonSpecs())
                {
                    if (TryGetHoveredButtonTooltip(scene, spec.EntityName, tooltipText))
                        break;
                }

                if (tooltipText.empty())
                    TryGetHoveredButtonTooltip(scene, "VN_Command_Hide", tooltipText);
            }

            if (!tooltipText.empty())
            {
                if (component.CommandTooltipFollowMouse)
                    MoveVNCommandTooltipToMouse(scene, component);
                SetTextVisible(scene, component.CommandTooltipEntityName, tooltipText, true);
                return;
            }

            if (component.CommandTooltipEntityName != component.SystemMessageEntityName)
                SetTextVisible(scene, component.CommandTooltipEntityName, "", false);
        }

        static std::filesystem::path BuildSavePath(const VisualNovelComponent& component, int slot)
        {
            return GameProgress::GetGameRuntimeSavePath(slot, component.SaveDirectory);
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
            return HasVNSaveSlot(component, slot) || GameProgress::IsGameSaveSlotOccupied(slot, component.SaveDirectory);
        }

        static std::string BuildVNSaveSlotText(const VisualNovelComponent& component,
            int slot,
            bool saveMode)
        {
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            return GameProgress::BuildGameSaveSlotButtonText(safeSlot, saveMode, component.SaveDirectory);
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

            (void)runtime;

            const bool useSlotScroll = static_cast<bool>(FindEntityByName(scene, "VN_SaveLoadSlotScroll"));
            SetWidgetVisible(scene, "VN_SaveLoad_SaveSlot1", visible && !useSlotScroll && saveMode);
            SetWidgetVisible(scene, "VN_SaveLoad_LoadSlot1", visible && !useSlotScroll && !saveMode);
            SetButtonCommand(scene, "VN_SaveLoad_SaveSlot1", "gamesave:slot_save_1");
            SetButtonCommand(scene, "VN_SaveLoad_LoadSlot1", "gamesave:load_1");

            SetText(scene, "VN_SaveLoadTitle", saveMode ? "保存" : "读取");
            SetTextVisible(scene,
                component.SaveLoadTextEntityName,
                "",
                false);

            if (useSlotScroll)
            {
                Entity scroll = EnsureVNSettingsWidget(scene,
                    "VN_SaveLoadSlotScroll",
                    component.SaveLoadPanelEntityName,
                    { 0.055f, 0.325f },
                    { 0.890f, 0.585f },
                    100,
                    visible);
                if (scroll)
                {
                    if (scroll.HasComponent<UIScrollViewComponent>())
                        scroll.GetComponent<UIScrollViewComponent>().ClampOffset();
                    else
                        WarnMissingAuthoredVNUI(scene, "VN_SaveLoadSlotScroll", "UIScrollViewComponent");
                }

                for (int slot = 1; slot <= GameProgress::GetMaxSaveSlots(); ++slot)
                {
                    constexpr float slotStep = 0.195f;
                    const std::string entityName = "VN_SaveLoad_Slot_" + std::to_string(slot);
                    Entity slotEntity = EnsureVNSettingsButton(scene,
                        entityName,
                        "VN_SaveLoadSlotScroll",
                        { 0.025f, 0.025f + static_cast<float>(slot - 1) * slotStep },
                        { 0.940f, 0.180f },
                        120 + slot,
                        BuildVNSaveSlotText(component, slot, saveMode),
                        saveMode
                            ? "gamesave:slot_save_" + std::to_string(slot)
                            : "gamesave:load_" + std::to_string(slot),
                        visible);
                    if (!slotEntity)
                        continue;
                }
            }

            const bool confirmVisible = visible && saveMode && pendingOverwriteSlot > 0;
            EnsureVNSettingsWidget(scene,
                "VN_SaveLoadConfirmPanel",
                component.SaveLoadPanelEntityName,
                { 0.20f, 0.37f },
                { 0.60f, 0.23f },
                260,
                confirmVisible);

            EnsureVNSettingsText(scene,
                "VN_SaveLoadConfirmText",
                "VN_SaveLoadConfirmPanel",
                { 0.08f, 0.14f },
                { 0.84f, 0.36f },
                261,
                pendingOverwriteSlot > 0
                    ? "该槽位已有存档。\n是否覆盖 " + std::to_string(pendingOverwriteSlot) + " 号槽？"
                    : "",
                24.0f,
                { 0.98f, 0.94f, 0.82f, 1.0f },
                confirmVisible);
            EnsureVNSettingsButton(scene,
                "VN_SaveLoadConfirmYes",
                "VN_SaveLoadConfirmPanel",
                { 0.16f, 0.64f },
                { 0.25f, 0.24f },
                262,
                "覆盖",
                "vn:confirm_overwrite",
                confirmVisible);
            EnsureVNSettingsButton(scene,
                "VN_SaveLoadConfirmNo",
                "VN_SaveLoadConfirmPanel",
                { 0.58f, 0.64f },
                { 0.25f, 0.24f },
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
                return "选项";

            return speaker;
        }

        static std::string BuildHistoryText(const VisualNovelRuntime& runtime)
        {
            const auto& history = runtime.GetHistory();
            if (history.empty())
                return "剧情回顾\n\n暂无记录。";

            std::ostringstream stream;
            stream << "剧情回顾\n\n";
            for (size_t i = 0; i < history.size(); ++i)
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

        static size_t CountTextLines(const std::string& text)
        {
            if (text.empty())
                return 0;
            return static_cast<size_t>(std::count(text.begin(), text.end(), '\n')) + 1;
        }

        static size_t EstimateHistoryLineCount(const VisualNovelRuntime& runtime,
            const std::string& historyText)
        {
            const size_t explicitLines = CountTextLines(historyText);
            const size_t entryLines = runtime.GetHistory().empty()
                ? 4
                : 3 + runtime.GetHistory().size() * 2;
            return std::max(explicitLines, entryLines);
        }

        static void UpdateHistoryScroll(Scene* scene,
            const VisualNovelComponent& component,
            const VisualNovelRuntime& runtime,
            const std::string& historyText,
            bool visible)
        {
            if (!scene || component.HistoryScrollEntityName.empty())
                return;

            Entity scrollEntity = FindEntityByName(scene, component.HistoryScrollEntityName);
            if (!scrollEntity || !scrollEntity.HasComponent<UIWidgetComponent>())
                return;

            SetWidgetVisible(scene, component.HistoryScrollEntityName, visible);

            const size_t lineCount = std::max<size_t>(EstimateHistoryLineCount(runtime, historyText), 4);
            const float contentHeight = std::max(1.0f, 0.16f + static_cast<float>(lineCount) * 0.07f);

            if (scrollEntity.HasComponent<UIScrollViewComponent>())
            {
                auto& scroll = scrollEntity.GetComponent<UIScrollViewComponent>();
                scroll.ContentHeight = contentHeight;
                scroll.ClampOffset();
            }
            else
            {
                WarnMissingAuthoredVNUI(scene, component.HistoryScrollEntityName, "UIScrollViewComponent");
            }

            Entity textEntity = FindEntityByName(scene, component.HistoryTextEntityName);
            if (textEntity && textEntity.HasComponent<UIWidgetComponent>())
                textEntity.GetComponent<UIWidgetComponent>().Size.y = contentHeight;
        }

        static std::string BuildSettingsText(const VisualNovelComponent& component,
            const VisualNovelRuntime& runtime)
        {
            std::ostringstream stream;
            stream << "视觉小说设置\n\n";
            stream << "文字速度: " << static_cast<int>(component.CharactersPerSecond) << " 字/秒\n";
            stream << "自动播放延迟: " << runtime.GetAutoPlayDelay() << " 秒\n";
            stream << "自动播放: " << (runtime.IsAutoPlay() ? "开启" : "关闭") << "\n";
            stream << "滚轮、空格和命令栏会跟随全局输入设置。";
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
            else
            {
                // Hot reload: if the .vn file changed on disk (edited in the VN
                // Script Editor), reload the script in place, preserving the
                // variable table and clamping the playback index so the designer
                // sees edits immediately without restarting Play mode.
                std::error_code error;
                const auto writeTime = std::filesystem::exists(resolvedPath, error)
                    ? std::filesystem::last_write_time(resolvedPath, error)
                    : std::filesystem::file_time_type{};
                if (writeTime != state.LastScriptWriteTime)
                {
                    // Preserve script variables across the reload so dialogue
                    // state (gold, flags, etc.) survives an in-editor edit; the
                    // playhead restarts from the top of the script.
                    const auto variables = state.Runtime.GetVariables();
                    state.Runtime.SetScript(VisualNovelScript::FromFile(resolvedPath));
                    for (const auto& [name, value] : variables)
                        state.Runtime.SetVariable(name, value);
                    state.LastScriptWriteTime = writeTime;
                    state.SystemMessage = "VN script reloaded (hot).";
                    state.SystemMessageTimer = 2.0f;
                }
            }

            component.CharactersPerSecond = static_cast<float>(UserSettings::Get().TextSpeed);
            state.Runtime.SetCharactersPerSecond(component.CharactersPerSecond);
            state.Runtime.SetAutoPlayDelay(component.AutoPlayDelay);
            const float deltaSeconds = ts.GetSeconds();
            state.SystemMessageTimer = std::max(0.0f, state.SystemMessageTimer - deltaSeconds);
            state.BGMNoticeTimer = std::max(0.0f, state.BGMNoticeTimer - deltaSeconds);

            for (const std::string& command : CommandBus::DrainGameplayCommands("vn:"))
                ExecuteCommand(scene, component, state, command);
            for (const std::string& command : CommandBus::DrainGameplayCommands("gamesave:"))
                ExecuteCommand(scene, component, state, command);

            UpdateInput(scene, component, state);

            const bool uiBlocksStory = state.ShowHistory || state.ShowSettings || state.ShowSaveLoad || state.DialogueHidden;
            if (uiBlocksStory)
            {
                StopSkip(state);
            }
            else if (state.SkipMode)
            {
                UpdateSkip(state, deltaSeconds);
            }
            else
            {
                state.Runtime.Update(deltaSeconds);
            }

            UpdateBGM(scene, component, state);
            UpdateSceneBindings(scene, component, state);
            UpdateMusicNotice(scene, component, state, deltaSeconds);
        }
    }

    VisualNovelSystem::RuntimeState& VisualNovelSystem::GetState(UUID id)
    {
        return m_RuntimeStates[id];
    }

    void VisualNovelSystem::StopSkip(RuntimeState& state)
    {
        state.SkipMode = false;
        state.SkipTimer = 0.0f;
    }

    void VisualNovelSystem::UpdateSkip(RuntimeState& state, float deltaSeconds)
    {
        if (!state.SkipMode || !state.Loaded)
            return;

        if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
        {
            StopSkip(state);
            return;
        }

        state.SkipTimer += std::max(0.0f, deltaSeconds);
        int steps = 0;
        while (state.SkipTimer >= kVNSkipStepInterval && steps < kVNMaxSkipStepsPerFrame)
        {
            state.SkipTimer -= kVNSkipStepInterval;

            if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
            {
                StopSkip(state);
                break;
            }

            state.Runtime.Advance();
            ++steps;

            if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
            {
                StopSkip(state);
                break;
            }
        }

        if (steps >= kVNMaxSkipStepsPerFrame)
            state.SkipTimer = std::min(state.SkipTimer, kVNSkipStepInterval);
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
            if (component.MusicNoticeTextEntityName != component.MusicNoticePanelEntityName)
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

        const glm::vec2 panelSize = { 0.300f, 0.058f };
        const float slide = (1.0f - alpha) * (panelSize.x * 0.72f);
        const glm::vec2 panelPosition = { 0.030f - slide, 0.047f };
        const glm::vec2 textPosition = { panelPosition.x + 0.014f, panelPosition.y + 0.012f };
        const glm::vec2 textSize = { panelSize.x - 0.026f, panelSize.y - 0.016f };
        const std::string parentTag = FindFirstCanvasTag(scene);

        Entity panelEntity = EnsureNoticePanel(
            scene,
            component.MusicNoticePanelEntityName,
            parentTag,
            panelPosition,
            panelSize);
        const bool textOnPanel = component.MusicNoticeTextEntityName.empty()
            || component.MusicNoticeTextEntityName == component.MusicNoticePanelEntityName;
        Entity textEntity = textOnPanel
            ? EnsureNoticeText(
                scene,
                component.MusicNoticePanelEntityName,
                parentTag,
                textPosition,
                textSize)
            : EnsureNoticeText(
                scene,
                component.MusicNoticeTextEntityName,
                parentTag,
                textPosition,
                textSize);

        if (panelEntity && panelEntity.HasComponent<UIWidgetComponent>())
        {
            auto& widget = panelEntity.GetComponent<UIWidgetComponent>();
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = panelPosition;
            widget.Size = panelSize;
        }

        if (!textOnPanel && textEntity && textEntity.HasComponent<UIWidgetComponent>())
        {
            auto& widget = textEntity.GetComponent<UIWidgetComponent>();
            widget.Anchor = UIAnchor::TopLeft;
            widget.Position = textPosition;
            widget.Size = textSize;
        }

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
            const std::string notice = "音乐 " + state.CurrentBGMTitle;
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
        if (!textOnPanel)
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
        StopSkip(state);
        state.PreviousChoicePressed.assign(9, false);

        if (!state.Loaded)
        {
            WT_CORE_WARN("VisualNovelSystem: failed to load script '{}'", state.LoadedPath.string());
            return false;
        }

        state.Runtime.SetAutoPlay(component.AutoPlayOnStart);

        std::error_code error;
        state.LastScriptWriteTime = std::filesystem::exists(state.LoadedPath, error)
            ? std::filesystem::last_write_time(state.LoadedPath, error)
            : std::filesystem::file_time_type{};

        if (component.AutoLoadSlot > 0)
        {
            const std::filesystem::path savePath = BuildSavePath(component, component.AutoLoadSlot);
            const bool runtimeLoaded = state.Runtime.LoadState(savePath);
            if (runtimeLoaded || GameProgress::IsGameSaveSlotOccupied(component.AutoLoadSlot, component.SaveDirectory))
                GameProgress::LoadSlot(component.AutoLoadSlot);
        }

        return true;
    }

    bool VisualNovelSystem::ExecuteCommand(Scene* scene,
        VisualNovelComponent& component,
        RuntimeState& state,
        const std::string& command)
    {
        const bool isVNCommand = StartsWith(command, "vn:");
        const bool isGameSaveCommand = StartsWith(command, "gamesave:");
        if (!isVNCommand && !isGameSaveCommand)
            return false;

        const std::string action = ToLower(command.substr(isGameSaveCommand ? 9 : 3));
        auto pushMessage = [&](const std::string& message)
        {
            state.SystemMessage = message;
            state.SystemMessageTimer = 2.0f;
        };
        auto stopSkip = [&]()
        {
            StopSkip(state);
        };
        auto saveToSlot = [&](int slot, bool allowOverwrite)
        {
            stopSkip();
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            if (!allowOverwrite && HasAnySaveSlotData(component, safeSlot))
            {
                state.PendingOverwriteSlot = safeSlot;
                pushMessage("该槽位已有存档，是否覆盖 " + std::to_string(safeSlot) + " 号槽？");
                return;
            }

            const std::filesystem::path savePath = BuildSavePath(component, safeSlot);
            const bool vnSaved = state.Runtime.SaveState(savePath);
            const bool progressSaved = GameProgress::SaveSlot(safeSlot);
            state.PendingOverwriteSlot = 0;

            if (vnSaved && progressSaved)
            {
                state.ShowSaveLoad = false;
                pushMessage("已保存到 " + std::to_string(safeSlot) + " 号槽。");
            }
            else if (vnSaved)
            {
                pushMessage("已保存到 " + std::to_string(safeSlot) + " 号槽。");
            }
            else
            {
                pushMessage("保存失败。");
            }
        };
        auto loadFromSlot = [&](int slot)
        {
            stopSkip();
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            if (!GameProgress::IsGameSaveSlotOccupied(safeSlot, component.SaveDirectory))
            {
                pushMessage("槽位 " + std::to_string(safeSlot) + " 没有存档。");
                return;
            }

            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            CommandBus::Execute(scene, GameProgress::BuildLoadGameCommand(safeSlot));
            pushMessage("正在读取 " + std::to_string(safeSlot) + " 号槽。");
        };

        if (action == "auto")
        {
            stopSkip();
            state.Runtime.ToggleAutoPlay();
            state.DialogueHidden = false;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            pushMessage(state.Runtime.IsAutoPlay() ? "自动播放已开启。" : "自动播放已关闭。");
            return true;
        }

        if (action == "history")
        {
            stopSkip();
            state.ShowHistory = !state.ShowHistory;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "settings")
        {
            stopSkip();
            state.ShowSettings = !state.ShowSettings;
            state.ShowHistory = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.DialogueHidden = false;
            return true;
        }

        if (action == "close")
        {
            stopSkip();
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            return true;
        }

        if (action == "hide")
        {
            stopSkip();
            state.DialogueHidden = true;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            return true;
        }

        if (action == "savemenu" || action == "loadmenu")
        {
            stopSkip();
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

        if (isGameSaveCommand)
        {
            if (action.rfind("slot_save_", 0) == 0)
            {
                state.SaveLoadSaveMode = true;
                saveToSlot(ParseVNSaveSlot(action.substr(10)), false);
                return true;
            }

            if (action.rfind("save_", 0) == 0)
            {
                saveToSlot(ParseVNSaveSlot(action.substr(5)), true);
                return true;
            }

            if (action.rfind("load_", 0) == 0)
            {
                state.SaveLoadSaveMode = false;
                state.PendingOverwriteSlot = 0;
                loadFromSlot(ParseVNSaveSlot(action.substr(5)));
                return true;
            }
        }

        if (action.rfind("saveslot:", 0) == 0)        {
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
            stopSkip();
            state.PendingOverwriteSlot = 0;
            pushMessage("Overwrite canceled.");
            return true;
        }

        if (action == "textspeed+" || action == "speed+")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::min(180, settings.TextSpeed + 12);
            UserSettings::Save();
            UserSettings::ApplyToRuntime();
            component.CharactersPerSecond = static_cast<float>(settings.TextSpeed);
            pushMessage("文字速度已提高。");
            return true;
        }

        if (action == "textspeed-" || action == "speed-")
        {
            auto& settings = UserSettings::Get();
            settings.TextSpeed = std::max(12, settings.TextSpeed - 12);
            UserSettings::Save();
            UserSettings::ApplyToRuntime();
            component.CharactersPerSecond = static_cast<float>(settings.TextSpeed);
            pushMessage("文字速度已降低。");
            return true;
        }

        if (action == "autodelay+")
        {
            component.AutoPlayDelay = std::min(6.0f, component.AutoPlayDelay + 0.25f);
            pushMessage("自动播放延迟已增加。");
            return true;
        }

        if (action == "autodelay-")
        {
            component.AutoPlayDelay = std::max(0.4f, component.AutoPlayDelay - 0.25f);
            pushMessage("自动播放延迟已减少。");
            return true;
        }

        if (action == "advance")
        {
            stopSkip();
            if (component.RestartOnFinish || !state.Runtime.IsFinished())
                state.Runtime.Advance();
            return true;
        }

        if (action == "skip")
        {
            if (state.SkipMode)
            {
                stopSkip();
                pushMessage("快进已关闭。");
                return true;
            }

            if (state.Runtime.IsFinished() || state.Runtime.IsWaitingForChoice())
            {
                stopSkip();
                return true;
            }

            state.Runtime.SetAutoPlay(false);
            state.DialogueHidden = false;
            state.ShowHistory = false;
            state.ShowSettings = false;
            state.ShowSaveLoad = false;
            state.PendingOverwriteSlot = 0;
            state.SkipMode = true;
            state.SkipTimer = kVNSkipStepInterval;
            pushMessage("快进已开启，遇到选项会自动停止。");
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
        const bool commandButtonMousePressed = input.PrimaryMousePressed && IsAnyVNCommandButtonHovered(scene);
        const bool advancePressed = input.AdvanceActionPressed || (input.PrimaryMousePressed && !commandButtonMousePressed);
        const bool commandPressed = input.PrimaryMousePressed;
        state.PreviousCommandPressed = commandPressed;

        if (state.DialogueHidden)
        {
            StopSkip(state);
            const bool pressed = advancePressed;
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
            StopSkip(state);
            state.PreviousAdvancePressed = advancePressed;
            return;
        }

        if (state.Runtime.IsWaitingForChoice())
        {
            StopSkip(state);
            const auto& choices = state.Runtime.GetCurrentChoices();
            // Visible indices already exclude gated-out options; input maps a
            // displayed N to the underlying script choice so Choose advances to
            // the correct TargetLabel even when earlier options are hidden.
            const std::vector<size_t> visibleIndices = CollectVisibleChoiceIndices(choices, state.Runtime);
            const size_t maxChoices = std::min<size_t>(visibleIndices.size(), 9);

            for (size_t i = 0; i < maxChoices; ++i)
            {
                const bool pressed = input.ChoicePressed[i];
                if (pressed && !state.PreviousChoicePressed[i])
                {
                    const size_t scriptIndex = visibleIndices[i];
                    if (IsExternalChoiceCommand(choices[scriptIndex].TargetLabel))
                        component.RuntimeRequestedCommand = choices[scriptIndex].TargetLabel;
                    else
                        state.Runtime.Choose(scriptIndex);
                    break;
                }
                state.PreviousChoicePressed[i] = pressed;
            }

            const bool mousePressed = input.PrimaryMousePressed && !commandButtonMousePressed;
            if (mousePressed && !state.PreviousAdvancePressed)
            {
                for (size_t i = 0; i < maxChoices; ++i)
                {
                    if (IsEntityHoveredButton(scene, component.ChoiceEntityPrefix + std::to_string(i + 1)))
                    {
                        const size_t scriptIndex = visibleIndices[i];
                        if (IsExternalChoiceCommand(choices[scriptIndex].TargetLabel))
                            component.RuntimeRequestedCommand = choices[scriptIndex].TargetLabel;
                        else
                            state.Runtime.Choose(scriptIndex);
                        break;
                    }
                }
            }

            state.PreviousAdvancePressed = advancePressed;
            return;
        }

        if (state.SkipMode)
        {
            const bool pressed = advancePressed;
            if (pressed && !state.PreviousAdvancePressed)
                StopSkip(state);
            state.PreviousAdvancePressed = pressed;
            return;
        }

        const bool pressed = advancePressed;
        if (pressed && !state.PreviousAdvancePressed)
        {
            if (component.RestartOnFinish || !state.Runtime.IsFinished())
                state.Runtime.Advance();
        }
        state.PreviousAdvancePressed = pressed;
    }

    std::vector<size_t> VisualNovelSystem::CollectVisibleChoiceIndices(
        const std::vector<VisualNovelChoice>& choices,
        const VisualNovelRuntime& runtime)
    {
        std::vector<size_t> indices;
        indices.reserve(choices.size());
        const auto& flags = GameProgress::GetState().StoryFlags;
        for (size_t i = 0; i < choices.size(); ++i)
        {
            const VisualNovelChoice& choice = choices[i];
            bool visible = true;
            if (!choice.RequiredFlag.empty())
                visible = flags.count(choice.RequiredFlag) > 0;
            else if (!choice.RequiredCondition.empty())
                visible = EvaluateVNExpression(choice.RequiredCondition, runtime.GetVariables());
            if (visible)
                indices.push_back(i);
        }
        return indices;
    }

    void VisualNovelSystem::UpdateSceneBindings(Scene* scene,
        const VisualNovelComponent& component,
        RuntimeState& state)
    {
        const VisualNovelLine* line = state.Runtime.GetCurrentLine();
        const bool showStoryUi = !state.DialogueHidden && !state.ShowHistory && !state.ShowSettings && !state.ShowSaveLoad;
        const bool waitingForChoice = showStoryUi && state.Runtime.IsWaitingForChoice();

        UpdateVNSettingsAudioControls(scene, component, state.ShowSettings);

        SetWidgetsWithPrefixVisible(scene, "VN_Command", showStoryUi);
        SetWidgetsWithPrefixVisible(scene, "VN_History", state.ShowHistory);
        SetWidgetsWithPrefixVisible(scene, "VN_Settings", state.ShowSettings);
        SetWidgetsWithPrefixVisible(scene, "VN_SaveLoad", state.ShowSaveLoad);

        SetWidgetVisible(scene, component.CommandBarEntityName, showStoryUi);
        SetWidgetVisible(scene, component.HistoryPanelEntityName, state.ShowHistory);
        SetWidgetVisible(scene, component.SettingsPanelEntityName, state.ShowSettings);
        SetWidgetVisible(scene, component.SaveLoadPanelEntityName, state.ShowSaveLoad);
        SetText(scene, "VN_Command_Auto", state.Runtime.IsAutoPlay() ? "自动开" : "自动");

        ApplyVNCommandBar(scene,
            component,
            showStoryUi,
            state.Runtime.IsAutoPlay(),
            state.SkipMode,
            state.ShowSettings,
            state.ShowHistory);

        const std::string historyText = BuildHistoryText(state.Runtime);
        UpdateHistoryScroll(scene, component, state.Runtime, historyText, state.ShowHistory);
        SetTextVisible(scene, component.HistoryTextEntityName,
            historyText,
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
        UpdateVNCommandTooltip(scene, component, showStoryUi);

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
        if (state.SkipMode)
            hint = "快进中";
        else if (waitingForChoice)
            hint = "选项";
        else if (state.Runtime.IsLineComplete())
            hint = "继续";
        SetText(scene, component.AdvanceHintEntityName, hint);

        const auto& choices = state.Runtime.GetCurrentChoices();
        // Gated choices (RequiredFlag set but flag not in StoryFlags) are filtered
        // out here and in UpdateRuntimeInputs, so the Nth visible button stays
        // stable across hidden options instead of indexing choices by raw N.
        const std::vector<size_t> visibleIndices = CollectVisibleChoiceIndices(choices, state.Runtime);
        const uint32_t maxVisibleChoices = std::min<uint32_t>(
            component.MaxVisibleChoices,
            static_cast<uint32_t>(visibleIndices.size()));

        for (uint32_t i = 0; i < component.MaxVisibleChoices; ++i)
        {
            const std::string entityName = component.ChoiceEntityPrefix + std::to_string(i + 1);
            const bool visible = waitingForChoice && i < maxVisibleChoices;
            SetWidgetVisible(scene, entityName, visible);
            if (visible)
            {
                const size_t scriptIndex = visibleIndices[i];
                const std::string choiceText = std::to_string(i + 1) + ". " + choices[scriptIndex].Text;
                PreloadTextForEntity(scene, entityName, choiceText);
                SetText(scene, entityName, choiceText);
                SetButtonCommand(scene, entityName,
                    IsExternalChoiceCommand(choices[scriptIndex].TargetLabel) ? choices[scriptIndex].TargetLabel : "");
            }
            else
            {
                SetButtonCommand(scene, entityName, "");
            }
        }

        const std::string playModeIndicator = state.SkipMode
            ? "快进中"
            : (state.Runtime.IsAutoPlay() ? "自动" : "");
        SetTextVisible(scene,
            component.AutoPlayIndicatorEntityName,
            playModeIndicator,
            !playModeIndicator.empty() && showStoryUi);

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
