using System;
using System.Collections.Generic;
using System.Threading.Tasks;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#if OAE_UNITY_ADDRESSABLES
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;
#endif
#endif

namespace OpenAudio
{
    public struct OaeBankPayload
    {
        public byte[] BankBytes;
        public uint? ExpectedCrc32;
    }

    public delegate Task<OaeBankPayload> OaeBankPayloadLoader(string bankKey);

    /// <summary>
    /// Reference-counted asynchronous bank loader for AssetBundles, Addressables,
    /// and custom memory providers. Validates CRC-32 when provided and unloads only
    /// when the reference count reaches zero.
    /// </summary>
    public sealed class OaeBankReferenceManager : IDisposable
    {
        private sealed class Entry
        {
            public Bank Bank;
            public int RefCount;
            public Task<Bank> PendingTask;
        }

        private readonly OaeSystem m_System;
        private readonly Dictionary<string, Entry> m_Entries = new Dictionary<string, Entry>(StringComparer.Ordinal);
        private readonly object m_Lock = new object();

        public OaeBankReferenceManager(OaeSystem system)
        {
            m_System = system ?? throw new ArgumentNullException(nameof(system));
        }

        public int GetRefCount(string bankKey)
        {
            lock (m_Lock)
            {
                return m_Entries.TryGetValue(bankKey, out var entry) ? entry.RefCount : 0;
            }
        }

        public Task<Bank> AcquireBankAsync(string bankKey, OaeBankPayloadLoader loader)
        {
            if (string.IsNullOrEmpty(bankKey))
                throw new ArgumentException("Bank key cannot be null or empty.", nameof(bankKey));
            if (loader == null)
                throw new ArgumentNullException(nameof(loader));

            lock (m_Lock)
            {
                if (m_Entries.TryGetValue(bankKey, out var existing))
                {
                    existing.RefCount++;
                    if (existing.Bank != null)
                        return Task.FromResult(existing.Bank);
                    return existing.PendingTask;
                }

                var entry = new Entry { RefCount = 1 };
                entry.PendingTask = LoadInternalAsync(bankKey, entry, loader);
                m_Entries[bankKey] = entry;
                return entry.PendingTask;
            }
        }

        private async Task<Bank> LoadInternalAsync(string bankKey, Entry entry, OaeBankPayloadLoader loader)
        {
            try
            {
                var payload = await loader(bankKey).ConfigureAwait(false);
                if (payload.BankBytes == null || payload.BankBytes.Length == 0)
                    throw new InvalidOperationException($"Empty bank payload for '{bankKey}'.");

                if (payload.ExpectedCrc32.HasValue)
                {
                    uint actualCrc = ComputeCrc32(payload.BankBytes);
                    if (actualCrc != payload.ExpectedCrc32.Value)
                        throw new InvalidOperationException(
                            $"CRC-32 mismatch for '{bankKey}': expected 0x{payload.ExpectedCrc32.Value:x8}, got 0x{actualCrc:x8}.");
                }

                var bank = m_System.LoadBankMemory(payload.BankBytes);
                lock (m_Lock)
                {
                    if (entry.RefCount <= 0)
                    {
                        bank.Unload();
                        m_Entries.Remove(bankKey);
                        return null;
                    }
                    entry.Bank = bank;
                    entry.PendingTask = null;
                }
                return bank;
            }
            catch
            {
                lock (m_Lock)
                {
                    m_Entries.Remove(bankKey);
                }
                throw;
            }
        }

        public void ReleaseBank(string bankKey)
        {
            if (string.IsNullOrEmpty(bankKey))
                return;

            Bank toUnload = null;
            lock (m_Lock)
            {
                if (!m_Entries.TryGetValue(bankKey, out var entry))
                    return;

                entry.RefCount--;
                if (entry.RefCount <= 0)
                {
                    toUnload = entry.Bank;
                    if (entry.PendingTask == null)
                    {
                        m_Entries.Remove(bankKey);
                    }
                }
            }

            if (toUnload != null)
            {
                toUnload.Unload();
                toUnload.Dispose();
            }
        }

        public void Dispose()
        {
            List<Bank> banks = new List<Bank>();
            lock (m_Lock)
            {
                foreach (var kv in m_Entries)
                {
                    kv.Value.RefCount = 0;
                    if (kv.Value.Bank != null)
                        banks.Add(kv.Value.Bank);
                }
                m_Entries.Clear();
            }

            for (int i = 0; i < banks.Count; i++)
            {
                banks[i].Unload();
                banks[i].Dispose();
            }
        }

        public static uint ComputeCrc32(byte[] data)
        {
            uint crc = 0xFFFFFFFFu;
            for (int i = 0; i < data.Length; i++)
            {
                crc ^= data[i];
                for (int b = 0; b < 8; b++)
                {
                    uint mask = (uint)-(int)(crc & 1u);
                    crc = (crc >> 1) ^ (0xEDB88320u & mask);
                }
            }
            return ~crc;
        }
    }

#if UNITY_5_3_OR_NEWER
    public static class OaeAssetBundleBankProvider
    {
        public static Task<Bank> LoadFromAssetBundleAsync(
            OaeBankReferenceManager manager, AssetBundle bundle, string assetName)
        {
            if (manager == null) throw new ArgumentNullException(nameof(manager));
            if (bundle == null) throw new ArgumentNullException(nameof(bundle));

            return manager.AcquireBankAsync($"{bundle.name}:{assetName}", async _ =>
            {
                var tcs = new TaskCompletionSource<byte[]>();
                var req = bundle.LoadAssetAsync<TextAsset>(assetName);
                req.completed += _op =>
                {
                    var ta = req.asset as TextAsset;
                    if (ta == null)
                        tcs.TrySetException(new InvalidOperationException($"Asset '{assetName}' not found in AssetBundle."));
                    else
                        tcs.TrySetResult(ta.bytes);
                };
                var bytes = await tcs.Task;
                return new OaeBankPayload { BankBytes = bytes };
            });
        }
    }

#if OAE_UNITY_ADDRESSABLES
    public static class OaeAddressablesBankProvider
    {
        public static Task<Bank> LoadFromAddressablesAsync(
            OaeBankReferenceManager manager, string addressKey)
        {
            if (manager == null) throw new ArgumentNullException(nameof(manager));
            return manager.AcquireBankAsync(addressKey, async key =>
            {
                AsyncOperationHandle<TextAsset> handle = Addressables.LoadAssetAsync<TextAsset>(key);
                await handle.Task;
                if (handle.Status != AsyncOperationStatus.Succeeded || handle.Result == null)
                    throw new InvalidOperationException($"Failed to load addressable bank '{key}'.");

                byte[] copy = new byte[handle.Result.bytes.Length];
                Buffer.BlockCopy(handle.Result.bytes, 0, copy, 0, copy.Length);
                Addressables.Release(handle);
                return new OaeBankPayload { BankBytes = copy };
            });
        }
    }
#endif
#endif
}
