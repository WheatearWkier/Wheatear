#include "wtpch.h"
#include "TerrainSystem.h"

#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Renderer/Mesh.h"
#include "Wheatear/Renderer/Texture.h"

#include <vector>

namespace Wheatear {

    namespace {

        Ref<Texture2D> CreateCheckerTexture()
        {
            static Ref<Texture2D> texture;
            if (texture)
                return texture;

            constexpr uint32_t size = 64;
            constexpr uint32_t cell = 8;
            std::vector<uint32_t> pixels(size * size);
            for (uint32_t y = 0; y < size; ++y)
            {
                for (uint32_t x = 0; x < size; ++x)
                {
                    const bool light = ((x / cell) + (y / cell)) % 2 == 0;
                    const uint8_t r = light ? 96 : 78;
                    const uint8_t g = light ? 128 : 106;
                    const uint8_t b = light ? 74 : 62;
                    pixels[static_cast<size_t>(y) * size + x] =
                        0xff000000u | (static_cast<uint32_t>(b) << 16)
                        | (static_cast<uint32_t>(g) << 8) | r;
                }
            }

            texture = Texture2D::Create(size, size);
            texture->SetData(pixels.data(),
                static_cast<uint32_t>(pixels.size() * sizeof(uint32_t)));
            return texture;
        }

    } // namespace

    bool TerrainSystem::BuildTerrain(Scene* scene, TerrainComponent& terrain)
    {
        if (!scene)
            return false;

        if (terrain.CellsX <= 0 || terrain.CellsZ <= 0 || terrain.CellSize <= 0.0f)
            return false;

        if (terrain.Heights.empty())
            terrain.GenerateProcedural();

        const size_t expected = static_cast<size_t>(terrain.CellsX + 1)
            * static_cast<size_t>(terrain.CellsZ + 1);
        if (terrain.Heights.size() < expected)
            terrain.GenerateProcedural();

        std::vector<MeshVertex> vertices(expected);
        std::vector<uint32_t> indices;
        indices.reserve(static_cast<size_t>(terrain.CellsX) * terrain.CellsZ * 6);

        const float halfWidth = terrain.CellsX * terrain.CellSize * 0.5f;
        const float halfDepth = terrain.CellsZ * terrain.CellSize * 0.5f;
        const auto heightAt = [&](int x, int z) -> float
        {
            const int clampedX = std::clamp(x, 0, terrain.CellsX);
            const int clampedZ = std::clamp(z, 0, terrain.CellsZ);
            return terrain.Heights[static_cast<size_t>(clampedZ)
                * static_cast<size_t>(terrain.CellsX + 1)
                + static_cast<size_t>(clampedX)];
        };

        for (int z = 0; z <= terrain.CellsZ; ++z)
        {
            for (int x = 0; x <= terrain.CellsX; ++x)
            {
                const int index = z * (terrain.CellsX + 1) + x;
                MeshVertex& vertex = vertices[static_cast<size_t>(index)];
                const float worldX = static_cast<float>(x) * terrain.CellSize - halfWidth;
                const float worldZ = static_cast<float>(z) * terrain.CellSize - halfDepth;
                vertex.Position = { worldX, heightAt(x, z), worldZ };

                const float left = heightAt(x - 1, z);
                const float right = heightAt(x + 1, z);
                const float back = heightAt(x, z - 1);
                const float front = heightAt(x, z + 1);
                const float slopeX = (right - left) / (2.0f * terrain.CellSize);
                const float slopeZ = (front - back) / (2.0f * terrain.CellSize);
                vertex.Normal = glm::normalize(glm::vec3(-slopeX, 1.0f, -slopeZ));
                vertex.TexCoord = {
                    vertex.Position.x * 0.5f,
                    vertex.Position.z * 0.5f
                };
            }
        }

        for (int z = 0; z < terrain.CellsZ; ++z)
        {
            for (int x = 0; x < terrain.CellsX; ++x)
            {
                const uint32_t topLeft = static_cast<uint32_t>(
                    z * (terrain.CellsX + 1) + x);
                const uint32_t topRight = topLeft + 1;
                const uint32_t bottomLeft = static_cast<uint32_t>(
                    (z + 1) * (terrain.CellsX + 1) + x);
                const uint32_t bottomRight = bottomLeft + 1;

                indices.insert(indices.end(), {
                    topLeft, bottomLeft, topRight,
                    topRight, bottomLeft, bottomRight
                });
            }
        }

        terrain.Mesh = Mesh::Create(vertices, indices);
        if (!terrain.Mesh)
            return false;

        if (!terrain.Material)
        {
            terrain.Material = Material::Create();
            terrain.Material->Albedo = glm::vec4(1.0f);
            terrain.Material->Metallic = 0.0f;
            terrain.Material->Roughness = 0.92f;
            terrain.Material->AlbedoMap = CreateCheckerTexture();
        }

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TerrainComponent>())
        {
            if (&registry.get<TerrainComponent>(e) != &terrain)
                continue;

            Entity entity(e, scene);
            if (!entity.HasComponent<MeshRendererComponent>())
                entity.AddComponent<MeshRendererComponent>();

            auto& renderer = entity.GetComponent<MeshRendererComponent>();
            renderer.Mesh = terrain.Mesh;
            renderer.Material = terrain.Material;
            renderer.Model = nullptr;
            break;
        }

        terrain.Dirty = false;
        return true;
    }

    void TerrainSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TerrainComponent>())
        {
            TerrainComponent& terrain = registry.get<TerrainComponent>(e);
            if (terrain.Heights.empty())
                terrain.GenerateProcedural();
            BuildTerrain(scene, terrain);
        }
    }

    void TerrainSystem::OnEditorStart(Scene* scene)
    {
        OnRuntimeStart(scene);
    }

    void TerrainSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        (void)ts;
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TerrainComponent>())
        {
            TerrainComponent& terrain = registry.get<TerrainComponent>(e);
            if (terrain.Dirty)
                BuildTerrain(scene, terrain);
        }
    }

    void TerrainSystem::OnUpdateEditor(Scene* scene, Timestep ts)
    {
        OnUpdateRuntime(scene, ts);
    }

} // namespace Wheatear
