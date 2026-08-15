#pragma once

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Utils/StringUtils.h"

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

    using StringUtils::StartsWith;
    using StringUtils::PayloadAfter;
    using StringUtils::Trim;
    using StringUtils::ToLower;

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
        GameSavePushAllowAll,
        GameSavePushBlockAll,
        GameSavePopPolicy,
        GameSaveClearPolicy,
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
        VnSaveMenu, VnLoadMenu, VnConfirmOverwrite, VnCancelOverwrite,
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


    // ---- Implementation lives in CommandBuilder.cpp (was header-inline) ----

    const char* CommandKindLabel(CommandKind kind);
    std::string FormatSeek(float seconds);
    std::string FormatFloat(float value);
    std::string BuildCommand(const CommandSpec& spec);
    CommandSpec ParseCommand(const std::string& command);
    void SeedCommandKind(CommandSpec& spec, CommandKind kind, const std::string& currentCommand);
    bool DrawOptionPicker(const char* label, std::string& value,
        const std::vector<std::string>& options, size_t capacity);
    bool DrawCommandBuilder(const char* label, std::string& command, size_t rawCapacity = 512);

} // namespace Wheatear::EditorCommandBuilder
