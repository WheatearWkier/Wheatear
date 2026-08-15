#pragma once

#include "entt.hpp"

#include "Wheatear/Core/Core.h"
#include "Wheatear/Core/Timestep.h"
#include "Wheatear/Core/UUID.h"
#include "Wheatear/Renderer/EditorCamera.h"
#include "Wheatear/Systems/ISystem.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

class b2World;

namespace Wheatear {

    class Entity;

    enum class SceneExecutionMode : uint8_t
    {
        None = 0,
        Edit,
        Runtime
    };

    struct SavePolicy
    {
        bool CanSave = true;
        bool CanLoad = true;
        std::string SaveDirectory = "assets/saves";
        int AutoLoadSlot = 0;
    };

    class Scene
    {
    public:
        Scene();
        ~Scene();

        static Ref<Scene> Copy(Ref<Scene> other);

        Entity CreateEntity(const std::string& name = {});
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = {});

        void DestroyEntityImmediate(Entity entity);
        void DestroyEntity(Entity entity);

        Entity DuplicateEntity(Entity entity);
        Entity InstantiateFromPrefab(const std::filesystem::path& prefabPath,
            const glm::vec3& position = {});

        void OnRuntimeStart();
        void OnRuntimeStop();
        void OnUpdateRuntime(Timestep ts);
        void OnEditorStart();
        void OnEditorStop();
        void OnUpdateEditor(Timestep ts, EditorCamera& camera);
        void RenderWithSceneCamera(const Camera& camera,
            const glm::mat4& cameraTransform,
            bool includeUI = false);

        void OnViewportResize(uint32_t width, uint32_t height);
        void SetViewportOffset(float x, float y);

        uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }
        const glm::vec2& GetViewportOffset() const { return m_ViewportOffset; }
        SceneExecutionMode GetExecutionMode() const { return m_ExecutionMode; }
        const SavePolicy& GetSavePolicy() const { return m_SavePolicy; }
        SavePolicy& GetSavePolicy() { return m_SavePolicy; }
        void SetSavePolicy(const SavePolicy& policy) { m_SavePolicy = policy; }

        Entity GetPrimaryCameraEntity();
        Entity FindEntityByUUID(UUID uuid);
        Entity GetEntityByUUID(UUID uuid);
        Entity GetEntityByName(const std::string& name);
        void InvalidateEntityLookupCache();

        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        template<typename... Components>
        auto GetAllEntitiesWith() { return m_Registry.view<Components...>(); }

        template<typename T>
        T* GetSystem()
        {
            // Lazily rebuild the type-indexed cache after any system registration,
            // so per-frame GetSystem calls (Render/UI wiring, viewport setters)
            // avoid a linear dynamic_cast scan.
            if (m_SystemCacheDirty)
            {
                m_SystemCache.clear();
                for (const auto& system : m_Systems)
                {
                    ISystem* raw = system.get();
                    if (raw)
                        m_SystemCache[std::type_index(typeid(*raw))] = raw;
                }
                m_SystemCacheDirty = false;
            }

            const auto it = m_SystemCache.find(std::type_index(typeid(T)));
            return it == m_SystemCache.end() ? nullptr : static_cast<T*>(it->second);
        }

        void SetAnimationEditorPreviewActive(bool active);

    private:
        void ConfigureRuntimeSystems();
        void ConfigureEditorSystems();
        void StartSystems(SceneExecutionMode mode);
        void StopSystems();
        void RebuildEntityLookupCaches();

        template<typename T, typename... Args>
        T& RegisterSystem(Args&&... args)
        {
            auto system = CreateScope<T>(std::forward<Args>(args)...);
            T& systemReference = *system;
            m_Systems.push_back(std::move(system));
            m_SystemCacheDirty = true;
            return systemReference;
        }

        void FlushDestroyQueue();
        void FlushDestroyQueueEditor();

    private:
        entt::registry m_Registry;
        std::unordered_map<std::string, entt::entity> m_EntityNameCache;
        std::unordered_map<UUID, entt::entity> m_EntityUUIDCache;
        bool m_EntityLookupCacheDirty = true;

        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;
        glm::vec2 m_ViewportOffset = { 0.0f, 0.0f };
        SavePolicy m_SavePolicy;
        SceneExecutionMode m_ExecutionMode = SceneExecutionMode::None;

        std::vector<Scope<ISystem>> m_Systems;
        std::unordered_map<std::type_index, ISystem*> m_SystemCache;
        bool m_SystemCacheDirty = true;
        // Deferred-destroy batch: vector + sort/unique at flush gives deterministic
        // destroy order and avoids per-insert node allocations (an unordered_set
        // would also rehash if a handler destroys during flush iteration).
        std::vector<entt::entity> m_DestroyQueue;

        friend class Entity;
        friend class SceneSerializer;
        friend class SceneHierarchyPanel;
    };

} // namespace Wheatear
