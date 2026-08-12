#pragma once

#include "Wheatear/Modules/SideCombat/SideCombatComponents.h"

#include <string>

namespace Wheatear {

    namespace SideCombatEditorRequests {
        void RequestOpenHudPreset(const std::string& sourcePath);
        bool ConsumeOpenHudPresetRequest(std::string& sourcePath);
    }

    class SideCombatHudPresetEditorPanel
    {
    public:
        void Open(const std::string& sourcePath);
        void OnImGuiRender();

    private:
        void Load();
        void Save();
        void RefreshRawPreview();

        void DrawToolbar();
        void DrawBindingsTab();
        void DrawLayoutsTab();
        void DrawSlotsTab();
        void DrawTextTab();
        void DrawRawPreviewTab();

    private:
        bool m_Open = false;
        bool m_Loaded = false;
        bool m_Dirty = false;
        bool m_Valid = true;
        std::string m_SourcePath = "side.hud.preset";
        std::string m_Status;
        std::string m_RawPreview;
        SideCombatLevelComponent m_Level;
    };

} // namespace Wheatear
