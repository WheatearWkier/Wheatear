namespace Wheatear
{
    public class Rigidbody2DComponent : Component
    {
        public enum BodyType { Static = 0, Dynamic, Kinematic }

        public BodyType Type
        {
            get => (BodyType)InternalCalls.Rigidbody2DComponent_GetBodyType(Entity.ID);
            set => InternalCalls.Rigidbody2DComponent_SetBodyType(Entity.ID, (int)value);
        }

        public float GravityScale
        {
            get => InternalCalls.Rigidbody2DComponent_GetGravityScale(Entity.ID);
            set => InternalCalls.Rigidbody2DComponent_SetGravityScale(Entity.ID, value);
        }

        public bool FixedRotation
        {
            get => InternalCalls.Rigidbody2DComponent_GetFixedRotation(Entity.ID);
            set => InternalCalls.Rigidbody2DComponent_SetFixedRotation(Entity.ID, value);
        }

        public Vector2 LinearVelocity
        {
            get { InternalCalls.Rigidbody2DComponent_GetLinearVelocity(Entity.ID, out Vector2 value); return value; }
            set => InternalCalls.Rigidbody2DComponent_SetLinearVelocity(Entity.ID, ref value);
        }

        public void ApplyLinearImpulse(Vector2 impulse, Vector2 point, bool wake = true)
        {
            InternalCalls.Rigidbody2DComponent_ApplyLinearImpulse(Entity.ID, ref impulse, ref point, wake);
        }

        public void ApplyLinearImpulseToCenter(Vector2 impulse, bool wake = true)
        {
            Vector2 center = Vector2.Zero;
            InternalCalls.Rigidbody2DComponent_ApplyLinearImpulse(Entity.ID, ref impulse, ref center, wake);
        }
    }

    public class BoxCollider2DComponent : Component { }
    public class CircleCollider2DComponent : Component { }
}
