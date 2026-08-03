#pragma once
#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Buffer.h"
#include "Wheatear/Renderer/VertexArray.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Wheatear {

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    // -------------------------------------------------------------------------
    //
    //   auto mesh = Mesh::CreateCube();
    //   Renderer3D::DrawMesh(transform, mesh, material, entityID);
    // -------------------------------------------------------------------------
    class Mesh
    {
    public:
        Mesh() = default;

        static Ref<Mesh> Create(const std::string& filepath);

        static Ref<Mesh> Create(const std::vector<MeshVertex>& vertices,
            const std::vector<uint32_t>& indices);

        static Ref<Mesh> CreateCube();
        static Ref<Mesh> CreateSphere(uint32_t sectorCount = 36,
            uint32_t stackCount = 18);

        // ---- Getters ----
        const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
        uint32_t                GetIndexCount()  const { return m_IndexCount; }
        const std::string& GetFilepath()    const { return m_Filepath; }

    private:
        void UploadToGPU(const std::vector<MeshVertex>& vertices,
            const std::vector<uint32_t>& indices);

        bool LoadOBJ(const std::string& filepath,
            std::vector<MeshVertex>& outVertices,
            std::vector<uint32_t>& outIndices);

        static void BuildSphere(uint32_t sectorCount, uint32_t stackCount,
            std::vector<MeshVertex>& outVertices,
            std::vector<uint32_t>& outIndices);

    private:
        Ref<VertexArray> m_VertexArray;
        uint32_t         m_IndexCount = 0;
        std::string      m_Filepath;
    };

} // namespace Wheatear
