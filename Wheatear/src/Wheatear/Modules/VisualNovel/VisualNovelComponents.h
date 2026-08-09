#pragma once

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

        std::string SpeakerTextEntityName = "VN_SpeakerText";
        std::string BodyTextEntityName = "VN_BodyText";
        std::string AdvanceHintEntityName = "VN_AdvanceHint";
        std::string BackgroundEntityName = "VN_Background";
        std::string FloorEntityName = "VN_Floor";
        std::string CharacterEntityPrefix = "VN_Character_";
        std::string ChoiceEntityPrefix = "VN_Choice_";
        uint32_t    MaxVisibleChoices = 3;

        bool        AutoPlayOnStart = false;
        float       AutoPlayDelay = 1.4f;
        std::string HistoryTextEntityName = "VN_HistoryText";
        std::string AutoPlayIndicatorEntityName = "VN_AutoPlayIndicator";
        std::string CommandBarEntityName = "VN_CommandBar";
        std::string CommandTooltipEntityName = "VN_CommandTooltip";
        bool        CommandTooltipFollowMouse = true;
        glm::vec2   CommandTooltipMouseOffset = { 0.018f, -0.064f };
        std::string HistoryPanelEntityName = "VN_HistoryPanel";
        std::string HistoryScrollEntityName = "VN_HistoryScroll";
        std::string SettingsPanelEntityName = "VN_SettingsPanel";
        std::string SettingsTextEntityName = "VN_SettingsText";
        std::string SaveLoadPanelEntityName = "VN_SaveLoadPanel";
        std::string SaveLoadTextEntityName = "VN_SaveLoadText";
        std::string SystemMessageEntityName = "VN_SystemMessage";
        std::string MusicNoticePanelEntityName = "VN_MusicNoticePanel";
        std::string MusicNoticeTextEntityName = "VN_MusicNoticePanel";
        std::string SaveDirectory = "assets/saves";
        int         AutoLoadSlot = 0;
        std::string RuntimeRequestedCommand = "";

        VisualNovelComponent() = default;
        VisualNovelComponent(const VisualNovelComponent&) = default;
    };

} // namespace Wheatear
