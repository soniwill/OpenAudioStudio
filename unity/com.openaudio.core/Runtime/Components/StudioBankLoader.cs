using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace OpenAudio
{
    public enum BankLoadTrigger
    {
        Awake,
        Start,
        Manual
    }

    public enum BankUnloadTrigger
    {
        OnDestroy,
        Manual
    }

    public enum BankLoadMode
    {
        Synchronous,
        Asynchronous
    }

#if UNITY_5_3_OR_NEWER
    [AddComponentMenu("OpenAudio/OpenAudio Studio Bank Loader")]
    public class StudioBankLoader : MonoBehaviour
#else
#pragma warning disable CS0414
    public class StudioBankLoader
#endif
    {
#if UNITY_5_3_OR_NEWER
        [Header("Bank Settings")]
        [SerializeField] private string[] m_BankFiles = new string[0];
        [SerializeField] private BankLoadMode m_LoadMode = BankLoadMode.Synchronous;
        [SerializeField] private BankLoadTrigger m_LoadTrigger = BankLoadTrigger.Awake;
        [SerializeField] private BankUnloadTrigger m_UnloadTrigger = BankUnloadTrigger.OnDestroy;
        [SerializeField] private bool m_PreloadSampleData = false;
#else
        private string[] m_BankFiles = new string[0];
        private BankLoadMode m_LoadMode = BankLoadMode.Synchronous;
        private BankLoadTrigger m_LoadTrigger = BankLoadTrigger.Awake;
        private BankUnloadTrigger m_UnloadTrigger = BankUnloadTrigger.OnDestroy;
        private bool m_PreloadSampleData = false;
#endif

        private readonly List<Bank> m_LoadedBanks = new List<Bank>();

        public IReadOnlyList<Bank> LoadedBanks => m_LoadedBanks;
        public bool IsLoaded { get; private set; }

#if UNITY_5_3_OR_NEWER
        private void Awake()
        {
            if (m_LoadTrigger == BankLoadTrigger.Awake)
            {
                Load();
            }
        }

        private void Start()
        {
            if (m_LoadTrigger == BankLoadTrigger.Start)
            {
                Load();
            }
        }

        private void OnDestroy()
        {
            if (m_UnloadTrigger == BankUnloadTrigger.OnDestroy)
            {
                Unload();
            }
        }
#endif

        public void Load()
        {
            if (IsLoaded)
                return;

            var sys = OaeAudioManager.Instance != null ? OaeAudioManager.Instance.System : null;
            if (sys == null || !sys.IsValid)
            {
#if UNITY_5_3_OR_NEWER
                Debug.LogWarning("[OpenAudio] Cannot load banks: OaeAudioManager is not initialized.");
#endif
                return;
            }

            foreach (var bankFile in m_BankFiles)
            {
                if (string.IsNullOrWhiteSpace(bankFile))
                    continue;

                string resolvedPath = ResolveBankPath(bankFile);
                if (!File.Exists(resolvedPath))
                {
#if UNITY_5_3_OR_NEWER
                    Debug.LogError($"[OpenAudio] Bank file not found: {resolvedPath}");
#endif
                    continue;
                }

                try
                {
                    if (m_LoadMode == BankLoadMode.Synchronous)
                    {
                        var bank = sys.LoadBankFile(resolvedPath);
                        m_LoadedBanks.Add(bank);
                    }
                    else
                    {
                        var bank = sys.LoadBankFileAsync(resolvedPath);
                        m_LoadedBanks.Add(bank);
#if UNITY_5_3_OR_NEWER
                        StartCoroutine(WaitForBankLoadAsync(bank, bankFile));
#endif
                    }
                }
                catch (Exception ex)
                {
#if UNITY_5_3_OR_NEWER
                    Debug.LogError($"[OpenAudio] Failed to load bank '{bankFile}': {ex}");
#else
                    Console.Error.WriteLine($"[OpenAudio] Failed to load bank '{bankFile}': {ex}");
#endif
                }
            }

            if (m_LoadMode == BankLoadMode.Synchronous)
            {
                IsLoaded = true;
            }
        }

#if UNITY_5_3_OR_NEWER
        private IEnumerator WaitForBankLoadAsync(Bank bank, string bankName)
        {
            while (bank.LoadingState == OaeBankLoadingState.Loading)
            {
                yield return null;
            }

            if (bank.LoadingState == OaeBankLoadingState.Loaded)
            {
                IsLoaded = true;
                Debug.Log($"[OpenAudio] Bank loaded asynchronously: {bankName}");
            }
            else
            {
                Debug.LogError($"[OpenAudio] Asynchronous loading failed for bank: {bankName}");
            }
        }
#endif

        public void Unload()
        {
            foreach (var bank in m_LoadedBanks)
            {
                try
                {
                    bank?.Unload();
                }
                catch (Exception ex)
                {
#if UNITY_5_3_OR_NEWER
                    Debug.LogError($"[OpenAudio] Error unloading bank: {ex}");
#else
                    Console.Error.WriteLine($"[OpenAudio] Error unloading bank: {ex}");
#endif
                }
            }

            m_LoadedBanks.Clear();
            IsLoaded = false;
        }

        private string ResolveBankPath(string path)
        {
            if (Path.IsPathRooted(path))
                return path;

#if UNITY_5_3_OR_NEWER
            return Path.Combine(Application.streamingAssetsPath, path);
#else
            return Path.GetFullPath(path);
#endif
        }
    }
#if !UNITY_5_3_OR_NEWER
#pragma warning restore CS0414
#endif
}
