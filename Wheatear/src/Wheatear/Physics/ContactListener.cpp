#include "wtpch.h"
#include "ContactListener.h"

#include <box2d/b2_fixture.h>
#include <box2d/b2_body.h>

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scripting/ScriptEngine.h"

namespace Wheatear {

    ContactListener::ContactListener(Scene* scene)
        : m_Scene(scene)
    {
    }

    Entity ContactListener::GetEntityFromFixture(b2Fixture* fixture)
    {
        if (!fixture)
            return {};

        b2Body* body = fixture->GetBody();
        if (!body)
            return {};

        uintptr_t data = body->GetUserData().pointer;
        if (data == 0)
            return {};

        UUID uuid = (UUID)data;
        return m_Scene->GetEntityByUUID(uuid);
    }

    void ContactListener::BeginContact(b2Contact* contact)
    {
        Entity entityA = GetEntityFromFixture(contact->GetFixtureA());
        Entity entityB = GetEntityFromFixture(contact->GetFixtureB());

        if (!entityA || !entityB)
        {
            WT_CORE_WARN("Collision but entity invalid!");
            return;
        }

        // 只负责通知脚本（Unity）
        ScriptEngine::OnCollisionBegin(entityA, entityB);
        ScriptEngine::OnCollisionBegin(entityB, entityA);
    }

    void ContactListener::EndContact(b2Contact* contact)
    {
        Entity entityA = GetEntityFromFixture(contact->GetFixtureA());
        Entity entityB = GetEntityFromFixture(contact->GetFixtureB());

        if (!entityA || !entityB)
            return;

        ScriptEngine::OnCollisionEnd(entityA, entityB);
        ScriptEngine::OnCollisionEnd(entityB, entityA);
    }

}
