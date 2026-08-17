#pragma once

#include "Wheatear/Gameplay/SystemBindingRegistry.h"

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace Wheatear {

    struct VisualNovelComponent
    {
        std::string ScriptPath = "assets/vn/demo.vn";
        float       CharactersPerSecond = 42.0f;
        bool        PlayOnStart = true;
        bool        RestartOnFinish = true;

        std::string SpeakerTextEntityName = SystemBindings::VisualNovel::SpeakerText;
        std::string BodyTextEntityName = SystemBindings::VisualNovel::BodyText;
        std::string AdvanceHintEntityName = SystemBindings::VisualNovel::AdvanceHint;
        std::string BackgroundEntityName = SystemBindings::VisualNovel::Background;
        std::string FloorEntityName = SystemBindings::VisualNovel::Floor;
        std::string CharacterEntityPrefix = SystemBindings::VisualNovel::CharacterPrefix;
        std::string ChoiceEntityPrefix = SystemBindings::VisualNovel::ChoicePrefix;
        uint32_t    MaxVisibleChoices = 3;

        bool        AutoPlayOnStart = false;
        float       AutoPlayDelay = 1.4f;
        std::string HistoryTextEntityName = SystemBindings::VisualNovel::HistoryText;
        std::string AutoPlayIndicatorEntityName = SystemBindings::VisualNovel::AutoPlayIndicator;
        std::string CommandBarEntityName = SystemBindings::VisualNovel::CommandBar;
        std::string CommandTooltipEntityName = SystemBindings::VisualNovel::CommandTooltip;
        bool        CommandTooltipFollowMouse = true;
        glm::vec2   CommandTooltipMouseOffset = { 0.018f, -0.064f };
        std::string HistoryPanelEntityName = SystemBindings::VisualNovel::HistoryPanel;
        std::string HistoryScrollEntityName = SystemBindings::VisualNovel::HistoryScroll;
        std::string SettingsPanelEntityName = SystemBindings::VisualNovel::SettingsPanel;
        std::string SettingsTextEntityName = SystemBindings::VisualNovel::SettingsText;
        std::string SaveLoadPanelEntityName = SystemBindings::VisualNovel::SaveLoadPanel;
        std::string SaveLoadTextEntityName = SystemBindings::VisualNovel::SaveLoadText;
        std::string SystemMessageEntityName = SystemBindings::VisualNovel::SystemMessage;
        std::string MusicNoticePanelEntityName = SystemBindings::VisualNovel::MusicNoticePanel;
        std::string MusicNoticeTextEntityName = SystemBindings::VisualNovel::MusicNoticePanel;
        std::string RuntimeRequestedCommand = "";

        VisualNovelComponent() = default;
        VisualNovelComponent(const VisualNovelComponent&) = default;
    };

} // namespace Wheatear
