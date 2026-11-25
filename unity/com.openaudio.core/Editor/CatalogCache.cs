using System;
using System.Collections.Generic;
using System.IO;

namespace OpenAudio.Editor
{
    [Serializable]
    public class CatalogEventEntry
    {
        public string path;
        public string kind;
    }

    [Serializable]
    public class BankCatalogData
    {
        public int version;
        public long bank_size;
        public uint bank_crc32;
        public List<CatalogEventEntry> events = new List<CatalogEventEntry>();
    }

    public class CachedCatalog
    {
        public string CatalogPath { get; set; }
        public string BankPath { get; set; }
        public DateTime LastWriteTimeUtc { get; set; }
        public long FileSize { get; set; }
        public bool IsCrcValid { get; set; }
        public List<string> EventPaths { get; set; } = new List<string>();
        public List<string> SnapshotPaths { get; set; } = new List<string>();
    }

    public static class CatalogCache
    {
        private const string CatalogSuffix = ".catalog.json";
        private const FileShare PublicationSharing = FileShare.ReadWrite | FileShare.Delete;
        private static readonly Dictionary<string, (string Text, BankCatalogData Data)> s_Cache =
            new Dictionary<string, (string, BankCatalogData)>();

        public static void Clear()
        {
            s_Cache.Clear();
        }

        public static uint CalculateCrc32(byte[] bytes, int offset = 0, int count = -1)
        {
            if (bytes == null) return 0;
            if (count < 0) count = bytes.Length - offset;
            return ~UpdateCrc32(0xFFFFFFFFu, bytes, offset, count);
        }

        private static uint UpdateCrc32(uint crc, byte[] bytes, int offset, int count)
        {
            for (int i = offset; i < offset + count; ++i)
            {
                crc ^= bytes[i];
                for (int j = 0; j < 8; ++j)
                {
                    uint mask = (uint)-(int)(crc & 1);
                    crc = (crc >> 1) ^ (0xEDB88320u & mask);
                }
            }
            return crc;
        }

        public static uint CalculateFileCrc32(string filePath)
        {
            if (!File.Exists(filePath)) return 0;
            using (var input = OpenForRead(filePath))
            {
                return CalculateStreamCrc32(input, out _);
            }
        }

        private static FileStream OpenForRead(string path)
        {
            return new FileStream(path, FileMode.Open, FileAccess.Read, PublicationSharing);
        }

        private static uint CalculateStreamCrc32(Stream input, out long size)
        {
            var buffer = new byte[64 * 1024];
            uint crc = 0xFFFFFFFFu;
            size = 0;
            int read;
            while ((read = input.Read(buffer, 0, buffer.Length)) != 0)
            {
                crc = UpdateCrc32(crc, buffer, 0, read);
                size += read;
            }
            return ~crc;
        }

        /// <summary>Reads a catalog and offers paths only when the corresponding bank bytes match.</summary>
        public static CachedCatalog GetOrLoadCatalog(string catalogPath)
        {
            if (string.IsNullOrEmpty(catalogPath) ||
                !catalogPath.EndsWith(CatalogSuffix, StringComparison.OrdinalIgnoreCase))
                return null;

            string bankPath = catalogPath.Substring(0, catalogPath.Length - CatalogSuffix.Length);
            var result = new CachedCatalog { CatalogPath = catalogPath, BankPath = bankPath };
            BankCatalogData data;
            try
            {
                string text;
                using (var input = OpenForRead(catalogPath))
                using (var reader = new StreamReader(input))
                {
                    result.FileSize = input.Length;
                    text = reader.ReadToEnd();
                }
                result.LastWriteTimeUtc = File.GetLastWriteTimeUtc(catalogPath);
                if (!s_Cache.TryGetValue(catalogPath, out var parsed) || parsed.Text != text)
                {
                    parsed = (text, ParseCatalogJson(text));
                    s_Cache[catalogPath] = parsed;
                }

                data = parsed.Data;
                if (data.version != 1 || data.bank_size <= 0) return result;
                using (var bank = OpenForRead(bankPath))
                {
                    if (bank.Length != data.bank_size) return result;
                    uint crc = CalculateStreamCrc32(bank, out long size);
                    result.IsCrcValid = size == data.bank_size && crc == data.bank_crc32;
                }
            }
            catch (FileNotFoundException ex)
            {
                if (ex.FileName == catalogPath) s_Cache.Remove(catalogPath);
                return result;
            }
            catch (IOException)
            {
                return result;
            }
            catch (UnauthorizedAccessException)
            {
                return result;
            }

            if (!result.IsCrcValid || data.events == null) return result;
            foreach (var entry in data.events)
            {
                if (entry == null || string.IsNullOrEmpty(entry.path)) continue;
                if (entry.kind == "snapshot" && entry.path.StartsWith("snapshot:/", StringComparison.Ordinal))
                    result.SnapshotPaths.Add(entry.path);
                else if (entry.kind == "event" && entry.path.StartsWith("event:/", StringComparison.Ordinal))
                    result.EventPaths.Add(entry.path);
            }
            return result;
        }

        public static List<string> GetAllKnownEventPaths(string searchRoot)
        {
            var list = new List<string>();
            if (string.IsNullOrEmpty(searchRoot) || !Directory.Exists(searchRoot))
                return list;

            foreach (var path in Directory.EnumerateFiles(searchRoot, "*.catalog.json", SearchOption.AllDirectories))
            {
                var catalog = GetOrLoadCatalog(path);
                if (catalog == null || !catalog.IsCrcValid) continue;
                list.AddRange(catalog.EventPaths);
                list.AddRange(catalog.SnapshotPaths);
            }

            list.Sort(StringComparer.OrdinalIgnoreCase);
            return list;
        }

        [Serializable]
        private class ParsedCatalog
        {
            public int version = 0;
            public long bank_size = 0;
            public long bank_crc32 = -1;
            public List<CatalogEventEntry> events = new List<CatalogEventEntry>();
        }

        public static BankCatalogData ParseCatalogJson(string json)
        {
            var invalid = new BankCatalogData();
            if (string.IsNullOrWhiteSpace(json)) return invalid;

            var parsed = new ParsedCatalog();
#if UNITY_EDITOR
            try { UnityEngine.JsonUtility.FromJsonOverwrite(json, parsed); }
            catch (ArgumentException) { return invalid; }
#else
            try
            {
                parsed = System.Text.Json.JsonSerializer.Deserialize<ParsedCatalog>(json,
                    new System.Text.Json.JsonSerializerOptions { IncludeFields = true });
            }
            catch (System.Text.Json.JsonException) { return invalid; }
#endif
            if (parsed == null || parsed.bank_crc32 < 0 || parsed.bank_crc32 > uint.MaxValue)
                return invalid;
            return new BankCatalogData
            {
                version = parsed.version,
                bank_size = parsed.bank_size,
                bank_crc32 = (uint)parsed.bank_crc32,
                events = parsed.events
            };
        }
    }
}
