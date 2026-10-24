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
        // GPU skinning influences (up to four bones per vertex). Static meshes
        // leave these zeroed; the shader only reads them when u_Skinned != 0.
        glm::ivec4 BoneIndices = { 0, 0, 0, 0 };
        glm::vec4  BoneWeights = { 0.0f, 0.0f, 0.0f, 0.0f };
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
        const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
        const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

        // Builds a throw-away VAO that shares this mesh's vertex/index buffers
        // and adds per-instance model matrices (locations 5-8). The returned
        // VAO owns the instance buffer and must stay alive for the draw.
        Ref<VertexArray> CreateInstancedVAO(
            const std::vector<glm::mat4>& transforms) const;

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
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer>  m_IndexBuffer;
        mutable Ref<VertexArray>  m_InstancedVAO;
        mutable Ref<VertexBuffer> m_InstanceBuffer;
        mutable size_t            m_InstanceCapacity = 0;
        uint32_t         m_IndexCount = 0;
        std::string      m_Filepath;
        glm::vec3        m_BoundsMin = { 0.0f, 0.0f, 0.0f };
        glm::vec3        m_BoundsMax = { 0.0f, 0.0f, 0.0f };
    };

} // namespace Wheatear
