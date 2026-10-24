#include "wtpch.h"
#include "Renderer.h"

#include "Renderer2D.h"
#include "Renderer3D.h"
#include "RenderCommand.h"
#include "Model.h"
#include "TextRenderer.h"
#include "Texture.h"

#include "Wheatear/Assets/SpriteSheetAsset.h"
#include "Wheatear/UI/UIRenderer.h"

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
        // Release caches that own GL objects while the context is still alive.
        UIRenderer::Shutdown();
        TextRenderer::Shutdown();
        SpriteSheetAsset::ClearCache();
        Model::ClearCache();
        Texture2D::ClearCache();

        Renderer2D::Shutdown();
        Renderer3D::Shutdown();
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }

} // namespace Wheatear
