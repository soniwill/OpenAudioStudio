using System;
using System.Collections.Generic;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace OpenAudio
{
    [Serializable]
    public struct OaeBusTelemetry
    {
        public string Path;
        public float PeakDb;
        public float RmsDb;
        public float K14Vu;
    }

    public sealed class OaeDiagnosticsSnapshot
    {
        public int TotalVoices;
        public int RealVoices;
        public int VirtualVoices => Math.Max(0, TotalVoices - RealVoices);
        public List<string> LoadedBanks { get; } = new List<string>();
        public List<string> ActiveEvents { get; } = new List<string>();
        public List<OaeBusTelemetry> BusMeters { get; } = new List<OaeBusTelemetry>();

        public static float LinearToDb(float linear)
        {
            if (float.IsNaN(linear) || float.IsInfinity(linear) || linear <= 0.0001f)
                return -80.0f;
            return Math.Max(-80.0f, 20.0f * (float)Math.Log10(linear));
        }

        public static OaeDiagnosticsSnapshot Capture(
            OaeSystem system,
            IEnumerable<string> bankNames = null,
            IEnumerable<EventDescription> watchedDescriptions = null,
            IEnumerable<string> busPaths = null)
        {
            var snap = new OaeDiagnosticsSnapshot();
            if (system == null || !system.IsValid)
                return snap;

            system.GetChannelsPlaying(out snap.TotalVoices, out snap.RealVoices);

            if (bankNames != null)
            {
                foreach (var b in bankNames)
                {
                    if (!string.IsNullOrEmpty(b))
                        snap.LoadedBanks.Add(b);
                }
            }

            if (watchedDescriptions != null)
            {
                foreach (var desc in watchedDescriptions)
                {
                    if (desc == null || !desc.IsValid) continue;
                    try
                    {
                        int count = desc.InstanceCount;
                        if (count > 0)
                            snap.ActiveEvents.Add($"event ({count})");
                    }
                    catch (OaeException) { }
                }
            }

            var buses = busPaths ?? new[] { "bus:/", "bus:/music", "bus:/sfx", "bus:/ui", "bus:/ambience" };
            foreach (var busPath in buses)
            {
                if (string.IsNullOrEmpty(busPath)) continue;
                try
                {
                    var bus = system.GetBus(busPath);
                    unsafe
                    {
                        var meter = bus.GetMeter();
                        float peakDb = LinearToDb(meter.peak[0]);
                        float rmsDb = LinearToDb(meter.rms[0]);
                        snap.BusMeters.Add(new OaeBusTelemetry
                        {
                            Path = busPath,
                            PeakDb = peakDb,
                            RmsDb = rmsDb,
                            K14Vu = rmsDb + 14.0f
                        });
                    }
                }
                catch (OaeException) { }
            }

            return snap;
        }
    }

#if UNITY_5_3_OR_NEWER
    [AddComponentMenu("OpenAudio/OpenAudio Diagnostics Overlay")]
    public class OaeDiagnosticsOverlay : MonoBehaviour
    {
        [SerializeField] private bool m_Visible = true;
        [SerializeField] private KeyCode m_ToggleKey = KeyCode.F9;
        [SerializeField] private List<string> m_WatchedEvents = new List<string>();
        [SerializeField] private List<string> m_WatchedBuses = new List<string>
        {
            "bus:/", "bus:/music", "bus:/sfx", "bus:/ui", "bus:/ambience"
        };

        private readonly List<string> m_LoadedBankNames = new List<string>();
        private OaeDiagnosticsSnapshot m_LastSnapshot;

        public bool Visible
        {
            get => m_Visible;
            set => m_Visible = value;
        }

        public OaeDiagnosticsSnapshot LastSnapshot => m_LastSnapshot;

        public void RegisterBankName(string bankName)
        {
            if (!string.IsNullOrEmpty(bankName) && !m_LoadedBankNames.Contains(bankName))
                m_LoadedBankNames.Add(bankName);
        }

        public void UnregisterBankName(string bankName)
        {
            m_LoadedBankNames.Remove(bankName);
        }

        private void Update()
        {
            if (Input.GetKeyDown(m_ToggleKey))
                m_Visible = !m_Visible;

            var mgr = OaeAudioManager.Instance;
            if (mgr != null && mgr.IsInitialized)
            {
                var descs = new List<EventDescription>();
                foreach (var p in m_WatchedEvents)
                {
                    if (string.IsNullOrEmpty(p)) continue;
                    try { descs.Add(mgr.System.GetEvent(p)); } catch (OaeException) {}
                }
                m_LastSnapshot = OaeDiagnosticsSnapshot.Capture(
                    mgr.System, m_LoadedBankNames, descs, m_WatchedBuses);
            }
        }

        private void OnGUI()
        {
            if (!m_Visible || m_LastSnapshot == null)
                return;

            if (!Debug.isDebugBuild && !Application.isEditor)
                return;

            GUI.Box(new Rect(Screen.width - 330, 10, 320, 260), "OpenAudio Diagnostics (F9)");
            GUILayout.BeginArea(new Rect(Screen.width - 320, 34, 300, 230));
            GUILayout.Label($"Voices: {m_LastSnapshot.RealVoices} Real / {m_LastSnapshot.VirtualVoices} Virtual (Total {m_LastSnapshot.TotalVoices})");
            GUILayout.Label("Banks: " + (m_LastSnapshot.LoadedBanks.Count > 0 ? string.Join(", ", m_LastSnapshot.LoadedBanks) : "none"));
            GUILayout.Label("Active Events: " + (m_LastSnapshot.ActiveEvents.Count > 0 ? string.Join(", ", m_LastSnapshot.ActiveEvents) : "0"));
            GUILayout.Space(4);
            foreach (var bm in m_LastSnapshot.BusMeters)
            {
                GUILayout.Label($"{bm.Path}: Peak {bm.PeakDb:F1} dBFS | RMS {bm.RmsDb:F1} dBFS (K-14: {bm.K14Vu:+0.0;-0.0;0.0} VU)");
            }
            GUILayout.EndArea();
        }
    }
#endif
}
