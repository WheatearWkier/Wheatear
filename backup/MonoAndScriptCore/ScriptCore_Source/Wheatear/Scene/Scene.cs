namespace Wheatear
{
    /// <summary>
    /// 场景级脚本接口，负责查找、实例化和销毁实体。
    /// </summary>
    public static class Scene
    {
        public static Entity? FindEntityByName(string name)
        {
            ulong id = InternalCalls.Scene_FindEntityByName(name);
            return id == 0 ? null : new Entity(id);
        }

        public static T? FindScriptByName<T>(string name) where T : Entity
        {
            Entity? entity = FindEntityByName(name);
            return entity?.GetScript<T>();
        }

        public static Entity? InstantiatePrefab(string prefabPath, Vector3 position)
        {
            ulong id = InternalCalls.Scene_InstantiateFromPrefab(prefabPath, ref position);
            return id == 0 ? null : new Entity(id);
        }

        public static T? InstantiatePrefab<T>(string prefabPath, Vector3 position) where T : Entity
        {
            Entity? entity = InstantiatePrefab(prefabPath, position);
            return entity?.GetScript<T>();
        }

        public static void Destroy(Entity? entity)
        {
            if (entity != null && entity.IsValid)
                InternalCalls.Entity_Destroy(entity.ID);
        }
    }
}
