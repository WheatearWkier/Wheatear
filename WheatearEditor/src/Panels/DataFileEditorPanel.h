#pragma once

#include "Wheatear/Core/Core.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear {

    namespace DataFileEditorRequests {
        // Cross-panel open request (ContentBrowser double-click, raw-preview
        // "Open in Data Editor" buttons, ...).
        void RequestOpen(const std::string& sourcePath);
        bool ConsumeOpenRequest(std::string& sourcePath);
    } // namespace DataFileEditorRequests

    // Generic data-file editor: opens any .yaml / .json / .wtsettings / ...
    // file in the editor instead of requiring designers to hand-edit text
    // files. YAML files get a structured tree view (shared YamlTreeEditor
    // widget with typed asset pickers); every file gets an editable raw view
    // with parse validation and a save path. Tree edits keep a small undo
    // stack; the raw view uses ImGui's built-in undo/redo.
    class DataFileEditorPanel
    {
    public:
        void OnImGuiRender();

    private:
        void Open(const std::string& sourcePath);
        void Load();
        void Save();
        void ReloadFromDisk();
        void ValidateRaw();

        std::string SerializeTree() const;

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_Dirty = false;
        bool m_ParseValid = false;
        bool m_SupportsTree = false; // YAML-ish extension -> structured tab
        bool m_TreeEdited = false;   // which view changed last (save source)

        std::string m_Path; // project-relative
        std::filesystem::path m_ResolvedPath;
        std::string m_RawText;
        std::string m_Status;

        YAML::Node m_YamlRoot;
        std::unordered_map<std::string, std::string> m_NewScalarValues;
        std::unordered_map<std::string, std::string> m_NewMapKeys;

        // Tree-edit undo: serialized states captured before each change.
        std::vector<std::string> m_UndoStack;
        std::string m_LastSerialized;
    };

} // namespace Wheatear
