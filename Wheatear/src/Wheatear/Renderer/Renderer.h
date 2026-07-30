#pragma once

#include "RendererAPI.h"
#include "Shader.h"
#include "VertexArray.h"

#include <chrono>
#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace Wheatear {

    class Renderer
    {
    public:
        static void Init();
        static void Shutdown();
        static void OnWindowResize(uint32_t width, uint32_t height);

        static void BeginScene(const glm::mat4& viewProjection);
        static void EndScene();

        static void Submit(
            const std::shared_ptr<Shader>& shader,
            const std::shared_ptr<VertexArray>& vertexArray,
            const glm::mat4& transform = glm::mat4(1.0f)
        );

        static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

    private:
        struct SceneData
        {
            glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
            float     Time = 0.0f;
        };

        static SceneData* m_SceneData;
        static std::chrono::high_resolution_clock::time_point s_StartTime;
    };

} // namespace Wheatear
