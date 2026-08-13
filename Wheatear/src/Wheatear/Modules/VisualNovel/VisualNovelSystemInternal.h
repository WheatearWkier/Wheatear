#pragma once

// Shared file-internal helpers for the Visual Novel system, extracted from
// VisualNovelSystem.cpp so per-responsibility translation units can be split
// off without duplicating UI/scene-binding logic. Inline so each TU compiles
// independently; callers use `using namespace VisualNovelSystemInternal;`.

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

namespace Wheatear::VisualNovelSystemInternal {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::IsButtonHovered;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetVisible;

        constexpr float kVNCommandBarLeft = 0.295f;
        constexpr float kVNCommandBarTop = 0.884f;
        constexpr float kVNCommandBarWidth = 0.410f;
        constexpr float kVNCommandBarHeight = 0.086f;
        constexpr float kVNSkipStepInterval = 0.020f;
        constexpr int kVNMaxSkipStepsPerFrame = 16;

        inline bool StartsWith(const std::string& value, const std::string& prefix)
        {
            return value.rfind(prefix, 0) == 0;
        }

        inline bool IsExternalChoiceCommand(const std::string& command)
        {
            return StartsWith(command, "scene:")
                || StartsWith(command, "newgame:")
                || StartsWith(command, "loadgame:")
                || StartsWith(command, "event:");
        }

        inline std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        inline int ParseInt(const std::string& value, int fallback)
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

        inline void WarnMissingAuthoredVNUI(Scene* scene,
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

        inline Entity FindAuthoredVNUIWidget(Scene* scene, const std::string& entityName)
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

        inline void SetWidgetsWithPrefixVisible(Scene* scene, const std::string& prefix, bool visible)
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

        inline std::string FindFirstCanvasTag(Scene* scene)
        {
            if (!scene)
                return {};

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TagComponent, UICanvasComponent>())
                return registry.get<TagComponent>(e).Tag;
            return {};
        }

        inline std::string VNUiAsset(const std::string& key, const std::string& fallback)
        {
            return AssetAliasRegistry::Path("vn.ui." + key, fallback);
        }

        constexpr float kVNUiAtlasWidth = 3200.0f;
        constexpr float kVNUiAtlasHeight = 1584.0f;

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

        inline std::string VNUiAtlasAsset()
        {
            return VNUiAsset("atlas", "assets/vertical_slice/ui/atlases/vn_ui_atlas.png");
        }

        inline glm::vec2 VNUiAtlasUVMin(const VNUiAtlasRegion& region)
        {
            return {
                region.X / kVNUiAtlasWidth,
                (kVNUiAtlasHeight - region.Y - region.Height) / kVNUiAtlasHeight
            };
        }

        inline glm::vec2 VNUiAtlasUVMax(const VNUiAtlasRegion& region)
        {
            return {
                (region.X + region.Width) / kVNUiAtlasWidth,
                (kVNUiAtlasHeight - region.Y) / kVNUiAtlasHeight
            };
        }

        inline bool TryReadVNUiAtlasRegion(const YAML::Node& node,
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

        inline const std::unordered_map<std::string, VNUiAtlasRegion>& VNUiAtlasRegions()
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

        inline VNUiAtlasRegion VNUiAtlasRegionFor(const std::string& key)
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

        inline VNUiAtlasRegion VNCommandIconRegion(uint32_t atlasColumn, bool highlighted)
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

        inline void ApplyUIImage(Scene* scene,
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

        inline void ApplyVNUiAtlasRegion(Scene* scene,
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

        inline const std::array<VNCommandButtonSpec, 8>& VNCommandButtonSpecs()
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

        inline glm::vec2 VNCommandIconUVMin(uint32_t atlasColumn, bool highlighted)
        {
            return VNUiAtlasUVMin(VNCommandIconRegion(atlasColumn, highlighted));
        }

        inline glm::vec2 VNCommandIconUVMax(uint32_t atlasColumn, bool highlighted)
        {
            return VNUiAtlasUVMax(VNCommandIconRegion(atlasColumn, highlighted));
        }

        inline Entity EnsureNoticePanel(Scene* scene,
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

        inline Entity EnsureNoticeText(Scene* scene,
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

        inline Entity EnsureVNSettingsWidget(Scene* scene,
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

        inline Entity EnsureVNSettingsText(Scene* scene,
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

        inline Entity EnsureVNSettingsButton(Scene* scene,
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

        inline Entity EnsureVNSettingsSlider(Scene* scene,
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

        inline void SetVNSettingsSliderValue(Scene* scene, const std::string& entityName, float value)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UISliderComponent>())
                return;

            auto& slider = entity.GetComponent<UISliderComponent>();
            if (!slider.IsDragging)
                slider.Value = std::clamp(value, slider.MinValue, slider.MaxValue);
        }

        inline void UpdateVNSettingsAudioControls(Scene* scene,
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

        inline void PreloadTextForEntity(Scene* scene, const std::string& entityName, const std::string& value)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (!entity || !entity.HasComponent<UITextComponent>() || value.empty())
                return;

            UITextComponent text = entity.GetComponent<UITextComponent>();
            text.Text = value;
            UIRenderer::PreloadUIText(text);
        }

        inline void SetButtonCommand(Scene* scene, const std::string& entityName, const std::string& command)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<UIButtonComponent>())
                entity.GetComponent<UIButtonComponent>().OnClickFunction = command;
        }

        inline bool IsVNCommandActive(const VNCommandButtonSpec& spec,
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

        inline void EnsureVNCommandBar(Scene* scene,
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

        inline void ConfigureVNCommandButton(Scene* scene,
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

        inline void ApplyVNCommandBar(Scene* scene,
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

        inline void SetTextVisible(Scene* scene,
            const std::string& entityName,
            const std::string& value,
            bool visible)
        {
            SetText(scene, entityName, value);
            SetWidgetVisible(scene, entityName, visible);
        }

        inline void SetSpriteColor(Scene* scene, const std::string& entityName, const glm::vec4& color)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<SpriteRendererComponent>())
                entity.GetComponent<SpriteRendererComponent>().Color = color;
        }

        inline std::string NormalizeAssetPath(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }

        inline std::string ResolveMusicTitle(const std::string& musicPath, const std::string& explicitTitle)
        {
            if (!explicitTitle.empty())
                return explicitTitle;

            std::string stem = std::filesystem::path(musicPath).stem().generic_string();
            std::replace(stem.begin(), stem.end(), '_', ' ');
            return stem.empty() ? musicPath : stem;
        }

        inline std::string ReplaceAll(std::string value,
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

        inline bool IsTextureReference(const std::string& value)
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

        inline Ref<Texture2D> LoadSpriteTexture(const std::string& texturePath)
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

        inline bool TrySetSpriteTexture(Scene* scene,
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

        inline bool TryPlaySpriteAnimation(Scene* scene,
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

        inline void ClearSpriteTexture(Scene* scene, const std::string& entityName)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<SpriteRendererComponent>())
                entity.GetComponent<SpriteRendererComponent>().Texture = nullptr;
        }

        inline std::string ResolveCharacterTexturePath(
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

        inline glm::vec4 ResolveBackgroundColor(const std::string& background)
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

        inline glm::vec4 ResolveFloorColor(const std::string& background)
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

        inline glm::vec4 ResolveCharacterColor(
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

        inline bool IsEntityHoveredButton(Scene* scene, const std::string& entityName)
        {
            Entity entity = FindEntityByName(scene, entityName);
            return entity
                && entity.HasComponent<UIWidgetComponent>()
                && entity.GetComponent<UIWidgetComponent>().Visible
                && entity.HasComponent<UIButtonComponent>()
                && entity.GetComponent<UIButtonComponent>().IsHovered;
        }

        inline bool TryGetHoveredButtonTooltip(Scene* scene, const std::string& entityName, std::string& tooltipText)
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

        inline bool TryGetNormalizedMousePosition(Scene* scene, glm::vec2& position)
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

        inline float ClampTooltipAxis(float value, float minValue, float maxValue)
        {
            if (maxValue < minValue)
                return (minValue + maxValue) * 0.5f;
            return std::clamp(value, minValue, maxValue);
        }

        inline glm::vec2 ClampTooltipPosition(const UIWidgetComponent& widget, glm::vec2 position)
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

        inline void MoveVNCommandTooltipToMouse(Scene* scene, const VisualNovelComponent& component)
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

        inline bool IsAnyVNCommandButtonHovered(Scene* scene)
        {
            for (const auto& spec : VNCommandButtonSpecs())
            {
                if (IsEntityHoveredButton(scene, spec.EntityName))
                    return true;
            }

            return IsEntityHoveredButton(scene, "VN_Command_Hide");
        }

        inline void UpdateVNCommandTooltip(Scene* scene,
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

        inline std::filesystem::path BuildSavePath(const VisualNovelComponent& component, int slot)
        {
            return GameProgress::GetGameRuntimeSavePath(slot, component.SaveDirectory);
        }

        inline int ParseVNSaveSlot(const std::string& value, int fallback = 1)
        {
            return std::clamp(ParseInt(value, fallback), 1, GameProgress::GetMaxSaveSlots());
        }

        inline bool HasVNSaveSlot(const VisualNovelComponent& component, int slot)
        {
            return std::filesystem::exists(BuildSavePath(component, slot));
        }

        inline bool HasAnySaveSlotData(const VisualNovelComponent& component, int slot)
        {
            return HasVNSaveSlot(component, slot) || GameProgress::IsGameSaveSlotOccupied(slot, component.SaveDirectory);
        }

        inline std::string BuildVNSaveSlotText(const VisualNovelComponent& component,
            int slot,
            bool saveMode)
        {
            const int safeSlot = std::clamp(slot, 1, GameProgress::GetMaxSaveSlots());
            return GameProgress::BuildGameSaveSlotButtonText(safeSlot, saveMode, component.SaveDirectory);
        }
        inline void EnsureVNSaveLoadLayout(Scene* scene,
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

        inline std::string ResolveSpeakerDisplayName(const VisualNovelRuntime& runtime, const std::string& speaker)
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

        inline std::string BuildHistoryText(const VisualNovelRuntime& runtime)
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

        inline size_t CountTextLines(const std::string& text)
        {
            if (text.empty())
                return 0;
            return static_cast<size_t>(std::count(text.begin(), text.end(), '\n')) + 1;
        }

        inline size_t EstimateHistoryLineCount(const VisualNovelRuntime& runtime,
            const std::string& historyText)
        {
            const size_t explicitLines = CountTextLines(historyText);
            const size_t entryLines = runtime.GetHistory().empty()
                ? 4
                : 3 + runtime.GetHistory().size() * 2;
            return std::max(explicitLines, entryLines);
        }

        inline void UpdateHistoryScroll(Scene* scene,
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

        inline std::string BuildSettingsText(const VisualNovelComponent& component,
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


} // namespace Wheatear::VisualNovelSystemInternal
