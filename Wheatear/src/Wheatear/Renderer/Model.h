#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Renderer/Mesh.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace Wheatear {

    struct ModelPart
    {
        Ref<Mesh> Mesh;
        Ref<Material> Material;
        glm::mat4 Transform = glm::mat4(1.0f);
        std::string Name;

        // Skinned parts keep their vertices in mesh space and are deformed by
        // the bone palette at draw time. Transform is then only the import
        // scale, never the mesh node's world transform.
        bool Skinned = false;

        // CPU-side data kept by the importer so the editor can write cooked
        // .wtmodel files without re-reading the source format.
        std::vector<MeshVertex> SourceVertices;
        std::vector<uint32_t> SourceIndices;
    };

    struct ModelBone
    {
        std::string Name;
        int Parent = -1;
        glm::vec3 BindTranslation = { 0.0f, 0.0f, 0.0f };
        glm::quat BindRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 BindScale = { 1.0f, 1.0f, 1.0f };
        glm::mat4 InverseBind = glm::mat4(1.0f);
    };

    // Animation is baked to a fixed frame rate at import/cook time. This keeps
    // runtime sampling independent of the source format's curve types and makes
    // the cooked asset self-contained.
    struct ModelAnimation
    {
        std::string Name;
        float Duration = 0.0f;
        uint32_t FrameCount = 0; // samples at 0..FrameCount inclusive
        float FramesPerSecond = 30.0f;

        // One row per bone, FrameCount + 1 samples.
        std::vector<std::vector<glm::vec3>> Translations;
        std::vector<std::vector<glm::quat>> Rotations;
        std::vector<std::vector<glm::vec3>> Scales;

        // FBX uses precomputed model-space bone palettes because compressing
        // its inheritance/axis rules into local TRS can change the pose. The
        // matrices are already boneWorld * geometryToBone.
        bool Precomputed = false;
        std::vector<std::vector<glm::mat4>> BoneMatrices;
    };

    // Static or skinned model asset assembled from material-split submeshes.
    // FBX/glTF import produces parts; MeshRendererComponent can draw either a
    // single Mesh (procedural cube/sphere/OBJ) or a full Model.
    class WHEATEAR_API Model
    {
    public:
        static Ref<Model> Create(const std::string& path);
        static bool Cook(const std::string& sourcePath,
            const std::string& cookedPath = {});
        static void ClearCache();

        const std::vector<ModelPart>& GetParts() const { return m_Parts; }
        const std::vector<ModelBone>& GetBones() const { return m_Bones; }
        const std::vector<ModelAnimation>& GetAnimations() const { return m_Animations; }
        const std::string& GetPath() const { return m_Path; }
        bool IsValid() const { return !m_Parts.empty(); }
        bool HasSkin() const { return !m_Bones.empty(); }

    private:
        std::vector<ModelPart> m_Parts;
        std::vector<ModelBone> m_Bones;
        std::vector<ModelAnimation> m_Animations;
        std::string m_Path;
    };

} // namespace Wheatear
