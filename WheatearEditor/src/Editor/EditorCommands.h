#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <tuple>
#include <type_traits>
#include <vector>
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    struct ICommand
    {
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;

        virtual bool TryMerge(ICommand*) { return false; }
    };

    class CompositeCommand : public ICommand
    {
    public:
        void Add(std::unique_ptr<ICommand> command)
        {
            if (command)
                m_Commands.push_back(std::move(command));
        }

        bool Empty() const { return m_Commands.empty(); }

        void Execute() override
        {
            for (auto& command : m_Commands)
                command->Execute();
        }

        void Undo() override
        {
            for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it)
                (*it)->Undo();
        }

    private:
        std::vector<std::unique_ptr<ICommand>> m_Commands;
    };

    class CommandHistory
    {
    public:
        static CommandHistory& Get()
        {
            static CommandHistory s_Instance;
            return s_Instance;
        }

        void Push(std::unique_ptr<ICommand> cmd, bool tryMerge = false)
        {
            if (tryMerge && !m_UndoStack.empty())
            {
                if (m_UndoStack.top()->TryMerge(cmd.get()))
                    return;
            }
            m_UndoStack.push(std::move(cmd));
            while (!m_RedoStack.empty())
                m_RedoStack.pop();
        }

        void Undo()
        {
            if (m_UndoStack.empty()) return;
            auto cmd = std::move(m_UndoStack.top());
            m_UndoStack.pop();
            cmd->Undo();
            m_RedoStack.push(std::move(cmd));
        }

        void Redo()
        {
            if (m_RedoStack.empty()) return;
            auto cmd = std::move(m_RedoStack.top());
            m_RedoStack.pop();
            cmd->Execute();
            m_UndoStack.push(std::move(cmd));
        }

        void Clear()
        {
            while (!m_RedoStack.empty()) m_RedoStack.pop();
            while (!m_UndoStack.empty()) m_UndoStack.pop();
        }

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }

    private:
        CommandHistory() = default;
        std::stack<std::unique_ptr<ICommand>> m_UndoStack;
        std::stack<std::unique_ptr<ICommand>> m_RedoStack;
    };

    namespace EditorCommandDetail {

        template<typename T>
        struct ComponentSnapshotSlot
        {
            std::optional<T> Value;
        };

        template<typename... Ts>
        class EntityComponentSnapshot
        {
        public:
            void Capture(Entity entity)
            {
                (CaptureOne<Ts>(entity), ...);
            }

            void Restore(Entity entity, bool restoreIdentity = true) const
            {
                (RestoreOne<Ts>(entity, restoreIdentity), ...);
            }

        private:
            template<typename T>
            void CaptureOne(Entity entity)
            {
                auto& slot = std::get<ComponentSnapshotSlot<T>>(m_Slots);
                if (entity.HasComponent<T>())
                    slot.Value = entity.GetComponent<T>();
                else
                    slot.Value.reset();
            }

            template<typename T>
            void RestoreOne(Entity entity, bool restoreIdentity) const
            {
                if constexpr (std::is_same_v<T, IDComponent>)
                {
                    if (!restoreIdentity)
                        return;
                }

                const auto& slot = std::get<ComponentSnapshotSlot<T>>(m_Slots);
                if (!slot.Value)
                    return;

                entity.AddOrReplaceComponent<T>(*slot.Value);
            }

        private:
            std::tuple<ComponentSnapshotSlot<Ts>...> m_Slots;
        };

    } // namespace EditorCommandDetail

    using EditorEntitySnapshot = EditorCommandDetail::EntityComponentSnapshot<
        IDComponent,
        TagComponent,
        TransformComponent,
        SpriteRendererComponent,
        SpriteAnimatorComponent,
        CircleRendererComponent,
        MeshRendererComponent,
        DirectionalLightComponent,
        PointLightComponent,
        CameraComponent,
        NativeScriptComponent,
        Rigidbody2DComponent,
        BoxCollider2DComponent,
        CircleCollider2DComponent,
        ScriptComponent,
        EventScriptComponent,
        UICanvasComponent,
        UIWidgetComponent,
        UIAnimatorComponent,
        UIImageComponent,
        UIPanelComponent,
        UITextComponent,
        UIButtonComponent,
        UIProgressBarComponent,
        UISliderComponent,
        UIPagerComponent,
        UIScrollViewComponent,
        UIPathComponent,
        UISkillTreeViewComponent,
        UIPageItemComponent,
        UICheckboxComponent,
        AudioSourceComponent,
        VisualNovelComponent,
        ArcadeCombatLevelComponent,
        ArcadeCombatantComponent,
        ArcadePlayerControllerComponent,
        ArcadeBossComponent,
        ArcadeProjectileComponent,
        ArcadeCoverComponent,
        ArcadeTriggerComponent,
        SideCombatLevelComponent,
        SideCombatantComponent,
        SidePlayerControllerComponent,
        SideEnemyAIComponent,
        SideHitboxComponent,
        SidePickupComponent,
        TacticalCombatLevelComponent,
        TacticalUnitComponent
    >;

    class EntityCreateCommand : public ICommand
    {
    public:
        EntityCreateCommand(Scene* scene, const std::string& name)
            : m_Scene(scene), m_Name(name), m_IsCreate(true)
        {
        }

        EntityCreateCommand(Scene* scene, Entity existingEntity, bool /*isCreate_false*/)
            : m_Scene(scene)
            , m_Name(existingEntity.GetName())
            , m_IsCreate(false)
            , m_CreatedEntity(existingEntity)
        {
            m_Snapshot.Capture(existingEntity);
        }

        void Execute() override
        {
            if (m_IsCreate)
            {
                m_CreatedEntity = m_Scene->CreateEntity(m_Name);
                if (m_OnCreate)
                    m_OnCreate(m_CreatedEntity);
            }
            else
            {
                if (IsEntityValid())
                    m_Scene->DestroyEntityImmediate(m_CreatedEntity);
                m_CreatedEntity = {};
            }
        }

        void Undo() override
        {
            if (m_IsCreate)
            {
                if (IsEntityValid())
                    m_Scene->DestroyEntityImmediate(m_CreatedEntity);
                m_CreatedEntity = {};
            }
            else
            {
                m_CreatedEntity = m_Scene->CreateEntity(m_Name);
                m_Snapshot.Restore(m_CreatedEntity);
            }
        }

        Entity GetEntity() const { return m_CreatedEntity; }

        void SetOnCreate(std::function<void(Entity)> fn) { m_OnCreate = std::move(fn); }

    private:
        bool IsEntityValid() const
        {
            return m_CreatedEntity
                && m_Scene->GetRegistry().valid(
                    static_cast<entt::entity>(static_cast<uint32_t>(m_CreatedEntity)));
        }

        Scene* m_Scene = nullptr;
        std::string m_Name;
        bool        m_IsCreate = true;
        Entity      m_CreatedEntity{};
        std::function<void(Entity)> m_OnCreate;
        EditorEntitySnapshot m_Snapshot;
    };

    class EntityDuplicateCommand : public ICommand
    {
    public:
        EntityDuplicateCommand(Scene* scene, Entity source)
            : m_Scene(scene), m_Source(source)
        {
        }

        void Execute() override
        {
            if (!IsEntityValid(m_Source))
                return;

            m_Duplicate = m_Scene->DuplicateEntity(m_Source);
        }

        void Undo() override
        {
            if (IsEntityValid(m_Duplicate))
                m_Scene->DestroyEntityImmediate(m_Duplicate);
            m_Duplicate = {};
        }

        Entity GetEntity() const { return m_Duplicate; }

    private:
        bool IsEntityValid(Entity entity) const
        {
            return entity
                && m_Scene
                && m_Scene->GetRegistry().valid(
                    static_cast<entt::entity>(static_cast<uint32_t>(entity)));
        }

    private:
        Scene* m_Scene = nullptr;
        Entity m_Source;
        Entity m_Duplicate;
    };

    template<typename T>
    class ComponentValueCommand : public ICommand
    {
    public:
        ComponentValueCommand(Entity entity, const T& before, const T& after)
            : m_Entity(entity), m_Before(before), m_After(after)
        {
        }

        void Execute() override
        {
            if (m_Entity.HasComponent<T>())
                m_Entity.GetComponent<T>() = m_After;
        }

        void Undo() override
        {
            if (m_Entity.HasComponent<T>())
                m_Entity.GetComponent<T>() = m_Before;
        }

        bool TryMerge(ICommand* other) override
        {
            auto* o = dynamic_cast<ComponentValueCommand<T>*>(other);
            if (!o || o->m_Entity != m_Entity) return false;
            m_After = o->m_After;
            return true;
        }

    private:
        Entity m_Entity;
        T      m_Before;
        T      m_After;
    };

    template<typename T>
    std::unique_ptr<ICommand> MakeComponentValueCommand(Entity e, const T& before, const T& after)
    {
        return std::make_unique<ComponentValueCommand<T>>(e, before, after);
    }

    // Deep snapshot of a SpriteAnimatorComponent for undo/redo of structural
    // animation edits (add/delete/rename clip, add/delete frame/event/track,
    // generate frames, set default). Unlike ComponentValueCommand this deep-copies
    // the Clips map so undo restores actual clip contents, not just shared Refs.
    class AnimatorStateCommand : public ICommand
    {
    public:
        AnimatorStateCommand(Entity entity,
            const SpriteAnimatorComponent& before,
            const SpriteAnimatorComponent& after)
            : m_Entity(entity), m_Before(DeepCopy(before)), m_After(DeepCopy(after))
        {
        }

        void Execute() override
        {
            if (m_Entity.HasComponent<SpriteAnimatorComponent>())
                m_Entity.GetComponent<SpriteAnimatorComponent>() = DeepCopy(m_After);
        }

        void Undo() override
        {
            if (m_Entity.HasComponent<SpriteAnimatorComponent>())
                m_Entity.GetComponent<SpriteAnimatorComponent>() = DeepCopy(m_Before);
        }

        bool TryMerge(ICommand* other) override { return false; }

    private:
        static SpriteAnimatorComponent DeepCopy(const SpriteAnimatorComponent& source)
        {
            SpriteAnimatorComponent copy;
            copy.DefaultClipName = source.DefaultClipName;
            copy.PlayOnStart = source.PlayOnStart;
            copy.FireEvents = source.FireEvents;
            copy.PlaybackSpeed = source.PlaybackSpeed;
            copy.CurrentClipName = source.CurrentClipName;
            copy.CurrentFrameIndex = source.CurrentFrameIndex;
            copy.ElapsedTime = source.ElapsedTime;
            copy.IsPlaying = source.IsPlaying;
            copy.IsFinished = source.IsFinished;
            copy.ExternalClipAssets = source.ExternalClipAssets;
            for (const auto& [name, clip] : source.Clips)
                copy.Clips[name] = clip ? clip->Clone() : nullptr;
            return copy;
        }

        Entity m_Entity;
        SpriteAnimatorComponent m_Before;
        SpriteAnimatorComponent m_After;
    };

    // Convenience: captures before, applies the mutation lambda, captures after,
    // pushes an AnimatorStateCommand. The mutation must run against the same
    // entity's SpriteAnimatorComponent.
    inline void ApplyAnimatorEdit(Entity entity,
        const std::function<void(SpriteAnimatorComponent&)>& mutate)
    {
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>())
            return;

        SpriteAnimatorComponent& component = entity.GetComponent<SpriteAnimatorComponent>();
        const SpriteAnimatorComponent before = component;
        mutate(component);
        CommandHistory::Get().Push(
            std::make_unique<AnimatorStateCommand>(entity, before, component));
    }

    template<typename T>
    class AddComponentCommand : public ICommand
    {
    public:
        explicit AddComponentCommand(Entity entity) : m_Entity(entity) {}

        void Execute() override
        {
            if (!m_Entity.HasComponent<T>())
                m_Entity.AddComponent<T>();
        }

        void Undo() override
        {
            if (m_Entity.HasComponent<T>())
                m_Entity.RemoveComponent<T>();
        }

    private:
        Entity m_Entity;
    };

    template<typename T>
    class RemoveComponentCommand : public ICommand
    {
    public:
        explicit RemoveComponentCommand(Entity entity) : m_Entity(entity)
        {
            if (entity.HasComponent<T>())
                m_Snapshot = entity.GetComponent<T>();
        }

        void Execute() override
        {
            if (m_Entity.HasComponent<T>())
                m_Entity.RemoveComponent<T>();
        }

        void Undo() override
        {
            if (!m_Entity.HasComponent<T>())
                m_Entity.AddComponent<T>() = m_Snapshot;
        }

    private:
        Entity m_Entity;
        T      m_Snapshot{};
    };

} // namespace Wheatear
