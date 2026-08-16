#pragma once

#include "Wheatear/Core/Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wheatear {

    // One authored input action: identity (id), display name and the default
    // physical keys the action responds to. Definitions live in
    // assets/input/action_bindings.yaml (project root first, engine assets as
    // fallback) so new actions can be added without touching C++.
    struct InputActionDefinition
    {
        std::string Id;
        std::string Label;
        std::vector<int> DefaultKeys;   // WT_KEY_* or negative mouse binding
        bool Disabled = false;          // hidden from UI and yields no keys
    };

    // Data-driven action catalog. Built-in C++ defaults (UserSettings::Defaults)
    // remain as the final fallback; a catalog entry overrides them per action
    // and can introduce actions that have no C++ counterpart.
    class WHEATEAR_API InputActionCatalog
    {
    public:
        // Loads (or re-loads) the catalog from the resolved data file.
        static void Load();

        static bool HasLoaded();

        static const std::vector<InputActionDefinition>& Definitions();
        static const InputActionDefinition* FindDefinition(const std::string& actionId);

        // Default keys for an action, or nullptr when the action is not
        // defined in the catalog (callers fall back to C++ defaults).
        static const std::vector<int>* GetDefaultKeys(const std::string& actionId);

        // All known action ids: catalog first, then C++ defaults not already
        // listed (disabled catalog actions are still listed so they can be
        // re-enabled from the editor).
        static std::vector<std::string> GetAllActionIds();

        // Display name for an action (catalog label, else the id itself).
        static std::string GetDisplayLabel(const std::string& actionId);

        // Converts a key name ("Space", "Mouse Left", "F5") to the storage
        // encoding used by InputBindingService (WT_KEY_* or negative mouse).
        // Returns 0 when the name is not recognized.
        static int KeyFromName(const std::string& name);
        static std::string KeyToName(int keyCode);   // thin wrapper of KeyName

        // Editor support: mutate the in-memory definitions and persist them to
        // the project's own catalog file (created from the engine defaults the
        // first time). Returns false on write failure.
        static bool AddDefinition(const InputActionDefinition& definition);
        static bool RemoveDefinition(const std::string& actionId);
        static bool SaveProjectCatalog();

        static std::filesystem::path ResolvedDataPath();
        static std::filesystem::path ProjectDataPath();

    private:
        static std::vector<InputActionDefinition>& MutableDefinitions();
    };

} // namespace Wheatear
