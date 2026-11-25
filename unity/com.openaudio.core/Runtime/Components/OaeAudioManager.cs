using System;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace OpenAudio
{
#if UNITY_5_3_OR_NEWER
    [DefaultExecutionOrder(-100)]
    [AddComponentMenu("OpenAudio/OpenAudio Audio Manager")]
    public class OaeAudioManager : MonoBehaviour
#else
#pragma warning disable CS0414
    public class OaeAudioManager
#endif
    {
        public static OaeAudioManager Instance { get; private set; }

#if UNITY_5_3_OR_NEWER
        [Header("Audio Settings")]
        [SerializeField] private uint m_SampleRate = 48000;
        [SerializeField] private uint m_Channels = 2;
        [SerializeField] private uint m_MaxPhysicalVoices = 64;
        [SerializeField] private bool m_InitializeNoDevice = false;

        [Header("Lifecycle & Focus")]
        [SerializeField] private bool m_PauseOnFocusLoss = false;

        [Header("Live-Linking (Studio Bridge)")]
        [SerializeField] private bool m_EnableLiveLinking = true;
        [SerializeField] private string m_LiveHost = "127.0.0.1";
        [SerializeField] private int m_LivePort = LiveClient.DefaultPort;
#else
        private uint m_SampleRate = 48000;
        private uint m_Channels = 2;
        private uint m_MaxPhysicalVoices = 64;
        private bool m_InitializeNoDevice = false;
        private bool m_PauseOnFocusLoss = false;
        private bool m_EnableLiveLinking = true;
        private string m_LiveHost = "127.0.0.1";
        private int m_LivePort = LiveClient.DefaultPort;
#endif

        public OaeSystem System { get; private set; }
        public Bus MasterBus { get; private set; }
        public LiveClient LiveClient { get; private set; }

        public bool IsInitialized => System != null && System.IsValid;

#if UNITY_5_3_OR_NEWER
        private void Awake()
        {
            if (Instance != null && Instance != this)
            {
                Destroy(gameObject);
                return;
            }

            Instance = this;
            transform.SetParent(null);
            DontDestroyOnLoad(gameObject);

            InitializeEngine();
        }

        private void Update()
        {
            LiveClient?.Update();
            if (System != null && System.IsValid)
            {
                System.Update();
            }
        }

        private void OnApplicationFocus(bool hasFocus)
        {
            if (m_PauseOnFocusLoss && MasterBus != null && MasterBus.IsValid)
            {
                MasterBus.Paused = !hasFocus;
            }
        }

        private void OnApplicationPause(bool pauseStatus)
        {
            if (m_PauseOnFocusLoss && MasterBus != null && MasterBus.IsValid)
            {
                MasterBus.Paused = pauseStatus;
            }
        }

        private void OnDestroy()
        {
            if (Instance == this)
            {
                ShutdownEngine();
                Instance = null;
            }
        }
#endif

        public void InitializeEngine()
        {
            if (System != null && System.IsValid)
                return;

            try
            {
                OaeNative.VerifyStructSizes();
                System = OaeSystem.Create();
                var flags = m_InitializeNoDevice ? OaeInitFlags.NoDevice : OaeInitFlags.Default;
                System.Init(m_SampleRate, m_Channels, m_MaxPhysicalVoices, flags);
                MasterBus = System.GetMasterBus();
#if UNITY_EDITOR || DEVELOPMENT_BUILD
                if (m_EnableLiveLinking && Application.isPlaying)
                {
                    EnableLiveLinking(m_LiveHost, m_LivePort);
                }
#endif
#if UNITY_5_3_OR_NEWER
                Debug.Log($"[OpenAudio] OaeSystem initialized successfully (Rate: {m_SampleRate}Hz, Channels: {m_Channels}, MaxVoices: {m_MaxPhysicalVoices})");
#endif
            }
            catch (Exception ex)
            {
#if UNITY_5_3_OR_NEWER
                Debug.LogError($"[OpenAudio] Failed to initialize OaeSystem: {ex}");
#else
                Console.Error.WriteLine($"[OpenAudio] Failed to initialize OaeSystem: {ex}");
#endif
                throw;
            }
        }

        public void EnableLiveLinking(string host = "127.0.0.1", int port = LiveClient.DefaultPort, uint bankCrc32 = 0)
        {
            if (System == null || !System.IsValid) return;
            LiveClient?.Dispose();
            LiveClient = new LiveClient(System);
            LiveClient.ConnectAsync(host, port, bankCrc32);
        }

        public void ShutdownEngine()
        {
            if (LiveClient != null)
            {
                LiveClient.Dispose();
                LiveClient = null;
            }
            if (System != null)
            {
                try
                {
                    System.Release();
                }
                catch (Exception ex)
                {
#if UNITY_5_3_OR_NEWER
                    Debug.LogError($"[OpenAudio] Error releasing OaeSystem: {ex}");
#else
                    Console.Error.WriteLine($"[OpenAudio] Error releasing OaeSystem: {ex}");
#endif
                }
                finally
                {
                    System = null;
                    MasterBus = null;
                }
            }
        }
    }
#if !UNITY_5_3_OR_NEWER
#pragma warning restore CS0414
#endif
}
