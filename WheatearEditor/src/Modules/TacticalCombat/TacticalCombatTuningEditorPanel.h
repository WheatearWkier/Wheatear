#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace YAML { class Node; }

namespace Wheatear {

    class Scene;
    struct TacticalUnitComponent;

    namespace TacticalCombatEditorRequests {
        void RequestOpenTuning(const std::string& sourcePath);
        bool ConsumeOpenTuningRequest(std::string& sourcePath);
    }

    // Structured editor for assets/sandbox/data/tactical_combat_tuning.yaml
    // (flow timings + board layout + tile colors + formula coefficients).
    class TacticalCombatTuningEditorPanel
    {
    public:
        TacticalCombatTuningEditorPanel();
        ~TacticalCombatTuningEditorPanel();

        TacticalCombatTuningEditorPanel(const TacticalCombatTuningEditorPanel&) = delete;
        TacticalCombatTuningEditorPanel& operator=(const TacticalCombatTuningEditorPanel&) = delete;

        void Open(const std::string& sourcePath, Scene* scene = nullptr);
        void OnImGuiRender();

    private:
        void Load();
        void Save();
        void RefreshRawPreview();

        void DrawToolbar();
        void DrawLevelTab();
        void DrawBoardTab();
        void DrawFormulaTab();
        void DrawSceneUnitsTab();
        void DrawRawPreviewTab();

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_ParseValid = false;
        bool m_Dirty = false;
        std::string m_SourcePath = "assets/sandbox/data/tactical_combat_tuning.yaml";
        std::filesystem::path m_ResolvedPath;
        std::string m_Status;
        std::string m_RawPreview;
        std::unique_ptr<YAML::Node> m_Root;
        Scene* m_Scene = nullptr;   // scene open in the editor (unit overview)
        std::unordered_map<uint32_t, TacticalUnitComponent> m_UnitEditSnapshots;
        std::unordered_map<std::string, std::string> m_NewScalarValues;
        std::unordered_map<std::string, std::string> m_NewMapKeys;
    };

} // namespace Wheatear
