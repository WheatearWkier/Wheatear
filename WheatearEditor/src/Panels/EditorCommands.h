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

    // ─────────────────────────────────────────────
    //  抽象命令接口
    // ─────────────────────────────────────────────
    struct ICommand
    {
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;

        // 供属性修改命令合并用：同一次拖拽只保留一条历史
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

    // ─────────────────────────────────────────────
    //  历史管理器（单例）
    // ─────────────────────────────────────────────
    class CommandHistory
    {
    public:
        static CommandHistory& Get()
        {
            static CommandHistory s_Instance;
            return s_Instance;
        }

        // BUG FIX: Push 只负责入栈，不调用 Execute。
        // 调用方必须先手动调用 cmd->Execute()，再 Push。
        // 这样 Redo 重新调用 Execute() 时不会造成二重执行。
        void Push(std::unique_ptr<ICommand> cmd, bool tryMerge = false)
        {
            if (tryMerge && !m_UndoStack.empty())
            {
                if (m_UndoStack.top()->TryMerge(cmd.get()))
                    return;
            }
            m_UndoStack.push(std::move(cmd));
            // 新操作清空 redo
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

        // BUG FIX: Clear は Scene 切り替え前に必ず呼ぶこと。
        // 古い Scene* を持つコマンドが残るとダングリングポインタになる。
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
        SidePickupComponent
    >;

    // ─────────────────────────────────────────────
    //  命令1：实体创建 / 删除
    // ─────────────────────────────────────────────
    class EntityCreateCommand : public ICommand
    {
    public:
        // 创建新实体
        EntityCreateCommand(Scene* scene, const std::string& name)
            : m_Scene(scene), m_Name(name), m_IsCreate(true)
        {
        }

        // 删除已有实体（isCreate=false）
        // BUG FIX: 删除时立即快照全部组件数据，Undo 时才能完整恢复
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

    // ─────────────────────────────────────────────
    //  命令2：组件属性修改（模板，支持任意组件）
    // ─────────────────────────────────────────────
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

        // 同一实体同一组件连续修改合并：只更新终态，丢弃中间值
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

    // ─────────────────────────────────────────────
    //  命令3：添加 / 移除组件
    // ─────────────────────────────────────────────
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
