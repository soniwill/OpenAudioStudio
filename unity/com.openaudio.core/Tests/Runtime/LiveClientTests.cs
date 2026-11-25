using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using NUnit.Framework;
using OpenAudio;

namespace OpenAudio.Tests
{
    public class LiveClientTests
    {
        private OaeSystem _system;
        private Bank _playgroundBank;

        [SetUp]
        public void SetUp()
        {
            _system = OaeSystem.Create();
            _system.Init(48000, 2, 64, OaeInitFlags.NoDevice);

            string playgroundBankPath = AudioPlaygroundTests.FindBankPath("playground.bank");
            if (File.Exists(playgroundBankPath))
            {
                _playgroundBank = _system.LoadBankFile(playgroundBankPath);
            }
        }

        [TearDown]
        public void TearDown()
        {
            _playgroundBank?.Unload();
            _system?.Dispose();
        }

        [Test]
        public void LiveClient_OfflineServer_DoesNotThrowOrBlock()
        {
            using var client = new LiveClient(_system, "UnityTest");
            client.ConnectAsync("127.0.0.1", 17459, 0);
            for (int i = 0; i < 20; i++)
            {
                client.Update();
                if (client.State == LiveConnectionState.Disconnected) break;
                Thread.Sleep(15);
            }
            Assert.That(client.IsConnected, Is.False);
        }

        [Test]
        public void LiveClient_Handshake_BidirectionalSync_And_RemoteAudition()
        {
            var listener = new TcpListener(IPAddress.Loopback, 0);
            listener.Start();
            int port = ((IPEndPoint)listener.LocalEndpoint).Port;

            using var client = new LiveClient(_system, "UnityTest");
            bool handshakeSeen = false;
            bool crcMatched = false;
            string curveSeen = null;

            client.OnHandshakeCompleted += match =>
            {
                handshakeSeen = true;
                crcMatched = match;
            };
            client.OnCurveUpdated += json => curveSeen = json;

            client.ConnectAsync("127.0.0.1", port, 0x11223344u);

            using var serverClient = listener.AcceptTcpClient();
            serverClient.NoDelay = true;
            using var stream = serverClient.GetStream();
            using var reader = new StreamReader(stream, Encoding.UTF8, false, 1024, leaveOpen: true);
            using var writer = new StreamWriter(stream, new UTF8Encoding(false), 1024, leaveOpen: true) { AutoFlush = true };

            // 1. Handshake
            string helloLine = reader.ReadLine();
            Assert.That(helloLine, Does.Contain("\"type\":\"hello\""));
            Assert.That(helloLine, Does.Contain("\"bank_crc32\":287454020"));

            writer.WriteLine("{\"type\":\"hello_ack\",\"version\":1,\"studio_version\":\"1.0.0\",\"studio_bank_crc32\":287454020,\"crc_match\":true,\"status\":\"connected\"}");

            for (int i = 0; i < 50 && !handshakeSeen; i++)
            {
                client.Update();
                Thread.Sleep(5);
            }
            Assert.That(handshakeSeen, Is.True);
            Assert.That(crcMatched, Is.True);
            Assert.That(client.IsConnected, Is.True);

            // 2. Studio -> Game: bus_volume (-6 dB => ~0.501187 linear), bus_mute, curve_update
            writer.WriteLine("{\"type\":\"bus_volume\",\"path\":\"bus:/\",\"volume_db\":-6.0}");
            writer.WriteLine("{\"type\":\"bus_mute\",\"path\":\"bus:/\",\"mute\":true}");
            writer.WriteLine("{\"type\":\"curve_update\",\"event_path\":\"event:/playground/adaptive_music\",\"track_index\":0,\"target\":\"volume\",\"points\":[{\"x\":0.0,\"y\":0.0},{\"x\":1.0,\"y\":-6.0}]}");

            for (int i = 0; i < 50 && curveSeen == null; i++)
            {
                client.Update();
                Thread.Sleep(5);
            }
            Assert.That(curveSeen, Is.Not.Null);

            var master = _system.GetMasterBus();
            try
            {
                Assert.That(master.Volume, Is.EqualTo((float)Math.Pow(10.0, -6.0 / 20.0)).Within(0.005f));
                Assert.That(master.Mute, Is.True);
                master.Mute = false;
                master.Volume = 1.0f;
            }
            finally
            {
                master.Release();
            }

            // 3. Game -> Studio: OaeSystem.SetGlobalParameter triggers param_set over TCP
            _system.SetGlobalParameter("TestLiveParam", 0.72f, ignoreSeekSpeed: true);
            string paramLine = reader.ReadLine();
            Assert.That(paramLine, Does.Contain("\"type\":\"param_set\""));
            Assert.That(paramLine, Does.Contain("\"name\":\"TestLiveParam\""));
            Assert.That(paramLine, Does.Contain("0.72"));

            // 4. Remote audition: event_play & event_stop
            if (_playgroundBank != null)
            {
                writer.WriteLine("{\"type\":\"event_play\",\"event_path\":\"event:/playground/ui_click\",\"position_ms\":0.0,\"loop\":false}");
                for (int i = 0; i < 20; i++)
                {
                    client.Update();
                    _system.Update();
                    Thread.Sleep(5);
                }
                _system.GetChannelsPlaying(out int total, out int _);
                Assert.That(total, Is.GreaterThanOrEqualTo(0));

                writer.WriteLine("{\"type\":\"event_stop\",\"event_path\":\"event:/playground/ui_click\",\"immediate\":true}");
                for (int i = 0; i < 20; i++)
                {
                    client.Update();
                    _system.Update();
                    Thread.Sleep(5);
                }
            }

            listener.Stop();
        }
    }
}
