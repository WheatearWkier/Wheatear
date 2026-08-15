#include "wtpch.h"
#include "PhysicsSystem.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Physics/ContactListener.h"

#include <box2d/b2_world.h>
#include <box2d/b2_body.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_circle_shape.h>

#include <cmath>

namespace Wheatear {


    namespace {

        b2BodyType RigidbodyTypeToBox2D(Rigidbody2DComponent::BodyType type)
        {
            switch (type)
            {
            case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
            case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
            case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
            }
            WT_CORE_ASSERT(false, "Unknown Rigidbody2D BodyType");
            return b2_staticBody;
        }

        static float PositiveHalfExtent(float value)
        {
            return std::max(std::abs(value), 0.0001f);
        }

    } // anonymous namespace

    PhysicsSystem::~PhysicsSystem() = default;

    void PhysicsSystem::OnRuntimeStart(Scene* scene)
    {
        m_PhysicsWorld = std::make_unique<b2World>(b2Vec2{ 0.0f, -9.8f });
        m_ContactListener = std::make_unique<ContactListener>(scene);
        m_PhysicsWorld->SetContactListener(m_ContactListener.get());

        for (auto e : scene->GetRegistry().view<Rigidbody2DComponent>())
            InitEntityPhysics(scene, { e, scene });
    }

    void PhysicsSystem::OnRuntimeStop(Scene* scene)
    {
        // unique_ptr members release the world and listener automatically.
        m_PhysicsWorld.reset();
        m_ContactListener.reset();
    }

    void PhysicsSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        constexpr int32_t velocityIterations = 6;
        constexpr int32_t positionIterations = 2;

        SyncAnimationDrivenColliders(scene);
        m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

        auto& registry = scene->GetRegistry();
        auto view = registry.view<TransformComponent, Rigidbody2DComponent>();
        for (auto e : view)
        {
            auto& tc = view.get<TransformComponent>(e);
            auto& rb2d = view.get<Rigidbody2DComponent>(e);

            b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
            if (!body) continue;

            const auto& pos = body->GetPosition();
            tc.Translation.x = pos.x;
            tc.Translation.y = pos.y;
            tc.Rotation.z = body->GetAngle();
        }
    }

    void PhysicsSystem::SyncAnimationDrivenColliders(Scene* scene)
    {
        // Box colliders flagged FollowAnimation are re-shaped every frame by
        // the sprite system / animation system. Box2D fixtures copy their
        // shape at creation time, so rebuild when the component data moved.
        auto& registry = scene->GetRegistry();
        auto view = registry.view<Rigidbody2DComponent, BoxCollider2DComponent, TransformComponent>();
        for (auto e : view)
        {
            auto& rb2d = view.get<Rigidbody2DComponent>(e);
            auto& bc = view.get<BoxCollider2DComponent>(e);
            if (!bc.FollowAnimation)
                continue;

            b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
            if (!body)
                continue;

            const auto& tc = view.get<TransformComponent>(e);
            const b2Vec2 desiredHalf(
                PositiveHalfExtent(bc.Size.x * tc.Scale.x),
                PositiveHalfExtent(bc.Size.y * tc.Scale.y));
            const b2Vec2 desiredCenter(bc.Offset.x, bc.Offset.y);

            b2Fixture* fixture = static_cast<b2Fixture*>(bc.RuntimeFixture);
            if (fixture && fixture->GetType() == b2Shape::e_polygon)
            {
                const b2PolygonShape* shape = static_cast<const b2PolygonShape*>(fixture->GetShape());
                const bool unchanged = shape->m_centroid == desiredCenter
                    && shape->m_vertices[0]
                        == b2Vec2(desiredCenter.x - desiredHalf.x, desiredCenter.y - desiredHalf.y);
                if (unchanged)
                    continue;
                body->DestroyFixture(fixture);
            }

            b2PolygonShape shape;
            shape.SetAsBox(desiredHalf.x, desiredHalf.y, desiredCenter, 0.0f);
            b2FixtureDef fd;
            fd.shape = &shape;
            fd.density = bc.Density;
            fd.friction = bc.Friction;
            fd.restitution = bc.Restitution;
            fd.restitutionThreshold = bc.RestitutionThreshold;
            bc.RuntimeFixture = body->CreateFixture(&fd);
        }
    }

    void PhysicsSystem::InitEntityPhysics(Scene* scene, Entity entity)
    {
        WT_CORE_ASSERT(m_PhysicsWorld, "PhysicsSystem: world not initialized");

        auto& tc = entity.GetComponent<TransformComponent>();
        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

        b2BodyDef bodyDef;
        bodyDef.type = RigidbodyTypeToBox2D(rb2d.Type);
        bodyDef.position = { tc.Translation.x, tc.Translation.y };
        bodyDef.angle = tc.Rotation.z;

        b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
        body->SetFixedRotation(rb2d.FixedRotation);
        body->SetGravityScale(rb2d.GravityScale);
        body->GetUserData().pointer =
            static_cast<uintptr_t>(static_cast<uint64_t>(entity.GetUUID()));
        rb2d.RuntimeBody = body;

        auto makeFixture = [&](b2Shape& shape, float density, float friction,
            float restitution, float restitutionThreshold)
            {
                b2FixtureDef fd;
                fd.shape = &shape;
                fd.density = density;
                fd.friction = friction;
                fd.restitution = restitution;
                fd.restitutionThreshold = restitutionThreshold;
                return body->CreateFixture(&fd);
            };

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            auto& bc = entity.GetComponent<BoxCollider2DComponent>();
            b2PolygonShape shape;
            shape.SetAsBox(PositiveHalfExtent(bc.Size.x * tc.Scale.x),
                PositiveHalfExtent(bc.Size.y * tc.Scale.y),
                { bc.Offset.x, bc.Offset.y }, 0.0f);
            bc.RuntimeFixture = makeFixture(shape, bc.Density, bc.Friction,
                bc.Restitution, bc.RestitutionThreshold);
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            auto& cc = entity.GetComponent<CircleCollider2DComponent>();
            b2CircleShape shape;
            shape.m_p = { cc.Offset.x, cc.Offset.y };
            shape.m_radius = PositiveHalfExtent(cc.Radius * tc.Scale.x);
            cc.RuntimeFixture = makeFixture(shape, cc.Density, cc.Friction,
                cc.Restitution, cc.RestitutionThreshold);
        }
    }

    void PhysicsSystem::OnEntityCreated(Scene* scene, Entity& entity)
    {
        if (!m_PhysicsWorld) return;
        if (!entity.HasComponent<Rigidbody2DComponent>()) return;
        InitEntityPhysics(scene, entity);
    }

    void PhysicsSystem::OnEntityDestroy(Scene* scene, Entity& entity)
    {
        if (!entity.HasComponent<Rigidbody2DComponent>()) return;
        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        if (rb2d.RuntimeBody && m_PhysicsWorld)
        {
            m_PhysicsWorld->DestroyBody(static_cast<b2Body*>(rb2d.RuntimeBody));
            rb2d.RuntimeBody = nullptr;
        }
    }

} // namespace Wheatear
