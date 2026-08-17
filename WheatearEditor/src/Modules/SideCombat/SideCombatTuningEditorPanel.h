#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace YAML { class Node; }

namespace Wheatear {

    namespace SideCombatEditorRequests {
        void RequestOpenTuning(const std::string& sourcePath);
        bool ConsumeOpenTuningRequest(std::string& sourcePath);
    }

    class SideCombatTuningEditorPanel
    {
    public:
        SideCombatTuningEditorPanel();
        ~SideCombatTuningEditorPanel();

        SideCombatTuningEditorPanel(const SideCombatTuningEditorPanel&) = delete;
        SideCombatTuningEditorPanel& operator=(const SideCombatTuningEditorPanel&) = delete;

        void Open(const std::string& sourcePath);
        void OnImGuiRender();

    private:
        void Load();
        void Save();
        void RefreshSelections();
        void RefreshRawPreview();

        void DrawToolbar();
        void DrawFeelTab();
        void DrawRulesTab();
        void DrawAttacksTab();
        void DrawAnimationsTab();
        void DrawSkillsTab();
        void DrawItemSlotsTab();
        void DrawSkillSlotsTab();
        void DrawEnemyTypesTab();
        void DrawProgressionTab();
        void DrawRawPreviewTab();

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_ParseValid = false;
        bool m_Dirty = false;
        std::string m_SourcePath = "side.tuning";
        std::filesystem::path m_ResolvedPath;
        std::string m_Status;
        std::string m_SelectedAttackId;
        std::string m_SelectedSkillId;
        std::string m_SelectedProfileId;
        std::string m_SelectedEnemyType;
        std::string m_SelectedPlayerAnimId;
        std::string m_SelectedGruntAnimId;
        std::string m_SelectedBossAnimId;
        std::array<char, 64> m_NewAnimId{};
        std::string m_RawPreview;
        std::string m_RawEditText;   // editable raw view (original file text)
        bool m_RawEdited = false;    // save writes m_RawEditText verbatim
        std::unique_ptr<YAML::Node> m_Root;
        // Per-path "add" scratch state for the shared Advanced tree editor.
        std::unordered_map<std::string, std::string> m_NewScalarValues;
        std::unordered_map<std::string, std::string> m_NewMapKeys;
    };

} // namespace Wheatear
