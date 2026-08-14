#include "wtpch.h"
#include "Renderer.h"

#include "Renderer2D.h"
#include "Renderer3D.h"
#include "RenderCommand.h"

namespace Wheatear {

    void Renderer::Init()
    {
        WT_PROFILE_FUNCTION();
        RenderCommand::Init();
        Renderer2D::Init();
        Renderer3D::Init();
    }

    void Renderer::Shutdown()
    {
        Renderer2D::Shutdown();
        Renderer3D::Shutdown();
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }

} // namespace Wheatear
