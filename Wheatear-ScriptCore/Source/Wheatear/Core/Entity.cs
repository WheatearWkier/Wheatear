using System;
using System.Collections.Generic;

namespace Wheatear
{
    /// <summary>
    /// C# 脚本看到的实体对象。它只保存运行时 ID，真正的数据仍然在 C++ ECS 里。
    /// </summary>
    public class Entity
    {
        public ulong ID { get; private set; }

        private readonly Dictionary<Type, Component> _componentCache = new Dictionary<Type, Component>();

        protected Entity() { ID = 0; }
        internal Entity(ulong id) { ID = id; }

        // C++ 创建脚本实例时先运行子类构造，再写入真实 Entity ID。
        internal void SetRuntimeID(ulong id) { ID = id; }

        public bool IsValid => ID != 0;
        public string Tag => InternalCalls.Entity_GetTag(ID);
        public TransformComponent Transform => GetComponent<TransformComponent>()!;

        public T? GetComponent<T>() where T : Component, new()
        {
            Type type = typeof(T);
            if (_componentCache.TryGetValue(type, out Component cached))
            {
                if (!InternalCalls.Entity_HasComponent(ID, type))
                {
                    _componentCache.Remove(type);
                    return null;
                }
                return (T)cached;
            }

            if (!InternalCalls.Entity_HasComponent(ID, type))
                return null;

            T component = new T();
            component.Entity = this;
            _componentCache[type] = component;
            return component;
        }

        public bool TryGetComponent<T>(out T? component) where T : Component, new()
        {
            component = GetComponent<T>();
            return component != null;
        }

        public bool HasComponent<T>() where T : Component
        {
            return InternalCalls.Entity_HasComponent(ID, typeof(T));
        }

        public T AddComponent<T>() where T : Component, new()
        {
            Type type = typeof(T);
            if (_componentCache.TryGetValue(type, out Component cached))
            {
                if (InternalCalls.Entity_HasComponent(ID, type))
                    return (T)cached;

                _componentCache.Remove(type);
            }

            if (HasComponent<T>())
                return GetComponent<T>()!;

            InternalCalls.Entity_AddComponent(ID, type);

            T component = new T();
            component.Entity = this;
            _componentCache[type] = component;
            return component;
        }

        public T GetOrAddComponent<T>() where T : Component, new()
        {
            return HasComponent<T>() ? GetComponent<T>()! : AddComponent<T>();
        }

        public void Destroy()
        {
            Scene.Destroy(this);
        }

        public T? GetScript<T>() where T : Entity
        {
            object instance = InternalCalls.Entity_GetScriptInstance(ID);
            return instance as T;
        }

        // 兼容旧脚本写法；新脚本更推荐使用 Scene.FindEntityByName / Scene.FindScriptByName。
        public static Entity? FindEntityByName(string name) => Scene.FindEntityByName(name);
        public static T? FindEntityByName<T>(string name) where T : Entity => Scene.FindScriptByName<T>(name);

        public virtual void OnCreate() { }
        public virtual void OnUpdate(float ts) { }

        public virtual void OnCollisionEnter(Entity other) { }
        public virtual void OnCollisionExit(Entity other) { }

        internal void OnCollisionEnter(ulong otherID) => OnCollisionEnter(new Entity(otherID));
        internal void OnCollisionExit(ulong otherID) => OnCollisionExit(new Entity(otherID));
    }
}
