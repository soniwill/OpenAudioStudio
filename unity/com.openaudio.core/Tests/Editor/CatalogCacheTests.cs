using System;
using System.IO;
using NUnit.Framework;

namespace OpenAudio.Editor.Tests
{
    [TestFixture]
    public class CatalogCacheTests
    {
        private string m_TempDir;

        [SetUp]
        public void SetUp()
        {
            m_TempDir = Path.Combine(Path.GetTempPath(), "oae_catalog_tests_" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(m_TempDir);
            CatalogCache.Clear();
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(m_TempDir))
            {
                try
                {
                    Directory.Delete(m_TempDir, true);
                }
                catch { }
            }
            CatalogCache.Clear();
        }

        [Test]
        public void Crc32_MatchesStandardIEEE8023()
        {
            byte[] testBytes = System.Text.Encoding.ASCII.GetBytes("123456789");
            uint crc = CatalogCache.CalculateCrc32(testBytes);
            // Standard check value for "123456789" in CRC-32 (IEEE 802.3) is 0xCBF43926 (3421760294)
            Assert.That(crc, Is.EqualTo(0xCBF43926u), "CRC-32 calculation failed standard IEEE 802.3 check value");
        }

        [Test]
        public void ParseCatalogJson_ExtractsFieldsAndEvents()
        {
            string json = @"
{
  ""version"": 1,
  ""bank_size"": 1024,
  ""bank_crc32"": 3421760294,
  ""events"": [
    {
      ""path"": ""event:/weapons/laser"",
      ""kind"": ""event""
    },
    {
      ""path"": ""snapshot:/environment/underwater"",
      ""kind"": ""snapshot""
    }
  ]
}";

            var data = CatalogCache.ParseCatalogJson(json);
            Assert.That(data.version, Is.EqualTo(1));
            Assert.That(data.bank_size, Is.EqualTo(1024));
            Assert.That(data.bank_crc32, Is.EqualTo(3421760294u));
            Assert.That(data.events.Count, Is.EqualTo(2));
            Assert.That(data.events[0].path, Is.EqualTo("event:/weapons/laser"));
            Assert.That(data.events[0].kind, Is.EqualTo("event"));
            Assert.That(data.events[1].path, Is.EqualTo("snapshot:/environment/underwater"));
            Assert.That(data.events[1].kind, Is.EqualTo("snapshot"));
        }

        [Test]
        public void LoadCatalog_WithValidBank_ValidatesCrcCorrectly()
        {
            byte[] bankDummyData = System.Text.Encoding.ASCII.GetBytes("123456789");
            string bankFile = Path.Combine(m_TempDir, "Master.bank");
            File.WriteAllBytes(bankFile, bankDummyData);

            uint bankCrc = CatalogCache.CalculateCrc32(bankDummyData);

            string catalogJson = $@"
{{
  ""version"": 1,
  ""bank_size"": {bankDummyData.Length},
  ""bank_crc32"": {bankCrc},
  ""events"": [
    {{
      ""path"": ""event:/ui/click"",
      ""kind"": ""event""
    }}
  ]
}}";
            string catalogFile = Path.Combine(m_TempDir, "Master.bank.catalog.json");
            File.WriteAllText(catalogFile, catalogJson);

            var catalog = CatalogCache.GetOrLoadCatalog(catalogFile);
            Assert.That(catalog, Is.Not.Null);
            Assert.That(catalog.IsCrcValid, Is.True, "CRC verification should succeed for valid bank");
            Assert.That(catalog.EventPaths.Count, Is.EqualTo(1));
            Assert.That(catalog.EventPaths[0], Is.EqualTo("event:/ui/click"));
        }

        [Test]
        public void LoadCatalog_WithCorruptedBank_ReportsInvalidCrc()
        {
            byte[] bankDummyData = System.Text.Encoding.ASCII.GetBytes("123456789");
            string bankFile = Path.Combine(m_TempDir, "Corrupted.bank");
            File.WriteAllBytes(bankFile, bankDummyData);

            // Intentional wrong CRC
            uint wrongCrc = 0x12345678u;

            string catalogJson = $@"
{{
  ""version"": 1,
  ""bank_size"": {bankDummyData.Length},
  ""bank_crc32"": {wrongCrc},
  ""events"": [
    {{
      ""path"": ""event:/game/over"",
      ""kind"": ""event""
    }}
  ]
}}";
            string catalogFile = Path.Combine(m_TempDir, "Corrupted.bank.catalog.json");
            File.WriteAllText(catalogFile, catalogJson);

            var catalog = CatalogCache.GetOrLoadCatalog(catalogFile);
            Assert.That(catalog, Is.Not.Null);
            Assert.That(catalog.IsCrcValid, Is.False, "CRC verification should fail for corrupted bank");
            Assert.That(catalog.EventPaths, Is.Empty);
            Assert.That(CatalogCache.GetAllKnownEventPaths(m_TempDir), Is.Empty);
        }

        [Test]
        public void CatalogFirstPublication_RechecksBankOnEachRead()
        {
            byte[] previous = { 1, 2, 3 };
            byte[] next = { 4, 5, 6 };
            string bank = Path.Combine(m_TempDir, "Master.bank");
            string catalog = bank + ".catalog.json";
            File.WriteAllBytes(bank, previous);
            File.WriteAllText(catalog,
                $"{{\"version\":1,\"bank_size\":{next.Length},\"bank_crc32\":{CatalogCache.CalculateCrc32(next)}," +
                "\"events\":[{\"kind\":\"event\",\"path\":\"event:/new\"}]}");

            Assert.That(CatalogCache.GetAllKnownEventPaths(m_TempDir), Is.Empty);
            File.WriteAllBytes(bank, next);
            Assert.That(CatalogCache.GetAllKnownEventPaths(m_TempDir), Is.EqualTo(new[] { "event:/new" }));
            File.WriteAllBytes(bank, previous);
            Assert.That(CatalogCache.GetAllKnownEventPaths(m_TempDir), Is.Empty);
        }
    }
}
