using Wheatear;

namespace WheatearSandbox
{
    public class Enemy : Entity
    {
        public float MaxHp = 100.0f;
        public float Hp = 100.0f;
        public string HealthBarEntityName = "HealthBar";

        private UIProgressBarComponent? _healthBar;
        private SpriteAnimatorComponent? _animator;
        private bool _isDying;

        public override void OnCreate()
        {
            Hp = MaxHp;

            Entity? barEntity = Scene.FindEntityByName(HealthBarEntityName);
            _healthBar = barEntity?.GetComponent<UIProgressBarComponent>();
            if (_healthBar != null)
            {
                _healthBar.MaxValue = MaxHp;
                _healthBar.Value = Hp;
            }
            else
            {
                Debug.LogWarning($"Enemy could not find {HealthBarEntityName} entity.");
            }

            _animator = GetComponent<SpriteAnimatorComponent>();
        }

        public override void OnUpdate(float ts)
        {
            if (_isDying && (_animator == null || _animator.IsFinished))
                Destroy();
        }

        public void TakeDamage(float amount)
        {
            if (_isDying)
                return;

            Hp -= amount;
            if (_healthBar != null)
                _healthBar.Value = Hp;

            if (Hp <= 0.0f)
            {
                _isDying = true;
                _animator?.Play("boom");
            }
        }
    }
}
