namespace Wheatear
{
    public class SpriteRendererComponent : Component
    {
        public Vector4 Color
        {
            get { InternalCalls.SpriteRendererComponent_GetColor(Entity.ID, out Vector4 value); return value; }
            set => InternalCalls.SpriteRendererComponent_SetColor(Entity.ID, ref value);
        }

        public float TilingFactor
        {
            get => InternalCalls.SpriteRendererComponent_GetTilingFactor(Entity.ID);
            set => InternalCalls.SpriteRendererComponent_SetTilingFactor(Entity.ID, value);
        }

        public bool FlipX
        {
            get => InternalCalls.SpriteRendererComponent_GetFlipX(Entity.ID);
            set => InternalCalls.SpriteRendererComponent_SetFlipX(Entity.ID, value);
        }
    }

    public class CircleRendererComponent : Component { }
    public class CameraComponent : Component { }
}
