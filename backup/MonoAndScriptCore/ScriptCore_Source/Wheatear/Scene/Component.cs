namespace Wheatear
{
    /// <summary>
    /// 所有 C# 组件包装类的基类。组件数据实际保存在 C++ ECS 中。
    /// </summary>
    public abstract class Component
    {
        public Entity Entity { get; internal set; } = null!;
    }
}
