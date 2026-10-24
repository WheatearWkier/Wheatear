#include "wtpch.h"
#include "LogicCommandRegistry.h"

#include "Wheatear/Utils/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <mutex>

namespace Wheatear::Logic {

    namespace {

        std::vector<CommandDescriptor>& Store()
        {
            static std::vector<CommandDescriptor> descriptors;
            return descriptors;
        }

        std::mutex& StoreMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool Matches(const CommandDescriptor& descriptor,
            const std::string& command)
        {
            if (descriptor.Name.empty() || command.empty())
                return false;
            if (command == descriptor.Name)
                return true;
            return command.size() > descriptor.Name.size()
                && command.compare(0, descriptor.Name.size(), descriptor.Name) == 0
                && command[descriptor.Name.size()] == ':';
        }

        void AddBuiltin(const char* name, const char* category,
            const char* description, const char* argumentHint, bool gameplay)
        {
            CommandDescriptor descriptor;
            descriptor.Name = name;
            descriptor.Category = category;
            descriptor.Description = description;
            descriptor.ArgumentHint = argumentHint;
            descriptor.Gameplay = gameplay;
            CommandRegistry::Register(descriptor);
        }

    } // namespace

    void CommandRegistry::EnsureBuiltins()
    {
        static std::once_flag once;
        std::call_once(once, []()
        {
            AddBuiltin("quit", "Flow", "Quit the current game.", "", false);
            AddBuiltin("scene", "Flow", "Request a scene transition.", "assets/scenes/Name.wt", false);
            AddBuiltin("newgame", "Flow", "Start a new game at a scene.", "scene_path", false);
            AddBuiltin("loadgame", "Flow", "Load a save slot.", "scene_path[:slot]", false);
            AddBuiltin("progression", "Progression", "Set flags, dungeons or progression state.", "set_flag FLAG", false);
            AddBuiltin("anim", "Animation", "Drive an entity animator.", "play|stop|speed ...", false);
            AddBuiltin("ui", "UI", "Drive UI widgets and pagers.", "pager:@UUID next", false);
            AddBuiltin("event", "Flow", "Dispatch an Event Script event.", "[@UUID] event_name", false);
            AddBuiltin("vn", "Visual Novel", "Drive the visual novel runtime.", "vn:command", true);
            AddBuiltin("gamesave", "Save", "Open save/load UI or perform a save operation.", "open_save_menu", true);
            AddBuiltin("turn", "Combat", "Turn-combat gameplay command.", "turn:command", true);
            AddBuiltin("tactic", "Combat", "Tactical-combat gameplay command.", "tactic:command", true);
            AddBuiltin("side", "Combat", "Side-combat gameplay command.", "side:command", true);
        });
    }

    void CommandRegistry::Register(const CommandDescriptor& descriptor)
    {
        if (descriptor.Name.empty())
            return;

        std::lock_guard<std::mutex> lock(StoreMutex());
        auto& descriptors = Store();
        std::string name = ToLowerAscii(Wheatear::StringUtils::Trim(descriptor.Name));
        while (!name.empty() && name.back() == ':')
            name.pop_back();
        if (name.empty())
            return;
        for (CommandDescriptor& existing : descriptors)
        {
            if (existing.Name == name)
            {
                if (!descriptor.Description.empty())
                    existing.Description = descriptor.Description;
                if (!descriptor.Category.empty())
                    existing.Category = descriptor.Category;
                if (!descriptor.ArgumentHint.empty())
                    existing.ArgumentHint = descriptor.ArgumentHint;
                existing.Gameplay = descriptor.Gameplay;
                return;
            }
        }

        CommandDescriptor normalized = descriptor;
        normalized.Name = name;
        descriptors.push_back(std::move(normalized));
    }

    bool CommandRegistry::Validate(const std::string& command, std::string* error)
    {
        EnsureBuiltins();
        const std::string trimmed = Wheatear::StringUtils::Trim(command);
        if (trimmed.empty())
        {
            if (error)
                *error = "Command is empty.";
            return false;
        }

        if (Find(trimmed))
            return true;

        if (error)
        {
            const size_t separator = trimmed.find(':');
            const std::string prefix = separator == std::string::npos
                ? trimmed
                : trimmed.substr(0, separator);
            *error = "Unknown command prefix '" + prefix
                + "'. Register it with CommandBus::RegisterGameplayCommandPrefix "
                  "before loading content.";
        }
        return false;
    }

    std::optional<CommandDescriptor> CommandRegistry::Find(const std::string& command)
    {
        EnsureBuiltins();
        const std::string normalized = ToLowerAscii(
            Wheatear::StringUtils::Trim(command));
        std::lock_guard<std::mutex> lock(StoreMutex());
        auto& descriptors = Store();
        for (const CommandDescriptor& descriptor : descriptors)
        {
            if (Matches(descriptor, normalized))
                return descriptor;
        }
        return std::nullopt;
    }

    std::vector<CommandDescriptor> CommandRegistry::Snapshot()
    {
        EnsureBuiltins();
        std::lock_guard<std::mutex> lock(StoreMutex());
        std::vector<CommandDescriptor> result = Store();
        std::sort(result.begin(), result.end(),
            [](const CommandDescriptor& lhs, const CommandDescriptor& rhs)
            {
                if (lhs.Category != rhs.Category)
                    return lhs.Category < rhs.Category;
                return lhs.Name < rhs.Name;
            });
        return result;
    }

} // namespace Wheatear::Logic
