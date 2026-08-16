#include "wtpch.h"
#include "Material.h"
#include "Wheatear/Assets/AssetPath.h"
#include "Wheatear/Core/Log.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>

namespace Wheatear {

    Ref<Material> Material::Create()
    {
        return CreateRef<Material>();
    }

    Ref<Material> Material::Load(const std::string& path)
    {
        const std::filesystem::path resolvedPath = AssetPath::Resolve(path);
        std::ifstream file(resolvedPath);
        if (!file.is_open())
        {
            WT_CORE_ERROR("Material::Load: 无法打开文件 {0}", resolvedPath.string());
            return CreateRef<Material>();
        }

        try
        {
            YAML::Node root = YAML::Load(file);
            YAML::Node node = root["Material"];
            if (!node)
            {
                WT_CORE_ERROR("Material::Load: 文件格式错误 {0}", resolvedPath.string());
                return CreateRef<Material>();
            }

            auto mat = CreateRef<Material>();
            mat->m_Path = AssetPath::ToProjectRelative(resolvedPath).generic_string();

            if (node["Albedo"])
            {
                auto a = node["Albedo"];
                mat->Albedo = { a[0].as<float>(1.0f), a[1].as<float>(1.0f),
                                a[2].as<float>(1.0f), a[3].as<float>(1.0f) };
            }
            mat->Metallic = node["Metallic"].as<float>(0.0f);
            mat->Roughness = node["Roughness"].as<float>(0.5f);
            mat->FlipNormals = node["FlipNormals"].as<bool>(false);

            auto loadTex = [](const YAML::Node& n, const char* key) -> Ref<Texture2D>
                {
                    if (!n[key]) return nullptr;
                    std::string p = n[key].as<std::string>("");
                    if (p.empty()) return nullptr;
                    return Texture2D::Create(p);
                };

            mat->AlbedoMap = loadTex(node, "AlbedoMap");
            mat->NormalMap = loadTex(node, "NormalMap");
            mat->RoughnessMap = loadTex(node, "RoughnessMap");
            mat->MetallicMap = loadTex(node, "MetallicMap");

            return mat;
        }
        catch (const YAML::Exception& e)
        {
            WT_CORE_ERROR("Material::Load: 解析失败 {0}: {1}", resolvedPath.string(), e.what());
            return CreateRef<Material>();
        }
    }

    void Material::Save(const std::string& path)
    {
        m_Path = AssetPath::ToProjectRelative(path).generic_string();
        Save();
    }

    void Material::Save()
    {
        if (m_Path.empty())
        {
            WT_CORE_ERROR("Material::Save: 路径为空 ");
            return;
        }

        const std::filesystem::path resolvedPath = AssetPath::Resolve(m_Path);
        std::filesystem::create_directories(resolvedPath.parent_path());

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Material" << YAML::BeginMap;

        out << YAML::Key << "Albedo" << YAML::Value
            << YAML::Flow << YAML::BeginSeq
            << Albedo.r << Albedo.g << Albedo.b << Albedo.a
            << YAML::EndSeq;
        out << YAML::Key << "Metallic" << YAML::Value << Metallic;
        out << YAML::Key << "Roughness" << YAML::Value << Roughness;
        out << YAML::Key << "FlipNormals" << YAML::Value << FlipNormals;

        auto texPath = [](const Ref<Texture2D>& t) -> std::string {
            return t ? t->GetPath() : "";
            };
        out << YAML::Key << "AlbedoMap" << YAML::Value << texPath(AlbedoMap);
        out << YAML::Key << "NormalMap" << YAML::Value << texPath(NormalMap);
        out << YAML::Key << "RoughnessMap" << YAML::Value << texPath(RoughnessMap);
        out << YAML::Key << "MetallicMap" << YAML::Value << texPath(MetallicMap);

        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream file(resolvedPath);
        file << out.c_str();
        m_Dirty = false;
    }

} // namespace Wheatear
