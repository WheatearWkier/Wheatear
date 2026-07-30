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

    protected:

        void OnBeginRender() override {}

        void OnOverlayRender() override;

        void OnImGuiExtra() override;
    };

} // namespace Wheatear
