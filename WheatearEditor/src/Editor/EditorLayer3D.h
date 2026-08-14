#pragma once

#include "EditorLayerBase.h"
#include "Wheatear/Renderer/IBLPrecompute.h"

namespace Wheatear {

    ///
    ///
    class EditorLayer3D : public EditorLayerBase
    {
    public:
        EditorLayer3D();
        ~EditorLayer3D() override = default;

        void OnAttach() override;

    protected:


        void OnBeginRender() override;

        void OnPostSceneUpdate() override;

        void OnOverlayRender() override;

        void OnImGuiExtra() override;

    private:
        Ref<IBLResult> m_IBL;
        std::string    m_IBLPath;

        float m_IBLIntensity = 1.0f;


        uint32_t m_LastSSAOWidth = 0;
        uint32_t m_LastSSAOHeight = 0;
    };

} // namespace Wheatear
