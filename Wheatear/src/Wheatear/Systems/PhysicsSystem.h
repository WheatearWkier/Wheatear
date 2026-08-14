#pragma once

#include "ISystem.h"
#include "Wheatear/Physics/ContactListener.h"
#include <box2d/b2_world.h>

#include <memory>


namespace Wheatear {


    /// 
    class PhysicsSystem : public ISystem
    {
    public:
        ~PhysicsSystem() override;

        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnEntityCreated(Scene* scene, Entity& entity) override;
        void OnEntityDestroy(Scene* scene, Entity& entity) override;

        void InitEntityPhysics(Scene* scene, Entity entity);

        b2World* GetPhysicsWorld() const { return m_PhysicsWorld.get(); }

    private:
        void SyncAnimationDrivenColliders(Scene* scene);

        // RAII ownership: world and listener are created per play session and
        // released automatically on stop (b2World is non-copyable/non-movable).
        std::unique_ptr<b2World> m_PhysicsWorld;
        std::unique_ptr<ContactListener> m_ContactListener;
    };

} // namespace Wheatear
