#pragma once

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Core/AssetPath.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace Wheatear::EditorCommandBuilder {

    enum class CommandKind
    {
        None,
        Scene,
        Event,
        NewGame,
        LoadGame,
        GameSaveOpenSaveMenu,
        GameSaveOpenLoadMenu,
        GameSaveSlotSave,
        GameSaveLoadSlot,
        GameSaveClose,
        GameSaveConfirmOverwrite,
        GameSaveCancelOverwrite,
        ProgressionSetFlag,
        ProgressionClearFlag,
        ProgressionSetActiveDungeon,
        ProgressionClearActiveDungeon,
        ProgressionSetChapter,
        // ui:pager:@UUID:action[:N]  - selector at parts[2], action at parts[3]
        UiPager,
        // anim:action:@UUID[:arg]     - action at parts[1], selector at parts[2]
        Anim,

        // vn: family (VisualNovelSystem) — no args unless noted
        VnAuto, VnHistory, VnSettings, VnClose, VnHide,
        VnSaveMenu, VnLoadMenu, VnQuickSave, VnQuickLoad,
        VnSaveSlot, VnLoadSlot, VnConfirmOverwrite, VnCancelOverwrite,
        VnTextSpeedUp, VnTextSpeedDown, VnAutoDelayUp, VnAutoDelayDown,
        VnAdvance, VnSkip,

        // turn: family (TurnCombatCommandService)
        TurnMenu, TurnCancel, TurnWait, TurnGuard, TurnItem, TurnSkill, TurnTarget,

        // tactic: family (TacticalCombatCommandService)
        TacticCell, TacticMenu, TacticCancel, TacticSkill,

        // side: family (SideCombatSystem) — exact strings, no args
        SideItem1, SideItem2, SideItem3, SideBasic, SideLauncher, SideMagic,
        SideSupport, SideDash, SideBreakLimit,

        // progression: long tail (GameProgress::ExecuteCommand)
        ProgUpgradeTravelerArmor, ProgLearnSelectedSkill, ProgSelectSkillNode,
        ProgEquipmentPageSlider, ProgEquipmentPage1, ProgEquipmentPage2,
        ProgSelectEquipmentSlot, ProgToggleSelectedEquipment, ProgSelectEquipment,
        ProgReset,
        ProgSelectSupportMentor, ProgSelectSupportWhiteMage, ProgSelectSupportGuard, ProgSelectSupportBlackMage,
        ProgTextSpeedUp, ProgTextSpeedDown,
        ProgMasterVolumeUp, ProgMasterVolumeDown,
        ProgBgmVolumeUp, ProgBgmVolumeDown,
        ProgSfxVolumeUp, ProgSfxVolumeDown,
        ProgToggleScreenShake, ProgToggleFullscreen,
        ProgSetTextSpeed, ProgSetMasterVolume, ProgSetBgmVolume, ProgSetSfxVolume,

        Quit,
        Raw
    };

    struct CommandSpec
    {
        CommandKind Kind = CommandKind::None;
        std::string Primary;   // scene path / event name / @UUID selector / target name / page key
        std::string Secondary; // action keyword for ui:pager & anim; clip name for anim:play
        std::string Raw;
        int Number = 1;        // slot / chapter / page index / tactic cell row
        int Number2 = 0;       // tactic cell column
        float FloatValue = 0.0f; // seek seconds / slider value
    };

    struct CommandAssetCache
    {
        bool ScenesLoaded = false;
        bool EventsLoaded = false;
        std::vector<std::string> Scenes;
        std::vector<std::string> Events;
    };

    inline CommandAssetCache& GetAssetCache()
    {
        static CommandAssetCache cache;
        return cache;
    }

    inline void RefreshAssetChoices()
    {
        GetAssetCache() = {};
    }

    inline bool StartsWith(const std::string& value, const std::string& prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    inline std::string PayloadAfter(const std::string& value, const std::string& prefix)
    {
        return StartsWith(value, prefix) ? value.substr(prefix.size()) : std::string{};
    }

    inline std::string Trim(std::string value)
    {
        const size_t start = value.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return {};

        const size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }

    inline std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    inline std::string StripEventName(std::string value)
    {
        value = Trim(value);
        if (!value.empty() && value.back() == ':')
            value.pop_back();
        return Trim(value);
    }

    inline bool TryParsePositiveInt(const std::string& value, int& result)
    {
        if (value.empty())
            return false;

        try
        {
            size_t parsed = 0;
            const int parsedValue = std::stoi(value, &parsed);
            if (parsed != value.size() || parsedValue < 1)
                return false;

            result = parsedValue;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    inline std::vector<std::string> CollectProjectFiles(const std::string& rootAssetPath, const std::string& extension)
    {
        std::vector<std::string> result;
        const std::filesystem::path root = AssetPath::Resolve(rootAssetPath);
        if (root.empty() || !std::filesystem::exists(root))
            return result;

        try
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            {
                if (!entry.is_regular_file() || entry.path().extension() != extension)
                    continue;

                result.push_back(AssetPath::ToProjectRelative(entry.path()).generic_string());
            }
        }
        catch (...)
        {
        }

        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    inline const std::vector<std::string>& SceneChoices()
    {
        auto& cache = GetAssetCache();
        if (!cache.ScenesLoaded)
        {
            cache.Scenes = CollectProjectFiles("assets/scenes", ".wt");
            cache.ScenesLoaded = true;
        }
        return cache.Scenes;
    }

    inline const std::vector<std::string>& EventChoices()
    {
        auto& cache = GetAssetCache();
        if (cache.EventsLoaded)
            return cache.Events;

        std::set<std::string> events;
        for (const std::string& sourcePath : CollectProjectFiles("assets/events", ".wts"))
        {
            std::ifstream input(AssetPath::Resolve(sourcePath), std::ios::binary);
            if (!input)
                continue;

            std::string line;
            while (std::getline(input, line))
            {
                const std::string trimmed = Trim(line);
                const std::string lower = ToLower(trimmed);
                if (StartsWith(lower, "event "))
                {
                    const std::string eventName = StripEventName(trimmed.substr(6));
                    if (!eventName.empty())
                        events.insert(eventName);
                }
                else if (StartsWith(lower, "event:"))
                {
                    const std::string eventName = StripEventName(trimmed.substr(6));
                    if (!eventName.empty())
                        events.insert(eventName);
                }
            }
        }

        cache.Events.assign(events.begin(), events.end());
        cache.EventsLoaded = true;
        return cache.Events;
    }

    inline const char* CommandKindLabel(CommandKind kind)
    {
        switch (kind)
        {
        case CommandKind::None: return "None";
        case CommandKind::Scene: return "Scene Load";
        case CommandKind::Event: return "Event";
        case CommandKind::NewGame: return "New Game";
        case CommandKind::LoadGame: return "Load Game";
        case CommandKind::GameSaveOpenSaveMenu: return "Open Save Menu";
        case CommandKind::GameSaveOpenLoadMenu: return "Open Load Menu";
        case CommandKind::GameSaveSlotSave: return "Save Slot";
        case CommandKind::GameSaveLoadSlot: return "Load Slot";
        case CommandKind::GameSaveClose: return "Close Save UI";
        case CommandKind::GameSaveConfirmOverwrite: return "Confirm Overwrite";
        case CommandKind::GameSaveCancelOverwrite: return "Cancel Overwrite";
        case CommandKind::ProgressionSetFlag: return "Set Story Flag";
        case CommandKind::ProgressionClearFlag: return "Clear Story Flag";
        case CommandKind::ProgressionSetActiveDungeon: return "Set Active Dungeon";
        case CommandKind::ProgressionClearActiveDungeon: return "Clear Active Dungeon";
        case CommandKind::ProgressionSetChapter: return "Set Chapter";
        case CommandKind::UiPager: return "UI Pager";
        case CommandKind::Anim: return "Animation";
        case CommandKind::VnAuto: return "VN: Toggle Auto";
        case CommandKind::VnHistory: return "VN: Toggle History";
        case CommandKind::VnSettings: return "VN: Toggle Settings";
        case CommandKind::VnClose: return "VN: Close Panels";
        case CommandKind::VnHide: return "VN: Hide Dialogue";
        case CommandKind::VnSaveMenu: return "VN: Open Save Menu";
        case CommandKind::VnLoadMenu: return "VN: Open Load Menu";
        case CommandKind::VnQuickSave: return "VN: Quick Save";
        case CommandKind::VnQuickLoad: return "VN: Quick Load";
        case CommandKind::VnSaveSlot: return "VN: Save Slot";
        case CommandKind::VnLoadSlot: return "VN: Load Slot";
        case CommandKind::VnConfirmOverwrite: return "VN: Confirm Overwrite";
        case CommandKind::VnCancelOverwrite: return "VN: Cancel Overwrite";
        case CommandKind::VnTextSpeedUp: return "VN: Text Speed +";
        case CommandKind::VnTextSpeedDown: return "VN: Text Speed -";
        case CommandKind::VnAutoDelayUp: return "VN: Auto Delay +";
        case CommandKind::VnAutoDelayDown: return "VN: Auto Delay -";
        case CommandKind::VnAdvance: return "VN: Advance";
        case CommandKind::VnSkip: return "VN: Toggle Skip";
        case CommandKind::TurnMenu: return "Turn: Menu Page";
        case CommandKind::TurnCancel: return "Turn: Cancel";
        case CommandKind::TurnWait: return "Turn: Wait";
        case CommandKind::TurnGuard: return "Turn: Guard";
        case CommandKind::TurnItem: return "Turn: Use Item";
        case CommandKind::TurnSkill: return "Turn: Use Skill";
        case CommandKind::TurnTarget: return "Turn: Select Target";
        case CommandKind::TacticCell: return "Tactic: Select Cell";
        case CommandKind::TacticMenu: return "Tactic: Menu Page";
        case CommandKind::TacticCancel: return "Tactic: Cancel";
        case CommandKind::TacticSkill: return "Tactic: Use Skill";
        case CommandKind::SideItem1: return "Side: Item Slot 1";
        case CommandKind::SideItem2: return "Side: Item Slot 2";
        case CommandKind::SideItem3: return "Side: Item Slot 3";
        case CommandKind::SideBasic: return "Side: Basic Attack";
        case CommandKind::SideLauncher: return "Side: Launcher";
        case CommandKind::SideMagic: return "Side: Magic Bolt";
        case CommandKind::SideSupport: return "Side: Support";
        case CommandKind::SideDash: return "Side: Dash";
        case CommandKind::SideBreakLimit: return "Side: Break Limit";
        case CommandKind::ProgUpgradeTravelerArmor: return "Progression: Upgrade Armor";
        case CommandKind::ProgLearnSelectedSkill: return "Progression: Learn Skill";
        case CommandKind::ProgSelectSkillNode: return "Progression: Select Skill Node";
        case CommandKind::ProgEquipmentPageSlider: return "Progression: Equipment Page Slider";
        case CommandKind::ProgEquipmentPage1: return "Progression: Equipment Page 1";
        case CommandKind::ProgEquipmentPage2: return "Progression: Equipment Page 2";
        case CommandKind::ProgSelectEquipmentSlot: return "Progression: Select Equip Slot";
        case CommandKind::ProgToggleSelectedEquipment: return "Progression: Toggle Equip";
        case CommandKind::ProgSelectEquipment: return "Progression: Select Equipment";
        case CommandKind::ProgReset: return "Progression: Reset (New Game)";
        case CommandKind::ProgSelectSupportMentor: return "Progression: Support Mentor";
        case CommandKind::ProgSelectSupportWhiteMage: return "Progression: Support White Mage";
        case CommandKind::ProgSelectSupportGuard: return "Progression: Support Guard";
        case CommandKind::ProgSelectSupportBlackMage: return "Progression: Support Black Mage";
        case CommandKind::ProgTextSpeedUp: return "Settings: Text Speed +";
        case CommandKind::ProgTextSpeedDown: return "Settings: Text Speed -";
        case CommandKind::ProgMasterVolumeUp: return "Settings: Master Volume +";
        case CommandKind::ProgMasterVolumeDown: return "Settings: Master Volume -";
        case CommandKind::ProgBgmVolumeUp: return "Settings: BGM Volume +";
        case CommandKind::ProgBgmVolumeDown: return "Settings: BGM Volume -";
        case CommandKind::ProgSfxVolumeUp: return "Settings: SFX Volume +";
        case CommandKind::ProgSfxVolumeDown: return "Settings: SFX Volume -";
        case CommandKind::ProgToggleScreenShake: return "Settings: Toggle Screen Shake";
        case CommandKind::ProgToggleFullscreen: return "Settings: Toggle Fullscreen";
        case CommandKind::ProgSetTextSpeed: return "Settings: Set Text Speed";
        case CommandKind::ProgSetMasterVolume: return "Settings: Set Master Volume";
        case CommandKind::ProgSetBgmVolume: return "Settings: Set BGM Volume";
        case CommandKind::ProgSetSfxVolume: return "Settings: Set SFX Volume";
        case CommandKind::Quit: return "Quit";
        case CommandKind::Raw: return "Raw Command";
        }
        return "Raw Command";
    }

    inline std::string FormatSeek(float seconds)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << std::max(0.0f, seconds);
        return stream.str();
    }

    inline std::string FormatFloat(float value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(1) << value;
        return stream.str();
    }

    inline std::string BuildCommand(const CommandSpec& spec)
    {
        switch (spec.Kind)
        {
        case CommandKind::None: return {};
        case CommandKind::Scene: return "scene:" + spec.Primary;
        case CommandKind::Event: return "event:" + spec.Primary;
        case CommandKind::NewGame: return "newgame:" + spec.Primary;
        case CommandKind::LoadGame: return "loadgame:" + spec.Primary + ":" + std::to_string(std::max(1, spec.Number));
        case CommandKind::GameSaveOpenSaveMenu: return "gamesave:open_save_menu";
        case CommandKind::GameSaveOpenLoadMenu: return "gamesave:open_load_menu";
        case CommandKind::GameSaveSlotSave: return "gamesave:slot_save_" + std::to_string(std::max(1, spec.Number));
        case CommandKind::GameSaveLoadSlot: return "gamesave:load_" + std::to_string(std::max(1, spec.Number));
        case CommandKind::GameSaveClose: return "gamesave:close";
        case CommandKind::GameSaveConfirmOverwrite: return "gamesave:confirm_overwrite";
        case CommandKind::GameSaveCancelOverwrite: return "gamesave:cancel_overwrite";
        case CommandKind::ProgressionSetFlag: return "progression:set_flag:" + spec.Primary;
        case CommandKind::ProgressionClearFlag: return "progression:clear_flag:" + spec.Primary;
        case CommandKind::ProgressionSetActiveDungeon: return "progression:set_active_dungeon:" + spec.Primary;
        case CommandKind::ProgressionClearActiveDungeon: return "progression:clear_active_dungeon";
        case CommandKind::ProgressionSetChapter: return "progression:set_chapter:" + std::to_string(std::max(1, spec.Number));
        case CommandKind::UiPager:
        {
            // Grammar: ui:pager:@UUID:action[:page]
            std::string action = spec.Secondary.empty() ? "next" : spec.Secondary;
            std::string result = "ui:pager:" + spec.Primary + ":" + action;
            if (action == "page" || action == "set")
                result += ":" + std::to_string(std::max(1, spec.Number));
            return result;
        }
        case CommandKind::Anim:
        {
            // Grammar: anim:action:@UUID[:arg]  - action FIRST, selector SECOND (opposite of ui:pager)
            std::string action = spec.Secondary.empty() ? "play" : spec.Secondary;
            if (action == "seek")
                return "anim:seek:" + spec.Primary + ":" + FormatSeek(spec.FloatValue);
            if (action == "play")
            {
                if (spec.Raw.empty())
                    return "anim:play:" + spec.Primary;
                return "anim:play:" + spec.Primary + ":" + spec.Raw;
            }
            return "anim:" + action + ":" + spec.Primary;
        }
                // ---- vn: family ----
        case CommandKind::VnAuto: return "vn:auto";
        case CommandKind::VnHistory: return "vn:history";
        case CommandKind::VnSettings: return "vn:settings";
        case CommandKind::VnClose: return "vn:close";
        case CommandKind::VnHide: return "vn:hide";
        case CommandKind::VnSaveMenu: return "vn:savemenu";
        case CommandKind::VnLoadMenu: return "vn:loadmenu";
        case CommandKind::VnQuickSave: return "vn:quicksave";
        case CommandKind::VnQuickLoad: return "vn:quickload";
        case CommandKind::VnSaveSlot: return "vn:saveslot:" + std::to_string(std::max(1, spec.Number));
        case CommandKind::VnLoadSlot: return "vn:loadslot:" + std::to_string(std::max(1, spec.Number));
        case CommandKind::VnConfirmOverwrite: return "vn:confirm_overwrite";
        case CommandKind::VnCancelOverwrite: return "vn:cancel_overwrite";
        case CommandKind::VnTextSpeedUp: return "vn:textspeed+";
        case CommandKind::VnTextSpeedDown: return "vn:textspeed-";
        case CommandKind::VnAutoDelayUp: return "vn:autodelay+";
        case CommandKind::VnAutoDelayDown: return "vn:autodelay-";
        case CommandKind::VnAdvance: return "vn:advance";
        case CommandKind::VnSkip: return "vn:skip";

        // ---- turn: family ----
        case CommandKind::TurnMenu: return "turn:menu:" + spec.Primary;
        case CommandKind::TurnCancel: return "turn:cancel";
        case CommandKind::TurnWait: return "turn:wait";
        case CommandKind::TurnGuard: return "turn:guard";
        case CommandKind::TurnItem: return "turn:item:" + spec.Primary;
        case CommandKind::TurnSkill: return "turn:skill:" + spec.Primary;
        case CommandKind::TurnTarget: return "turn:target:" + spec.Primary;

        // ---- tactic: family ----
        case CommandKind::TacticCell:
            return "tactic:cell:" + std::to_string(std::max(0, spec.Number))
                + ":" + std::to_string(std::max(0, spec.Number2));
        case CommandKind::TacticMenu: return "tactic:menu:" + spec.Primary;
        case CommandKind::TacticCancel: return "tactic:cancel";
        case CommandKind::TacticSkill: return "tactic:skill:" + spec.Primary;

        // ---- side: family ----
        case CommandKind::SideItem1: return "side:item:1";
        case CommandKind::SideItem2: return "side:item:2";
        case CommandKind::SideItem3: return "side:item:3";
        case CommandKind::SideBasic: return "side:basic";
        case CommandKind::SideLauncher: return "side:launcher";
        case CommandKind::SideMagic: return "side:magic";
        case CommandKind::SideSupport: return "side:support";
        case CommandKind::SideDash: return "side:dash";
        case CommandKind::SideBreakLimit: return "side:break_limit";

        // ---- progression: long tail ----
        case CommandKind::ProgUpgradeTravelerArmor: return "progression:upgrade_traveler_armor";
        case CommandKind::ProgLearnSelectedSkill: return "progression:learn_selected_skill";
        case CommandKind::ProgSelectSkillNode: return "progression:select_skill_node:" + spec.Primary;
        case CommandKind::ProgEquipmentPageSlider:
            return "progression:equipment_page_slider:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgEquipmentPage1: return "progression:equipment_page_1";
        case CommandKind::ProgEquipmentPage2: return "progression:equipment_page_2";
        case CommandKind::ProgSelectEquipmentSlot: return "progression:select_equipment_slot:" + spec.Primary;
        case CommandKind::ProgToggleSelectedEquipment: return "progression:toggle_selected_equipment";
        case CommandKind::ProgSelectEquipment: return "progression:select_equipment_" + spec.Primary;
        case CommandKind::ProgReset: return "progression:reset";
        case CommandKind::ProgSelectSupportMentor: return "progression:select_support_mentor";
        case CommandKind::ProgSelectSupportWhiteMage: return "progression:select_support_white_mage";
        case CommandKind::ProgSelectSupportGuard: return "progression:select_support_guard";
        case CommandKind::ProgSelectSupportBlackMage: return "progression:select_support_black_mage";
        case CommandKind::ProgTextSpeedUp: return "progression:text_speed_up";
        case CommandKind::ProgTextSpeedDown: return "progression:text_speed_down";
        case CommandKind::ProgMasterVolumeUp: return "progression:master_volume_up";
        case CommandKind::ProgMasterVolumeDown: return "progression:master_volume_down";
        case CommandKind::ProgBgmVolumeUp: return "progression:bgm_volume_up";
        case CommandKind::ProgBgmVolumeDown: return "progression:bgm_volume_down";
        case CommandKind::ProgSfxVolumeUp: return "progression:sfx_volume_up";
        case CommandKind::ProgSfxVolumeDown: return "progression:sfx_volume_down";
        case CommandKind::ProgToggleScreenShake: return "progression:toggle_screen_shake";
        case CommandKind::ProgToggleFullscreen: return "progression:toggle_fullscreen";
        case CommandKind::ProgSetTextSpeed: return "progression:set_text_speed:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgSetMasterVolume: return "progression:set_master_volume:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgSetBgmVolume: return "progression:set_bgm_volume:" + FormatFloat(spec.FloatValue);
        case CommandKind::ProgSetSfxVolume: return "progression:set_sfx_volume:" + FormatFloat(spec.FloatValue);

        case CommandKind::Quit: return "quit";
        case CommandKind::Raw: return spec.Raw;
        }
        return spec.Raw;
    }

    inline CommandSpec ParseCommand(const std::string& command)
    {
        CommandSpec spec;
        spec.Raw = command;

        // Local split-on-':' mirroring CommandBus::SplitCommand (runtime, CommandBus.cpp).
        auto splitColon = [](const std::string& value)
        {
            std::vector<std::string> parts;
            std::string current;
            for (char c : value)
            {
                if (c == ':')
                {
                    parts.push_back(current);
                    current.clear();
                }
                else
                    current.push_back(c);
            }
            parts.push_back(current);
            return parts;
        };

        if (command.empty())
            return spec;

        if (command == "quit")
        {
            spec.Kind = CommandKind::Quit;
            return spec;
        }

        if (StartsWith(command, "scene:"))
        {
            spec.Kind = CommandKind::Scene;
            spec.Primary = PayloadAfter(command, "scene:");
            return spec;
        }

        if (StartsWith(command, "event:"))
        {
            spec.Kind = CommandKind::Event;
            spec.Primary = PayloadAfter(command, "event:");
            return spec;
        }

        if (StartsWith(command, "newgame:"))
        {
            spec.Kind = CommandKind::NewGame;
            spec.Primary = PayloadAfter(command, "newgame:");
            return spec;
        }

        if (StartsWith(command, "loadgame:"))
        {
            spec.Kind = CommandKind::LoadGame;
            spec.Primary = PayloadAfter(command, "loadgame:");
            const size_t slotSeparator = spec.Primary.rfind(':');
            if (slotSeparator != std::string::npos)
            {
                int parsedSlot = 1;
                if (TryParsePositiveInt(spec.Primary.substr(slotSeparator + 1), parsedSlot))
                {
                    spec.Number = parsedSlot;
                    spec.Primary = spec.Primary.substr(0, slotSeparator);
                }
            }
            return spec;
        }

        if (command == "gamesave:open_save_menu")
        {
            spec.Kind = CommandKind::GameSaveOpenSaveMenu;
            return spec;
        }

        if (command == "gamesave:open_load_menu")
        {
            spec.Kind = CommandKind::GameSaveOpenLoadMenu;
            return spec;
        }

        if (command == "gamesave:close")
        {
            spec.Kind = CommandKind::GameSaveClose;
            return spec;
        }

        if (command == "gamesave:confirm_overwrite")
        {
            spec.Kind = CommandKind::GameSaveConfirmOverwrite;
            return spec;
        }

        if (command == "gamesave:cancel_overwrite")
        {
            spec.Kind = CommandKind::GameSaveCancelOverwrite;
            return spec;
        }

        if (StartsWith(command, "gamesave:slot_save_"))
        {
            spec.Kind = CommandKind::GameSaveSlotSave;
            TryParsePositiveInt(PayloadAfter(command, "gamesave:slot_save_"), spec.Number);
            return spec;
        }

        if (StartsWith(command, "gamesave:load_"))
        {
            spec.Kind = CommandKind::GameSaveLoadSlot;
            TryParsePositiveInt(PayloadAfter(command, "gamesave:load_"), spec.Number);
            return spec;
        }

        if (StartsWith(command, "progression:set_flag:"))
        {
            spec.Kind = CommandKind::ProgressionSetFlag;
            spec.Primary = PayloadAfter(command, "progression:set_flag:");
            return spec;
        }

        if (StartsWith(command, "progression:clear_flag:"))
        {
            spec.Kind = CommandKind::ProgressionClearFlag;
            spec.Primary = PayloadAfter(command, "progression:clear_flag:");
            return spec;
        }

        if (StartsWith(command, "progression:set_active_dungeon:"))
        {
            spec.Kind = CommandKind::ProgressionSetActiveDungeon;
            spec.Primary = PayloadAfter(command, "progression:set_active_dungeon:");
            return spec;
        }

        if (command == "progression:clear_active_dungeon")
        {
            spec.Kind = CommandKind::ProgressionClearActiveDungeon;
            return spec;
        }

        if (StartsWith(command, "progression:set_chapter:"))
        {
            spec.Kind = CommandKind::ProgressionSetChapter;
            TryParsePositiveInt(PayloadAfter(command, "progression:set_chapter:"), spec.Number);
            return spec;
        }

        // Grammar: ui:pager:@UUID:action[:page]  (selector at parts[2], action at parts[3])
        if (StartsWith(command, "ui:pager:"))
        {
            spec.Kind = CommandKind::UiPager;
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 3) spec.Primary = parts[2];        // @UUID
            if (parts.size() >= 4) spec.Secondary = parts[3];     // action
            if (parts.size() >= 5 && (parts[3] == "page" || parts[3] == "set"))
                TryParsePositiveInt(parts[4], spec.Number);
            return spec;
        }

        // Grammar: anim:action:@UUID[:arg]  (action at parts[1], selector at parts[2])
        if (StartsWith(command, "anim:"))
        {
            spec.Kind = CommandKind::Anim;
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 2) spec.Secondary = parts[1];     // action
            if (parts.size() >= 3) spec.Primary = parts[2];       // @UUID
            if (parts.size() >= 4)
            {
                if (spec.Secondary == "play")
                    spec.Raw = parts[3];                          // clip name
                else if (spec.Secondary == "seek")
                {
                    try { spec.FloatValue = std::stof(parts[3]); }
                    catch (...) { spec.FloatValue = 0.0f; }
                }
            }
            return spec;
        }

        // ---- vn: family ----
        if (StartsWith(command, "vn:"))
        {
            const std::string action = command.substr(3);
            if (action == "auto") spec.Kind = CommandKind::VnAuto;
            else if (action == "history") spec.Kind = CommandKind::VnHistory;
            else if (action == "settings") spec.Kind = CommandKind::VnSettings;
            else if (action == "close") spec.Kind = CommandKind::VnClose;
            else if (action == "hide") spec.Kind = CommandKind::VnHide;
            else if (action == "savemenu") spec.Kind = CommandKind::VnSaveMenu;
            else if (action == "loadmenu") spec.Kind = CommandKind::VnLoadMenu;
            else if (action == "save" || action == "quicksave") spec.Kind = CommandKind::VnQuickSave;
            else if (action == "load" || action == "quickload") spec.Kind = CommandKind::VnQuickLoad;
            else if (StartsWith(action, "saveslot:"))
            {
                spec.Kind = CommandKind::VnSaveSlot;
                TryParsePositiveInt(action.substr(9), spec.Number);
            }
            else if (StartsWith(action, "loadslot:"))
            {
                spec.Kind = CommandKind::VnLoadSlot;
                TryParsePositiveInt(action.substr(9), spec.Number);
            }
            else if (action == "confirm_overwrite") spec.Kind = CommandKind::VnConfirmOverwrite;
            else if (action == "cancel_overwrite") spec.Kind = CommandKind::VnCancelOverwrite;
            else if (action == "textspeed+" || action == "speed+") spec.Kind = CommandKind::VnTextSpeedUp;
            else if (action == "textspeed-" || action == "speed-") spec.Kind = CommandKind::VnTextSpeedDown;
            else if (action == "autodelay+") spec.Kind = CommandKind::VnAutoDelayUp;
            else if (action == "autodelay-") spec.Kind = CommandKind::VnAutoDelayDown;
            else if (action == "advance") spec.Kind = CommandKind::VnAdvance;
            else if (action == "skip") spec.Kind = CommandKind::VnSkip;
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- turn: family ----
        if (StartsWith(command, "turn:"))
        {
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 2)
            {
                const std::string action = parts[1];
                if (action == "menu" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnMenu;
                    spec.Primary = parts[2];
                }
                else if (action == "cancel") spec.Kind = CommandKind::TurnCancel;
                else if (action == "wait") spec.Kind = CommandKind::TurnWait;
                else if (action == "guard") spec.Kind = CommandKind::TurnGuard;
                else if (action == "item" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnItem;
                    spec.Primary = parts[2];
                }
                else if (action == "skill" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnSkill;
                    spec.Primary = parts[2];
                }
                else if (action == "target" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TurnTarget;
                    spec.Primary = parts[2];
                }
                else spec.Kind = CommandKind::Raw;
            }
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- tactic: family ----
        if (StartsWith(command, "tactic:"))
        {
            const std::vector<std::string> parts = splitColon(command);
            if (parts.size() >= 2)
            {
                const std::string action = parts[1];
                if (action == "cell" && parts.size() >= 4)
                {
                    spec.Kind = CommandKind::TacticCell;
                    try { spec.Number = std::stoi(parts[2]); } catch (...) { spec.Number = 0; }
                    try { spec.Number2 = std::stoi(parts[3]); } catch (...) { spec.Number2 = 0; }
                }
                else if (action == "menu" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TacticMenu;
                    spec.Primary = parts[2];
                }
                else if (action == "cancel") spec.Kind = CommandKind::TacticCancel;
                else if (action == "skill" && parts.size() >= 3)
                {
                    spec.Kind = CommandKind::TacticSkill;
                    spec.Primary = parts[2];
                }
                else spec.Kind = CommandKind::Raw;
            }
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- side: family ----
        if (StartsWith(command, "side:"))
        {
            if (command == "side:item:1") spec.Kind = CommandKind::SideItem1;
            else if (command == "side:item:2") spec.Kind = CommandKind::SideItem2;
            else if (command == "side:item:3") spec.Kind = CommandKind::SideItem3;
            else if (command == "side:basic") spec.Kind = CommandKind::SideBasic;
            else if (command == "side:launcher") spec.Kind = CommandKind::SideLauncher;
            else if (command == "side:magic") spec.Kind = CommandKind::SideMagic;
            else if (command == "side:support") spec.Kind = CommandKind::SideSupport;
            else if (command == "side:dash") spec.Kind = CommandKind::SideDash;
            else if (command == "side:break_limit") spec.Kind = CommandKind::SideBreakLimit;
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        // ---- progression: long tail ----
        if (StartsWith(command, "progression:"))
        {
            const std::string action = command.substr(12);
            if (action == "upgrade_traveler_armor") spec.Kind = CommandKind::ProgUpgradeTravelerArmor;
            else if (action == "learn_selected_skill") spec.Kind = CommandKind::ProgLearnSelectedSkill;
            else if (StartsWith(action, "select_skill_node:"))
            {
                spec.Kind = CommandKind::ProgSelectSkillNode;
                spec.Primary = action.substr(18);
            }
            else if (StartsWith(action, "equipment_page_slider:"))
            {
                spec.Kind = CommandKind::ProgEquipmentPageSlider;
                try { spec.FloatValue = std::stof(action.substr(22)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else if (action == "equipment_page_1") spec.Kind = CommandKind::ProgEquipmentPage1;
            else if (action == "equipment_page_2") spec.Kind = CommandKind::ProgEquipmentPage2;
            else if (StartsWith(action, "select_equipment_slot:"))
            {
                spec.Kind = CommandKind::ProgSelectEquipmentSlot;
                spec.Primary = action.substr(22);
            }
            else if (action == "toggle_selected_equipment") spec.Kind = CommandKind::ProgToggleSelectedEquipment;
            else if (StartsWith(action, "select_equipment_"))
            {
                spec.Kind = CommandKind::ProgSelectEquipment;
                spec.Primary = action.substr(17);
            }
            else if (action == "reset") spec.Kind = CommandKind::ProgReset;
            else if (action == "select_support_mentor") spec.Kind = CommandKind::ProgSelectSupportMentor;
            else if (action == "select_support_white_mage") spec.Kind = CommandKind::ProgSelectSupportWhiteMage;
            else if (action == "select_support_guard") spec.Kind = CommandKind::ProgSelectSupportGuard;
            else if (action == "select_support_black_mage") spec.Kind = CommandKind::ProgSelectSupportBlackMage;
            else if (action == "text_speed_up") spec.Kind = CommandKind::ProgTextSpeedUp;
            else if (action == "text_speed_down") spec.Kind = CommandKind::ProgTextSpeedDown;
            else if (action == "master_volume_up") spec.Kind = CommandKind::ProgMasterVolumeUp;
            else if (action == "master_volume_down") spec.Kind = CommandKind::ProgMasterVolumeDown;
            else if (action == "bgm_volume_up") spec.Kind = CommandKind::ProgBgmVolumeUp;
            else if (action == "bgm_volume_down") spec.Kind = CommandKind::ProgBgmVolumeDown;
            else if (action == "sfx_volume_up") spec.Kind = CommandKind::ProgSfxVolumeUp;
            else if (action == "sfx_volume_down") spec.Kind = CommandKind::ProgSfxVolumeDown;
            else if (action == "toggle_screen_shake") spec.Kind = CommandKind::ProgToggleScreenShake;
            else if (action == "toggle_fullscreen") spec.Kind = CommandKind::ProgToggleFullscreen;
            else if (StartsWith(action, "set_text_speed:"))
            {
                spec.Kind = CommandKind::ProgSetTextSpeed;
                try { spec.FloatValue = std::stof(action.substr(15)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else if (StartsWith(action, "set_master_volume:"))
            {
                spec.Kind = CommandKind::ProgSetMasterVolume;
                try { spec.FloatValue = std::stof(action.substr(18)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else if (StartsWith(action, "set_bgm_volume:") || StartsWith(action, "set_sfx_volume:"))
            {
                spec.Kind = StartsWith(action, "set_bgm_volume:")
                    ? CommandKind::ProgSetBgmVolume : CommandKind::ProgSetSfxVolume;
                try { spec.FloatValue = std::stof(action.substr(15)); } catch (...) { spec.FloatValue = 0.0f; }
            }
            else spec.Kind = CommandKind::Raw;
            return spec;
        }

        spec.Kind = CommandKind::Raw;
        return spec;
    }

    inline void SeedCommandKind(CommandSpec& spec, CommandKind kind, const std::string& currentCommand)
    {
        spec.Kind = kind;
        spec.Raw = currentCommand;
        spec.Number = std::max(1, spec.Number);

        if (kind == CommandKind::Scene || kind == CommandKind::NewGame || kind == CommandKind::LoadGame)
        {
            if (spec.Primary.empty() && !SceneChoices().empty())
                spec.Primary = SceneChoices().front();
        }
        else if (kind == CommandKind::Event)
        {
            if (spec.Primary.empty() && !EventChoices().empty())
                spec.Primary = EventChoices().front();
        }
        else
        {
            // Defaults for commands that carry arguments.
            if (kind == CommandKind::VnSaveSlot || kind == CommandKind::VnLoadSlot)
                spec.Number = std::max(1, spec.Number);
            else if (kind == CommandKind::TurnMenu)
            {
                if (spec.Primary.empty()) spec.Primary = "root";
            }
            else if (kind == CommandKind::TurnItem || kind == CommandKind::TurnSkill)
            {
                if (spec.Primary.empty()) spec.Primary = "basic";
            }
            else if (kind == CommandKind::TacticCell)
            {
                spec.Number = std::max(0, spec.Number);
                spec.Number2 = std::max(0, spec.Number2);
            }
            else if (kind == CommandKind::TacticMenu)
            {
                if (spec.Primary.empty()) spec.Primary = "move";
            }
            else if (kind == CommandKind::TacticSkill)
            {
                if (spec.Primary.empty()) spec.Primary = "basic";
            }
            else if (kind == CommandKind::ProgSelectSkillNode)
            {
                if (spec.Primary.empty()) spec.Primary = "magic_sword_core";
            }
            else if (kind == CommandKind::ProgSelectEquipmentSlot)
            {
                if (spec.Primary.empty()) spec.Primary = "weapon";
            }
            else if (kind == CommandKind::ProgSetTextSpeed)
                spec.FloatValue = std::clamp(spec.FloatValue, 12.0f, 180.0f);
            else if (kind == CommandKind::ProgSetMasterVolume
                || kind == CommandKind::ProgSetBgmVolume
                || kind == CommandKind::ProgSetSfxVolume)
                spec.FloatValue = std::clamp(spec.FloatValue, 0.0f, 100.0f);
        }
    }

    inline bool DrawOptionPicker(
        const char* label,
        std::string& value,
        const std::vector<std::string>& options,
        size_t capacity)
    {
        bool changed = EditorWidgets::InputString(label, value, capacity);

        const std::string comboLabel = std::string("Pick ") + label;
        if (ImGui::BeginCombo(comboLabel.c_str(), value.empty() ? "(select)" : value.c_str()))
        {
            for (size_t i = 0; i < options.size(); ++i)
            {
                const bool selected = options[i] == value;
                const std::string itemLabel = EditorWidgets::LabelWithId(options[i], std::string(label) + ":" + std::to_string(i));
                if (ImGui::Selectable(itemLabel.c_str(), selected))
                {
                    value = options[i];
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh Assets"))
            RefreshAssetChoices();

        return changed;
    }

    inline bool DrawCommandBuilder(const char* label, std::string& command, size_t rawCapacity = 512)
    {
        bool changed = false;
        CommandSpec spec = ParseCommand(command);
        const char* displayLabel = label ? label : "Command";

        ImGui::PushID(displayLabel);
        ImGui::TextDisabled("%s", displayLabel);

        static const CommandKind kinds[] = {
            CommandKind::None,
            CommandKind::Scene,
            CommandKind::Event,
            CommandKind::NewGame,
            CommandKind::LoadGame,
            CommandKind::GameSaveOpenSaveMenu,
            CommandKind::GameSaveOpenLoadMenu,
            CommandKind::GameSaveSlotSave,
            CommandKind::GameSaveLoadSlot,
            CommandKind::GameSaveClose,
            CommandKind::GameSaveConfirmOverwrite,
            CommandKind::GameSaveCancelOverwrite,
            CommandKind::ProgressionSetFlag,
            CommandKind::ProgressionClearFlag,
            CommandKind::ProgressionSetActiveDungeon,
            CommandKind::ProgressionClearActiveDungeon,
            CommandKind::ProgressionSetChapter,
            CommandKind::UiPager,
            CommandKind::Anim,
            CommandKind::VnAuto, CommandKind::VnHistory, CommandKind::VnSettings,
            CommandKind::VnClose, CommandKind::VnHide,
            CommandKind::VnSaveMenu, CommandKind::VnLoadMenu,
            CommandKind::VnQuickSave, CommandKind::VnQuickLoad,
            CommandKind::VnSaveSlot, CommandKind::VnLoadSlot,
            CommandKind::VnConfirmOverwrite, CommandKind::VnCancelOverwrite,
            CommandKind::VnTextSpeedUp, CommandKind::VnTextSpeedDown,
            CommandKind::VnAutoDelayUp, CommandKind::VnAutoDelayDown,
            CommandKind::VnAdvance, CommandKind::VnSkip,
            CommandKind::TurnMenu, CommandKind::TurnCancel, CommandKind::TurnWait,
            CommandKind::TurnGuard, CommandKind::TurnItem, CommandKind::TurnSkill,
            CommandKind::TurnTarget,
            CommandKind::TacticCell, CommandKind::TacticMenu, CommandKind::TacticCancel,
            CommandKind::TacticSkill,
            CommandKind::SideItem1, CommandKind::SideItem2, CommandKind::SideItem3,
            CommandKind::SideBasic, CommandKind::SideLauncher, CommandKind::SideMagic,
            CommandKind::SideSupport, CommandKind::SideDash, CommandKind::SideBreakLimit,
            CommandKind::ProgUpgradeTravelerArmor, CommandKind::ProgLearnSelectedSkill,
            CommandKind::ProgSelectSkillNode,
            CommandKind::ProgEquipmentPageSlider, CommandKind::ProgEquipmentPage1,
            CommandKind::ProgEquipmentPage2,
            CommandKind::ProgSelectEquipmentSlot, CommandKind::ProgToggleSelectedEquipment,
            CommandKind::ProgSelectEquipment, CommandKind::ProgReset,
            CommandKind::ProgSelectSupportMentor, CommandKind::ProgSelectSupportWhiteMage,
            CommandKind::ProgSelectSupportGuard, CommandKind::ProgSelectSupportBlackMage,
            CommandKind::ProgTextSpeedUp, CommandKind::ProgTextSpeedDown,
            CommandKind::ProgMasterVolumeUp, CommandKind::ProgMasterVolumeDown,
            CommandKind::ProgBgmVolumeUp, CommandKind::ProgBgmVolumeDown,
            CommandKind::ProgSfxVolumeUp, CommandKind::ProgSfxVolumeDown,
            CommandKind::ProgToggleScreenShake, CommandKind::ProgToggleFullscreen,
            CommandKind::ProgSetTextSpeed, CommandKind::ProgSetMasterVolume,
            CommandKind::ProgSetBgmVolume, CommandKind::ProgSetSfxVolume,
            CommandKind::Quit,
            CommandKind::Raw
        };

        // Family markers so the combo groups related commands; returns the label
        // to draw as a separator header, or nullptr for regular items.
        auto groupHeader = [](CommandKind kind) -> const char*
        {
            if (kind == CommandKind::GameSaveOpenSaveMenu) return "Game Save";
            if (kind == CommandKind::ProgressionSetFlag) return "Progression (core)";
            if (kind == CommandKind::UiPager) return "UI / Animation";
            if (kind == CommandKind::VnAuto) return "Visual Novel";
            if (kind == CommandKind::TurnMenu) return "Turn Combat";
            if (kind == CommandKind::TacticCell) return "Tactical Combat";
            if (kind == CommandKind::SideItem1) return "Side Combat";
            if (kind == CommandKind::ProgUpgradeTravelerArmor) return "Progression (actions)";
            if (kind == CommandKind::ProgTextSpeedUp) return "Settings";
            if (kind == CommandKind::Quit) return "System";
            return nullptr;
        };

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Type", CommandKindLabel(spec.Kind)))
        {
            for (CommandKind kind : kinds)
            {
                if (const char* header = groupHeader(kind))
                {
                    ImGui::Separator();
                    ImGui::TextDisabled("%s", header);
                }
                const bool selected = kind == spec.Kind;
                if (ImGui::Selectable(CommandKindLabel(kind), selected))
                {
                    if (kind != spec.Kind)
                    {
                        SeedCommandKind(spec, kind, command);
                        command = BuildCommand(spec);
                        changed = true;
                    }
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        switch (spec.Kind)
        {
        case CommandKind::Scene:
        case CommandKind::NewGame:
        case CommandKind::LoadGame:
            if (DrawOptionPicker("Scene", spec.Primary, SceneChoices(), 512))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            if (spec.Kind == CommandKind::LoadGame)
            {
                int slot = std::max(1, spec.Number);
                if (ImGui::DragInt("Slot", &slot, 1.0f, 1, 20))
                {
                    spec.Number = std::max(1, slot);
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            break;
        case CommandKind::Event:
            if (DrawOptionPicker("Event Name", spec.Primary, EventChoices(), 256))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::GameSaveSlotSave:
        case CommandKind::GameSaveLoadSlot:
        {
            int slot = std::max(1, spec.Number);
            if (ImGui::DragInt("Slot", &slot, 1.0f, 1, 20))
            {
                spec.Number = std::max(1, slot);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::ProgressionSetFlag:
        case CommandKind::ProgressionClearFlag:
            if (EditorContentPickers::DrawStoryFlagField("Flag", spec.Primary, 256))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgressionSetActiveDungeon:
            if (EditorContentPickers::DrawProgressionIdField("Dungeon",
                spec.Primary,
                EditorContentPickers::ProgressionIdKind::Dungeon,
                256))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgressionSetChapter:
        {
            int chapter = std::max(1, spec.Number);
            if (ImGui::DragInt("Chapter", &chapter, 1.0f, 1, 99))
            {
                spec.Number = std::max(1, chapter);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::UiPager:
        {
            // Grammar: ui:pager:@UUID:action[:page]
            if (EditorWidgets::InputString("Pager Target", spec.Primary, 64))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Target is the pager entity UUID, e.g. @123456789.");
            static const char* pagerActions[] = { "next", "prev", "first", "last", "page", "set" };
            int actionIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(pagerActions); ++i)
                if (spec.Secondary == pagerActions[i]) { actionIndex = i; break; }
            if (ImGui::Combo("Action", &actionIndex, pagerActions, IM_ARRAYSIZE(pagerActions)))
            {
                spec.Secondary = pagerActions[actionIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (spec.Secondary == "page" || spec.Secondary == "set")
            {
                int page = std::max(1, spec.Number);
                if (ImGui::DragInt("Page", &page, 1.0f, 1, 999))
                {
                    spec.Number = std::max(1, page);
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            break;
        }
        case CommandKind::Anim:
        {
            // Grammar: anim:action:@UUID[:arg]
            if (EditorWidgets::InputString("Animation Target", spec.Primary, 64))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Target is the animator entity UUID, e.g. @123456789.");
            static const char* animActions[] = { "play", "restart", "pause", "resume", "stop", "seek" };
            int actionIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(animActions); ++i)
                if (spec.Secondary == animActions[i]) { actionIndex = i; break; }
            if (ImGui::Combo("Action", &actionIndex, animActions, IM_ARRAYSIZE(animActions)))
            {
                spec.Secondary = animActions[actionIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (spec.Secondary == "play")
            {
                if (EditorWidgets::InputString("Clip Name", spec.Raw, 128))
                {
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            else if (spec.Secondary == "seek")
            {
                float seek = std::max(0.0f, spec.FloatValue);
                if (ImGui::DragFloat("Seek Time", &seek, 0.01f, 0.0f, 120.0f, "%.2f"))
                {
                    spec.FloatValue = std::max(0.0f, seek);
                    command = BuildCommand(spec);
                    changed = true;
                }
            }
            break;
        }
        case CommandKind::VnSaveSlot:
        case CommandKind::VnLoadSlot:
        {
            int slot = std::max(1, spec.Number);
            if (ImGui::DragInt("Save Slot", &slot, 1.0f, 1, 20))
            {
                spec.Number = std::max(1, slot);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::TurnMenu:
            if (EditorWidgets::InputString("Menu Page", spec.Primary, 64))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Menu page key, e.g. root / skills / items.");
            break;
        case CommandKind::TurnItem:
        case CommandKind::TurnSkill:
        {
            static const char* turnSlots[] = { "basic", "slot0", "slot1", "slot2", "slot3", "item0", "potion" };
            int slotIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(turnSlots); ++i)
                if (spec.Primary == turnSlots[i]) { slotIndex = i; break; }
            if (ImGui::Combo("Slot", &slotIndex, turnSlots, IM_ARRAYSIZE(turnSlots)))
            {
                spec.Primary = turnSlots[slotIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (EditorWidgets::InputString("Skill Id (optional)", spec.Raw, 128))
            {
                spec.Primary = spec.Raw.empty() ? spec.Primary : spec.Raw;
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Choose a slot, or type a raw skill id (e.g. turn.aether_edge) for custom skills.");
            break;
        }
        case CommandKind::TurnTarget:
            if (EditorWidgets::InputString("Target Name", spec.Primary, 128))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Combatant entity name or its Target button name.");
            break;
        case CommandKind::TacticCell:
        {
            int row = std::max(0, spec.Number);
            int col = std::max(0, spec.Number2);
            bool cellChanged = false;
            if (ImGui::DragInt("Row", &row, 1.0f, 0, 99)) cellChanged = true;
            if (ImGui::DragInt("Column", &col, 1.0f, 0, 99)) cellChanged = true;
            if (cellChanged)
            {
                spec.Number = std::max(0, row);
                spec.Number2 = std::max(0, col);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::TacticMenu:
        {
            static const char* tacticPages[] = { "move", "attack", "skills", "items" };
            int pageIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(tacticPages); ++i)
                if (spec.Primary == tacticPages[i]) { pageIndex = i; break; }
            if (ImGui::Combo("Menu Page", &pageIndex, tacticPages, IM_ARRAYSIZE(tacticPages)))
            {
                spec.Primary = tacticPages[pageIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::TacticSkill:
        {
            static const char* tacticSlots[] = { "cancel", "wait", "guard", "basic", "slot0", "slot1", "slot2", "item0", "potion" };
            int slotIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(tacticSlots); ++i)
                if (spec.Primary == tacticSlots[i]) { slotIndex = i; break; }
            if (ImGui::Combo("Slot", &slotIndex, tacticSlots, IM_ARRAYSIZE(tacticSlots)))
            {
                spec.Primary = tacticSlots[slotIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            if (EditorWidgets::InputString("Skill Id (optional)", spec.Raw, 128))
            {
                spec.Primary = spec.Raw.empty() ? spec.Primary : spec.Raw;
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip("Choose a slot, or type a raw skill id for custom skills.");
            break;
        }
        case CommandKind::ProgSelectSkillNode:
            if (EditorWidgets::InputString("Skill Node Id", spec.Primary, 128))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgEquipmentPageSlider:
        {
            float value = spec.FloatValue;
            if (ImGui::DragFloat("Slider Value", &value, 0.05f, 0.0f, 3.0f, "%.1f"))
            {
                spec.FloatValue = value;
                command = BuildCommand(spec);
                changed = true;
            }
            EditorWidgets::HelpTooltip(">= 1.5 switches to page 2, else page 1.");
            break;
        }
        case CommandKind::ProgSelectEquipmentSlot:
        {
            static const char* equipSlots[] = { "weapon", "armor", "ring", "charm", "boots", "special" };
            int slotIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(equipSlots); ++i)
                if (spec.Primary == equipSlots[i]) { slotIndex = i; break; }
            if (ImGui::Combo("Equipment Slot", &slotIndex, equipSlots, IM_ARRAYSIZE(equipSlots)))
            {
                spec.Primary = equipSlots[slotIndex];
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::ProgSelectEquipment:
            if (EditorWidgets::InputString("Equipment Id", spec.Primary, 128))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        case CommandKind::ProgSetTextSpeed:
        case CommandKind::ProgSetMasterVolume:
        case CommandKind::ProgSetBgmVolume:
        case CommandKind::ProgSetSfxVolume:
        {
            float value = spec.FloatValue;
            const float maxValue = (spec.Kind == CommandKind::ProgSetTextSpeed) ? 180.0f : 100.0f;
            if (ImGui::DragFloat("Value", &value, 1.0f, 0.0f, maxValue, "%.1f"))
            {
                spec.FloatValue = std::clamp(value, 0.0f, maxValue);
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        }
        case CommandKind::Raw:
            if (EditorWidgets::InputString("Raw", spec.Raw, rawCapacity))
            {
                command = BuildCommand(spec);
                changed = true;
            }
            break;
        default:
            break;
        }

        ImGui::TextDisabled("Command: %s", command.empty() ? "(none)" : command.c_str());
        ImGui::PopID();
        return changed;
    }

} // namespace Wheatear::EditorCommandBuilder
