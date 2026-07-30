#pragma once

#include "ISystem.h"

class b2World;

namespace Wheatear {

    class ContactListener;

    /// 
    class PhysicsSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnEntityCreated(Scene* scene, Entity& entity) override;
        void OnEntityDestroy(Scene* scene, Entity& entity) override;

        void InitEntityPhysics(Scene* scene, Entity entity);

        b2World* GetPhysicsWorld() const { return m_PhysicsWorld; }

    private:
        b2World* m_PhysicsWorld = nullptr;
        ContactListener* m_ContactListener = nullptr;
    };

} // namespace Wheatear
