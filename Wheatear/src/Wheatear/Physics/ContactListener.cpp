#include "wtpch.h"
#include "ContactListener.h"

#include <box2d/b2_fixture.h>
#include <box2d/b2_body.h>

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Entity.h"

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

    // Box2D reports raw contact pairs here. The former C#/Mono collision
    // callbacks were removed with the scripting runtime; gameplay rules
    // (e.g. SideCombat hitboxes) do their own overlap detection, so these
    // hooks are intentionally empty until a native consumer is wired up.
    void ContactListener::BeginContact(b2Contact* contact)
    {
        (void)contact;
    }

    void ContactListener::EndContact(b2Contact* contact)
    {
        (void)contact;
    }

}
