#include "wtpch.h"
#include "Shader.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Wheatear {


    Ref<Shader> Shader::Create(const std::string& filepath)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLShader>(filepath);
        case RendererAPI::API::None:
            WT_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        WT_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    Ref<Shader> Shader::Create(const std::string& name,
        const std::string& vertexSrc,
        const std::string& fragmentSrc)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLShader>(name, vertexSrc, fragmentSrc);
        case RendererAPI::API::None:
            WT_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        WT_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

} // namespace Wheatear
