#pragma once
#include <functional>
#include <memory>
#include <stack>
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

    // ─────────────────────────────────────────────
    //  命令1：实体创建 / 删除
    //
    //  使用约定（重要）：
    //    1. 先构造命令，设置好 OnCreate 回调
    //    2. 调用 cmd->Execute()
    //    3. 调用 CommandHistory::Get().Push(std::move(cmd))
    //
    //  Undo（撤销删除）会重建实体并恢复所有已快照的组件。
    //  为了让 Undo 完整恢复，删除前需调用 SnapshotComponents()。
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
            SnapshotComponents(existingEntity);
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
                    m_Scene->DestroyEntity(m_CreatedEntity);
                m_CreatedEntity = {};
            }
        }

        void Undo() override
        {
            if (m_IsCreate)
            {
                if (IsEntityValid())
                    m_Scene->DestroyEntity(m_CreatedEntity);
                m_CreatedEntity = {};
            }
            else
            {
                // 撤销删除：重建并恢复快照数据
                m_CreatedEntity = m_Scene->CreateEntity(m_Name);
                RestoreComponents(m_CreatedEntity);
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

        // ── 组件快照（按需扩展） ──────────────────────────────────────
        // 在这里列出所有需要支持撤销删除的组件类型。
        // TransformComponent 是最基本的，其他按项目需要追加即可。
        void SnapshotComponents(Entity e)
        {
            if (e.HasComponent<TransformComponent>())
                m_TransformSnapshot = e.GetComponent<TransformComponent>();
            m_HasTransform = e.HasComponent<TransformComponent>();

            if (e.HasComponent<SpriteRendererComponent>())
            {
                m_SpriteSnapshot = e.GetComponent<SpriteRendererComponent>(); m_HasSprite = true;
            }
            if (e.HasComponent<CircleRendererComponent>())
            {
                m_CircleSnapshot = e.GetComponent<CircleRendererComponent>(); m_HasCircle = true;
            }
            if (e.HasComponent<CameraComponent>())
            {
                m_CameraSnapshot = e.GetComponent<CameraComponent>(); m_HasCamera = true;
            }
            if (e.HasComponent<Rigidbody2DComponent>())
            {
                m_Rb2dSnapshot = e.GetComponent<Rigidbody2DComponent>(); m_HasRb2d = true;
            }
            if (e.HasComponent<BoxCollider2DComponent>())
            {
                m_BoxCol2dSnapshot = e.GetComponent<BoxCollider2DComponent>(); m_HasBoxCol2d = true;
            }
            if (e.HasComponent<CircleCollider2DComponent>())
            {
                m_CircleCol2dSnapshot = e.GetComponent<CircleCollider2DComponent>(); m_HasCircleCol2d = true;
            }
        }

        void RestoreComponents(Entity e)
        {
            if (m_HasTransform)   e.AddComponent<TransformComponent>() = m_TransformSnapshot;
            if (m_HasSprite)      e.AddComponent<SpriteRendererComponent>() = m_SpriteSnapshot;
            if (m_HasCircle)      e.AddComponent<CircleRendererComponent>() = m_CircleSnapshot;
            if (m_HasCamera)      e.AddComponent<CameraComponent>() = m_CameraSnapshot;
            if (m_HasRb2d)        e.AddComponent<Rigidbody2DComponent>() = m_Rb2dSnapshot;
            if (m_HasBoxCol2d)    e.AddComponent<BoxCollider2DComponent>() = m_BoxCol2dSnapshot;
            if (m_HasCircleCol2d) e.AddComponent<CircleCollider2DComponent>() = m_CircleCol2dSnapshot;
        }

        Scene* m_Scene = nullptr;
        std::string m_Name;
        bool        m_IsCreate = true;
        Entity      m_CreatedEntity{};
        std::function<void(Entity)> m_OnCreate;

        // 各组件快照（仅在 isCreate=false 时填充）
        bool m_HasTransform = false;  TransformComponent       m_TransformSnapshot{};
        bool m_HasSprite = false;  SpriteRendererComponent  m_SpriteSnapshot{};
        bool m_HasCircle = false;  CircleRendererComponent  m_CircleSnapshot{};
        bool m_HasCamera = false;  CameraComponent          m_CameraSnapshot{};
        bool m_HasRb2d = false;  Rigidbody2DComponent     m_Rb2dSnapshot{};
        bool m_HasBoxCol2d = false;  BoxCollider2DComponent   m_BoxCol2dSnapshot{};
        bool m_HasCircleCol2d = false;  CircleCollider2DComponent m_CircleCol2dSnapshot{};
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