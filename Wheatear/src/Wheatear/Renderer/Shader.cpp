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

    //  ShaderLibrary

    void ShaderLibrary::Add(const Ref<Shader>& shader)
    {
        Add(shader->GetName(), shader);
    }

    void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
    {
        WT_CORE_ASSERT(!Exists(name), "Shader '{0}' already exists in library", name);

        shader->SetName(name);
        m_Shaders[name] = shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);
        Add(shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& name,
        const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);
        Add(name, shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Get(const std::string& name) const
    {
        WT_CORE_ASSERT(Exists(name), "Shader '{0}' not found in library", name);
        return m_Shaders.at(name);
    }

    bool ShaderLibrary::Exists(const std::string& name) const
    {
        return m_Shaders.find(name) != m_Shaders.end();
    }

} // namespace Wheatear
