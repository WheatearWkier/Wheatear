using Wheatear;

namespace WheatearSandbox
{
    public class Bullet : Entity
    {
        public float Speed = 10.0f;
        public float Damage = 25.0f;
        public float Direction = 1.0f;
        public string ShootSoundPath = "assets/audios/bullet.mp3";
        public float ShootVolume = 1.0f;

        private Rigidbody2DComponent? _rigidbody;
        private SpriteRendererComponent? _sprite;
        private bool _initialized;

        public override void OnCreate()
        {
            _rigidbody = GetComponent<Rigidbody2DComponent>();
            _sprite = GetComponent<SpriteRendererComponent>();

            if (_rigidbody != null)
            {
                _rigidbody.Type = Rigidbody2DComponent.BodyType.Dynamic;
                _rigidbody.GravityScale = 0.0f;
            }

            if (!string.IsNullOrEmpty(ShootSoundPath))
                Audio.PlaySound(ShootSoundPath, ShootVolume);
        }

        public override void OnUpdate(float ts)
        {
            if (_initialized || _rigidbody == null)
                return;

            _rigidbody.LinearVelocity = new Vector2(Speed * Direction, 0.0f);
            if (_sprite != null)
                _sprite.FlipX = Direction < 0.0f;

            _initialized = true;
        }

        public override void OnCollisionEnter(Entity other)
        {
            if (other.Tag == "Player")
                return;

            if (other.Tag == "Enemy")
            {
                Enemy? enemy = other.GetScript<Enemy>();
                enemy?.TakeDamage(Damage);
            }

            Destroy();
        }
    }
}
