#pragma once

#include <filesystem>
#include <memory>
#include <string>

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
        void DrawSkillsTab();
        void DrawProgressionTab();
        void DrawRawPreviewTab();

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_ParseValid = false;
        bool m_Dirty = false;
        std::string m_SourcePath = "assets/vertical_slice/data/side_combat_tuning.yaml";
        std::filesystem::path m_ResolvedPath;
        std::string m_Status;
        std::string m_SelectedAttackId;
        std::string m_SelectedSkillId;
        std::string m_SelectedProfileId;
        std::string m_RawPreview;
        std::unique_ptr<YAML::Node> m_Root;
    };

} // namespace Wheatear
