#pragma once
#include "Wheatear/Core/Core.h"
#include "VertexArray.h"

#include <cstdint>
#include <glm/glm.hpp>

namespace Wheatear {

    enum class DepthFunc
    {
        Less,        // GL_LESS  (default)
        LessOrEqual  // GL_LEQUAL (skybox pass)
    };

    class RendererAPI
    {
    public:
        enum class API
        {
            None = 0,
            OpenGL = 1
        };

        virtual ~RendererAPI() = default;

        virtual void Init() = 0;

        virtual void SetViewport(uint32_t x, uint32_t y,
            uint32_t width, uint32_t height) = 0;
        virtual void SetClearColor(const glm::vec4& color) = 0;
        virtual void Clear() = 0;

        virtual void DrawIndexed(const Ref<VertexArray>& vertexArray,
            uint32_t indexCount = 0) = 0;
        virtual void DrawLines(const Ref<VertexArray>& vertexArray,
            uint32_t vertexCount) = 0;
        virtual void DrawArrays(const Ref<VertexArray>& vertexArray,
            uint32_t vertexCount) = 0;

        virtual void SetLineWidth(float width) = 0;

        virtual void SetScissor(uint32_t x, uint32_t y,
            uint32_t width, uint32_t height) = 0;
        virtual void SetScissorTest(bool enabled) = 0;

        virtual void EnableDepthTest() = 0;
        virtual void DisableDepthTest() = 0;
        virtual void SetDepthWrite(bool enabled) = 0;
        virtual void SetDepthFunc(DepthFunc func) = 0;

        virtual void SetBlend(bool enabled) = 0;

        virtual uint32_t GetBoundFramebuffer() = 0;
        virtual void     BindFramebuffer(uint32_t id) = 0;

        virtual void BindTextureUnit(uint32_t slot, uint32_t textureID) = 0;

        static API GetAPI() { return s_API; }

        static Scope<RendererAPI> Create();

    private:
        static API s_API;
    };

} // namespace Wheatear
