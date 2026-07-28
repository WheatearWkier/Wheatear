#include "wtpch.h"
#include "UniformBuffer.h"

#include "Wheatear/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace Wheatear {

    Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLUniformBuffer>(size, binding);
        case RendererAPI::API::None:
            WT_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        WT_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

} // namespace Wheatear