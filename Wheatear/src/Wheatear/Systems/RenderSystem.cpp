#include "wtpch.h"
#include "RenderSystem.h"
#include "UISystem.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Renderer/Framebuffer.h"
#include "Wheatear/Renderer/Frustum.h"
#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Navigation/DebugDraw3D.h"
#include "Wheatear/Renderer/Renderer2D.h"
#include "Wheatear/Renderer/Renderer3D.h"
#include "Wheatear/UI/UIRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <bit>
#include <map>
#include <tuple>
#include <utility>

namespace Wheatear {

    namespace {

        static bool IsHiddenInEditor(Scene* scene, entt::entity entity, bool respectEditorVisibility)
        {
            return respectEditorVisibility &&
                scene &&
                scene->GetRegistry().all_of<EditorHiddenComponent>(entity);
        }

    } // namespace

    // =========================================================================
    // Helpers
    // =========================================================================

    glm::mat4 RenderSystem::ComputeLightSpaceMatrix(Scene* scene, bool respectEditorVisibility)
    {
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TransformComponent, DirectionalLightComponent>())
        {
            if (IsHiddenInEditor(scene, e, respectEditorVisibility))
                continue;

            auto& dl = registry.get<DirectionalLightComponent>(e);

            const glm::mat4 world = scene->GetWorldTransform(Entity(e, scene));
            glm::vec3 dir = glm::normalize(
                glm::vec3(world * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));

            // Orthographic projection that covers the scene; adjust extents as needed.
            const float size = 20.0f;
            glm::mat4 proj = glm::ortho(-size, size, -size, size, 0.1f, 100.0f);
            glm::vec3 lightPos = -dir * 30.0f;
            glm::mat4 view = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            return proj * view;
        }
        return glm::mat4(1.0f); // No directional light -- identity matrix
    }

    void RenderSystem::CollectLights(Scene* scene, bool respectEditorVisibility)
    {
        auto& registry = scene->GetRegistry();

        // Only the first directional light is used.
        bool hasDirLight = false;
        for (auto e : registry.view<TransformComponent, DirectionalLightComponent>())
        {
            if (IsHiddenInEditor(scene, e, respectEditorVisibility))
                continue;

            if (hasDirLight) break;
            auto& dl = registry.get<DirectionalLightComponent>(e);

            const glm::mat4 world = scene->GetWorldTransform(Entity(e, scene));
            glm::vec3 dir = glm::normalize(
                glm::vec3(world * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)));

            Renderer3D::SetDirectionalLight(dir, dl.Color, dl.Intensity);
            hasDirLight = true;
        }

        for (auto e : registry.view<TransformComponent, PointLightComponent>())
        {
            if (IsHiddenInEditor(scene, e, respectEditorVisibility))
                continue;

            auto& pl = registry.get<PointLightComponent>(e);
            Renderer3D::AddPointLight(
                scene->GetWorldTranslation(Entity(e, scene)),
                pl.Color, pl.Intensity,
                pl.Constant, pl.Linear, pl.Quadratic);
        }
    }

    void RenderSystem::RenderSceneShadow(Scene* scene, bool respectEditorVisibility)
    {
        const Frustum lightFrustum(Renderer3D::GetLightSpaceMatrix());
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<TransformComponent, MeshRendererComponent>())
        {
            if (IsHiddenInEditor(scene, e, respectEditorVisibility))
                continue;

            auto& mr = registry.get<MeshRendererComponent>(e);
            if (mr.Model)
            {
                const glm::mat4 world = scene->GetWorldTransform(Entity(e, scene));
                const auto* animator = registry.try_get<Animator3DComponent>(e);
                const std::vector<glm::mat4>* palette =
                    animator && !animator->BonePalette.empty()
                        ? &animator->BonePalette : nullptr;

                for (const ModelPart& part : mr.Model->GetParts())
                {
                    if (!part.Mesh)
                        continue;
                    // Skinned bounds change with the pose; keep them in the
                    // shadow pass instead of risking a pop.
                    if (!part.Skinned && !lightFrustum.IsTransformedAABBVisible(
                        part.Mesh->GetBoundsMin(), part.Mesh->GetBoundsMax(),
                        world * part.Transform))
                    {
                        continue;
                    }
                    Renderer3D::DrawMeshShadow(world * part.Transform, part.Mesh,
                        part.Skinned ? palette : nullptr);
                }
            }
            else if (mr.Mesh)
            {
                const glm::mat4 world = scene->GetWorldTransform(Entity(e, scene));
                if (!lightFrustum.IsTransformedAABBVisible(
                    mr.Mesh->GetBoundsMin(), mr.Mesh->GetBoundsMax(), world))
                {
                    continue;
                }
                Renderer3D::DrawMeshShadow(world, mr.Mesh);
            }
        }
    }

    void RenderSystem::RenderScene2D(Scene* scene, bool respectEditorVisibility)
    {
        RenderCommand::DisableDepthTest();
        auto& registry = scene->GetRegistry();

        // Sort sprites back-to-front by world Z before drawing. When two
        // sprites share a Z layer, draw higher world-Y first so side-view lane
        // characters overlap consistently, then fall back to entity id for a
        // stable order. World transforms keep parented decals/props correct.
        m_SpriteSortBuffer.clear();
        for (auto e : registry.view<TransformComponent, SpriteRendererComponent>())
        {
            if (IsHiddenInEditor(scene, e, respectEditorVisibility))
                continue;

            const auto& sprite = registry.get<SpriteRendererComponent>(e);
            m_SpriteSortBuffer.push_back({
                e,
                scene->GetWorldTransform(Entity(e, scene)),
                sprite.SortingLayer,
                sprite.OrderInLayer,
                false
            });
        }
        for (auto e : registry.view<TransformComponent, CircleRendererComponent>())
        {
            if (IsHiddenInEditor(scene, e, respectEditorVisibility))
                continue;

            const auto& circle = registry.get<CircleRendererComponent>(e);
            m_SpriteSortBuffer.push_back({
                e,
                scene->GetWorldTransform(Entity(e, scene)),
                circle.SortingLayer,
                circle.OrderInLayer,
                true
            });
        }

        std::stable_sort(m_SpriteSortBuffer.begin(), m_SpriteSortBuffer.end(),
            [](const SpriteSortItem& a, const SpriteSortItem& b)
            {
                if (a.SortingLayer != b.SortingLayer)
                    return a.SortingLayer < b.SortingLayer;
                if (a.OrderInLayer != b.OrderInLayer)
                    return a.OrderInLayer < b.OrderInLayer;

                const float az = a.World[3][2];
                const float bz = b.World[3][2];
                if (az != bz)
                    return az < bz;

                const float ay = a.World[3][1];
                const float by = b.World[3][1];
                if (ay != by)
                    return ay > by;

                return static_cast<uint32_t>(a.Entity) < static_cast<uint32_t>(b.Entity);
            });

        for (const SpriteSortItem& item : m_SpriteSortBuffer)
        {
            if (item.IsCircle)
            {
                const auto& circle = registry.get<CircleRendererComponent>(item.Entity);
                Renderer2D::DrawCircle(item.World, circle.Color,
                    circle.Thickness, circle.Fade,
                    static_cast<int>(item.Entity));
            }
            else
            {
                auto& sprite = registry.get<SpriteRendererComponent>(item.Entity);
                Renderer2D::DrawSprite(item.World, sprite,
                    static_cast<int>(item.Entity));
            }
        }

        RenderCommand::EnableDepthTest();
    }

    void RenderSystem::RenderScene3D(Scene* scene, bool respectEditorVisibility)
    {
        RenderCommand::EnableDepthTest();
        const Frustum cameraFrustum(Renderer3D::GetViewProjection());
        auto& registry = scene->GetRegistry();

        // Runtime meshes that share a Mesh and Material are batched into one
        // instanced draw. The editor keeps per-entity draws so picking IDs and
        // selection outlines continue to work.
        struct InstanceBatch
        {
            Ref<Mesh> Mesh;
            Ref<Material> Material;
            std::vector<glm::mat4> Transforms;
        };
        // Units often own separate Material instances with identical values;
        // key by mesh + material state so those still collapse into one draw.
        using InstanceKey = std::tuple<
            const void*,
            uint32_t, uint32_t, uint32_t, uint32_t,
            uint32_t, uint32_t,
            const void*, const void*, const void*, const void*,
            bool>;
        std::map<InstanceKey, InstanceBatch> batches;

        for (auto e : registry.view<TransformComponent, MeshRendererComponent>())
        {
            if (IsHiddenInEditor(scene, e, respectEditorVisibility))
                continue;

            auto& mr = registry.get<MeshRendererComponent>(e);
            if (mr.Model)
            {
                const glm::mat4 world = scene->GetWorldTransform(Entity(e, scene));
                const auto* animator = registry.try_get<Animator3DComponent>(e);
                const std::vector<glm::mat4>* palette =
                    animator && !animator->BonePalette.empty()
                        ? &animator->BonePalette : nullptr;

                for (const ModelPart& part : mr.Model->GetParts())
                {
                    if (!part.Mesh)
                        continue;
                    if (!part.Skinned && !cameraFrustum.IsTransformedAABBVisible(
                        part.Mesh->GetBoundsMin(), part.Mesh->GetBoundsMax(),
                        world * part.Transform))
                    {
                        continue;
                    }
                    Renderer3D::DrawMesh(world * part.Transform,
                        part.Mesh, part.Material, static_cast<int>(e),
                        part.Skinned ? palette : nullptr);
                }
            }
            else if (mr.Mesh)
            {
                const glm::mat4 world = scene->GetWorldTransform(Entity(e, scene));
                if (!cameraFrustum.IsTransformedAABBVisible(
                    mr.Mesh->GetBoundsMin(), mr.Mesh->GetBoundsMax(), world))
                {
                    continue;
                }

                if (respectEditorVisibility)
                {
                    Renderer3D::DrawMesh(world, mr.Mesh, mr.Material,
                        static_cast<int>(e));
                    continue;
                }

                const Material* material = mr.Material.get();
                static const Material s_DefaultMaterial;
                if (!material)
                    material = &s_DefaultMaterial;

                const InstanceKey key(
                    static_cast<const void*>(mr.Mesh.get()),
                    std::bit_cast<uint32_t>(material->Albedo.r),
                    std::bit_cast<uint32_t>(material->Albedo.g),
                    std::bit_cast<uint32_t>(material->Albedo.b),
                    std::bit_cast<uint32_t>(material->Albedo.a),
                    std::bit_cast<uint32_t>(material->Metallic),
                    std::bit_cast<uint32_t>(material->Roughness),
                    static_cast<const void*>(material->AlbedoMap.get()),
                    static_cast<const void*>(material->NormalMap.get()),
                    static_cast<const void*>(material->RoughnessMap.get()),
                    static_cast<const void*>(material->MetallicMap.get()),
                    material->FlipNormals);
                auto& batch = batches[key];
                if (!batch.Mesh)
                {
                    batch.Mesh = mr.Mesh;
                    batch.Material = mr.Material;
                }
                batch.Transforms.push_back(world);
            }
        }

        for (auto& [key, batch] : batches)
        {
            if (batch.Transforms.size() > 1)
            {
                Renderer3D::DrawMeshInstanced(batch.Transforms,
                    batch.Mesh, batch.Material, -1);
            }
            else if (!batch.Transforms.empty())
            {
                Renderer3D::DrawMesh(batch.Transforms.front(),
                    batch.Mesh, batch.Material, -1);
            }
        }

        DebugDraw3D::Flush();
    }

    // =========================================================================
    // Runtime update (game camera)
    // =========================================================================

    void RenderSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        // Find the primary camera.
        Camera* mainCamera = nullptr;
        glm::mat4 cameraTransform = glm::mat4(1.0f);
        for (auto e : scene->GetRegistry().view<TransformComponent, CameraComponent>())
        {
            auto& cc = scene->GetRegistry().get<CameraComponent>(e);
            if (cc.Primary)
            {
                mainCamera = &cc.Camera;
                cameraTransform = scene->GetWorldTransform(Entity(e, scene));
                break;
            }
        }
        if (!mainCamera) return;

        glm::mat4 gameVP = mainCamera->GetProjection() * glm::inverse(cameraTransform);

        uint32_t previousFBO = RenderCommand::GetBoundFramebuffer();
        // Shadow pass
        Renderer3D::GetShadowMapFB()->Bind();
        RenderCommand::Clear();
        Renderer3D::BeginShadowPass(ComputeLightSpaceMatrix(scene, false));
        RenderSceneShadow(scene, false);
        Renderer3D::EndShadowPass();
        RenderCommand::BindFramebuffer(previousFBO);
        RenderCommand::SetViewport(0, 0, scene->GetViewportWidth(), scene->GetViewportHeight());

        Renderer2D::BeginScene(*mainCamera, cameraTransform);
        RenderScene2D(scene, false);
        Renderer2D::EndScene();

        Renderer3D::BeginScene(*mainCamera, cameraTransform);
        CollectLights(scene, false);
        RenderScene3D(scene, false);
        Renderer3D::EndScene();

        UIRenderer::BeginUIPass(scene->GetViewportWidth(), scene->GetViewportHeight());
        if (auto* ui = scene->GetSystem<UISystem>())
            ui->RenderUI(scene);
        UIRenderer::EndUIPass(gameVP);
    }

    // =========================================================================
    // Editor update (editor camera)
    // =========================================================================

    void RenderSystem::RenderWithEditorCamera(Scene* scene,
        EditorCamera& camera,
        bool includeUI)
    {
        // Save the currently bound framebuffer so we can restore it after the
        // shadow pass (which binds its own FBO and changes the viewport).
        uint32_t previousFBO = RenderCommand::GetBoundFramebuffer();

        // Shadow pass
        Renderer3D::GetShadowMapFB()->Bind();
        RenderCommand::Clear();
        Renderer3D::BeginShadowPass(ComputeLightSpaceMatrix(scene, true));
        RenderSceneShadow(scene, true);
        Renderer3D::EndShadowPass();

        // Restore the editor's framebuffer and viewport.
        RenderCommand::BindFramebuffer(previousFBO);
        RenderCommand::SetViewport(0, 0, scene->GetViewportWidth(), scene->GetViewportHeight());

        Renderer2D::BeginScene(camera);
        RenderScene2D(scene, true);
        Renderer2D::EndScene();

        Renderer3D::BeginScene(camera);
        CollectLights(scene, true);
        RenderScene3D(scene, true);
        Renderer3D::EndScene();

        if (includeUI)
        {
            UIRenderer::BeginUIPass(scene->GetViewportWidth(), scene->GetViewportHeight());
            if (auto* ui = scene->GetSystem<UISystem>())
                ui->RenderUI(scene);
            UIRenderer::EndUIPass(camera.GetViewProjection());
        }
    }

    void RenderSystem::RenderWithSceneCamera(Scene* scene,
        const Camera& camera,
        const glm::mat4& cameraTransform,
        bool includeUI)
    {
        const uint32_t previousFBO = RenderCommand::GetBoundFramebuffer();
        const bool respectEditorVisibility = scene && scene->GetExecutionMode() == SceneExecutionMode::Edit;

        Renderer3D::GetShadowMapFB()->Bind();
        RenderCommand::Clear();
        Renderer3D::BeginShadowPass(ComputeLightSpaceMatrix(scene, respectEditorVisibility));
        RenderSceneShadow(scene, respectEditorVisibility);
        Renderer3D::EndShadowPass();

        RenderCommand::BindFramebuffer(previousFBO);
        RenderCommand::SetViewport(0, 0, scene->GetViewportWidth(), scene->GetViewportHeight());

        Renderer2D::BeginScene(camera, cameraTransform);
        RenderScene2D(scene, respectEditorVisibility);
        Renderer2D::EndScene();

        Renderer3D::BeginScene(camera, cameraTransform);
        CollectLights(scene, respectEditorVisibility);
        RenderScene3D(scene, respectEditorVisibility);
        Renderer3D::EndScene();

        if (includeUI)
        {
            const glm::mat4 viewProjection = camera.GetProjection() * glm::inverse(cameraTransform);
            UIRenderer::BeginUIPass(scene->GetViewportWidth(), scene->GetViewportHeight());
            if (auto* ui = scene->GetSystem<UISystem>())
                ui->RenderUI(scene);
            UIRenderer::EndUIPass(viewProjection);
        }
    }

} // namespace Wheatear
