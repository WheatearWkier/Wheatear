namespace Wheatear
{
    public class TransformComponent : Component
    {
        public Vector3 Translation
        {
            get { InternalCalls.TransformComponent_GetTranslation(Entity.ID, out Vector3 value); return value; }
            set => InternalCalls.TransformComponent_SetTranslation(Entity.ID, ref value);
        }

        public Vector3 Rotation
        {
            get { InternalCalls.TransformComponent_GetRotation(Entity.ID, out Vector3 value); return value; }
            set => InternalCalls.TransformComponent_SetRotation(Entity.ID, ref value);
        }

        public Vector3 Scale
        {
            get { InternalCalls.TransformComponent_GetScale(Entity.ID, out Vector3 value); return value; }
            set => InternalCalls.TransformComponent_SetScale(Entity.ID, ref value);
        }

        public void Translate(Vector3 delta)
        {
            Translation += delta;
        }

        public void Translate(float x, float y, float z = 0.0f)
        {
            Translate(new Vector3(x, y, z));
        }
    }
}
