using System;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace OpenAudio
{
#if UNITY_5_3_OR_NEWER
    [AddComponentMenu("OpenAudio/OpenAudio Studio Listener")]
    public class StudioListener : MonoBehaviour
    {
        public static StudioListener CurrentListener { get; private set; }

        private Vector3 m_PreviousPosition;
        private bool m_HasPreviousPosition = false;
        public Vector3 Velocity { get; private set; }

        private void OnEnable()
        {
            if (CurrentListener == null)
            {
                CurrentListener = this;
            }
            m_PreviousPosition = transform.position;
            m_HasPreviousPosition = true;
            Velocity = Vector3.zero;
            SyncToNative();
        }

        private void OnDisable()
        {
            if (CurrentListener == this)
            {
                CurrentListener = null;
            }
            m_HasPreviousPosition = false;
            Velocity = Vector3.zero;
        }

        private void Update()
        {
            Vector3 currentPos = transform.position;
            float dt = Time.deltaTime;
            if (m_HasPreviousPosition && dt > 1e-6f)
            {
                Velocity = (currentPos - m_PreviousPosition) / dt;
            }
            else
            {
                Velocity = Vector3.zero;
            }
            m_PreviousPosition = currentPos;
            m_HasPreviousPosition = true;
            SyncToNative();
        }

        public void SyncToNative()
        {
            var sys = OaeAudioManager.Instance != null ? OaeAudioManager.Instance.System : null;
            if (sys != null && sys.IsValid && CurrentListener == this)
            {
                sys.SetListener3DAttributes(To3DAttributes());
            }
        }

        public Oae3DAttributes To3DAttributes()
        {
            return new Oae3DAttributes
            {
                position = new OaeVec3(transform.position.x, transform.position.y, transform.position.z),
                velocity = new OaeVec3(Velocity.x, Velocity.y, Velocity.z),
                forward = new OaeVec3(transform.forward.x, transform.forward.y, transform.forward.z),
                up = new OaeVec3(transform.up.x, transform.up.y, transform.up.z)
            };
        }

        public Vector3 Position => transform.position;
        public Vector3 Forward => transform.forward;
        public Vector3 Up => transform.up;
        public Vector3 Right => transform.right;
    }
#else
    public class StudioListener
    {
        public static StudioListener CurrentListener { get; set; }
        public OaeVec3 Position { get; set; } = new OaeVec3(0f, 0f, 0f);
        public OaeVec3 Velocity { get; set; } = new OaeVec3(0f, 0f, 0f);
        public OaeVec3 Forward { get; set; } = new OaeVec3(0f, 0f, 1f);
        public OaeVec3 Up { get; set; } = new OaeVec3(0f, 1f, 0f);

        public Oae3DAttributes To3DAttributes()
        {
            return new Oae3DAttributes
            {
                position = Position,
                velocity = Velocity,
                forward = Forward,
                up = Up
            };
        }
    }
#endif
}
