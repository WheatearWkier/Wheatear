namespace Wheatear
{
    public class AudioSourceComponent : Component
    {
        public bool IsPlaying => InternalCalls.AudioSourceComponent_IsPlaying(Entity.ID);

        public float Volume
        {
            get => InternalCalls.AudioSourceComponent_GetVolume(Entity.ID);
            set => InternalCalls.AudioSourceComponent_SetVolume(Entity.ID, value);
        }

        public void Play()
        {
            InternalCalls.AudioSourceComponent_Play(Entity.ID);
        }

        public void Stop()
        {
            InternalCalls.AudioSourceComponent_Stop(Entity.ID);
        }
    }
}
