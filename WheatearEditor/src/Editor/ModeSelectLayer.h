#pragma once

#include "Wheatear/Core/Layer.h"

namespace Wheatear {

    ///
    ///
    class ModeSelectLayer : public Layer
    {
    public:
        ModeSelectLayer();
        ~ModeSelectLayer() override = default;

        void OnAttach()      override;
        void OnImGuiRender() override;

    private:
        void LaunchEditor2D();
        void LaunchEditor3D();

        bool m_Decided = false;
    };

} // namespace Wheatear
