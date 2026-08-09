using Wheatear;

namespace WheatearSandbox
{
    public class Player : Entity
    {
        public float Speed = 6.0f;
        public float JumpForce = 6.0f;
        public string BulletPrefabPath = "assets/prefabs/Bullet.wtprefab";

        private Rigidbody2DComponent? _rigidbody;
        private SpriteAnimatorComponent? _animator;
        private SpriteRendererComponent? _sprite;
        private bool _lastJumpDown;
        private bool _lastShootDown;
        private float _moveInput;

        public override void OnCreate()
        {
            _rigidbody = GetComponent<Rigidbody2DComponent>();
            _animator = GetComponent<SpriteAnimatorComponent>();
            _sprite = GetComponent<SpriteRendererComponent>();

            _animator?.Play("idle");
        }

        public override void OnUpdate(float ts)
        {
            if (_rigidbody == null)
                return;

            _moveInput = Input.GetAxisRaw("Horizontal");

            bool jumpDown = Input.IsKeyDown(KeyCode.W);
            bool jumpPressed = jumpDown && !_lastJumpDown;
            _lastJumpDown = jumpDown;

            Vector2 velocity = _rigidbody.LinearVelocity;
            velocity.X = _moveInput * Speed;

            bool isGrounded = Mathf.Abs(velocity.Y) < 0.01f;
            if (jumpPressed && isGrounded)
                velocity.Y = JumpForce;

            if (velocity.Y < 0.0f)
                velocity.Y += -10.0f * ts;

            _rigidbody.LinearVelocity = velocity;

            UpdateAnimation(velocity, isGrounded);

            bool shootDown = Input.IsKeyDown(KeyCode.Space);
            if (shootDown && !_lastShootDown)
                Shoot();
            _lastShootDown = shootDown;
        }

        private void UpdateAnimation(Vector2 velocity, bool isGrounded)
        {
            if (_sprite != null)
            {
                if (_moveInput > 0.0f) _sprite.FlipX = false;
                else if (_moveInput < 0.0f) _sprite.FlipX = true;
            }

            if (_animator == null)
                return;

            if (!isGrounded)
                _animator.Play(velocity.Y > 0.0f ? "jump" : "fall");
            else if (Mathf.Abs(_moveInput) > 0.01f)
                _animator.Play("run");
            else
                _animator.Play("idle");
        }

        private void Shoot()
        {
            Vector3 spawnPos = Transform.Translation;
            float direction = (_sprite != null && _sprite.FlipX) ? -1.0f : 1.0f;
            spawnPos.X += 0.5f * direction;

            Entity? bulletEntity = Scene.InstantiatePrefab(BulletPrefabPath, spawnPos);
            Bullet? bullet = bulletEntity?.GetScript<Bullet>();
            if (bullet != null)
                bullet.Direction = direction;
        }
    }
}
