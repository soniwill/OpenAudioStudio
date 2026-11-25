using System;
using System.Collections;
using System.IO;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace OpenAudio
{
    /// <summary>
    /// Ground surface types exercised by Station 1 multi-instrument footsteps.
    /// </summary>
    public enum PlaygroundSurface
    {
        Wood = 0,
        Concrete = 1,
        Grass = 2
    }

    /// <summary>
    /// Dual-target controller (Unity Runtime and headless .NET 8) for the 3D Audio Playground showcase room.
    /// </summary>
#if UNITY_5_3_OR_NEWER
    [AddComponentMenu("OpenAudio/OpenAudio Audio Playground Controller")]
    public class AudioPlaygroundController : MonoBehaviour
#else
#pragma warning disable CS0414
    public class AudioPlaygroundController
#endif
    {
        private const float DefaultEmitterX = 0.0f;
        private const float DefaultEmitterY = 1.2f;
        private const float DefaultEmitterZ = 8.0f;

#if UNITY_5_3_OR_NEWER
        [Header("Event & Snapshot Paths")]
        [SerializeField, EventRef] private string m_FootstepEventPath = "event:/playground/footstep";
        [SerializeField, EventRef] private string m_Emitter3DEventPath = "event:/playground/emitter_3d";
        [SerializeField, EventRef] private string m_AdaptiveMusicEventPath = "event:/playground/adaptive_music";
        [SerializeField, EventRef] private string m_ForestScattererEventPath = "event:/playground/forest_scatterer";
        [SerializeField, EventRef] private string m_DialogueVoEventPath = "event:/playground/dialogue_vo";
        [SerializeField, EventRef] private string m_UiClickEventPath = "event:/playground/ui_click";
        [SerializeField, EventRef] private string m_CaveSnapshotPath = "snapshot:/playground/cave_reverb";
        [SerializeField, EventRef] private string m_VoDuckingSnapshotPath = "snapshot:/playground/vo_ducking";

        [Header("VCAs & On-Demand Bank")]
        [SerializeField] private string m_MasterVcaPath = "vca:/playground_master";
        [SerializeField] private string m_MusicVcaPath = "vca:/playground_music";
        [SerializeField] private string m_SfxVcaPath = "vca:/playground_sfx";
        [SerializeField] private string m_VoBankFileName = "playground_vo.bank";

        [Header("Player & Room Settings")]
        [SerializeField] private float m_MoveSpeed = 5.0f;
        [SerializeField] private float m_LookSensitivity = 2.0f;
        [SerializeField] private float m_StepIntervalSeconds = 0.42f;
        [SerializeField] private bool m_AutoStartAmbience = true;
        [SerializeField] private bool m_BuildProceduralRoom = true;
        [SerializeField] private bool m_ShowHud = true;
#else
        private string m_FootstepEventPath = "event:/playground/footstep";
        private string m_Emitter3DEventPath = "event:/playground/emitter_3d";
        private string m_AdaptiveMusicEventPath = "event:/playground/adaptive_music";
        private string m_ForestScattererEventPath = "event:/playground/forest_scatterer";
        private string m_DialogueVoEventPath = "event:/playground/dialogue_vo";
        private string m_UiClickEventPath = "event:/playground/ui_click";
        private string m_CaveSnapshotPath = "snapshot:/playground/cave_reverb";
        private string m_VoDuckingSnapshotPath = "snapshot:/playground/vo_ducking";

        private string m_MasterVcaPath = "vca:/playground_master";
        private string m_MusicVcaPath = "vca:/playground_music";
        private string m_SfxVcaPath = "vca:/playground_sfx";
        private string m_VoBankFileName = "playground_vo.bank";
        private float m_StepIntervalSeconds = 0.42f;
#endif

        private enum CameraCueMode
        {
            None = 0,
            Move = 1,
            Orbit = 2
        }

        private OaeSystem m_System;
        private EventInstance m_FootstepInstance;
        private EventInstance m_Emitter3DInstance;
        private EventInstance m_AdaptiveMusicInstance;
        private EventInstance m_ForestScattererInstance;
        private EventInstance m_CaveSnapshotInstance;
        private EventInstance m_VoDuckingSnapshotInstance;
        private EventInstance m_DialogueVoInstance;
        private bool m_VoDuckingActive;
        private Bank m_VoBank;

        private float m_YawDegrees = 0.0f;
        private float m_PitchDegrees = 5.0f;
        private bool m_WasInCombatTrigger = false;
        private bool m_WasInForestTrigger = false;
        private bool m_WasInCaveTrigger = false;

        private CameraCueMode m_CameraCueMode = CameraCueMode.None;
        private float m_CueElapsed = 0.0f;
        private float m_CueDuration = 0.0f;
        private float m_CueStartX = 0.0f;
        private float m_CueStartY = 1.6f;
        private float m_CueStartZ = -4.0f;
        private float m_CueTargetX = 0.0f;
        private float m_CueTargetY = 1.6f;
        private float m_CueTargetZ = -4.0f;
        private float m_CueStartYaw = 0.0f;
        private float m_CueStartPitch = 5.0f;
        private float m_CueTargetYaw = 0.0f;
        private float m_CueTargetPitch = 5.0f;
        private float m_CueStepTimer = 0.0f;
        private float m_OrbitCenterX = 0.0f;
        private float m_OrbitCenterY = 1.2f;
        private float m_OrbitCenterZ = 8.0f;
        private float m_OrbitRadius = 3.6f;
        private float m_OrbitHeight = 1.6f;
        private float m_OrbitStartAngleDeg = 180.0f;
        private float m_OrbitSweepDeg = 360.0f;

        /// <summary>Active ground surface selected for footsteps.</summary>
        public PlaygroundSurface CurrentSurface { get; private set; } = PlaygroundSurface.Wood;

        /// <summary>Total number of footsteps triggered during the session.</summary>
        public int TotalFootstepsTriggered { get; private set; }

        /// <summary>Latest Euclidean distance read from the 3D emitter built-in parameter.</summary>
        public float LastDistance { get; private set; }

        /// <summary>Latest horizontal direction angle read from the 3D emitter built-in parameter.</summary>
        public float LastDirection { get; private set; }

        /// <summary>Latest vertical elevation angle read from the 3D emitter built-in parameter.</summary>
        public float LastElevation { get; private set; }

        /// <summary>Latest emitter cone deviation angle read from the 3D emitter built-in parameter.</summary>
        public float LastEventConeAngle { get; private set; }

        /// <summary>Latest relative speed read from the 3D emitter built-in parameter.</summary>
        public float LastSpeed { get; private set; }

        /// <summary>True while the player or controller is in the Combat Arena zone.</summary>
        public bool IsCombatZoneActive { get; private set; }

        /// <summary>Current normalized value of CombatIntensity (0.0 Exploration to 1.0 Combat).</summary>
        public float CombatIntensity { get; private set; }

        /// <summary>Current timeline cursor position in milliseconds for adaptive music.</summary>
        public int AdaptiveMusicTimelinePositionMs =>
            m_AdaptiveMusicInstance != null && m_AdaptiveMusicInstance.IsValid
                ? m_AdaptiveMusicInstance.TimelinePositionMs
                : 0;

        /// <summary>True while the Forest Scatterer station is active.</summary>
        public bool IsForestZoneActive { get; private set; }

        /// <summary>True while the Cave Acoustic Tunnel snapshot is active.</summary>
        public bool IsCaveZoneActive { get; private set; }

        /// <summary>True after playground_vo.bank has been loaded on demand.</summary>
        public bool IsVoBankLoaded => m_VoBank != null && m_VoBank.IsValid;

        /// <summary>Audio table key passed to the latest Programmer Instrument dialogue trigger.</summary>
        public string LastDialogueKey { get; private set; } = string.Empty;

        /// <summary>Current linear gain of vca:/playground_master.</summary>
        public float MasterVcaLevel { get; private set; } = 1.0f;

        /// <summary>Current linear gain of vca:/playground_music.</summary>
        public float MusicVcaLevel { get; private set; } = 1.0f;

        /// <summary>Current linear gain of vca:/playground_sfx.</summary>
        public float SfxVcaLevel { get; private set; } = 1.0f;

        /// <summary>Total number of 2D UI click events triggered.</summary>
        public int UiClicksTriggered { get; private set; }

        /// <summary>True when LiveClient is connected to OpenAudio Studio.</summary>
        public bool IsLiveLinkConnected { get; private set; }

        /// <summary>True while an automated tutorial camera move or orbit cue is running.</summary>
        public bool IsCameraCueActive => m_CameraCueMode != CameraCueMode.None;

        /// <summary>Latest player/listener world X coordinate.</summary>
        public float PlayerPosX { get; private set; } = 0.0f;

        /// <summary>Latest player/listener world Y coordinate.</summary>
        public float PlayerPosY { get; private set; } = 1.6f;

        /// <summary>Latest player/listener world Z coordinate.</summary>
        public float PlayerPosZ { get; private set; } = -4.0f;

        /// <summary>Latest player/listener yaw orientation in degrees.</summary>
        public float PlayerYawDegrees => m_YawDegrees;

        /// <summary>Latest player/listener pitch orientation in degrees.</summary>
        public float PlayerPitchDegrees => m_PitchDegrees;

        /// <summary>
        /// Binds an initialized OaeSystem instance for headless or runtime execution.
        /// </summary>
        public void Initialize(OaeSystem system)
        {
            m_System = system ?? throw new ArgumentNullException(nameof(system));
        }

        /// <summary>
        /// Starts the Station 2 3D emitter and Station 3 adaptive music in Exploration state.
        /// </summary>
        public void StartPlaygroundAmbience()
        {
            if (m_System == null || !m_System.IsValid)
                return;

#if UNITY_5_3_OR_NEWER
            if (m_SceneEmitter3D != null)
            {
                if (!m_SceneEmitter3D.IsPlaying)
                {
                    m_SceneEmitter3D.Play();
                }
                Refresh3DBuiltinParameters();
            }
            else
            {
                UpdateEmitter3D();
            }
#else
            UpdateEmitter3D();
#endif
            SetCombatZoneActive(false);
        }

        /// <summary>
        /// Updates listener 3D transform and refreshes Station 2 built-in parameter readouts.
        /// </summary>
        public void SetPlayerTransform(
            float px, float py, float pz,
            float fx, float fy, float fz,
            float vx = 0.0f, float vy = 0.0f, float vz = 0.0f)
        {
            if (m_System == null || !m_System.IsValid)
                return;

            float mag = (float)Math.Sqrt(fx * fx + fy * fy + fz * fz);
            float nfx = mag > 1e-5f ? fx / mag : 0.0f;
            float nfy = mag > 1e-5f ? fy / mag : 0.0f;
            float nfz = mag > 1e-5f ? fz / mag : 1.0f;

            var attrs = new Oae3DAttributes
            {
                position = new OaeVec3(px, py, pz),
                velocity = new OaeVec3(vx, vy, vz),
                forward = new OaeVec3(nfx, nfy, nfz),
                up = new OaeVec3(0.0f, 1.0f, 0.0f)
            };
            m_System.SetListener3DAttributes(attrs);

            PlayerPosX = px;
            PlayerPosY = py;
            PlayerPosZ = pz;

#if UNITY_5_3_OR_NEWER
            if (m_PlayerTransform != null)
            {
                m_PlayerTransform.position = new Vector3(px, py, pz);
                if (mag > 1e-5f)
                {
                    m_PlayerTransform.forward = new Vector3(nfx, nfy, nfz);
                }
            }
#endif
            Refresh3DBuiltinParameters();
        }

        /// <summary>
        /// Resolves the playground ground surface from an X coordinate.
        /// </summary>
        public static PlaygroundSurface DetectSurfaceAtX(float posX)
        {
            if (posX < -2.0f) return PlaygroundSurface.Wood;
            if (posX > 2.0f) return PlaygroundSurface.Grass;
            return PlaygroundSurface.Concrete;
        }

        /// <summary>
        /// Starts a SmoothStep camera movement cue toward a target position and orientation.
        /// </summary>
        public void BeginCameraMoveCue(
            float targetX, float targetY, float targetZ,
            float targetYawDeg, float targetPitchDeg,
            float durationSeconds)
        {
            m_CueStartX = PlayerPosX;
            m_CueStartY = PlayerPosY;
            m_CueStartZ = PlayerPosZ;
            m_CueStartYaw = m_YawDegrees;
            m_CueStartPitch = m_PitchDegrees;

            m_CueTargetX = targetX;
            m_CueTargetY = targetY;
            m_CueTargetZ = targetZ;
            m_CueTargetYaw = targetYawDeg;
            m_CueTargetPitch = Math.Max(-65.0f, Math.Min(65.0f, targetPitchDeg));

            m_CueElapsed = 0.0f;
            m_CueDuration = Math.Max(0.0f, durationSeconds);
            m_CueStepTimer = m_StepIntervalSeconds * 0.8f;
            m_CameraCueMode = CameraCueMode.Move;

            if (m_CueDuration <= 1e-5f)
            {
                StepCameraCue(1.0f);
            }
        }

        /// <summary>
        /// Starts a SmoothStep circular camera orbit cue continuously facing the given center point.
        /// </summary>
        public void BeginCameraOrbitCue(
            float centerX, float centerY, float centerZ,
            float radius, float height, float durationSeconds,
            float startAngleDeg = 180.0f, float sweepDeg = 360.0f)
        {
            m_OrbitCenterX = centerX;
            m_OrbitCenterY = centerY;
            m_OrbitCenterZ = centerZ;
            m_OrbitRadius = Math.Max(0.25f, radius);
            m_OrbitHeight = height;
            m_OrbitStartAngleDeg = startAngleDeg;
            m_OrbitSweepDeg = sweepDeg;

            m_CueElapsed = 0.0f;
            m_CueDuration = Math.Max(0.0f, durationSeconds);
            m_CueStepTimer = 0.0f;
            m_CameraCueMode = CameraCueMode.Orbit;

            if (m_CueDuration <= 1e-5f)
            {
                StepCameraCue(1.0f);
            }
        }

        /// <summary>
        /// Cancels any active automated camera move or orbit cue immediately.
        /// </summary>
        public void CancelCameraCue()
        {
            m_CameraCueMode = CameraCueMode.None;
            m_CueElapsed = 0.0f;
            m_CueDuration = 0.0f;
        }

        /// <summary>
        /// Advances the active camera cue by deltaSeconds using SmoothStep interpolation, updating listener, footsteps, and zones.
        /// </summary>
        public void StepCameraCue(float deltaSeconds)
        {
            if (m_CameraCueMode == CameraCueMode.None || deltaSeconds <= 0.0f)
                return;

            float prevX = PlayerPosX;
            float prevY = PlayerPosY;
            float prevZ = PlayerPosZ;

            m_CueElapsed += deltaSeconds;
            float rawT = m_CueDuration > 1e-5f
                ? Math.Max(0.0f, Math.Min(1.0f, m_CueElapsed / m_CueDuration))
                : 1.0f;
            if (rawT >= 1.0f - 1e-5f)
            {
                rawT = 1.0f;
            }
            float smooth = rawT * rawT * (3.0f - 2.0f * rawT);

            float nextX;
            float nextY;
            float nextZ;
            float nextYawDeg;
            float nextPitchDeg;

            if (m_CameraCueMode == CameraCueMode.Move)
            {
                nextX = m_CueStartX + (m_CueTargetX - m_CueStartX) * smooth;
                nextY = m_CueStartY + (m_CueTargetY - m_CueStartY) * smooth;
                nextZ = m_CueStartZ + (m_CueTargetZ - m_CueStartZ) * smooth;
                nextYawDeg = m_CueStartYaw + ShortestAngleDeltaDeg(m_CueStartYaw, m_CueTargetYaw) * smooth;
                nextPitchDeg = m_CueStartPitch + (m_CueTargetPitch - m_CueStartPitch) * smooth;
            }
            else
            {
                float angleDeg = m_OrbitStartAngleDeg + m_OrbitSweepDeg * smooth;
                double angleRad = angleDeg * (Math.PI / 180.0);
                nextX = m_OrbitCenterX + (float)Math.Sin(angleRad) * m_OrbitRadius;
                nextY = m_OrbitHeight;
                nextZ = m_OrbitCenterZ + (float)Math.Cos(angleRad) * m_OrbitRadius;

                float dx = m_OrbitCenterX - nextX;
                float dy = m_OrbitCenterY - nextY;
                float dz = m_OrbitCenterZ - nextZ;
                double horiz = Math.Sqrt(dx * dx + dz * dz);
                nextYawDeg = (float)(Math.Atan2(dx, dz) * (180.0 / Math.PI));
                nextPitchDeg = (float)(-Math.Atan2(dy, Math.Max(1e-4, horiz)) * (180.0 / Math.PI));
            }

            double yawRad = nextYawDeg * (Math.PI / 180.0);
            double pitchRad = nextPitchDeg * (Math.PI / 180.0);
            double cosPitch = Math.Cos(pitchRad);
            float fx = (float)(Math.Sin(yawRad) * cosPitch);
            float fy = (float)(-Math.Sin(pitchRad));
            float fz = (float)(Math.Cos(yawRad) * cosPitch);

            float vx = (nextX - prevX) / deltaSeconds;
            float vy = (nextY - prevY) / deltaSeconds;
            float vz = (nextZ - prevZ) / deltaSeconds;

            m_YawDegrees = nextYawDeg;
            m_PitchDegrees = nextPitchDeg;
            SetPlayerTransform(nextX, nextY, nextZ, fx, fy, fz, vx, vy, vz);

#if UNITY_5_3_OR_NEWER
            if (m_PlayerTransform != null)
            {
                m_PlayerTransform.rotation = Quaternion.Euler(m_PitchDegrees, m_YawDegrees, 0.0f);
                var listener = m_PlayerTransform.GetComponent<StudioListener>();
                listener?.SyncToNative();
            }
#endif

            if (m_CameraCueMode == CameraCueMode.Move)
            {
                float movedSq = (nextX - prevX) * (nextX - prevX) + (nextZ - prevZ) * (nextZ - prevZ);
                if (movedSq > 1e-6f)
                {
                    m_CueStepTimer += deltaSeconds;
                    if (m_CueStepTimer >= m_StepIntervalSeconds)
                    {
                        m_CueStepTimer = 0.0f;
                        SelectSurfaceAndStep(DetectSurfaceAtX(nextX));
                    }
                }
            }

            UpdateZoneTriggersFromCoordinates(nextX, nextZ);

            if (rawT >= 1.0f)
            {
                m_CameraCueMode = CameraCueMode.None;
            }
        }

        /// <summary>
        /// Updates SurfaceType parameter and triggers a multi-instrument footstep instance.
        /// </summary>
        public bool SelectSurfaceAndStep(PlaygroundSurface surface)
        {
            CurrentSurface = surface;
            if (m_System == null || !m_System.IsValid)
                return false;

            try
            {
                m_System.SetGlobalParameter("SurfaceType", (float)surface, ignoreSeekSpeed: true);
                var desc = m_System.GetEvent(m_FootstepEventPath);
                if (!desc.IsValid)
                    return false;

                StopAndReleaseInstance(ref m_FootstepInstance, OaeStopMode.Immediate);
                m_FootstepInstance = desc.CreateInstance();
                m_FootstepInstance.Start();
                TotalFootstepsTriggered++;
#if UNITY_5_3_OR_NEWER
                m_StatusMessage = $"Passo em {surface} (SurfaceType={(int)surface}, Total={TotalFootstepsTriggered}).";
#endif
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        /// <summary>
        /// Starts or updates the Station 2 3D emitter spatial attributes, directional cone, and built-in readouts.
        /// </summary>
        public void UpdateEmitter3D(float distanceOverrideOrZero = 0.0f)
        {
            if (m_System == null || !m_System.IsValid)
                return;

            float targetZ = distanceOverrideOrZero > 0.0f ? distanceOverrideOrZero : DefaultEmitterZ;
            var settings = new OaeSpatializerSettings
            {
                enabled = 1,
                rolloff = (int)OaeRolloffType.Inverse,
                min_distance = 1.5f,
                max_distance = 22.0f,
                sound_size = 1.2f,
                min_extent_deg = 15.0f,
                cone_inner_angle_deg = 45.0f,
                cone_outer_angle_deg = 115.0f,
                cone_outer_gain = 0.04f
            };

#if UNITY_5_3_OR_NEWER
            if (m_SceneEmitter3D != null)
            {
                if (distanceOverrideOrZero > 0.0f)
                {
                    m_SceneEmitter3D.transform.position = new Vector3(DefaultEmitterX, DefaultEmitterY, targetZ);
                }
                if (!m_SceneEmitter3D.IsPlaying)
                {
                    m_SceneEmitter3D.Play();
                }
                var sceneInst = m_SceneEmitter3D.Instance;
                if (sceneInst != null && sceneInst.IsValid)
                {
                    Vector3 p = m_SceneEmitter3D.transform.position;
                    Vector3 f = m_SceneEmitter3D.transform.forward;
                    Vector3 u = m_SceneEmitter3D.transform.up;
                    sceneInst.Set3DAttributes(new Oae3DAttributes
                    {
                        position = new OaeVec3(p.x, p.y, p.z),
                        velocity = new OaeVec3(0.0f, 0.0f, 0.0f),
                        forward = new OaeVec3(f.x, f.y, f.z),
                        up = new OaeVec3(u.x, u.y, u.z)
                    });
                    sceneInst.SetSpatializerSettings(settings);
                }
                Refresh3DBuiltinParameters();
                return;
            }
#endif

            try
            {
                if (m_Emitter3DInstance == null || !m_Emitter3DInstance.IsValid)
                {
                    var desc = m_System.GetEvent(m_Emitter3DEventPath);
                    if (!desc.IsValid)
                        return;
                    m_Emitter3DInstance = desc.CreateInstance();
                    m_Emitter3DInstance.Set3DAttributes(new Oae3DAttributes
                    {
                        position = new OaeVec3(DefaultEmitterX, DefaultEmitterY, targetZ),
                        velocity = new OaeVec3(0.0f, 0.0f, 0.0f),
                        forward = new OaeVec3(0.0f, 0.0f, -1.0f),
                        up = new OaeVec3(0.0f, 1.0f, 0.0f)
                    });
                    m_Emitter3DInstance.SetSpatializerSettings(settings);
                    m_Emitter3DInstance.Start();
                }
                else
                {
                    m_Emitter3DInstance.Set3DAttributes(new Oae3DAttributes
                    {
                        position = new OaeVec3(DefaultEmitterX, DefaultEmitterY, targetZ),
                        velocity = new OaeVec3(0.0f, 0.0f, 0.0f),
                        forward = new OaeVec3(0.0f, 0.0f, -1.0f),
                        up = new OaeVec3(0.0f, 1.0f, 0.0f)
                    });
                    m_Emitter3DInstance.SetSpatializerSettings(settings);
                }
            }
            catch (Exception)
            {
            }

            Refresh3DBuiltinParameters();
        }

        /// <summary>
        /// Refreshes cached built-in 3D parameter readings from the active emitter instance.
        /// </summary>
        public void Refresh3DBuiltinParameters()
        {
            var inst = GetActiveEmitter3DInstance();
            if (inst == null || !inst.IsValid)
                return;

            try
            {
                inst.GetParameter("Distance", out float dist, out _);
                inst.GetParameter("Direction", out float dir, out _);
                inst.GetParameter("Elevation", out float elev, out _);
                inst.GetParameter("EventConeAngle", out float cone, out _);
                inst.GetParameter("Speed", out float speed, out _);
                LastDistance = dist;
                LastDirection = dir;
                LastElevation = elev;
                LastEventConeAngle = cone;
                LastSpeed = speed;
            }
            catch (Exception)
            {
            }
        }

        /// <summary>
        /// Toggles Station 3 adaptive music between Exploration (0.0) and Combat (1.0).
        /// </summary>
        public void SetCombatZoneActive(bool active)
        {
            IsCombatZoneActive = active;
            CombatIntensity = active ? 1.0f : 0.0f;

            if (m_System == null || !m_System.IsValid)
                return;

            try
            {
                m_System.SetGlobalParameter("CombatIntensity", CombatIntensity, ignoreSeekSpeed: true);

                if (m_AdaptiveMusicInstance == null || !m_AdaptiveMusicInstance.IsValid)
                {
                    var desc = m_System.GetEvent(m_AdaptiveMusicEventPath);
                    if (desc.IsValid)
                    {
                        m_AdaptiveMusicInstance = desc.CreateInstance();
                        m_AdaptiveMusicInstance.Start();
                    }
                }

                if (m_AdaptiveMusicInstance != null && m_AdaptiveMusicInstance.IsValid)
                {
                    m_AdaptiveMusicInstance.SetParameter("CombatIntensity", CombatIntensity, ignoreSeekSpeed: true);
                }
            }
            catch (Exception)
            {
            }
        }

        /// <summary>
        /// Starts or stops the Station 4 stochastic Forest Scatterer instrument.
        /// </summary>
        public void SetForestZoneActive(bool active)
        {
            IsForestZoneActive = active;
            if (m_System == null || !m_System.IsValid)
                return;

            try
            {
                if (active)
                {
                    if (m_ForestScattererInstance == null || !m_ForestScattererInstance.IsValid)
                    {
                        var desc = m_System.GetEvent(m_ForestScattererEventPath);
                        if (desc.IsValid)
                        {
                            m_ForestScattererInstance = desc.CreateInstance();
                            m_ForestScattererInstance.Set3DAttributes(new Oae3DAttributes
                            {
                                position = new OaeVec3(7.0f, 1.5f, 6.0f),
                                velocity = new OaeVec3(0.0f, 0.0f, 0.0f),
                                forward = new OaeVec3(-1.0f, 0.0f, 0.0f),
                                up = new OaeVec3(0.0f, 1.0f, 0.0f)
                            });
                        }
                    }
                    if (m_ForestScattererInstance != null && m_ForestScattererInstance.IsValid)
                    {
                        m_ForestScattererInstance.Start();
                    }
                }
                else
                {
                    if (m_ForestScattererInstance != null && m_ForestScattererInstance.IsValid)
                    {
                        m_ForestScattererInstance.Stop(OaeStopMode.AllowFadeout);
                    }
                }
            }
            catch (Exception)
            {
            }
        }

        /// <summary>
        /// Activates or releases the Station 5 acoustic tunnel snapshot (cave_reverb with reverb + lowpass).
        /// </summary>
        public void SetCaveZoneActive(bool active)
        {
            IsCaveZoneActive = active;
            if (m_System == null || !m_System.IsValid)
                return;

            try
            {
                if (active)
                {
                    if (m_CaveSnapshotInstance == null || !m_CaveSnapshotInstance.IsValid)
                    {
                        var desc = m_System.GetEvent(m_CaveSnapshotPath);
                        if (desc.IsValid)
                        {
                            m_CaveSnapshotInstance = desc.CreateInstance();
                        }
                    }
                    if (m_CaveSnapshotInstance != null && m_CaveSnapshotInstance.IsValid)
                    {
                        m_CaveSnapshotInstance.Start();
                    }
                }
                else
                {
                    if (m_CaveSnapshotInstance != null && m_CaveSnapshotInstance.IsValid)
                    {
                        m_CaveSnapshotInstance.Stop(OaeStopMode.AllowFadeout);
                    }
                }
            }
            catch (Exception)
            {
            }
        }

        /// <summary>
        /// Loads playground_vo.bank on demand, engages auto-releasing music ducking snapshot, and triggers dialogue via Programmer Instrument.
        /// </summary>
        public bool TriggerProgrammerDialogue(string audioTableKey = "vo_welcome", string voBankPath = null)
        {
            if (m_System == null || !m_System.IsValid)
                return false;

            string key = string.IsNullOrEmpty(audioTableKey) ? "vo_welcome" : audioTableKey;

            try
            {
                if (m_VoBank == null || !m_VoBank.IsValid)
                {
                    string resolvedPath = ResolveVoBankPath(voBankPath);
                    if (string.IsNullOrEmpty(resolvedPath) || !File.Exists(resolvedPath))
                        return false;

                    m_VoBank = m_System.LoadBankFile(resolvedPath);
                }

                StartVoDuckingSnapshot();
                StopAndReleaseInstance(ref m_DialogueVoInstance, OaeStopMode.Immediate);

                var desc = m_System.GetEvent(m_DialogueVoEventPath);
                if (!desc.IsValid)
                    return false;

                m_DialogueVoInstance = desc.CreateInstance();
                m_DialogueVoInstance.SetParameterWithLabel("DialogueLine", key, ignoreSeekSpeed: true);
                m_DialogueVoInstance.Start();
                LastDialogueKey = key;
#if UNITY_5_3_OR_NEWER
                m_StatusMessage = $"VO Programmer: '{key}' (ducking ativo — musica volta ao fim da fala).";
#endif
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        private void StartVoDuckingSnapshot()
        {
            try
            {
                if (m_VoDuckingSnapshotInstance == null || !m_VoDuckingSnapshotInstance.IsValid)
                {
                    var duckDesc = m_System.GetEvent(m_VoDuckingSnapshotPath);
                    if (duckDesc.IsValid)
                    {
                        m_VoDuckingSnapshotInstance = duckDesc.CreateInstance();
                    }
                }
                if (m_VoDuckingSnapshotInstance != null && m_VoDuckingSnapshotInstance.IsValid)
                {
                    m_VoDuckingSnapshotInstance.Start();
                    m_VoDuckingActive = true;
                }
            }
            catch (Exception)
            {
            }
        }

        /// <summary>
        /// Releases the VO ducking snapshot smoothly once the active dialogue voice finishes playing.
        /// </summary>
        public void UpdateVoDuckingLifecycle()
        {
            if (!m_VoDuckingActive)
                return;

            bool voStillPlaying = m_DialogueVoInstance != null
                && m_DialogueVoInstance.IsValid
                && m_DialogueVoInstance.PlaybackState != OaePlaybackState.Stopped;

            if (!voStillPlaying)
            {
                m_VoDuckingActive = false;
                try
                {
                    if (m_VoDuckingSnapshotInstance != null && m_VoDuckingSnapshotInstance.IsValid)
                    {
                        m_VoDuckingSnapshotInstance.Stop(OaeStopMode.AllowFadeout);
                    }
                }
                catch (Exception)
                {
                }
            }
        }

        /// <summary>
        /// Sets the linear volume level on Master, Music, or SFX playground VCAs.
        /// </summary>
        public void SetVcaLevel(string vcaKind, float linearLevel)
        {
            float clamped = Math.Max(0.0f, Math.Min(1.0f, linearLevel));
            string path = ResolveVcaPath(vcaKind);

            if (path == m_MasterVcaPath)
                MasterVcaLevel = clamped;
            else if (path == m_MusicVcaPath)
                MusicVcaLevel = clamped;
            else if (path == m_SfxVcaPath)
                SfxVcaLevel = clamped;

            if (m_System == null || !m_System.IsValid || string.IsNullOrEmpty(path))
                return;

            try
            {
                var vca = m_System.GetVca(path);
                if (vca.IsValid)
                {
                    vca.Volume = clamped;
                }
            }
            catch (Exception)
            {
            }
        }

        /// <summary>
        /// Fires a 2D UI click one-shot through bus:/ui.
        /// </summary>
        public void TriggerUiClick()
        {
            if (m_System == null || !m_System.IsValid)
                return;

            try
            {
                m_System.PlayOneShot(m_UiClickEventPath);
                UiClicksTriggered++;
            }
            catch (Exception)
            {
            }
        }

        /// <summary>
        /// Stops and releases all active event/snapshot instances and unloads the on-demand VO bank.
        /// </summary>
        public void ReleaseAllInstances()
        {
            StopAndReleaseInstance(ref m_FootstepInstance, OaeStopMode.Immediate);
            StopAndReleaseInstance(ref m_Emitter3DInstance, OaeStopMode.Immediate);
            StopAndReleaseInstance(ref m_AdaptiveMusicInstance, OaeStopMode.Immediate);
            StopAndReleaseInstance(ref m_ForestScattererInstance, OaeStopMode.Immediate);
            StopAndReleaseInstance(ref m_CaveSnapshotInstance, OaeStopMode.Immediate);
            StopAndReleaseInstance(ref m_VoDuckingSnapshotInstance, OaeStopMode.Immediate);
            StopAndReleaseInstance(ref m_DialogueVoInstance, OaeStopMode.Immediate);

            m_VoDuckingActive = false;
            CancelCameraCue();

            IsCombatZoneActive = false;
            IsForestZoneActive = false;
            IsCaveZoneActive = false;
            m_WasInCombatTrigger = false;
            m_WasInForestTrigger = false;
            m_WasInCaveTrigger = false;
            CombatIntensity = 0.0f;

            try
            {
                if (m_VoBank != null && m_VoBank.IsValid)
                {
                    m_VoBank.Unload();
                }
            }
            catch (Exception)
            {
            }
            finally
            {
                m_VoBank = null;
            }
        }

        private EventInstance GetActiveEmitter3DInstance()
        {
#if UNITY_5_3_OR_NEWER
            if (m_SceneEmitter3D != null && m_SceneEmitter3D.Instance != null && m_SceneEmitter3D.Instance.IsValid)
            {
                return m_SceneEmitter3D.Instance;
            }
#endif
            return m_Emitter3DInstance;
        }

        private string ResolveVcaPath(string vcaKind)
        {
            if (string.IsNullOrEmpty(vcaKind))
                return null;

            string key = vcaKind.Trim();
            if (key.StartsWith("vca:/", StringComparison.OrdinalIgnoreCase))
                return key;

            if (string.Equals(key, "Master", StringComparison.OrdinalIgnoreCase))
                return m_MasterVcaPath;
            if (string.Equals(key, "Music", StringComparison.OrdinalIgnoreCase))
                return m_MusicVcaPath;
            if (string.Equals(key, "Sfx", StringComparison.OrdinalIgnoreCase))
                return m_SfxVcaPath;

            return null;
        }

        private string ResolveVoBankPath(string explicitPath)
        {
            if (!string.IsNullOrEmpty(explicitPath) && File.Exists(explicitPath))
                return explicitPath;

#if UNITY_5_3_OR_NEWER
            string streamingCandidate = Path.Combine(Application.streamingAssetsPath, m_VoBankFileName);
            if (File.Exists(streamingCandidate))
                return streamingCandidate;
#endif
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string localCandidate = Path.Combine(baseDir, m_VoBankFileName);
            if (File.Exists(localCandidate))
                return localCandidate;

            var dir = new DirectoryInfo(baseDir);
            for (int i = 0; i < 8 && dir != null; i++)
            {
                string exampleCandidate = Path.Combine(dir.FullName, "example_project", m_VoBankFileName);
                if (File.Exists(exampleCandidate))
                    return exampleCandidate;
                dir = dir.Parent;
            }

            return localCandidate;
        }

        private static void StopAndReleaseInstance(ref EventInstance instance, OaeStopMode mode)
        {
            try
            {
                if (instance != null && instance.IsValid)
                {
                    try { instance.Stop(mode); } catch (Exception) { }
                    try { instance.Release(); } catch (Exception) { }
                }
            }
            catch (Exception)
            {
            }
            finally
            {
                instance = null;
            }
        }

        private static float ShortestAngleDeltaDeg(float fromDeg, float toDeg)
        {
            float diff = (toDeg - fromDeg) % 360.0f;
            if (diff > 180.0f) diff -= 360.0f;
            if (diff < -180.0f) diff += 360.0f;
            return diff;
        }

        private void UpdateZoneTriggersFromCoordinates(float posX, float posZ)
        {
            bool inCombat = posX <= -4.5f && posZ >= 3.5f && posZ <= 9.5f;
            if (inCombat != m_WasInCombatTrigger)
            {
                m_WasInCombatTrigger = inCombat;
                SetCombatZoneActive(inCombat);
            }

            bool inForest = posX >= 4.5f && posZ >= 3.5f && posZ <= 9.5f;
            if (inForest != m_WasInForestTrigger)
            {
                m_WasInForestTrigger = inForest;
                SetForestZoneActive(inForest);
            }

            bool inCave = Math.Abs(posX) <= 3.0f && posZ >= 11.0f && posZ <= 17.0f;
            if (inCave != m_WasInCaveTrigger)
            {
                m_WasInCaveTrigger = inCave;
                SetCaveZoneActive(inCave);
            }
        }

#if UNITY_5_3_OR_NEWER
        private Transform m_PlayerTransform;
        private StudioEventEmitter m_SceneEmitter3D;
        private GameObject m_RoomRoot;
        private AudioPlaygroundEnvironment m_Environment;
        private float m_StepTimer = 0.0f;
        private int m_NextDialogueIndex = 0;
        private string m_StatusMessage = "WASD move, Botao Direito gira, 1/2/3 passos, C arena, F bosque, V tunel, E dialogo VO.";
        private static readonly string[] DialogueKeys = new string[] { "vo_welcome", "vo_spatial_test", "vo_ducking_active" };

        /// <summary>Root GameObject created for the procedural 3D showcase room.</summary>
        public GameObject RoomRoot => m_RoomRoot;

        /// <summary>Scene StudioEventEmitter attached to the Station 2 3D totem.</summary>
        public StudioEventEmitter SceneEmitter3D => m_SceneEmitter3D;

        private IEnumerator Start()
        {
            if (m_System == null && OaeAudioManager.Instance != null)
            {
                while (!OaeAudioManager.Instance.IsInitialized)
                {
                    yield return null;
                }
                Initialize(OaeAudioManager.Instance.System);
            }

            var bankLoader = FindBankLoader();
            if (bankLoader != null)
            {
                while (!bankLoader.IsLoaded)
                {
                    yield return null;
                }
            }

            if (m_BuildProceduralRoom)
            {
                BuildProceduralRoomIfNeeded();
            }

            if (m_AutoStartAmbience)
            {
                StartPlaygroundAmbience();
            }
        }

        private void Update()
        {
            if (OaeAudioManager.Instance != null && OaeAudioManager.Instance.LiveClient != null)
            {
                IsLiveLinkConnected = OaeAudioManager.Instance.LiveClient.IsConnected;
            }
            else
            {
                IsLiveLinkConnected = false;
            }

            if (m_PlayerTransform != null)
            {
                UpdatePlayerMovementAndLook();
                UpdateZoneTriggersFromPosition(m_PlayerTransform.position);
            }

            HandleHotkeys();
            Refresh3DBuiltinParameters();
            UpdateVoDuckingLifecycle();
            m_Environment?.UpdateVisuals(Time.time, IsCombatZoneActive, IsForestZoneActive, IsCaveZoneActive, IsVoBankLoaded);
        }

        private void OnDestroy()
        {
            ReleaseAllInstances();
            m_Environment?.DisposeResources();
            m_Environment = null;
        }

        /// <summary>
        /// Builds the 3D showcase room geometry, procedural PBR materials, atmospheric lighting, and FPS listener if not yet created.
        /// </summary>
        public void BuildProceduralRoomIfNeeded()
        {
            if (m_RoomRoot != null)
                return;

            m_Environment = AudioPlaygroundEnvironmentBuilder.Build(
                m_Emitter3DEventPath,
                new Vector3(DefaultEmitterX, DefaultEmitterY, DefaultEmitterZ));
            m_RoomRoot = m_Environment.RoomRoot;
            m_SceneEmitter3D = m_Environment.SceneEmitter3D;

            SetupPlayerListenerCamera();
        }

        private void SetupPlayerListenerCamera()
        {
            Camera cam = Camera.main;
            if (cam == null)
            {
#if UNITY_2023_1_OR_NEWER
                cam = UnityEngine.Object.FindFirstObjectByType<Camera>();
#else
                cam = UnityEngine.Object.FindObjectOfType<Camera>();
#endif
            }

            if (cam == null)
            {
                var camGo = new GameObject("PlaygroundPlayerCamera");
                cam = camGo.AddComponent<Camera>();
                cam.tag = "MainCamera";
            }

            m_PlayerTransform = cam.transform;
            m_PlayerTransform.position = new Vector3(0.0f, 1.6f, -4.0f);
            m_PlayerTransform.rotation = Quaternion.Euler(m_PitchDegrees, m_YawDegrees, 0.0f);
            PlayerPosX = m_PlayerTransform.position.x;
            PlayerPosY = m_PlayerTransform.position.y;
            PlayerPosZ = m_PlayerTransform.position.z;

            var listener = cam.GetComponent<StudioListener>();
            if (listener == null)
            {
                listener = cam.gameObject.AddComponent<StudioListener>();
            }
            listener.SyncToNative();
        }

        private void UpdatePlayerMovementAndLook()
        {
            bool manualInput = Input.GetMouseButton(1)
                || Input.GetKey(KeyCode.A) || Input.GetKey(KeyCode.LeftArrow)
                || Input.GetKey(KeyCode.D) || Input.GetKey(KeyCode.RightArrow)
                || Input.GetKey(KeyCode.W) || Input.GetKey(KeyCode.UpArrow)
                || Input.GetKey(KeyCode.S) || Input.GetKey(KeyCode.DownArrow);

            if (IsCameraCueActive)
            {
                if (manualInput)
                {
                    CancelCameraCue();
                }
                else
                {
                    StepCameraCue(Time.deltaTime);
                    return;
                }
            }

            if (Input.GetMouseButton(1))
            {
                m_YawDegrees += Input.GetAxis("Mouse X") * m_LookSensitivity;
                m_PitchDegrees = Mathf.Clamp(m_PitchDegrees - Input.GetAxis("Mouse Y") * m_LookSensitivity, -65.0f, 65.0f);
                m_PlayerTransform.rotation = Quaternion.Euler(m_PitchDegrees, m_YawDegrees, 0.0f);
            }

            float h = 0.0f;
            float v = 0.0f;
            if (Input.GetKey(KeyCode.A) || Input.GetKey(KeyCode.LeftArrow)) h -= 1.0f;
            if (Input.GetKey(KeyCode.D) || Input.GetKey(KeyCode.RightArrow)) h += 1.0f;
            if (Input.GetKey(KeyCode.W) || Input.GetKey(KeyCode.UpArrow)) v += 1.0f;
            if (Input.GetKey(KeyCode.S) || Input.GetKey(KeyCode.DownArrow)) v -= 1.0f;

            Vector3 forwardFlat = Vector3.ProjectOnPlane(m_PlayerTransform.forward, Vector3.up).normalized;
            if (forwardFlat.sqrMagnitude < 1e-4f) forwardFlat = Vector3.forward;
            Vector3 rightFlat = Vector3.ProjectOnPlane(m_PlayerTransform.right, Vector3.up).normalized;

            Vector3 moveDir = (forwardFlat * v + rightFlat * h);
            if (moveDir.sqrMagnitude > 1e-4f)
            {
                moveDir.Normalize();
                Vector3 nextPos = m_PlayerTransform.position + moveDir * (m_MoveSpeed * Time.deltaTime);
                nextPos.x = Mathf.Clamp(nextPos.x, -12.0f, 12.0f);
                nextPos.z = Mathf.Clamp(nextPos.z, -7.0f, 17.0f);
                nextPos.y = 1.6f;
                m_PlayerTransform.position = nextPos;
                PlayerPosX = nextPos.x;
                PlayerPosY = nextPos.y;
                PlayerPosZ = nextPos.z;

                PlaygroundSurface detected = DetectSurfaceAtPosition(nextPos);
                m_StepTimer += Time.deltaTime;
                if (m_StepTimer >= m_StepIntervalSeconds)
                {
                    m_StepTimer = 0.0f;
                    SelectSurfaceAndStep(detected);
                }
            }
            else
            {
                m_StepTimer = m_StepIntervalSeconds * 0.8f;
            }
        }

        private PlaygroundSurface DetectSurfaceAtPosition(Vector3 pos)
        {
            return DetectSurfaceAtX(pos.x);
        }

        private void UpdateZoneTriggersFromPosition(Vector3 pos)
        {
            UpdateZoneTriggersFromCoordinates(pos.x, pos.z);
        }

        private void HandleHotkeys()
        {
            if (Input.GetKeyDown(KeyCode.Alpha1))
                SelectSurfaceAndStep(PlaygroundSurface.Wood);
            else if (Input.GetKeyDown(KeyCode.Alpha2))
                SelectSurfaceAndStep(PlaygroundSurface.Concrete);
            else if (Input.GetKeyDown(KeyCode.Alpha3))
                SelectSurfaceAndStep(PlaygroundSurface.Grass);
            else if (Input.GetKeyDown(KeyCode.C))
                SetCombatZoneActive(!IsCombatZoneActive);
            else if (Input.GetKeyDown(KeyCode.F))
                SetForestZoneActive(!IsForestZoneActive);
            else if (Input.GetKeyDown(KeyCode.V))
                SetCaveZoneActive(!IsCaveZoneActive);
            else if (Input.GetKeyDown(KeyCode.E))
            {
                string key = DialogueKeys[m_NextDialogueIndex % DialogueKeys.Length];
                m_NextDialogueIndex++;
                TriggerProgrammerDialogue(key);
            }
        }

        private void OnGUI()
        {
            if (!m_ShowHud)
                return;

            const float width = 420.0f;
            const float height = 390.0f;
            GUI.Box(new Rect(14.0f, 14.0f, width, height), GUIContent.none);
            GUILayout.BeginArea(new Rect(24.0f, 22.0f, width - 20.0f, height - 16.0f));

            GUILayout.Label("<b>OpenAudio Engine — Audio Playground 3D</b>");
            GUILayout.Label($"<b>Status:</b> {m_StatusMessage}");
            GUILayout.Space(4.0f);

            GUILayout.Label(
                $"<b>Estacao 2 (3D):</b> Dist={LastDistance:F1}m | Dir={LastDirection:F0}° | " +
                $"Elev={LastElevation:F0}° | Cone={LastEventConeAngle:F0}° | Speed={LastSpeed:F1}m/s");

            GUILayout.BeginHorizontal();
            if (GUILayout.Button("1: Madeira")) SelectSurfaceAndStep(PlaygroundSurface.Wood);
            if (GUILayout.Button("2: Concreto")) SelectSurfaceAndStep(PlaygroundSurface.Concrete);
            if (GUILayout.Button("3: Grama")) SelectSurfaceAndStep(PlaygroundSurface.Grass);
            GUILayout.EndHorizontal();

            GUILayout.BeginHorizontal();
            if (GUILayout.Button(IsCombatZoneActive ? "Arena: COMBATE" : "Arena: Exploracao"))
                SetCombatZoneActive(!IsCombatZoneActive);
            if (GUILayout.Button(IsForestZoneActive ? "Bosque: ATIVO" : "Bosque: OFF"))
                SetForestZoneActive(!IsForestZoneActive);
            if (GUILayout.Button(IsCaveZoneActive ? "Tunel: ATIVO" : "Tunel: OFF"))
                SetCaveZoneActive(!IsCaveZoneActive);
            GUILayout.EndHorizontal();

            GUILayout.BeginHorizontal();
            if (GUILayout.Button("Falar VO (Programmer + Banco)"))
            {
                string key = DialogueKeys[m_NextDialogueIndex % DialogueKeys.Length];
                m_NextDialogueIndex++;
                TriggerProgrammerDialogue(key);
            }
            if (GUILayout.Button("UI Click 2D"))
            {
                TriggerUiClick();
            }
            GUILayout.EndHorizontal();

            GUILayout.Space(6.0f);
            DrawVcaSlider("VCA Master", "Master", MasterVcaLevel);
            DrawVcaSlider("VCA Musica", "Music", MusicVcaLevel);
            DrawVcaSlider("VCA SFX", "Sfx", SfxVcaLevel);

            GUILayout.Space(4.0f);
            string liveStatus = IsLiveLinkConnected ? "Conectado (127.0.0.1:7450)" : "Desconectado";
            GUILayout.Label($"<b>Live-Linking Studio:</b> {liveStatus} | <b>VO Bank:</b> {(IsVoBankLoaded ? "Carregado" : "Sob Demanda")}");

            GUILayout.EndArea();
        }

        private void DrawVcaSlider(string label, string vcaKind, float currentLevel)
        {
            GUILayout.BeginHorizontal();
            GUILayout.Label(label, GUILayout.Width(85.0f));
            float next = GUILayout.HorizontalSlider(currentLevel, 0.0f, 1.0f, GUILayout.Width(230.0f));
            if (Math.Abs(next - currentLevel) > 0.005f)
            {
                SetVcaLevel(vcaKind, next);
            }
            GUILayout.Label($"{next * 100.0f:F0}%", GUILayout.Width(45.0f));
            GUILayout.EndHorizontal();
        }

        private StudioBankLoader FindBankLoader()
        {
            var loader = GetComponent<StudioBankLoader>();
            if (loader != null)
                return loader;

#if UNITY_2023_1_OR_NEWER
            return UnityEngine.Object.FindFirstObjectByType<StudioBankLoader>();
#else
            return UnityEngine.Object.FindObjectOfType<StudioBankLoader>();
#endif
        }
#endif
    }
#if !UNITY_5_3_OR_NEWER
#pragma warning restore CS0414
#endif
}
