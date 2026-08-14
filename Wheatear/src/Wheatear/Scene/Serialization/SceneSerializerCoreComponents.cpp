#include "wtpch.h"
#include "SceneSerializerComponentSupport.h"

namespace Wheatear {

    template<> struct ComponentSerializer<TagComponent> {
        static constexpr const char* Key = "TagComponent";
        static void Serialize(YAML::Emitter& o, const TagComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Tag" << YAML::Value << c.Tag;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TagComponent& c) {
            c.Tag = n["Tag"].as<std::string>();
        }
    };

    template<> struct ComponentSerializer<TransformComponent> {
        static constexpr const char* Key = "TransformComponent";
        static void Serialize(YAML::Emitter& o, const TransformComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Translation" << YAML::Value << c.Translation;
            o << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
            o << YAML::Key << "Scale" << YAML::Value << c.Scale;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, TransformComponent& c) {
            c.Translation = n["Translation"].as<glm::vec3>();
            c.Rotation = n["Rotation"].as<glm::vec3>();
            c.Scale = n["Scale"].as<glm::vec3>();
        }
    };

    template<> struct ComponentSerializer<CameraComponent> {
        static constexpr const char* Key = "CameraComponent";
        static void Serialize(YAML::Emitter& o, const CameraComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Camera" << YAML::BeginMap;
            o << YAML::Key << "ProjectionType" << YAML::Value << (int)c.Camera.GetProjectionType();
            o << YAML::Key << "PerspectiveFOV" << YAML::Value << c.Camera.GetPerspectiveVerticalFOV();
            o << YAML::Key << "PerspectiveNear" << YAML::Value << c.Camera.GetPerspectiveNearClip();
            o << YAML::Key << "PerspectiveFar" << YAML::Value << c.Camera.GetPerspectiveFarClip();
            o << YAML::Key << "OrthographicSize" << YAML::Value << c.Camera.GetOrthographicSize();
            o << YAML::Key << "OrthographicNear" << YAML::Value << c.Camera.GetOrthographicNearClip();
            o << YAML::Key << "OrthographicFar" << YAML::Value << c.Camera.GetOrthographicFarClip();
            o << YAML::EndMap;
            o << YAML::Key << "Primary" << YAML::Value << c.Primary;
            o << YAML::Key << "FixedAspectRatio" << YAML::Value << c.FixedAspectRatio;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CameraComponent& c) {
            auto cam = n["Camera"];
            c.Camera.SetProjectionType(
                (SceneCamera::ProjectionType)cam["ProjectionType"].as<int>());
            c.Camera.SetPerspectiveVerticalFOV(cam["PerspectiveFOV"].as<float>());
            c.Camera.SetPerspectiveNearClip(cam["PerspectiveNear"].as<float>());
            c.Camera.SetPerspectiveFarClip(cam["PerspectiveFar"].as<float>());
            c.Camera.SetOrthographicSize(cam["OrthographicSize"].as<float>());
            c.Camera.SetOrthographicNearClip(cam["OrthographicNear"].as<float>());
            c.Camera.SetOrthographicFarClip(cam["OrthographicFar"].as<float>());
            c.Primary = n["Primary"].as<bool>();
            c.FixedAspectRatio = n["FixedAspectRatio"].as<bool>();
        }
    };

    template<> struct ComponentSerializer<SpriteRendererComponent> {
        static constexpr const char* Key = "SpriteRendererComponent";
        static void Serialize(YAML::Emitter& o, const SpriteRendererComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Texture" << YAML::Value << (c.Texture ? c.Texture->GetPath() : "");
            o << YAML::Key << "TilingFactor" << YAML::Value << c.TilingFactor;
            o << YAML::Key << "UVMin" << YAML::Value << c.UVMin;
            o << YAML::Key << "UVMax" << YAML::Value << c.UVMax;
            o << YAML::Key << "FlipX" << YAML::Value << c.FlipX;
            o << YAML::Key << "DrawOffset" << YAML::Value << c.DrawOffset;
            o << YAML::Key << "DrawScale" << YAML::Value << c.DrawScale;
            o << YAML::Key << "SpriteSheet" << YAML::Value << c.SpriteSheet;
            o << YAML::Key << "CellIndex" << YAML::Value << c.CellIndex;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, SpriteRendererComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            c.TilingFactor = n["TilingFactor"].as<float>(1.0f);
            c.UVMin = n["UVMin"].as<glm::vec2>(c.UVMin);
            c.UVMax = n["UVMax"].as<glm::vec2>(c.UVMax);
            c.FlipX = n["FlipX"].as<bool>(c.FlipX);
            c.DrawOffset = n["DrawOffset"].as<glm::vec2>(c.DrawOffset);
            c.DrawScale = n["DrawScale"].as<glm::vec2>(c.DrawScale);
            c.SpriteSheet = n["SpriteSheet"].as<std::string>("");
            c.CellIndex = n["CellIndex"].as<int>(-1);
            if (auto p = n["Texture"].as<std::string>(""); !p.empty())
                c.Texture = Texture2D::Create(p);
        }
    };

    template<> struct ComponentSerializer<CircleRendererComponent> {
        static constexpr const char* Key = "CircleRendererComponent";
        static void Serialize(YAML::Emitter& o, const CircleRendererComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Thickness" << YAML::Value << c.Thickness;
            o << YAML::Key << "Fade" << YAML::Value << c.Fade;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CircleRendererComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            c.Thickness = n["Thickness"].as<float>();
            c.Fade = n["Fade"].as<float>();
        }
    };

    template<> struct ComponentSerializer<Rigidbody2DComponent> {
        static constexpr const char* Key = "Rigidbody2DComponent";
        static void Serialize(YAML::Emitter& o, const Rigidbody2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "BodyType" << YAML::Value << BodyTypeToString(c.Type);
            o << YAML::Key << "FixedRotation" << YAML::Value << c.FixedRotation;
            o << YAML::Key << "GravityScale" << YAML::Value << c.GravityScale;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, Rigidbody2DComponent& c) {
            c.Type = BodyTypeFromString(n["BodyType"].as<std::string>());
            c.FixedRotation = n["FixedRotation"].as<bool>();
            c.GravityScale = n["GravityScale"].as<float>(1.0f);
        }
    };

    template<> struct ComponentSerializer<BoxCollider2DComponent> {
        static constexpr const char* Key = "BoxCollider2DComponent";
        static void Serialize(YAML::Emitter& o, const BoxCollider2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Offset" << YAML::Value << c.Offset;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Density" << YAML::Value << c.Density;
            o << YAML::Key << "Friction" << YAML::Value << c.Friction;
            o << YAML::Key << "Restitution" << YAML::Value << c.Restitution;
            o << YAML::Key << "RestitutionThreshold" << YAML::Value << c.RestitutionThreshold;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, BoxCollider2DComponent& c) {
            c.Offset = n["Offset"].as<glm::vec2>();
            c.Size = n["Size"].as<glm::vec2>();
            c.Density = n["Density"].as<float>();
            c.Friction = n["Friction"].as<float>();
            c.Restitution = n["Restitution"].as<float>();
            c.RestitutionThreshold = n["RestitutionThreshold"].as<float>();
        }
    };

    template<> struct ComponentSerializer<CircleCollider2DComponent> {
        static constexpr const char* Key = "CircleCollider2DComponent";
        static void Serialize(YAML::Emitter& o, const CircleCollider2DComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Offset" << YAML::Value << c.Offset;
            o << YAML::Key << "Radius" << YAML::Value << c.Radius;
            o << YAML::Key << "Density" << YAML::Value << c.Density;
            o << YAML::Key << "Friction" << YAML::Value << c.Friction;
            o << YAML::Key << "Restitution" << YAML::Value << c.Restitution;
            o << YAML::Key << "RestitutionThreshold" << YAML::Value << c.RestitutionThreshold;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, CircleCollider2DComponent& c) {
            c.Offset = n["Offset"].as<glm::vec2>();
            c.Radius = n["Radius"].as<float>();
            c.Density = n["Density"].as<float>();
            c.Friction = n["Friction"].as<float>();
            c.Restitution = n["Restitution"].as<float>();
            c.RestitutionThreshold = n["RestitutionThreshold"].as<float>();
        }
    };

    template<> struct ComponentSerializer<MeshRendererComponent>
    {
        static constexpr const char* Key = "MeshRendererComponent";
        static void Serialize(YAML::Emitter& o, const MeshRendererComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "MeshPath" << YAML::Value
                << (c.Mesh ? c.Mesh->GetFilepath() : "");
            o << YAML::Key << "MaterialPath" << YAML::Value
                << (c.Material ? c.Material->GetPath() : "");
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, MeshRendererComponent& c)
        {
            auto meshPath = n["MeshPath"].as<std::string>("");
            if (!meshPath.empty()) c.Mesh = Mesh::Create(meshPath);
            else                   c.Mesh = Mesh::CreateCube();

            auto matPath = n["MaterialPath"].as<std::string>("");
            if (!matPath.empty()) c.Material = Material::Load(matPath);
            else                  c.Material = Material::Create();
        }
    };

    template<> struct ComponentSerializer<DirectionalLightComponent>
    {
        static constexpr const char* Key = "DirectionalLightComponent";
        static void Serialize(YAML::Emitter& o, const DirectionalLightComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Intensity" << YAML::Value << c.Intensity;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, DirectionalLightComponent& c)
        {
            c.Color = n["Color"].as<glm::vec3>();
            c.Intensity = n["Intensity"].as<float>(1.0f);
        }
    };

    template<> struct ComponentSerializer<PointLightComponent>
    {
        static constexpr const char* Key = "PointLightComponent";
        static void Serialize(YAML::Emitter& o, const PointLightComponent& c)
        {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "Intensity" << YAML::Value << c.Intensity;
            o << YAML::Key << "Constant" << YAML::Value << c.Constant;
            o << YAML::Key << "Linear" << YAML::Value << c.Linear;
            o << YAML::Key << "Quadratic" << YAML::Value << c.Quadratic;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, PointLightComponent& c)
        {
            c.Color = n["Color"].as<glm::vec3>();
            c.Intensity = n["Intensity"].as<float>(1.0f);
            c.Constant = n["Constant"].as<float>(1.0f);
            c.Linear = n["Linear"].as<float>(0.09f);
            c.Quadratic = n["Quadratic"].as<float>(0.032f);
        }
    };

    using CoreSceneComponents = ComponentGroup
    <
        TagComponent,
        TransformComponent,
        CameraComponent,
        SpriteRendererComponent,
        CircleRendererComponent,
        Rigidbody2DComponent,
        BoxCollider2DComponent,
        CircleCollider2DComponent,
        MeshRendererComponent,
        DirectionalLightComponent,
        PointLightComponent
    >;

    void SerializeCoreSceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(CoreSceneComponents{}, out, entity);
    }

    void DeserializeCoreSceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(CoreSceneComponents{}, node, entity);
    }

} // namespace Wheatear
