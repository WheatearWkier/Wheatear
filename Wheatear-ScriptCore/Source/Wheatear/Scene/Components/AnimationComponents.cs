namespace Wheatear
{
    public class SpriteAnimatorComponent : Component
    {
        public string CurrentClip => InternalCalls.SpriteAnimatorComponent_GetCurrentClip(Entity.ID);
        public bool IsFinished => InternalCalls.SpriteAnimatorComponent_IsFinished(Entity.ID);

        public void Play(string clipName)
        {
            InternalCalls.SpriteAnimatorComponent_Play(Entity.ID, clipName);
        }

        public void Stop()
        {
            InternalCalls.SpriteAnimatorComponent_Stop(Entity.ID);
        }

        public void Resume()
        {
            InternalCalls.SpriteAnimatorComponent_Resume(Entity.ID);
        }
    }
}
