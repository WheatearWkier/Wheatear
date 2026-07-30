#include "wtpch.h"
#include "Renderer.h"

#include "Renderer2D.h"
#include "Renderer3D.h"
#include "RenderCommand.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Wheatear {

    Renderer::SceneData* Renderer::m_SceneData =
        new Renderer::SceneData;

    std::chrono::high_resolution_clock::time_point Renderer::s_StartTime =
        std::chrono::high_resolution_clock::now();


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
        delete m_SceneData;
        m_SceneData = nullptr;
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }


    void Renderer::BeginScene(const glm::mat4& viewProjection)
    {
        m_SceneData->ViewProjectionMatrix = viewProjection;

        const auto now = std::chrono::high_resolution_clock::now();
        m_SceneData->Time = std::chrono::duration<float>(now - s_StartTime).count();
    }

    void Renderer::EndScene()
    {
    }


    void Renderer::Submit(
        const std::shared_ptr<Shader>& shader,
        const std::shared_ptr<VertexArray>& vertexArray,
        const glm::mat4& transform)
    {
        auto glShader = std::dynamic_pointer_cast<OpenGLShader>(shader);
        glShader->Bind();
        glShader->UploadUniformMat4("u_ViewProjection", m_SceneData->ViewProjectionMatrix);
        glShader->UploadUniformMat4("u_Transform", transform);
        glShader->UploadUniformFloat("u_Time", m_SceneData->Time);

        vertexArray->Bind();
        RenderCommand::DrawIndexed(vertexArray);
    }

} // namespace Wheatear
