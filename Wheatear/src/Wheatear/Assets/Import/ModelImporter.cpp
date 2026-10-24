#include "wtpch.h"
#include "ModelImporter.h"

#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include "Wheatear/Renderer/Material.h"
#include "Wheatear/Renderer/Mesh.h"
#include "Wheatear/Renderer/Texture.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <ufbx.h>

#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Wheatear::ModelImporter {

    namespace {

        constexpr char kCookedMagic[8] = {
            'W', 'T', 'M', 'O', 'D', 'E', 'L', '1'
        };
        constexpr uint32_t kCookedVersion = 3;
        constexpr float kBakeFramesPerSecond = 30.0f;
        constexpr size_t kMaxBakeFrames = 100000;

        template<typename T>
        bool WriteValue(std::ofstream& stream, const T& value)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
            return stream.good();
        }

        bool WriteString(std::ofstream& stream, const std::string& value)
        {
            const uint32_t length = static_cast<uint32_t>(value.size());
            if (!WriteValue(stream, length))
                return false;
            if (length > 0)
                stream.write(value.data(), length);
            return stream.good();
        }

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return text;
        }

        std::string UfbxString(const ufbx_string& value)
        {
            return value.data ? std::string(value.data, value.length) : std::string{};
        }

        glm::mat4 UfbxMatrixToGlm(const ufbx_matrix& matrix)
        {
            glm::mat4 result(1.0f);
            result[0] = { matrix.cols[0].x, matrix.cols[0].y, matrix.cols[0].z, 0.0f };
            result[1] = { matrix.cols[1].x, matrix.cols[1].y, matrix.cols[1].z, 0.0f };
            result[2] = { matrix.cols[2].x, matrix.cols[2].y, matrix.cols[2].z, 0.0f };
            result[3] = { matrix.cols[3].x, matrix.cols[3].y, matrix.cols[3].z, 1.0f };
            return result;
        }

        glm::mat4 CgltfMatrixToGlm(const cgltf_float* values)
        {
            glm::mat4 result(1.0f);
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                    result[column][row] = values[column * 4 + row];
            }
            return result;
        }

        std::filesystem::path ResolveTexturePath(
            const std::filesystem::path& modelPath,
            const std::string& uri,
            const ImportSettings& settings)
        {
            if (uri.empty())
                return {};

            std::filesystem::path path = uri;
            if (path.is_relative())
                path = modelPath.parent_path() / path;

            std::error_code error;
            if (std::filesystem::is_regular_file(path, error))
                return path;

            if (!settings.TextureSearchPath.empty())
            {
                std::filesystem::path searchRoot =
                    AssetPath::Resolve(settings.TextureSearchPath);
                std::filesystem::path candidate = searchRoot / uri;
                if (std::filesystem::is_regular_file(candidate, error))
                    return candidate;

                candidate = searchRoot / path.filename();
                if (std::filesystem::is_regular_file(candidate, error))
                    return candidate;
            }

            return {};
        }

        struct ImageData
        {
            int Width = 0;
            int Height = 0;
            std::vector<unsigned char> Pixels;
        };

        bool LoadImageFile(const std::filesystem::path& path, ImageData& image)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            unsigned char* pixels = stbi_load(
                path.string().c_str(), &width, &height, &channels, 4);
            if (!pixels)
                return false;

            image.Width = width;
            image.Height = height;
            image.Pixels.assign(pixels,
                pixels + static_cast<size_t>(width) * height * 4);
            stbi_image_free(pixels);
            return true;
        }

        std::filesystem::path WriteImage(const std::filesystem::path& path,
            const ImageData& image)
        {
            if (image.Width <= 0 || image.Height <= 0 || image.Pixels.empty())
                return {};

            if (!stbi_write_png(path.string().c_str(),
                    image.Width, image.Height, 4,
                    image.Pixels.data(), image.Width * 4))
            {
                return {};
            }
            return path;
        }

        std::filesystem::path DerivedTexturePath(
            const std::filesystem::path& modelPath,
            const std::string& tag)
        {
            return modelPath.parent_path()
                / (modelPath.stem().string() + "_" + tag + ".png");
        }

        std::filesystem::path ProcessNormalTexture(
            const std::filesystem::path& normalPath,
            const std::filesystem::path& modelPath,
            const ImportSettings& settings)
        {
            if (!settings.FlipNormalGreen || normalPath.empty())
                return normalPath;

            ImageData image;
            if (!LoadImageFile(normalPath, image))
                return normalPath;

            for (size_t i = 0; i + 3 < image.Pixels.size(); i += 4)
                image.Pixels[i + 1] = static_cast<unsigned char>(
                    255 - image.Pixels[i + 1]);

            const std::filesystem::path output =
                DerivedTexturePath(modelPath, "normal_fixed");
            const std::filesystem::path written = WriteImage(output, image);
            return written.empty() ? normalPath : written;
        }

        bool SplitPackedTexture(const std::filesystem::path& packedPath,
            MaterialChannelMode mode,
            const std::filesystem::path& modelPath,
            std::filesystem::path& roughnessPath,
            std::filesystem::path& metallicPath)
        {
            ImageData source;
            if (!LoadImageFile(packedPath, source))
                return false;

            ImageData roughness;
            ImageData metallic;
            roughness.Width = source.Width;
            roughness.Height = source.Height;
            metallic.Width = source.Width;
            metallic.Height = source.Height;
            roughness.Pixels.resize(source.Pixels.size());
            metallic.Pixels.resize(source.Pixels.size());

            for (size_t i = 0; i + 3 < source.Pixels.size(); i += 4)
            {
                const unsigned char r = source.Pixels[i + 0];
                const unsigned char g = source.Pixels[i + 1];
                const unsigned char b = source.Pixels[i + 2];
                const unsigned char a = source.Pixels[i + 3];

                unsigned char roughnessValue = g;
                unsigned char metallicValue = b;
                if (mode == MaterialChannelMode::MetallicSmoothness)
                {
                    metallicValue = r;
                    roughnessValue = static_cast<unsigned char>(255 - a);
                }

                roughness.Pixels[i + 0] = roughnessValue;
                roughness.Pixels[i + 1] = roughnessValue;
                roughness.Pixels[i + 2] = roughnessValue;
                roughness.Pixels[i + 3] = 255;

                metallic.Pixels[i + 0] = metallicValue;
                metallic.Pixels[i + 1] = metallicValue;
                metallic.Pixels[i + 2] = metallicValue;
                metallic.Pixels[i + 3] = 255;
            }

            roughnessPath = WriteImage(
                DerivedTexturePath(modelPath, "roughness"), roughness);
            metallicPath = WriteImage(
                DerivedTexturePath(modelPath, "metallic"), metallic);
            return !roughnessPath.empty() && !metallicPath.empty();
        }

        std::filesystem::path ExtractGltfTexture(
            const cgltf_texture* texture,
            const std::filesystem::path& modelPath,
            uint32_t index,
            const ImportSettings& settings)
        {
            if (!texture || !texture->image)
                return {};

            const cgltf_image& image = *texture->image;
            if (image.uri && image.uri[0] != '\0')
                return ResolveTexturePath(modelPath, image.uri, settings);

            if (!image.buffer_view)
                return {};

            const uint8_t* bytes = cgltf_buffer_view_data(image.buffer_view);
            if (!bytes || image.buffer_view->size == 0)
                return {};

            std::string extension = ".png";
            if (image.mime_type)
            {
                const std::string mime = ToLower(image.mime_type);
                if (mime.find("jpeg") != std::string::npos
                    || mime.find("jpg") != std::string::npos)
                {
                    extension = ".jpg";
                }
            }

            const std::filesystem::path output =
                modelPath.parent_path() /
                (modelPath.stem().string() + "_embedded" +
                    std::to_string(index) + extension);
            std::ofstream stream(output, std::ios::binary | std::ios::trunc);
            if (!stream.is_open())
                return {};
            stream.write(reinterpret_cast<const char*>(bytes),
                static_cast<std::streamsize>(image.buffer_view->size));
            return stream.good() ? output : std::filesystem::path{};
        }

        std::filesystem::path ExtractUfbxTexture(
            const ufbx_texture* texture,
            const std::filesystem::path& modelPath,
            uint32_t index,
            const ImportSettings& settings)
        {
            if (!texture)
                return {};

            if (texture->content.data && texture->content.size >= 4)
            {
                const unsigned char* bytes =
                    static_cast<const unsigned char*>(texture->content.data);
                std::string extension = ".bin";
                if (bytes[0] == 0x89 && bytes[1] == 0x50)
                    extension = ".png";
                else if (bytes[0] == 0xFF && bytes[1] == 0xD8)
                    extension = ".jpg";

                const std::filesystem::path output =
                    modelPath.parent_path() /
                    (modelPath.stem().string() + "_embedded" +
                        std::to_string(index) + extension);
                std::ofstream stream(output, std::ios::binary | std::ios::trunc);
                if (!stream.is_open())
                    return {};
                stream.write(reinterpret_cast<const char*>(texture->content.data),
                    static_cast<std::streamsize>(texture->content.size));
                if (!stream.good())
                    return {};
                return output;
            }

            std::string uri = UfbxString(texture->filename);
            if (uri.empty())
                uri = UfbxString(texture->relative_filename);
            return ResolveTexturePath(modelPath, uri, settings);
        }

        Ref<Material> MakeMaterial(const glm::vec4& albedo,
            float metallic,
            float roughness,
            const std::filesystem::path& albedoPath,
            const std::filesystem::path& normalPath,
            const std::filesystem::path& packedPath,
            MaterialChannelMode channelMode,
            const ImportSettings& settings,
            const std::filesystem::path& modelPath)
        {
            Ref<Material> material = Material::Create();
            material->Albedo = albedo;
            material->Metallic = std::clamp(metallic, 0.0f, 1.0f);
            material->Roughness = std::clamp(roughness, 0.0f, 1.0f);

            if (!albedoPath.empty())
                material->AlbedoMap = Texture2D::Create(albedoPath.generic_string());

            const std::filesystem::path processedNormal =
                ProcessNormalTexture(normalPath, modelPath, settings);
            if (!processedNormal.empty())
                material->NormalMap = Texture2D::Create(processedNormal.generic_string());

            std::filesystem::path roughnessPath;
            std::filesystem::path metallicPath;
            if (!packedPath.empty())
            {
                const MaterialChannelMode effectiveMode =
                    channelMode == MaterialChannelMode::Separate
                        ? MaterialChannelMode::OcclusionRoughnessMetallic
                        : channelMode;
                SplitPackedTexture(packedPath, effectiveMode, modelPath,
                    roughnessPath, metallicPath);
            }

            if (!roughnessPath.empty())
                material->RoughnessMap = Texture2D::Create(roughnessPath.generic_string());
            if (!metallicPath.empty())
                material->MetallicMap = Texture2D::Create(metallicPath.generic_string());

            return material;
        }

        void GenerateNormals(std::vector<MeshVertex>& vertices,
            const std::vector<uint32_t>& indices)
        {
            for (const MeshVertex& vertex : vertices)
            {
                if (glm::dot(vertex.Normal, vertex.Normal) > 1e-8f)
                    return;
            }

            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                MeshVertex& a = vertices[indices[i + 0]];
                MeshVertex& b = vertices[indices[i + 1]];
                MeshVertex& c = vertices[indices[i + 2]];
                const glm::vec3 normal = glm::cross(
                    b.Position - a.Position,
                    c.Position - a.Position);
                if (glm::dot(normal, normal) <= 1e-12f)
                    continue;
                a.Normal += normal;
                b.Normal += normal;
                c.Normal += normal;
            }

            for (MeshVertex& vertex : vertices)
            {
                const float length = glm::length(vertex.Normal);
                if (length > 1e-8f)
                    vertex.Normal /= length;
                else
                    vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        // ---------------------------------------------------------------------
        //  Skeleton / animation baking
        // ---------------------------------------------------------------------

        glm::quat CgltfRotation(const cgltf_float* values)
        {
            glm::quat rotation(values[3], values[0], values[1], values[2]);
            if (glm::dot(rotation, rotation) < 1e-12f)
                return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            return glm::normalize(rotation);
        }

        glm::quat UfbxRotation(const ufbx_quat& value)
        {
            glm::quat rotation(
                static_cast<float>(value.w),
                static_cast<float>(value.x),
                static_cast<float>(value.y),
                static_cast<float>(value.z));
            if (glm::dot(rotation, rotation) < 1e-12f)
                return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            return glm::normalize(rotation);
        }

        glm::mat4 ComposeTransform(const glm::vec3& translation,
            const glm::quat& rotation,
            const glm::vec3& scale)
        {
            return glm::translate(glm::mat4(1.0f), translation)
                * glm::mat4_cast(rotation)
                * glm::scale(glm::mat4(1.0f), scale);
        }

        void DecomposeTransform(const glm::mat4& matrix,
            glm::vec3& translation,
            glm::quat& rotation,
            glm::vec3& scale)
        {
            glm::vec3 skew(0.0f);
            glm::vec4 perspective(0.0f);
            if (!glm::decompose(matrix, scale, rotation, translation, skew, perspective))
            {
                translation = glm::vec3(0.0f);
                rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                scale = glm::vec3(1.0f);
            }
        }

        struct GltfAnimationChannel
        {
            int Bone = -1;
            cgltf_animation_path_type Path = cgltf_animation_path_type_invalid;
            cgltf_interpolation_type Interpolation = cgltf_interpolation_type_linear;
            std::vector<float> Times;
            std::vector<float> Values;
            int Components = 3;
        };

        bool ReadGltfChannel(const cgltf_animation_channel& channel,
            int bone,
            GltfAnimationChannel& out)
        {
            if (!channel.sampler || !channel.sampler->input
                || !channel.sampler->output || bone < 0)
                return false;

            const cgltf_accessor* input = channel.sampler->input;
            const cgltf_accessor* output = channel.sampler->output;
            if (input->count == 0)
                return false;

            int components = 3;
            if (channel.target_path == cgltf_animation_path_type_rotation)
                components = 4;

            out.Bone = bone;
            out.Path = channel.target_path;
            out.Interpolation = channel.sampler->interpolation;
            out.Components = components;
            out.Times.resize(input->count);
            for (cgltf_size i = 0; i < input->count; ++i)
            {
                cgltf_float time = 0.0f;
                cgltf_accessor_read_float(input, i, &time, 1);
                out.Times[i] = time;
            }

            const int valueMultiplier = out.Interpolation == cgltf_interpolation_type_cubic_spline
                ? 3
                : 1;
            out.Values.resize(output->count * components * valueMultiplier);
            for (cgltf_size i = 0; i < output->count; ++i)
            {
                cgltf_float values[12] = {};
                cgltf_accessor_read_float(output, i, values, components * valueMultiplier);
                std::copy(values, values + components * valueMultiplier,
                    out.Values.begin() + static_cast<std::ptrdiff_t>(i * components * valueMultiplier));
            }
            return !out.Times.empty();
        }

        size_t GltfKeyframeBase(const GltfAnimationChannel& channel, size_t keyframe)
        {
            const size_t multiplier = channel.Interpolation == cgltf_interpolation_type_cubic_spline
                ? 3
                : 1;
            size_t offset = keyframe * channel.Components * multiplier;
            if (channel.Interpolation == cgltf_interpolation_type_cubic_spline)
                offset += channel.Components; // use the in-tangent key value
            return offset;
        }

        void SampleGltfChannel(const GltfAnimationChannel& channel,
            float time,
            glm::vec3& translation,
            glm::quat& rotation,
            glm::vec3& scale)
        {
            if (channel.Times.empty())
                return;

            size_t lower = 0;
            size_t upper = 0;
            const auto it = std::upper_bound(channel.Times.begin(), channel.Times.end(), time);
            if (it == channel.Times.begin())
            {
                lower = upper = 0;
            }
            else if (it == channel.Times.end())
            {
                lower = upper = channel.Times.size() - 1;
            }
            else
            {
                upper = static_cast<size_t>(it - channel.Times.begin());
                lower = upper - 1;
            }

            float alpha = 0.0f;
            if (lower != upper
                && channel.Interpolation != cgltf_interpolation_type_step)
            {
                const float span = channel.Times[upper] - channel.Times[lower];
                if (span > 1e-8f)
                    alpha = std::clamp((time - channel.Times[lower]) / span, 0.0f, 1.0f);
            }

            const size_t base0 = GltfKeyframeBase(channel, lower);
            const size_t base1 = GltfKeyframeBase(channel, upper);
            const float* value0 = channel.Values.data() + base0;
            const float* value1 = channel.Values.data() + base1;

            if (channel.Path == cgltf_animation_path_type_translation
                || channel.Path == cgltf_animation_path_type_scale)
            {
                glm::vec3 a(value0[0], value0[1], value0[2]);
                glm::vec3 b(value1[0], value1[1], value1[2]);
                if (channel.Path == cgltf_animation_path_type_translation)
                    translation = glm::mix(a, b, alpha);
                else
                    scale = glm::mix(a, b, alpha);
            }
            else if (channel.Path == cgltf_animation_path_type_rotation)
            {
                const glm::quat a = CgltfRotation(value0);
                const glm::quat b = CgltfRotation(value1);
                rotation = glm::normalize(glm::slerp(a, b, alpha));
            }
        }

        bool BuildGltfSkeleton(cgltf_data* data,
            std::vector<ModelBone>& bones,
            std::unordered_map<const cgltf_node*, int>& nodeToBone)
        {
            for (cgltf_size skinIndex = 0; skinIndex < data->skins_count; ++skinIndex)
            {
                const cgltf_skin& skin = data->skins[skinIndex];
                for (cgltf_size jointIndex = 0; jointIndex < skin.joints_count; ++jointIndex)
                {
                    const cgltf_node* joint = skin.joints[jointIndex];
                    if (!joint || nodeToBone.find(joint) != nodeToBone.end())
                        continue;

                    ModelBone bone;
                    bone.Name = joint->name ? joint->name : "Bone";
                    if (joint->has_matrix)
                    {
                        DecomposeTransform(CgltfMatrixToGlm(joint->matrix),
                            bone.BindTranslation, bone.BindRotation, bone.BindScale);
                    }
                    else
                    {
                        bone.BindTranslation = {
                            joint->translation[0], joint->translation[1], joint->translation[2] };
                        bone.BindRotation = CgltfRotation(joint->rotation);
                        bone.BindScale = { joint->scale[0], joint->scale[1], joint->scale[2] };
                    }

                    if (skin.inverse_bind_matrices
                        && jointIndex < skin.inverse_bind_matrices->count)
                    {
                        cgltf_float values[16] = {};
                        if (cgltf_accessor_read_float(skin.inverse_bind_matrices,
                            jointIndex, values, 16))
                        {
                            bone.InverseBind = CgltfMatrixToGlm(values);
                        }
                    }

                    const int index = static_cast<int>(bones.size());
                    nodeToBone[joint] = index;
                    bones.push_back(std::move(bone));
                }
            }

            for (const auto& [node, index] : nodeToBone)
            {
                auto parent = nodeToBone.find(node->parent);
                bones[index].Parent = parent == nodeToBone.end() ? -1 : parent->second;
            }

            return !bones.empty();
        }

        void BakeGltfAnimations(cgltf_data* data,
            const std::unordered_map<const cgltf_node*, int>& nodeToBone,
            const std::vector<ModelBone>& bones,
            std::vector<ModelAnimation>& animations)
        {
            if (bones.empty())
                return;

            for (cgltf_size animationIndex = 0;
                animationIndex < data->animations_count;
                ++animationIndex)
            {
                const cgltf_animation& source = data->animations[animationIndex];

                std::vector<GltfAnimationChannel> channels;
                float duration = 0.0f;
                for (cgltf_size channelIndex = 0;
                    channelIndex < source.channels_count;
                    ++channelIndex)
                {
                    const cgltf_animation_channel& channel = source.channels[channelIndex];
                    if (!channel.target_node)
                        continue;
                    const auto bone = nodeToBone.find(channel.target_node);
                    if (bone == nodeToBone.end())
                        continue;

                    GltfAnimationChannel baked;
                    if (!ReadGltfChannel(channel, bone->second, baked))
                        continue;

                    duration = std::max(duration, baked.Times.back());
                    channels.push_back(std::move(baked));
                }

                if (channels.empty())
                    continue;

                ModelAnimation animation;
                animation.Name = source.name ? source.name
                    : ("Animation " + std::to_string(animationIndex));
                animation.Duration = duration;
                animation.FramesPerSecond = kBakeFramesPerSecond;

                const size_t frameCount = std::max<size_t>(1,
                    static_cast<size_t>(std::ceil(duration * kBakeFramesPerSecond)));
                animation.FrameCount = static_cast<uint32_t>(std::min(
                    frameCount, kMaxBakeFrames));
                const size_t samples = animation.FrameCount + 1;

                animation.Translations.resize(bones.size());
                animation.Rotations.resize(bones.size());
                animation.Scales.resize(bones.size());
                for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
                {
                    animation.Translations[boneIndex].assign(samples,
                        bones[boneIndex].BindTranslation);
                    animation.Rotations[boneIndex].assign(samples,
                        bones[boneIndex].BindRotation);
                    animation.Scales[boneIndex].assign(samples,
                        bones[boneIndex].BindScale);
                }

                for (size_t frame = 0; frame < samples; ++frame)
                {
                    const float time = animation.FrameCount == 0
                        ? 0.0f
                        : duration * (static_cast<float>(frame)
                            / static_cast<float>(animation.FrameCount));
                    for (const GltfAnimationChannel& channel : channels)
                    {
                        SampleGltfChannel(channel, time,
                            animation.Translations[channel.Bone][frame],
                            animation.Rotations[channel.Bone][frame],
                            animation.Scales[channel.Bone][frame]);
                    }
                }

                animations.push_back(std::move(animation));
            }
        }

        struct FbxBoneRef
        {
            uint32_t NodeIndex = 0;
            uint32_t SkinIndex = 0;
            uint32_t ClusterIndex = 0;
        };

        bool BuildFbxSkeleton(ufbx_scene* scene,
            std::vector<ModelBone>& bones,
            std::unordered_map<const ufbx_skin_cluster*, int>& clusterToBone,
            std::vector<FbxBoneRef>& boneRefs)
        {
            // Each skin gets its own contiguous bone block. Two submeshes can
            // bind the same bone with different inverse bind matrices, so
            // deduplicating by node would make one part use the other part's
            // palette.
            std::unordered_set<const ufbx_skin_deformer*> processedSkins;
            for (size_t nodeIndex = 0; nodeIndex < scene->nodes.count; ++nodeIndex)
            {
                ufbx_node* node = scene->nodes.data[nodeIndex];
                if (!node || !node->mesh)
                    continue;

                for (size_t skinIndex = 0;
                    skinIndex < node->mesh->skin_deformers.count;
                    ++skinIndex)
                {
                    ufbx_skin_deformer* skin =
                        node->mesh->skin_deformers.data[skinIndex];
                    if (!skin || !processedSkins.insert(skin).second)
                        continue;

                    for (size_t clusterIndex = 0;
                        clusterIndex < skin->clusters.count;
                        ++clusterIndex)
                    {
                        ufbx_skin_cluster* cluster = skin->clusters.data[clusterIndex];
                        if (!cluster)
                            continue;

                        ModelBone bone;
                        bone.Name = cluster->bone_node
                            ? UfbxString(cluster->bone_node->name)
                            : UfbxString(cluster->name);
                        if (bone.Name.empty())
                            bone.Name = "Bone";
                        if (cluster->bone_node)
                        {
                            bone.BindTranslation = {
                                static_cast<float>(cluster->bone_node->local_transform.translation.x),
                                static_cast<float>(cluster->bone_node->local_transform.translation.y),
                                static_cast<float>(cluster->bone_node->local_transform.translation.z) };
                            bone.BindRotation = UfbxRotation(
                                cluster->bone_node->local_transform.rotation);
                            bone.BindScale = {
                                static_cast<float>(cluster->bone_node->local_transform.scale.x),
                                static_cast<float>(cluster->bone_node->local_transform.scale.y),
                                static_cast<float>(cluster->bone_node->local_transform.scale.z) };
                        }
                        bone.InverseBind = UfbxMatrixToGlm(cluster->geometry_to_bone);

                        const int index = static_cast<int>(bones.size());
                        clusterToBone[cluster] = index;
                        boneRefs.push_back({
                            static_cast<uint32_t>(nodeIndex),
                            static_cast<uint32_t>(skinIndex),
                            static_cast<uint32_t>(clusterIndex) });
                        bones.push_back(std::move(bone));
                    }
                }
            }

            return !bones.empty();
        }

        void BakeFbxAnimations(ufbx_scene* scene,
            const std::vector<FbxBoneRef>& boneRefs,
            const std::vector<ModelBone>& bones,
            std::vector<ModelAnimation>& animations)
        {
            if (bones.empty())
                return;

            for (size_t stackIndex = 0;
                stackIndex < scene->anim_stacks.count;
                ++stackIndex)
            {
                ufbx_anim_stack* stack = scene->anim_stacks.data[stackIndex];
                if (!stack || !stack->anim)
                    continue;

                const double begin = stack->time_begin;
                const double end = stack->time_end;
                const float duration = static_cast<float>(std::max(0.0, end - begin));

                ModelAnimation animation;
                animation.Name = UfbxString(stack->name);
                if (animation.Name.empty())
                    animation.Name = "Animation " + std::to_string(stackIndex);
                animation.Duration = duration;
                animation.FramesPerSecond = kBakeFramesPerSecond;

                const size_t frameCount = std::max<size_t>(1,
                    static_cast<size_t>(std::ceil(duration * kBakeFramesPerSecond)));
                animation.FrameCount = static_cast<uint32_t>(std::min(
                    frameCount, kMaxBakeFrames));
                const size_t samples = animation.FrameCount + 1;

                animation.Precomputed = true;
                animation.BoneMatrices.assign(bones.size(),
                    std::vector<glm::mat4>(samples, glm::mat4(1.0f)));

                ufbx_evaluate_opts evaluateOptions{};
                bool evaluationFailed = false;
                for (size_t frame = 0; frame < samples && !evaluationFailed; ++frame)
                {
                    const double time = animation.FrameCount == 0
                        ? begin
                        : begin + static_cast<double>(frame) / kBakeFramesPerSecond;

                    ufbx_error evaluateError{};
                    ufbx_scene* posed = ufbx_evaluate_scene(
                        scene, stack->anim, time, &evaluateOptions, &evaluateError);
                    if (!posed)
                    {
                        WT_CORE_WARN("Failed to evaluate FBX clip '{}' at {:.3f}s",
                            animation.Name, time);
                        evaluationFailed = true;
                        break;
                    }

                    // ufbx's skin cluster matrix already includes the bone's
                    // posed world transform, inverse bind, unit conversion and
                    // geometry transform. The evaluated scene preserves node /
                    // skin / cluster order, so index the same cluster directly
                    // instead of relying on element IDs being stable.
                    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
                    {
                        const FbxBoneRef& ref = boneRefs[boneIndex];
                        if (ref.NodeIndex >= posed->nodes.count)
                            continue;

                        const ufbx_node* posedNode = posed->nodes.data[ref.NodeIndex];
                        if (!posedNode || !posedNode->mesh
                            || ref.SkinIndex >= posedNode->mesh->skin_deformers.count)
                            continue;

                        const ufbx_skin_deformer* posedSkin =
                            posedNode->mesh->skin_deformers.data[ref.SkinIndex];
                        if (!posedSkin || ref.ClusterIndex >= posedSkin->clusters.count)
                            continue;

                        const ufbx_skin_cluster* cluster =
                            posedSkin->clusters.data[ref.ClusterIndex];
                        if (cluster)
                        {
                            animation.BoneMatrices[boneIndex][frame] =
                                UfbxMatrixToGlm(cluster->geometry_to_world);
                        }
                    }

                    ufbx_free_scene(posed);
                }

                if (evaluationFailed)
                    continue;

                animations.push_back(std::move(animation));
            }
        }

        bool LoadGltf(const std::filesystem::path& path,
            std::vector<ModelPart>& outParts,
            std::vector<ModelBone>& outBones,
            std::vector<ModelAnimation>& outAnimations,
            std::string* error,
            const ImportSettings& settings)
        {
            cgltf_options options{};
            cgltf_data* data = nullptr;

            cgltf_result result = cgltf_parse_file(
                &options, path.string().c_str(), &data);
            if (result != cgltf_result_success || !data)
            {
                if (error)
                    *error = "cgltf_parse_file failed.";
                return false;
            }

            result = cgltf_load_buffers(&options, data, path.string().c_str());
            if (result != cgltf_result_success)
            {
                cgltf_free(data);
                if (error)
                    *error = "cgltf_load_buffers failed.";
                return false;
            }

            std::unordered_map<const cgltf_node*, int> nodeToBone;
            BuildGltfSkeleton(data, outBones, nodeToBone);

            uint32_t textureIndex = 0;
            for (cgltf_size nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex)
            {
                const cgltf_node& node = data->nodes[nodeIndex];
                if (!node.mesh)
                    continue;

                cgltf_float worldValues[16];
                cgltf_node_transform_world(&node, worldValues);
                const glm::mat4 world = CgltfMatrixToGlm(worldValues);

                std::vector<int> skinJointToBone;
                if (node.skin)
                {
                    skinJointToBone.resize(node.skin->joints_count, -1);
                    for (cgltf_size jointIndex = 0;
                        jointIndex < node.skin->joints_count;
                        ++jointIndex)
                    {
                        const auto it = nodeToBone.find(node.skin->joints[jointIndex]);
                        if (it != nodeToBone.end())
                            skinJointToBone[jointIndex] = it->second;
                    }
                }

                for (cgltf_size primitiveIndex = 0;
                    primitiveIndex < node.mesh->primitives_count;
                    ++primitiveIndex)
                {
                    const cgltf_primitive& primitive =
                        node.mesh->primitives[primitiveIndex];
                    if (primitive.type != cgltf_primitive_type_triangles)
                        continue;

                    const cgltf_accessor* positions = nullptr;
                    const cgltf_accessor* normals = nullptr;
                    const cgltf_accessor* uvs = nullptr;
                    const cgltf_accessor* joints = nullptr;
                    const cgltf_accessor* weights = nullptr;
                    for (cgltf_size attributeIndex = 0;
                        attributeIndex < primitive.attributes_count;
                        ++attributeIndex)
                    {
                        const cgltf_attribute& attribute =
                            primitive.attributes[attributeIndex];
                        if (attribute.type == cgltf_attribute_type_position)
                            positions = attribute.data;
                        else if (attribute.type == cgltf_attribute_type_normal)
                            normals = attribute.data;
                        else if (attribute.type == cgltf_attribute_type_texcoord
                            && attribute.index == 0)
                            uvs = attribute.data;
                        else if (attribute.type == cgltf_attribute_type_joints
                            && attribute.index == 0)
                            joints = attribute.data;
                        else if (attribute.type == cgltf_attribute_type_weights
                            && attribute.index == 0)
                            weights = attribute.data;
                    }

                    const bool canSkin = node.skin && joints && weights
                        && !skinJointToBone.empty();

                    if (!positions || positions->count == 0)
                        continue;

                    std::vector<MeshVertex> vertices(positions->count);
                    for (cgltf_size i = 0; i < positions->count; ++i)
                    {
                        cgltf_float position[3] = { 0.0f, 0.0f, 0.0f };
                        cgltf_accessor_read_float(positions, i, position, 3);
                        vertices[i].Position = { position[0], position[1], position[2] };

                        if (normals)
                        {
                            cgltf_float normal[3] = { 0.0f, 1.0f, 0.0f };
                            cgltf_accessor_read_float(normals, i, normal, 3);
                            vertices[i].Normal = { normal[0], normal[1], normal[2] };
                        }

                        if (uvs)
                        {
                            cgltf_float uv[2] = { 0.0f, 0.0f };
                            cgltf_accessor_read_float(uvs, i, uv, 2);
                            vertices[i].TexCoord = { uv[0], uv[1] };
                        }

                        if (canSkin)
                        {
                            cgltf_uint jointValues[4] = {};
                            cgltf_float weightValues[4] = {};
                            cgltf_accessor_read_uint(joints, i, jointValues, 4);
                            cgltf_accessor_read_float(weights, i, weightValues, 4);

                            glm::vec4 weights4(
                                weightValues[0], weightValues[1],
                                weightValues[2], weightValues[3]);
                            const float total = weights4.x + weights4.y
                                + weights4.z + weights4.w;
                            if (total > 1e-6f)
                            {
                                weights4 /= total;
                                for (int influence = 0; influence < 4; ++influence)
                                {
                                    const cgltf_uint joint = jointValues[influence];
                                    vertices[i].BoneIndices[influence] =
                                        joint < skinJointToBone.size()
                                            ? skinJointToBone[joint]
                                            : 0;
                                    vertices[i].BoneWeights[influence] =
                                        weights4[influence];
                                }
                            }
                            else
                            {
                                // Unweighted vertices would collapse to the
                                // origin in the skinning shader. Bind them to
                                // the skin's root joint instead.
                                vertices[i].BoneIndices = { 0, 0, 0, 0 };
                                vertices[i].BoneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };
                            }
                        }
                    }

                    std::vector<uint32_t> indices;
                    if (primitive.indices)
                    {
                        indices.resize(primitive.indices->count);
                        for (cgltf_size i = 0; i < primitive.indices->count; ++i)
                            indices[i] = static_cast<uint32_t>(
                                cgltf_accessor_read_index(primitive.indices, i));
                    }
                    else
                    {
                        indices.resize(vertices.size());
                        for (size_t i = 0; i < vertices.size(); ++i)
                            indices[i] = static_cast<uint32_t>(i);
                    }

                    GenerateNormals(vertices, indices);

                    glm::vec4 albedo(1.0f);
                    float metallic = 0.0f;
                    float roughness = 0.5f;
                    std::filesystem::path albedoPath;
                    std::filesystem::path normalPath;
                    std::filesystem::path packedPath;

                    if (primitive.material)
                    {
                        const cgltf_material& sourceMaterial = *primitive.material;
                        const cgltf_pbr_metallic_roughness& pbr =
                            sourceMaterial.pbr_metallic_roughness;
                        albedo = {
                            pbr.base_color_factor[0],
                            pbr.base_color_factor[1],
                            pbr.base_color_factor[2],
                            pbr.base_color_factor[3]
                        };
                        metallic = pbr.metallic_factor;
                        roughness = pbr.roughness_factor;

                        if (pbr.base_color_texture.texture)
                        {
                            albedoPath = ExtractGltfTexture(
                                pbr.base_color_texture.texture, path,
                                textureIndex++, settings);
                        }
                        if (sourceMaterial.normal_texture.texture)
                        {
                            normalPath = ExtractGltfTexture(
                                sourceMaterial.normal_texture.texture, path,
                                textureIndex++, settings);
                        }
                        if (pbr.metallic_roughness_texture.texture)
                        {
                            packedPath = ExtractGltfTexture(
                                pbr.metallic_roughness_texture.texture, path,
                                textureIndex++, settings);
                        }
                    }

                    ModelPart part;
                    part.Name = node.name ? node.name : "glTF Part";
                    part.Skinned = canSkin;
                    part.Mesh = Mesh::Create(vertices, indices);
                    part.SourceVertices = vertices;
                    part.SourceIndices = indices;
                    part.Material = MakeMaterial(albedo, metallic, roughness,
                        albedoPath, normalPath, packedPath,
                        settings.ChannelMode, settings, path);
                    part.Transform = canSkin
                        ? glm::scale(glm::mat4(1.0f), glm::vec3(settings.Scale))
                        : glm::scale(glm::mat4(1.0f), glm::vec3(settings.Scale)) * world;
                    outParts.push_back(std::move(part));
                }
            }

            BakeGltfAnimations(data, nodeToBone, outBones, outAnimations);
            WT_CORE_INFO("Imported glTF '{}': {} part(s), {} bone(s), {} clip(s)",
                path.filename().string(), outParts.size(), outBones.size(),
                outAnimations.size());

            cgltf_free(data);

            if (outParts.empty())
            {
                if (error)
                    *error = "No triangle primitives found in glTF.";
                return false;
            }
            return true;
        }

        bool LoadFbx(const std::filesystem::path& path,
            std::vector<ModelPart>& outParts,
            std::vector<ModelBone>& outBones,
            std::vector<ModelAnimation>& outAnimations,
            std::string* error,
            const ImportSettings& settings)
        {
            ufbx_load_opts options{};
            options.load_external_files = true;
            options.ignore_missing_external_files = true;
            options.generate_missing_normals = true;
            if (settings.ConvertToYUp)
            {
                options.target_axes = {
                    UFBX_COORDINATE_AXIS_POSITIVE_X,
                    UFBX_COORDINATE_AXIS_POSITIVE_Y,
                    UFBX_COORDINATE_AXIS_POSITIVE_Z
                };
            }
            options.target_unit_meters = 1.0f;

            ufbx_error loadError{};
            ufbx_scene* scene = ufbx_load_file(
                path.string().c_str(), &options, &loadError);
            if (!scene)
            {
                if (error)
                {
                    *error = loadError.description.data
                        ? std::string(loadError.description.data,
                            loadError.description.length)
                        : "ufbx_load_file failed.";
                }
                return false;
            }

            std::unordered_map<const ufbx_skin_cluster*, int> clusterBoneIndex;
            std::vector<FbxBoneRef> boneRefs;
            BuildFbxSkeleton(scene, outBones, clusterBoneIndex, boneRefs);

            uint32_t textureIndex = 0;
            for (size_t nodeIndex = 0; nodeIndex < scene->nodes.count; ++nodeIndex)
            {
                ufbx_node* node = scene->nodes.data[nodeIndex];
                if (!node || !node->mesh)
                    continue;

                ufbx_mesh* mesh = node->mesh;
                const glm::mat4 world = UfbxMatrixToGlm(node->geometry_to_world);

                ufbx_skin_deformer* skin = nullptr;
                std::vector<int> clusterToBone;
                if (mesh->skin_deformers.count > 0)
                {
                    skin = mesh->skin_deformers.data[0];
                    if (skin)
                    {
                        clusterToBone.resize(skin->clusters.count, -1);
                        for (size_t clusterIndex = 0;
                            clusterIndex < skin->clusters.count;
                            ++clusterIndex)
                        {
                            ufbx_skin_cluster* cluster =
                                skin->clusters.data[clusterIndex];
                            if (!cluster)
                                continue;
                            const auto it = clusterBoneIndex.find(cluster);
                            if (it != clusterBoneIndex.end())
                                clusterToBone[clusterIndex] = it->second;
                        }
                    }
                }

                for (size_t partIndex = 0;
                    partIndex < mesh->material_parts.count;
                    ++partIndex)
                {
                    ufbx_mesh_part* meshPart = &mesh->material_parts.data[partIndex];
                    if (meshPart->num_triangles == 0)
                        continue;

                    std::vector<MeshVertex> vertices;
                    std::vector<uint32_t> indices;
                    vertices.reserve(meshPart->num_triangles * 3);
                    indices.reserve(meshPart->num_triangles * 3);
                    size_t unweightedVertices = 0;

                    std::vector<uint32_t> triangleIndices(
                        mesh->max_face_triangles * 3);

                    for (size_t faceIndex = 0;
                        faceIndex < meshPart->num_faces;
                        ++faceIndex)
                    {
                        const ufbx_face face = mesh->faces.data[
                            meshPart->face_indices.data[faceIndex]];
                        const size_t triangleCount = ufbx_triangulate_face(
                            triangleIndices.data(), triangleIndices.size(),
                            mesh, face);

                        for (size_t i = 0; i < triangleCount * 3; ++i)
                        {
                            const uint32_t vertexIndex = triangleIndices[i];
                            const ufbx_vec3 position = ufbx_get_vertex_vec3(
                                &mesh->vertex_position, vertexIndex);
                            const ufbx_vec3 normal = mesh->vertex_normal.exists
                                ? ufbx_get_vertex_vec3(&mesh->vertex_normal, vertexIndex)
                                : ufbx_vec3{ 0.0f, 1.0f, 0.0f };
                            const ufbx_vec2 uv = mesh->vertex_uv.exists
                                ? ufbx_get_vertex_vec2(&mesh->vertex_uv, vertexIndex)
                                : ufbx_vec2{ 0.0f, 0.0f };

                            MeshVertex vertex;
                            vertex.Position = {
                                static_cast<float>(position.x),
                                static_cast<float>(position.y),
                                static_cast<float>(position.z)
                            };
                            vertex.Normal = {
                                static_cast<float>(normal.x),
                                static_cast<float>(normal.y),
                                static_cast<float>(normal.z)
                            };
                            vertex.TexCoord = {
                                static_cast<float>(uv.x),
                                static_cast<float>(uv.y)
                            };

                            // `ufbx_triangulate_face()` yields an index into
                            // mesh->vertex_indices, while skin weights are
                            // indexed by mesh vertex. Resolve the indirection
                            // before sampling the skin.
                            const uint32_t skinVertexIndex =
                                vertexIndex < mesh->vertex_indices.count
                                    ? mesh->vertex_indices.data[vertexIndex]
                                    : vertexIndex;
                            if (skin && skinVertexIndex < skin->vertices.count)
                            {
                                const ufbx_skin_vertex& skinVertex =
                                    skin->vertices.data[skinVertexIndex];
                                glm::vec4 weights(0.0f);
                                for (uint32_t influence = 0; influence < 4; ++influence)
                                {
                                    if (influence >= skinVertex.num_weights)
                                        break;
                                    const size_t weightIndex =
                                        skinVertex.weight_begin + influence;
                                    if (weightIndex >= skin->weights.count)
                                        break;
                                    const ufbx_skin_weight& weight =
                                        skin->weights.data[weightIndex];
                                    const int bone = weight.cluster_index < clusterToBone.size()
                                        ? clusterToBone[weight.cluster_index]
                                        : -1;
                                    vertex.BoneIndices[influence] = bone < 0 ? 0 : bone;
                                    vertex.BoneWeights[influence] =
                                        bone < 0 ? 0.0f : static_cast<float>(weight.weight);
                                    weights[influence] = vertex.BoneWeights[influence];
                                }

                                const float total = weights.x + weights.y
                                    + weights.z + weights.w;
                                if (total > 1e-6f)
                                {
                                    for (int influence = 0; influence < 4; ++influence)
                                        vertex.BoneWeights[influence] /= total;
                                }
                                else
                                {
                                    vertex.BoneIndices = { 0, 0, 0, 0 };
                                    vertex.BoneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };
                                    ++unweightedVertices;
                                }
                            }
                            else if (skin)
                            {
                                vertex.BoneIndices = { 0, 0, 0, 0 };
                                vertex.BoneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };
                                ++unweightedVertices;
                            }

                            indices.push_back(static_cast<uint32_t>(vertices.size()));
                            vertices.push_back(vertex);
                        }
                    }

                    GenerateNormals(vertices, indices);

                    glm::vec4 albedo(1.0f);
                    float metallic = 0.0f;
                    float roughness = 0.5f;
                    std::filesystem::path albedoPath;
                    std::filesystem::path normalPath;
                    std::filesystem::path packedPath;

                    if (meshPart->index < mesh->materials.count)
                    {
                        ufbx_material* material = mesh->materials.data[meshPart->index];
                        if (material)
                        {
                            const ufbx_material_pbr_maps& pbr = material->pbr;
                            if (pbr.base_color.has_value)
                            {
                                albedo = {
                                    static_cast<float>(pbr.base_color.value_vec4.x),
                                    static_cast<float>(pbr.base_color.value_vec4.y),
                                    static_cast<float>(pbr.base_color.value_vec4.z),
                                    static_cast<float>(pbr.base_color.value_vec4.w)
                                };
                            }
                            metallic = static_cast<float>(pbr.metalness.value_real);
                            roughness = static_cast<float>(pbr.roughness.value_real);

                            if (pbr.base_color.texture_enabled
                                && pbr.base_color.texture)
                            {
                                albedoPath = ExtractUfbxTexture(
                                    pbr.base_color.texture, path,
                                    textureIndex++, settings);
                            }
                            if (pbr.normal_map.texture_enabled
                                && pbr.normal_map.texture)
                            {
                                normalPath = ExtractUfbxTexture(
                                    pbr.normal_map.texture, path,
                                    textureIndex++, settings);
                            }
                            if (settings.ChannelMode != MaterialChannelMode::Separate
                                && pbr.metalness.texture_enabled
                                && pbr.roughness.texture_enabled
                                && pbr.metalness.texture
                                && pbr.metalness.texture == pbr.roughness.texture)
                            {
                                packedPath = ExtractUfbxTexture(
                                    pbr.roughness.texture, path,
                                    textureIndex++, settings);
                            }
                        }
                    }

                    ModelPart part;
                    part.Name = UfbxString(mesh->name);
                    if (part.Name.empty())
                        part.Name = UfbxString(node->name);
                    part.Skinned = skin != nullptr && skin->clusters.count > 0;
                    part.Mesh = Mesh::Create(vertices, indices);
                    part.SourceVertices = vertices;
                    part.SourceIndices = indices;
                    part.Material = MakeMaterial(albedo, metallic, roughness,
                        albedoPath, normalPath, packedPath,
                        settings.ChannelMode, settings, path);
                    part.Transform = part.Skinned
                        ? glm::scale(glm::mat4(1.0f), glm::vec3(settings.Scale))
                        : glm::scale(glm::mat4(1.0f), glm::vec3(settings.Scale)) * world;
                    if (unweightedVertices > 0)
                    {
                        WT_CORE_WARN("FBX part '{}': {} unweighted vertex/vertices "
                            "bound to the skin root", part.Name, unweightedVertices);
                    }
                    outParts.push_back(std::move(part));
                }
            }

            BakeFbxAnimations(scene, boneRefs, outBones, outAnimations);
            WT_CORE_INFO("Imported FBX '{}': {} part(s), {} bone(s), {} clip(s)",
                path.filename().string(), outParts.size(), outBones.size(),
                outAnimations.size());

            ufbx_free_scene(scene);

            if (outParts.empty())
            {
                if (error)
                    *error = "No triangle meshes found in FBX.";
                return false;
            }
            return true;
        }

        bool LoadObj(const std::filesystem::path& path,
            std::vector<ModelPart>& outParts,
            std::string* error,
            const ImportSettings& settings)
        {
            (void)settings;
            Ref<Mesh> mesh = Mesh::Create(path.generic_string());
            if (!mesh)
            {
                if (error)
                    *error = "OBJ load failed.";
                return false;
            }

            ModelPart part;
            part.Name = path.stem().string();
            part.Mesh = mesh;
            part.Material = Material::Create();
            part.Transform = glm::mat4(1.0f);
            outParts.push_back(std::move(part));
            return true;
        }

    } // namespace

        bool Load(const std::filesystem::path& path,
            std::vector<ModelPart>& outParts,
            std::string* error,
            const ImportSettings& settings,
            std::vector<ModelBone>* outBones,
            std::vector<ModelAnimation>* outAnimations)
        {
            outParts.clear();
            if (outBones)
                outBones->clear();
            if (outAnimations)
                outAnimations->clear();

            std::vector<ModelBone> bones;
            std::vector<ModelAnimation> animations;

            const std::string extension = ToLower(path.extension().string());
            bool result = false;
            if (extension == ".gltf" || extension == ".glb")
                result = LoadGltf(path, outParts, bones, animations, error, settings);
            else if (extension == ".fbx")
                result = LoadFbx(path, outParts, bones, animations, error, settings);
            else if (extension == ".obj")
                result = LoadObj(path, outParts, error, settings);
            else if (error)
                *error = "Unsupported model extension: " + extension;

            if (outBones)
                *outBones = std::move(bones);
            if (outAnimations)
                *outAnimations = std::move(animations);
            return result;
        }

    bool Cook(const std::filesystem::path& sourcePath,
        const std::filesystem::path& cookedPath,
        const ImportSettings& settings,
        std::string* error)
    {
        std::vector<ModelPart> parts;
        std::vector<ModelBone> bones;
        std::vector<ModelAnimation> animations;
        if (!Load(sourcePath, parts, error, settings, &bones, &animations))
            return false;

        for (const ModelPart& part : parts)
        {
            if (part.SourceVertices.empty() || part.SourceIndices.empty())
            {
                if (error)
                    *error = "Cook requires CPU mesh data; OBJ cooking is not supported yet.";
                return false;
            }
        }

        const std::filesystem::path parent = cookedPath.parent_path();
        if (!parent.empty())
        {
            std::error_code directoryError;
            std::filesystem::create_directories(parent, directoryError);
            if (directoryError)
            {
                if (error)
                    *error = "Failed to create cooked model directory.";
                return false;
            }
        }

        std::ofstream stream(cookedPath, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            if (error)
                *error = "Failed to open cooked model for writing.";
            return false;
        }

        stream.write(kCookedMagic, sizeof(kCookedMagic));
        WriteValue(stream, kCookedVersion);
        WriteValue(stream, static_cast<uint32_t>(parts.size()));

        for (const ModelPart& part : parts)
        {
            WriteString(stream, part.Name);
            stream.write(reinterpret_cast<const char*>(&part.Transform[0][0]),
                sizeof(float) * 16);
            const uint8_t skinned = part.Skinned ? 1 : 0;
            WriteValue(stream, skinned);

            WriteValue(stream, static_cast<uint32_t>(part.SourceVertices.size()));
            WriteValue(stream, static_cast<uint32_t>(part.SourceIndices.size()));
            stream.write(
                reinterpret_cast<const char*>(part.SourceVertices.data()),
                static_cast<std::streamsize>(
                    sizeof(MeshVertex) * part.SourceVertices.size()));
            stream.write(
                reinterpret_cast<const char*>(part.SourceIndices.data()),
                static_cast<std::streamsize>(
                    sizeof(uint32_t) * part.SourceIndices.size()));

            Material fallback;
            const Material& material = part.Material ? *part.Material : fallback;
            stream.write(reinterpret_cast<const char*>(&material.Albedo[0]),
                sizeof(float) * 4);
            WriteValue(stream, material.Metallic);
            WriteValue(stream, material.Roughness);
            const bool flipNormals = material.FlipNormals;
            WriteValue(stream, flipNormals);

            WriteString(stream, material.AlbedoMap ? material.AlbedoMap->GetPath() : "");
            WriteString(stream, material.NormalMap ? material.NormalMap->GetPath() : "");
            WriteString(stream, material.RoughnessMap ? material.RoughnessMap->GetPath() : "");
            WriteString(stream, material.MetallicMap ? material.MetallicMap->GetPath() : "");
        }

        WriteValue(stream, static_cast<uint32_t>(bones.size()));
        for (const ModelBone& bone : bones)
        {
            WriteString(stream, bone.Name);
            WriteValue(stream, bone.Parent);
            WriteValue(stream, bone.BindTranslation);
            WriteValue(stream, bone.BindRotation);
            WriteValue(stream, bone.BindScale);
            stream.write(reinterpret_cast<const char*>(&bone.InverseBind[0][0]),
                sizeof(float) * 16);
        }

        WriteValue(stream, static_cast<uint32_t>(animations.size()));
        for (const ModelAnimation& animation : animations)
        {
            WriteString(stream, animation.Name);
            WriteValue(stream, animation.Duration);
            WriteValue(stream, animation.FrameCount);
            WriteValue(stream, animation.FramesPerSecond);
            const uint8_t precomputed = animation.Precomputed ? 1 : 0;
            WriteValue(stream, precomputed);

            const size_t samples = static_cast<size_t>(animation.FrameCount) + 1;
            for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
            {
                if (animation.Precomputed)
                {
                    stream.write(reinterpret_cast<const char*>(
                        animation.BoneMatrices[boneIndex].data()),
                        static_cast<std::streamsize>(sizeof(glm::mat4) * samples));
                    continue;
                }

                const auto& translations = animation.Translations[boneIndex];
                const auto& rotations = animation.Rotations[boneIndex];
                const auto& scales = animation.Scales[boneIndex];
                stream.write(reinterpret_cast<const char*>(translations.data()),
                    static_cast<std::streamsize>(sizeof(glm::vec3) * samples));
                stream.write(reinterpret_cast<const char*>(rotations.data()),
                    static_cast<std::streamsize>(sizeof(glm::quat) * samples));
                stream.write(reinterpret_cast<const char*>(scales.data()),
                    static_cast<std::streamsize>(sizeof(glm::vec3) * samples));
            }
        }

        if (!stream.good())
        {
            if (error)
                *error = "Failed while writing cooked model.";
            return false;
        }

        return true;
    }

} // namespace Wheatear::ModelImporter
