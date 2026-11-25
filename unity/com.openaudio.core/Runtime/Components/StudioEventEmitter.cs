using System;
using System.Collections.Generic;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace OpenAudio
{
    public enum EmitterPlayTrigger
    {
        Awake,
        Start,
        OnEnable,
        OnTriggerEnter,
        Manual
    }

    public enum EmitterStopTrigger
    {
        OnDisable,
        OnDestroy,
        OnTriggerExit,
        Manual
    }

    public enum EmitterSpatialMode
    {
        None = 0,
        Spatial3D = 1,
        Spatial2D = 2
    }

    [Serializable]
    public struct InitialParameter
    {
        public string Name;
        public float Value;
    }

#if UNITY_5_3_OR_NEWER
    [AddComponentMenu("OpenAudio/OpenAudio Studio Event Emitter")]
    public class StudioEventEmitter : MonoBehaviour
#else
#pragma warning disable CS0414
    public class StudioEventEmitter
#endif
    {
#if UNITY_5_3_OR_NEWER
        [Header("Event Configuration")]
        [EventRef]
        [SerializeField] private string m_EventPath;
        [SerializeField] private EmitterPlayTrigger m_PlayTrigger = EmitterPlayTrigger.Start;
        [SerializeField] private EmitterStopTrigger m_StopTrigger = EmitterStopTrigger.OnDestroy;
        [SerializeField] private OaeStopMode m_StopMode = OaeStopMode.AllowFadeout;

        [Header("2D & 3D Spatial Settings")]
        [SerializeField] private EmitterSpatialMode m_SpatialMode = EmitterSpatialMode.None;
        [SerializeField] private bool m_Is3D = false;
        [SerializeField] private bool m_Enable2DPan = true;
        [SerializeField] private bool m_Enable2DAttenuation = true;
        [SerializeField] private StudioListener m_ListenerOverride;
        [SerializeField] private float m_MinDistance = 1.0f;
        [SerializeField] private float m_MaxDistance = 20.0f;
        [SerializeField] private OaeRolloffType m_Rolloff = OaeRolloffType.Linear;
        [SerializeField] private float m_SoundSize = 0.0f;
        [SerializeField] private float m_MinExtent = 0.0f;
        [SerializeField] private float m_ConeInnerAngle = 360.0f;
        [SerializeField] private float m_ConeOuterAngle = 360.0f;
        [SerializeField] private float m_ConeOuterGain = 1.0f;
        private Vector3 m_PreviousPosition;
        private bool m_HasPreviousPosition = false;

        [Header("Initial Parameters")]
        [SerializeField] private List<InitialParameter> m_InitialParameters = new List<InitialParameter>();
#else
        private string m_EventPath;
        private EmitterPlayTrigger m_PlayTrigger = EmitterPlayTrigger.Start;
        private EmitterStopTrigger m_StopTrigger = EmitterStopTrigger.OnDestroy;
        private OaeStopMode m_StopMode = OaeStopMode.AllowFadeout;
        private EmitterSpatialMode m_SpatialMode = EmitterSpatialMode.None;
        private bool m_Is3D = false;
        private bool m_Enable2DPan = true;
        private bool m_Enable2DAttenuation = true;
        private StudioListener m_ListenerOverride = null;
        private float m_MinDistance = 1.0f;
        private float m_MaxDistance = 20.0f;
        private OaeRolloffType m_Rolloff = OaeRolloffType.Linear;
        private float m_SoundSize = 0.0f;
        private float m_MinExtent = 0.0f;
        private float m_ConeInnerAngle = 360.0f;
        private float m_ConeOuterAngle = 360.0f;
        private float m_ConeOuterGain = 1.0f;
        private List<InitialParameter> m_InitialParameters = new List<InitialParameter>();
#endif

        public string EventPath
        {
            get => m_EventPath;
            set => m_EventPath = value;
        }

        public EmitterPlayTrigger PlayTrigger
        {
            get => m_PlayTrigger;
            set => m_PlayTrigger = value;
        }

        public EmitterStopTrigger StopTrigger
        {
            get => m_StopTrigger;
            set => m_StopTrigger = value;
        }

        public EmitterSpatialMode SpatialMode
        {
            get => m_SpatialMode;
            set => m_SpatialMode = value;
        }

        public bool Enable2DPan
        {
            get => m_Enable2DPan;
            set => m_Enable2DPan = value;
        }

        public bool Enable2DAttenuation
        {
            get => m_Enable2DAttenuation;
            set => m_Enable2DAttenuation = value;
        }

        public StudioListener ListenerOverride
        {
            get => m_ListenerOverride;
            set => m_ListenerOverride = value;
        }

        public float MinDistance
        {
            get => m_MinDistance;
            set => m_MinDistance = value;
        }

        public float MaxDistance
        {
            get => m_MaxDistance;
            set => m_MaxDistance = value;
        }

        public OaeRolloffType Rolloff
        {
            get => m_Rolloff;
            set => m_Rolloff = value;
        }

        public float SoundSize
        {
            get => m_SoundSize;
            set => m_SoundSize = value;
        }

        public float MinExtent
        {
            get => m_MinExtent;
            set => m_MinExtent = value;
        }

        public float ConeInnerAngle
        {
            get => m_ConeInnerAngle;
            set => m_ConeInnerAngle = value;
        }

        public float ConeOuterAngle
        {
            get => m_ConeOuterAngle;
            set => m_ConeOuterAngle = value;
        }

        public float ConeOuterGain
        {
            get => m_ConeOuterGain;
            set => m_ConeOuterGain = value;
        }

        public OaeSpatializerSettings BuildSpatializerSettings(bool enabled)
        {
            float clampedMin = Math.Max(0.0f, m_MinDistance);
            float clampedMax = Math.Max(clampedMin, m_MaxDistance);
            return new OaeSpatializerSettings
            {
                enabled = enabled ? 1 : 0,
                rolloff = (int)m_Rolloff,
                min_distance = clampedMin,
                max_distance = clampedMax,
                sound_size = Math.Max(0.0f, m_SoundSize),
                min_extent_deg = Math.Max(0.0f, Math.Min(360.0f, m_MinExtent)),
                cone_inner_angle_deg = Math.Max(0.0f, Math.Min(360.0f, m_ConeInnerAngle)),
                cone_outer_angle_deg = Math.Max(0.0f, Math.Min(360.0f, m_ConeOuterAngle)),
                cone_outer_gain = Math.Max(0.0f, Math.Min(1.0f, m_ConeOuterGain))
            };
        }

        public List<InitialParameter> InitialParameters => m_InitialParameters;

        public EventInstance Instance { get; private set; }
        public bool IsPlaying => Instance != null && Instance.IsValid && Instance.PlaybackState == OaePlaybackState.Playing;

#if UNITY_5_3_OR_NEWER
        private void Awake()
        {
            if (m_PlayTrigger == EmitterPlayTrigger.Awake)
                Play();
        }

        private void Start()
        {
            if (m_PlayTrigger == EmitterPlayTrigger.Start)
                Play();
        }

        private void OnEnable()
        {
            if (m_PlayTrigger == EmitterPlayTrigger.OnEnable)
                Play();
        }

        private void OnDisable()
        {
            if (m_StopTrigger == EmitterStopTrigger.OnDisable)
                Stop();
        }

        private void OnDestroy()
        {
            if (m_StopTrigger == EmitterStopTrigger.OnDestroy)
                Stop();

            CleanupInstance();
        }

#if OAE_UNITY_PHYSICS
        private void OnTriggerEnter(Collider other)
        {
            if (m_PlayTrigger == EmitterPlayTrigger.OnTriggerEnter)
                Play();
        }

        private void OnTriggerExit(Collider other)
        {
            if (m_StopTrigger == EmitterStopTrigger.OnTriggerExit)
                Stop();
        }
#endif

#if OAE_UNITY_PHYSICS2D
        private void OnTriggerEnter2D(Collider2D other)
        {
            if (m_PlayTrigger == EmitterPlayTrigger.OnTriggerEnter)
                Play();
        }

        private void OnTriggerExit2D(Collider2D other)
        {
            if (m_StopTrigger == EmitterStopTrigger.OnTriggerExit)
                Stop();
        }
#endif

        private void Update()
        {
            var listener = m_ListenerOverride != null ? m_ListenerOverride : StudioListener.CurrentListener;
            if (!IsPlaying || listener == null)
                return;

            var mode = m_SpatialMode;
            if (mode == EmitterSpatialMode.None && m_Is3D)
                mode = EmitterSpatialMode.Spatial3D;

            if (mode == EmitterSpatialMode.Spatial3D)
            {
                Update3DPositioning(listener);
            }
            else if (mode == EmitterSpatialMode.Spatial2D)
            {
                Vector3 lPos = listener.Position;
                Vector3 ePos = transform.position;
                Compute2DSpatial(ePos.x, ePos.y, lPos.x, lPos.y,
                    m_MinDistance, m_MaxDistance, m_Enable2DAttenuation, m_Enable2DPan,
                    out float vol, out float pan);
                if (m_Enable2DAttenuation) Instance.Volume = vol;
                if (m_Enable2DPan) Instance.Pan = pan;
            }
        }

        private void Update3DPositioning(StudioListener listener)
        {
            var sys = OaeAudioManager.Instance != null ? OaeAudioManager.Instance.System : null;
            if (listener != null && sys != null && sys.IsValid && listener == m_ListenerOverride)
            {
                sys.SetListener3DAttributes(listener.To3DAttributes());
            }

            Vector3 currentPos = transform.position;
            Vector3 velocity = Vector3.zero;
            float dt = Time.deltaTime;
            if (m_HasPreviousPosition && dt > 1e-6f)
            {
                velocity = (currentPos - m_PreviousPosition) / dt;
            }
            m_PreviousPosition = currentPos;
            m_HasPreviousPosition = true;

            var attrs = new Oae3DAttributes
            {
                position = new OaeVec3(currentPos.x, currentPos.y, currentPos.z),
                velocity = new OaeVec3(velocity.x, velocity.y, velocity.z),
                forward = new OaeVec3(transform.forward.x, transform.forward.y, transform.forward.z),
                up = new OaeVec3(transform.up.x, transform.up.y, transform.up.z)
            };
            Instance.Set3DAttributes(attrs);
            Instance.SetSpatializerSettings(BuildSpatializerSettings(enabled: true));
        }
#endif

        /// <summary>
        /// Computes 2D distance attenuation and stereo pan in the XY plane.
        /// </summary>
        public static void Compute2DSpatial(
            float emitterX, float emitterY,
            float listenerX, float listenerY,
            float minDistance, float maxDistance,
            bool enableAttenuation, bool enablePan,
            out float outVolume, out float outPan)
        {
            float dx = emitterX - listenerX;
            float dy = emitterY - listenerY;
            float distance = (float)Math.Sqrt(dx * dx + dy * dy);

            outVolume = 1.0f;
            if (enableAttenuation && maxDistance > minDistance && distance > minDistance)
            {
                if (distance >= maxDistance)
                {
                    outVolume = 0.0f;
                }
                else
                {
                    outVolume = 1.0f - ((distance - minDistance) / (maxDistance - minDistance));
                }
            }

            outPan = 0.0f;
            if (enablePan && distance > 0.0001f)
            {
                outPan = Math.Max(-1.0f, Math.Min(1.0f, dx / distance));
            }
        }

        public void Play()
        {
            var sys = OaeAudioManager.Instance != null ? OaeAudioManager.Instance.System : null;
            if (sys == null || !sys.IsValid)
            {
#if UNITY_5_3_OR_NEWER
                Debug.LogWarning("[OpenAudio] Cannot play event: OaeAudioManager is not initialized.");
#endif
                return;
            }

            if (string.IsNullOrEmpty(m_EventPath))
                return;

            try
            {
                // Clean up previous instance if any
                CleanupInstance();

                var desc = sys.GetEvent(m_EventPath);
                Instance = desc.CreateInstance();

                // Apply initial parameters
                foreach (var param in m_InitialParameters)
                {
                    if (!string.IsNullOrEmpty(param.Name))
                    {
                        Instance.SetParameter(param.Name, param.Value, ignoreSeekSpeed: true);
                    }
                }

#if UNITY_5_3_OR_NEWER
                var mode = m_SpatialMode;
                if (mode == EmitterSpatialMode.None && m_Is3D)
                    mode = EmitterSpatialMode.Spatial3D;

                var activeListener = m_ListenerOverride != null ? m_ListenerOverride : StudioListener.CurrentListener;
                if (mode == EmitterSpatialMode.Spatial3D)
                {
                    m_PreviousPosition = transform.position;
                    m_HasPreviousPosition = true;
                    Update3DPositioning(activeListener);
                }
#endif

                Instance.Start();
            }
            catch (Exception ex)
            {
#if UNITY_5_3_OR_NEWER
                Debug.LogError($"[OpenAudio] Failed to play event '{m_EventPath}': {ex}");
#else
                Console.Error.WriteLine($"[OpenAudio] Failed to play event '{m_EventPath}': {ex}");
#endif
            }
        }

        public void Stop()
        {
            if (Instance != null && Instance.IsValid)
            {
                Instance.Stop(m_StopMode);
            }
        }

        public void SetParameter(string name, float value, bool ignoreSeekSpeed = false)
        {
            if (Instance != null && Instance.IsValid)
            {
                Instance.SetParameter(name, value, ignoreSeekSpeed);
            }
        }

        private void CleanupInstance()
        {
            if (Instance != null)
            {
                Instance.Release();
                Instance = null;
            }
        }
    }
#if !UNITY_5_3_OR_NEWER
#pragma warning restore CS0414
#endif
}
