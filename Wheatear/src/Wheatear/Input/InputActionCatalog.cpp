#include "wtpch.h"
#include "InputActionCatalog.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Config/UserSettings.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Input/InputBindingService.h"
#include "Wheatear/Input/KeyCodes.h"
#include "Wheatear/Input/MouseButtonCodes.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Wheatear {

    namespace {

        constexpr const char* kSchema = "wheatear.input.actions.v1";
        constexpr const char* kRelativePath = "assets/input/action_bindings.yaml";

        std::vector<InputActionDefinition>& CatalogStorage()
        {
            static std::vector<InputActionDefinition> definitions;
            return definitions;
        }

        std::vector<std::string>& BuiltinIds()
        {
            static std::vector<std::string> ids;
            return ids;
        }

        bool& CatalogLoadedFlag()
        {
            static bool loaded = false;
            return loaded;
        }

        bool ParseKeyName(const std::string& name, int& outKey)
        {
            if (name.empty())
                return false;

            // Mouse buttons: "Mouse Left" / "Mouse Right" / "Mouse Middle".
            const std::string lower = [] (std::string value)
            {
                std::transform(value.begin(), value.end(), value.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return value;
            }(name);

            if (lower == "mouse left")   { outKey = InputBindingService::ButtonToMouseBinding(WT_MOUSE_BUTTON_LEFT);   return true; }
            if (lower == "mouse right")  { outKey = InputBindingService::ButtonToMouseBinding(WT_MOUSE_BUTTON_RIGHT);  return true; }
            if (lower == "mouse middle") { outKey = InputBindingService::ButtonToMouseBinding(WT_MOUSE_BUTTON_MIDDLE); return true; }

            // Keyboard keys: compare against the canonical KeyName output
            // (case-insensitive), covering letters, digits, F1-F12 and the
            // special keys the runtime can display.
            static const int kCandidates[] = {
                WT_KEY_SPACE, WT_KEY_ENTER, WT_KEY_ESCAPE, WT_KEY_TAB,
                WT_KEY_BACKSPACE, WT_KEY_DELETE,
                WT_KEY_LEFT, WT_KEY_RIGHT, WT_KEY_UP, WT_KEY_DOWN,
                WT_KEY_HOME, WT_KEY_END,
                WT_KEY_LEFT_SHIFT, WT_KEY_LEFT_CONTROL, WT_KEY_LEFT_ALT,
            };

            if (std::string(1, name[0]).size() == 1)
            {
                // Single letter or digit.
                if (name.size() == 1)
                {
                    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
                    if (c >= 'A' && c <= 'Z')
                    {
                        outKey = WT_KEY_A + (c - 'A');
                        return true;
                    }
                    if (c >= '0' && c <= '9')
                    {
                        outKey = WT_KEY_0 + (c - '0');
                        return true;
                    }
                }
                else if (lower.rfind("f", 0) == 0 && name.size() >= 2)
                {
                    try
                    {
                        const int number = std::stoi(name.substr(1));
                        if (number >= 1 && number <= 12)
                        {
                            outKey = WT_KEY_F1 + number - 1;
                            return true;
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }

            for (int candidate : kCandidates)
            {
                std::string canonical = InputBindingService::KeyName(candidate);
                std::transform(canonical.begin(), canonical.end(), canonical.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lower == canonical)
                {
                    outKey = candidate;
                    return true;
                }
            }

            return false;
        }

        // Merge one YAML "actions" sequence into the definitions list. Later
        // files override earlier ones per action id.
        void MergeActionsFromNode(const YAML::Node& root, bool markBuiltin)
        {
            if (!root.IsMap())
                return;

            const YAML::Node actions = root["actions"];
            if (!actions.IsSequence())
                return;

            std::vector<InputActionDefinition>& definitions = CatalogStorage();
            for (const YAML::Node& entry : actions)
            {
                if (!entry.IsMap())
                    continue;

                const std::string id = entry["id"].as<std::string>("");
                if (id.empty())
                    continue;

                InputActionDefinition definition;
                definition.Id = id;
                definition.Label = entry["label"].as<std::string>(id);
                definition.Disabled = entry["disabled"].as<bool>(false);

                const YAML::Node defaults = entry["defaults"];
                if (defaults.IsSequence())
                {
                    for (const YAML::Node& keyNode : defaults)
                    {
                        int keyCode = 0;
                        if (ParseKeyName(keyNode.as<std::string>(""), keyCode))
                            definition.DefaultKeys.push_back(keyCode);
                    }
                }

                // Replace or append.
                auto it = std::find_if(definitions.begin(), definitions.end(),
                    [&id](const InputActionDefinition& d) { return d.Id == id; });
                if (it != definitions.end())
                    *it = std::move(definition);
                else
                    definitions.push_back(std::move(definition));

                if (markBuiltin)
                    BuiltinIds().push_back(id);
            }
        }

        bool LoadFile(const std::filesystem::path& path, bool markBuiltin)
        {
            if (path.empty() || !std::filesystem::is_regular_file(path))
                return false;

            try
            {
                const YAML::Node root = YAML::LoadFile(path.string());
                if (!root.IsMap() || root["schema"].as<std::string>("") != kSchema)
                {
                    WT_CORE_WARN("InputActionCatalog: '{}' has an unknown schema, ignored.",
                        path.string());
                    return false;
                }
                MergeActionsFromNode(root, markBuiltin);
                return true;
            }
            catch (const std::exception& e)
            {
                WT_CORE_ERROR("InputActionCatalog: failed to parse '{}': {}",
                    path.string(), e.what());
                return false;
            }
        }

        void EmitYaml(const std::vector<InputActionDefinition>& definitions,
            std::string& outText)
        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            out << YAML::Key << "schema" << YAML::Value << kSchema;
            out << YAML::Key << "actions" << YAML::Value << YAML::BeginSeq;
            for (const InputActionDefinition& definition : definitions)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "id" << YAML::Value << definition.Id;
                if (!definition.Label.empty() && definition.Label != definition.Id)
                    out << YAML::Key << "label" << YAML::Value << definition.Label;
                out << YAML::Key << "defaults" << YAML::Value << YAML::BeginSeq;
                for (int keyCode : definition.DefaultKeys)
                    out << YAML::Value << InputBindingService::KeyName(keyCode);
                out << YAML::EndSeq;
                if (definition.Disabled)
                    out << YAML::Key << "disabled" << YAML::Value << true;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
            out << YAML::EndMap;
            outText = out.c_str();
        }

    } // namespace

    void InputActionCatalog::Load()
    {
        CatalogStorage().clear();
        BuiltinIds().clear();
        CatalogLoadedFlag() = true;

        // Engine built-ins first (project root and engine root can coincide in
        // single-project setups; skip the duplicate in that case).
        const std::filesystem::path enginePath =
            AssetPath::GetEngineRoot() / kRelativePath;
        const std::filesystem::path projectPath =
            AssetPath::GetProjectRoot() / kRelativePath;

        const bool engineLoaded = LoadFile(enginePath, true);
        if (projectPath != enginePath)
            LoadFile(projectPath, false);

        if (!engineLoaded)
            WT_CORE_INFO("InputActionCatalog: no data file found ({}), "
                "falling back to built-in C++ defaults.",
                (projectPath != enginePath ? enginePath.string() : projectPath.string()));
    }

    const std::vector<InputActionDefinition>& InputActionCatalog::Definitions()
    {
        if (!CatalogLoadedFlag())
            Load();
        return CatalogStorage();
    }

    const InputActionDefinition* InputActionCatalog::FindDefinition(const std::string& actionId)
    {
        for (const InputActionDefinition& definition : CatalogStorage())
        {
            if (definition.Id == actionId)
                return &definition;
        }
        return nullptr;
    }

    const std::vector<int>* InputActionCatalog::GetDefaultKeys(const std::string& actionId)
    {
        const InputActionDefinition* definition = FindDefinition(actionId);
        if (!definition || definition->Disabled)
            return nullptr;
        return &definition->DefaultKeys;
    }

    std::vector<std::string> InputActionCatalog::GetAllActionIds()
    {
        std::vector<std::string> ids;
        for (const InputActionDefinition& definition : CatalogStorage())
            ids.push_back(definition.Id);

        // Append C++ defaults not present in the catalog (keeps legacy
        // actions visible even when the data file predates them).
        const UserSettingsData defaults = UserSettings::Defaults();
        for (const auto& [actionId, _] : defaults.KeyBindings)
        {
            if (std::find(ids.begin(), ids.end(), actionId) == ids.end())
                ids.push_back(actionId);
        }
        return ids;
    }

    std::string InputActionCatalog::GetDisplayLabel(const std::string& actionId)
    {
        const InputActionDefinition* definition = FindDefinition(actionId);
        return (definition && !definition->Label.empty()) ? definition->Label : actionId;
    }

    int InputActionCatalog::KeyFromName(const std::string& name)
    {
        int keyCode = 0;
        return ParseKeyName(name, keyCode) ? keyCode : 0;
    }

    std::string InputActionCatalog::KeyToName(int keyCode)
    {
        return InputBindingService::KeyName(keyCode);
    }

    bool InputActionCatalog::AddDefinition(const InputActionDefinition& definition)
    {
        if (definition.Id.empty())
            return false;

        std::vector<InputActionDefinition>& definitions = CatalogStorage();
        auto it = std::find_if(definitions.begin(), definitions.end(),
            [&definition](const InputActionDefinition& d)
            {
                return d.Id == definition.Id;
            });
        if (it != definitions.end())
        {
            it->Label = definition.Label;
            it->DefaultKeys = definition.DefaultKeys;
            it->Disabled = false;
        }
        else
        {
            definitions.push_back(definition);
        }
        return SaveProjectCatalog();
    }

    bool InputActionCatalog::RemoveDefinition(const std::string& actionId)
    {
        std::vector<InputActionDefinition>& definitions = CatalogStorage();
        auto it = std::find_if(definitions.begin(), definitions.end(),
            [&actionId](const InputActionDefinition& d) { return d.Id == actionId; });
        if (it == definitions.end())
            return false;

        // Built-in actions cannot be physically removed (the engine file would
        // re-add them on the next load); disable them instead so the editor
        // can show and re-enable them.
        const bool builtin = std::find(BuiltinIds().begin(), BuiltinIds().end(),
            actionId) != BuiltinIds().end();
        if (builtin)
            it->Disabled = true;
        else
            definitions.erase(it);
        return SaveProjectCatalog();
    }

    std::filesystem::path InputActionCatalog::ProjectDataPath()
    {
        return AssetPath::GetProjectRoot() / kRelativePath;
    }

    std::filesystem::path InputActionCatalog::ResolvedDataPath()
    {
        const std::filesystem::path projectPath = ProjectDataPath();
        if (std::filesystem::is_regular_file(projectPath))
            return projectPath;
        return AssetPath::GetEngineRoot() / kRelativePath;
    }

    bool InputActionCatalog::SaveProjectCatalog()
    {
        const std::filesystem::path path = ProjectDataPath();
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            WT_CORE_ERROR("InputActionCatalog: cannot create '{}'", path.string());
            return false;
        }

        std::string text;
        EmitYaml(CatalogStorage(), text);
        if (text.empty())
            return false;

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            WT_CORE_ERROR("InputActionCatalog: cannot write '{}'", path.string());
            return false;
        }
        output << text;
        output.close();

        WT_CORE_INFO("InputActionCatalog: saved {} action(s) to '{}'",
            CatalogStorage().size(), path.string());
        return true;
    }

} // namespace Wheatear
