#pragma once

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Core/AssetPath.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
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
        Quit,
        Raw
    };

    struct CommandSpec
    {
        CommandKind Kind = CommandKind::None;
        std::string Primary;
        std::string Raw;
        int Number = 1;
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
        case CommandKind::Quit: return "Quit";
        case CommandKind::Raw: return "Raw Command";
        }
        return "Raw Command";
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
        case CommandKind::Quit: return "quit";
        case CommandKind::Raw: return spec.Raw;
        }
        return spec.Raw;
    }

    inline CommandSpec ParseCommand(const std::string& command)
    {
        CommandSpec spec;
        spec.Raw = command;

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
            CommandKind::Quit,
            CommandKind::Raw
        };

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Type", CommandKindLabel(spec.Kind)))
        {
            for (CommandKind kind : kinds)
            {
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
