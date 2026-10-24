#include "wtpch.h"
#include "Model.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Assets/Import/ModelImporter.h"
#include "Wheatear/Core/Log.h"

#include <cstring>
#include <fstream>
#include <unordered_map>

namespace Wheatear {

    namespace {

        std::unordered_map<std::string, Ref<Model>>& ModelCache()
        {
            static std::unordered_map<std::string, Ref<Model>> cache;
            return cache;
        }

        constexpr char kCookedMagic[8] = {
            'W', 'T', 'M', 'O', 'D', 'E', 'L', '1'
        };
        constexpr uint32_t kCookedVersion = 3;
        constexpr uint32_t kCookedVersionLegacy = 1;
        constexpr uint32_t kCookedVersionTrsAnim = 2;
        constexpr uint32_t kMaxParts = 10000;
        constexpr uint32_t kMaxVertices = 10000000;
        constexpr uint32_t kMaxBones = 4096;
        constexpr uint32_t kMaxAnimations = 256;
        constexpr uint32_t kMaxFrames = 100000;

        struct MeshVertexV1
        {
            glm::vec3 Position;
            glm::vec3 Normal;
            glm::vec2 TexCoord;
        };

        template<typename T>
        bool ReadValue(std::ifstream& stream, T& value)
        {
            stream.read(reinterpret_cast<char*>(&value), sizeof(T));
            return stream.good();
        }

        bool ReadString(std::ifstream& stream, std::string& value)
        {
            uint32_t length = 0;
            if (!ReadValue(stream, length) || length > 16 * 1024 * 1024)
                return false;
            value.resize(length);
            if (length > 0)
                stream.read(value.data(), length);
            return stream.good();
        }

        Ref<Texture2D> LoadTexture(const std::string& path)
        {
            return path.empty() ? nullptr : Texture2D::Create(path);
        }

        bool ReadCookedModel(const std::filesystem::path& path,
            std::vector<ModelPart>& parts,
            std::vector<ModelBone>& bones,
            std::vector<ModelAnimation>& animations,
            std::string* error)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream.is_open())
            {
                if (error)
                    *error = "Failed to open cooked model.";
                return false;
            }

            char magic[8] = {};
            stream.read(magic, sizeof(magic));
            if (std::memcmp(magic, kCookedMagic, sizeof(magic)) != 0)
            {
                if (error)
                    *error = "Invalid cooked model magic.";
                return false;
            }

            uint32_t version = 0;
            uint32_t partCount = 0;
            if (!ReadValue(stream, version)
                || (version != kCookedVersion
                    && version != kCookedVersionTrsAnim
                    && version != kCookedVersionLegacy)
                || !ReadValue(stream, partCount) || partCount > kMaxParts)
            {
                if (error)
                    *error = "Unsupported cooked model version.";
                return false;
            }

            parts.clear();
            bones.clear();
            animations.clear();
            parts.reserve(partCount);

            for (uint32_t i = 0; i < partCount; ++i)
            {
                ModelPart part;
                if (!ReadString(stream, part.Name))
                {
                    if (error)
                        *error = "Truncated cooked model name.";
                    return false;
                }

                stream.read(reinterpret_cast<char*>(&part.Transform[0][0]),
                    sizeof(float) * 16);
                if (!stream.good())
                {
                    if (error)
                        *error = "Truncated cooked model transform.";
                    return false;
                }

                if (version >= 2)
                {
                    uint8_t skinned = 0;
                    if (!ReadValue(stream, skinned))
                    {
                        if (error)
                            *error = "Truncated cooked model skin flag.";
                        return false;
                    }
                    part.Skinned = skinned != 0;
                }

                uint32_t vertexCount = 0;
                uint32_t indexCount = 0;
                if (!ReadValue(stream, vertexCount) || vertexCount > kMaxVertices
                    || !ReadValue(stream, indexCount) || indexCount > kMaxVertices * 3)
                {
                    if (error)
                        *error = "Invalid cooked model counts.";
                    return false;
                }

                std::vector<MeshVertex> vertices;
                if (version == kCookedVersionLegacy)
                {
                    std::vector<MeshVertexV1> legacy(vertexCount);
                    if (vertexCount > 0)
                    {
                        stream.read(reinterpret_cast<char*>(legacy.data()),
                            static_cast<std::streamsize>(
                                sizeof(MeshVertexV1) * vertexCount));
                    }
                    vertices.resize(vertexCount);
                    for (uint32_t i = 0; i < vertexCount; ++i)
                    {
                        vertices[i].Position = legacy[i].Position;
                        vertices[i].Normal = legacy[i].Normal;
                        vertices[i].TexCoord = legacy[i].TexCoord;
                    }
                }
                else if (vertexCount > 0)
                {
                    vertices.resize(vertexCount);
                    stream.read(reinterpret_cast<char*>(vertices.data()),
                        static_cast<std::streamsize>(
                            sizeof(MeshVertex) * vertexCount));
                }

                std::vector<uint32_t> indices(indexCount);
                if (indexCount > 0)
                {
                    stream.read(reinterpret_cast<char*>(indices.data()),
                        static_cast<std::streamsize>(
                            sizeof(uint32_t) * indexCount));
                }
                if (!stream.good())
                {
                    if (error)
                        *error = "Truncated cooked model mesh data.";
                    return false;
                }

                glm::vec4 albedo(1.0f);
                float metallic = 0.0f;
                float roughness = 0.5f;
                bool flipNormals = false;
                std::string albedoPath;
                std::string normalPath;
                std::string roughnessPath;
                std::string metallicPath;

                stream.read(reinterpret_cast<char*>(&albedo[0]), sizeof(float) * 4);
                ReadValue(stream, metallic);
                ReadValue(stream, roughness);
                ReadValue(stream, flipNormals);
                if (!ReadString(stream, albedoPath)
                    || !ReadString(stream, normalPath)
                    || !ReadString(stream, roughnessPath)
                    || !ReadString(stream, metallicPath))
                {
                    if (error)
                        *error = "Truncated cooked model material.";
                    return false;
                }

                part.Mesh = Mesh::Create(vertices, indices);
                part.SourceVertices = vertices;
                part.SourceIndices = indices;
                if (!part.Mesh)
                {
                    if (error)
                        *error = "Failed to create cooked model mesh.";
                    return false;
                }

                part.Material = Material::Create();
                part.Material->Albedo = albedo;
                part.Material->Metallic = metallic;
                part.Material->Roughness = roughness;
                part.Material->FlipNormals = flipNormals;
                part.Material->AlbedoMap = LoadTexture(albedoPath);
                part.Material->NormalMap = LoadTexture(normalPath);
                part.Material->RoughnessMap = LoadTexture(roughnessPath);
                part.Material->MetallicMap = LoadTexture(metallicPath);

                parts.push_back(std::move(part));
            }

            if (version >= 2)
            {
                uint32_t boneCount = 0;
                if (!ReadValue(stream, boneCount) || boneCount > kMaxBones)
                {
                    if (error)
                        *error = "Invalid cooked model bone count.";
                    return false;
                }

                bones.resize(boneCount);
                for (ModelBone& bone : bones)
                {
                    if (!ReadString(stream, bone.Name))
                    {
                        if (error)
                            *error = "Truncated cooked bone name.";
                        return false;
                    }
                    if (!ReadValue(stream, bone.Parent)
                        || !ReadValue(stream, bone.BindTranslation)
                        || !ReadValue(stream, bone.BindRotation)
                        || !ReadValue(stream, bone.BindScale))
                    {
                        if (error)
                            *error = "Truncated cooked bone bind pose.";
                        return false;
                    }
                    stream.read(reinterpret_cast<char*>(&bone.InverseBind[0][0]),
                        sizeof(float) * 16);
                    if (!stream.good())
                    {
                        if (error)
                            *error = "Truncated cooked inverse bind matrix.";
                        return false;
                    }
                }

                uint32_t animationCount = 0;
                if (!ReadValue(stream, animationCount) || animationCount > kMaxAnimations)
                {
                    if (error)
                        *error = "Invalid cooked animation count.";
                    return false;
                }

                animations.resize(animationCount);
                for (ModelAnimation& animation : animations)
                {
                    if (!ReadString(stream, animation.Name)
                        || !ReadValue(stream, animation.Duration)
                        || !ReadValue(stream, animation.FrameCount)
                        || !ReadValue(stream, animation.FramesPerSecond))
                    {
                        if (error)
                            *error = "Truncated cooked animation header.";
                        return false;
                    }
                    if (version >= 3)
                    {
                        uint8_t precomputed = 0;
                        if (!ReadValue(stream, precomputed))
                        {
                            if (error)
                                *error = "Truncated cooked animation mode.";
                            return false;
                        }
                        animation.Precomputed = precomputed != 0;
                    }
                    if (animation.FrameCount > kMaxFrames)
                    {
                        if (error)
                            *error = "Cooked animation has too many frames.";
                        return false;
                    }

                    const size_t samples = static_cast<size_t>(animation.FrameCount) + 1;
                    if (animation.Precomputed)
                    {
                        animation.BoneMatrices.assign(boneCount,
                            std::vector<glm::mat4>(samples, glm::mat4(1.0f)));
                        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
                        {
                            stream.read(reinterpret_cast<char*>(
                                animation.BoneMatrices[boneIndex].data()),
                                static_cast<std::streamsize>(
                                    sizeof(glm::mat4) * samples));
                        }
                    }
                    else
                    {
                        animation.Translations.assign(boneCount,
                            std::vector<glm::vec3>(samples));
                        animation.Rotations.assign(boneCount,
                            std::vector<glm::quat>(samples, glm::quat(1, 0, 0, 0)));
                        animation.Scales.assign(boneCount,
                            std::vector<glm::vec3>(samples, glm::vec3(1.0f)));

                        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
                        {
                            stream.read(reinterpret_cast<char*>(
                                animation.Translations[boneIndex].data()),
                                static_cast<std::streamsize>(
                                    sizeof(glm::vec3) * samples));
                            stream.read(reinterpret_cast<char*>(
                                animation.Rotations[boneIndex].data()),
                                static_cast<std::streamsize>(
                                    sizeof(glm::quat) * samples));
                            stream.read(reinterpret_cast<char*>(
                                animation.Scales[boneIndex].data()),
                                static_cast<std::streamsize>(
                                    sizeof(glm::vec3) * samples));
                        }
                    }
                    if (!stream.good())
                    {
                        if (error)
                            *error = "Truncated cooked animation samples.";
                        return false;
                    }
                }
            }

            return true;
        }

    } // namespace

    Ref<Model> Model::Create(const std::string& path)
    {
        if (path.empty())
            return nullptr;

        const std::filesystem::path resolved = AssetPath::Resolve(path);
        const std::string cacheKey = resolved.generic_string();
        auto& cache = ModelCache();
        if (const auto it = cache.find(cacheKey); it != cache.end())
            return it->second;

        Ref<Model> model = CreateRef<Model>();
        std::string error;

        const std::string extension = resolved.extension().string();
        if (extension == ".wtmodel")
        {
            if (!ReadCookedModel(resolved, model->m_Parts,
                model->m_Bones, model->m_Animations, &error))
            {
                WT_CORE_ERROR("Model::Create cooked load failed for '{}': {}",
                    resolved.string(), error);
                return nullptr;
            }
            model->m_Path = AssetPath::ToProjectRelative(resolved).generic_string();
        }
        else
        {
            std::filesystem::path cookedPath = resolved;
            cookedPath.replace_extension(".wtmodel");
            if (std::filesystem::is_regular_file(cookedPath))
            {
                if (!ReadCookedModel(cookedPath, model->m_Parts,
                    model->m_Bones, model->m_Animations, &error))
                {
                    WT_CORE_ERROR("Model::Create cooked load failed for '{}': {}",
                        cookedPath.string(), error);
                    return nullptr;
                }
                model->m_Path = AssetPath::ToProjectRelative(cookedPath).generic_string();
            }
            else
            {
                WT_CORE_WARN("Model::Create importing source '{}' at runtime; cook it to .wtmodel for shipping.",
                    resolved.string());
                if (!ModelImporter::Load(resolved, model->m_Parts, &error, {},
                    &model->m_Bones, &model->m_Animations))
                {
                    WT_CORE_ERROR("Model::Create import failed for '{}': {}",
                        resolved.string(), error);
                    return nullptr;
                }
                model->m_Path = AssetPath::ToProjectRelative(resolved).generic_string();
            }
        }

        if (model->m_Parts.empty())
        {
            WT_CORE_ERROR("Model::Create produced no drawable parts for '{}'",
                resolved.string());
            return nullptr;
        }

        cache[cacheKey] = model;
        return model;
    }

    bool Model::Cook(const std::string& sourcePath,
        const std::string& cookedPath)
    {
        const std::filesystem::path source = AssetPath::Resolve(sourcePath);
        std::filesystem::path destination = cookedPath.empty()
            ? source
            : AssetPath::Resolve(cookedPath);
        if (cookedPath.empty())
            destination.replace_extension(".wtmodel");

        ModelImporter::ImportSettings settings;
        std::string error;
        if (!ModelImporter::Cook(source, destination, settings, &error))
        {
            WT_CORE_ERROR("Model::Cook failed for '{}': {}",
                source.string(), error);
            return false;
        }
        return true;
    }

    void Model::ClearCache()
    {
        ModelCache().clear();
    }

} // namespace Wheatear
