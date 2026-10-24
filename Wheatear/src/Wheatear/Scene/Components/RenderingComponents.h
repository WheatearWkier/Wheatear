#pragma once

// Sprite/circle/mesh rendering and light components.

#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/UUID.h"
#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Renderer/Mesh.h"
#include "Wheatear/Renderer/Model.h"
#include "Wheatear/Renderer/Texture.h"

namespace Wheatear {

    struct SpriteRendererComponent
    {
        glm::vec4      Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Ref<Texture2D> Texture;
        // Stable reference to the source texture in the asset registry. The
        // path is still serialized as a fallback for hand-written scenes.
        UUID           TextureAsset = UUID(0);
        float          TilingFactor = 1.0f;

        int            SortingLayer = 0;
        int            OrderInLayer = 0;

        glm::vec2      UVMin = { 0.0f, 0.0f };
        glm::vec2      UVMax = { 1.0f, 1.0f };
        bool           FlipX = false;
        glm::vec2      DrawOffset = { 0.0f, 0.0f };
        glm::vec2      DrawScale = { 1.0f, 1.0f };

        // Pixels-per-world-unit. 0 = legacy behavior (quad is 1x1 x scale);
        // >0 renders the quad at (UV span in pixels) / PPU so sprites keep
        // their true aspect ratio — trimmed sheet cells included. Also the
        // basis for cell-driven collider sizes.
        float          PixelsPerUnit = 0.0f;

        // Optional reusable sprite-sheet reference (.wtsheet). When set with a
        // valid CellIndex, the SpriteSheetSystem resolves Texture/UVMin/UVMax
        // from the sheet definition each frame (grid changes hot-update).
        // SubRect (non-empty) references a named irregular rect instead of a
        // grid cell and takes priority when both are set.
        std::string    SpriteSheet;
        int            CellIndex = -1;
        std::string    SubRect;

        SpriteRendererComponent() = default;
        SpriteRendererComponent(const SpriteRendererComponent&) = default;
        SpriteRendererComponent(const glm::vec4& color) : Color(color) {}
    };

    struct CircleRendererComponent
    {
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float     Thickness = 1.0f;
        float     Fade = 0.005f;
        int       SortingLayer = 0;
        int       OrderInLayer = 0;

        CircleRendererComponent() = default;
        CircleRendererComponent(const CircleRendererComponent&) = default;
    };

    struct MeshRendererComponent
    {
        Ref<Mesh>     Mesh;
        Ref<Material> Material;
        Ref<Model>    Model;

        // Stable references to the source assets. ModelAsset points at the
        // FBX/glTF source; Model::Create resolves the cooked .wtmodel next to
        // it. Paths are kept in the scene for backwards compatibility.
        UUID          ModelAsset = UUID(0);
        UUID          MaterialAsset = UUID(0);

        MeshRendererComponent()
        {
            Material = Material::Create();
        }
        MeshRendererComponent(const MeshRendererComponent&) = default;
    };

    struct DirectionalLightComponent
    {
        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float     Intensity = 1.0f;

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent&) = default;
    };

    struct PointLightComponent
    {
        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float     Intensity = 1.0f;
        float     Constant = 1.0f;
        float     Linear = 0.09f;
        float     Quadratic = 0.032f;

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent&) = default;
    };

} // namespace Wheatear
