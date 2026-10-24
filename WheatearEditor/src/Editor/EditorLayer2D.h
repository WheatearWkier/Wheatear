#pragma once

#include "EditorLayerBase.h"

namespace Wheatear {

    ///
    ///
    class EditorLayer2D : public EditorLayerBase
    {
    public:
        EditorLayer2D();
        ~EditorLayer2D() override = default;

        const char* GetWorkspaceLabel() const override { return "2D STUDIO"; }
        bool Is3DWorkspace() const override { return false; }

    protected:

        void OnBeginRender() override {}

        void OnOverlayRender() override;

        void OnImGuiExtra() override;
    };

} // namespace Wheatear
