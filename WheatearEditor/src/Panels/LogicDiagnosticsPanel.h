#pragma once

#include "Editor/EditorToolRegistry.h"

#include <string>

namespace Wheatear {

    // Consolidated viewer for the shared Logic runtime diagnostics reported by
    // Event Script, Visual Novel and WAO loaders/evaluators.
    class LogicDiagnosticsPanel
    {
    public:
        void Open(const EditorToolContext& context);
        void OnImGuiRender();

    private:
        bool m_Open = false;
        bool m_ShowInfo = true;
        bool m_ShowWarnings = true;
        bool m_ShowErrors = true;
        std::string m_Filter;
    };

} // namespace Wheatear
