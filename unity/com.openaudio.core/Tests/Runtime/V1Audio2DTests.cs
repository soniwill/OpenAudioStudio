using System;
using System.IO;
using NUnit.Framework;
using OpenAudio;

namespace OpenAudio.Tests
{
    public class V1Audio2DTests
    {
        private OaeSystem _system;
        private Bank _playgroundBank;
        private string _playgroundBankPath;

        [SetUp]
        public void SetUp()
        {
            _system = OaeSystem.Create();
            _system.Init(48000, 2, 64, OaeInitFlags.NoDevice);

            _playgroundBankPath = AudioPlaygroundTests.FindBankPath("playground.bank");
            Assert.That(File.Exists(_playgroundBankPath), Is.True, $"playground.bank not found at {_playgroundBankPath}");

            _playgroundBank = _system.LoadBankFile(_playgroundBankPath);
        }

        [TearDown]
        public void TearDown()
        {
            _playgroundBank?.Unload();
            _system?.Dispose();
        }

        private static float RenderPeak(OaeSystem sys, int frames)
        {
            float[] buf = new float[frames * 2];
            sys.Render(buf, (uint)frames);
            float peak = 0.0f;
            for (int i = 0; i < buf.Length; i++)
            {
                float a = Math.Abs(buf[i]);
                if (a > peak) peak = a;
            }
            return peak;
        }

        [Test]
        public void V1_02_Compute2DSpatial_HandlesPanAndAttenuationOptions()
        {
            StudioEventEmitter.Compute2DSpatial(-10f, 0f, 0f, 0f, 2f, 20f, true, true, out float volL, out float panL);
            Assert.That(panL, Is.EqualTo(-1.0f).Within(0.001f));
            Assert.That(volL, Is.InRange(0.5f, 0.6f));

            StudioEventEmitter.Compute2DSpatial(10f, 0f, 0f, 0f, 2f, 20f, true, true, out float volR, out float panR);
            Assert.That(panR, Is.EqualTo(1.0f).Within(0.001f));
            Assert.That(volR, Is.EqualTo(volL).Within(0.001f));

            StudioEventEmitter.Compute2DSpatial(25f, 0f, 0f, 0f, 2f, 20f, true, false, out float volFar, out float panDisabled);
            Assert.That(volFar, Is.EqualTo(0.0f).Within(0.001f));
            Assert.That(panDisabled, Is.EqualTo(0.0f).Within(0.001f));
        }

        [Test]
        public void V1_03_AudioClockQuantization_TracksDspFramesDeterministically()
        {
            ulong t0 = _system.GetDspClock();
            RenderPeak(_system, 24000); // 0.5 seconds at 48 kHz
            ulong t1 = _system.GetDspClock();
            Assert.That(t1 - t0, Is.EqualTo(24000ul));
        }

        [Test]
        public void V1_04_StreamingMusicAndEmitter_PlayLoopAndMissingBankDiagnoses()
        {
            var music = _system.GetEvent("event:/playground/adaptive_music").CreateInstance();
            var amb = _system.GetEvent("event:/playground/emitter_3d").CreateInstance();
            music.Start();
            amb.Start();
            _system.Update();

            float peak = RenderPeak(_system, 4800);
            Assert.That(peak, Is.GreaterThan(0.01f));

            music.Stop(OaeStopMode.Immediate);
            amb.Stop(OaeStopMode.Immediate);
            music.Release();
            amb.Release();

            var ex = Assert.Throws<OaeException>(() => _system.LoadBankFile(Path.Combine(Path.GetTempPath(), "nonexistent_v104.bank")));
            Assert.That(ex.Result, Is.EqualTo(OaeResult.ERR_FILE_NOT_FOUND));
        }

        [Test]
        public void V1_05_DiagnosticsSnapshot_ExposesVoicesAndBusMeters()
        {
            var musicDesc = _system.GetEvent("event:/playground/adaptive_music");
            var musicInst = musicDesc.CreateInstance();
            musicInst.Start();

            _system.Update();
            RenderPeak(_system, 2048);

            var snap = OaeDiagnosticsSnapshot.Capture(
                _system,
                new[] { "playground.bank" },
                new[] { musicDesc });

            Assert.That(snap.TotalVoices, Is.GreaterThanOrEqualTo(1));
            Assert.That(snap.LoadedBanks, Contains.Item("playground.bank"));
            Assert.That(snap.ActiveEvents.Count, Is.GreaterThanOrEqualTo(1));
            Assert.That(snap.BusMeters.Count, Is.GreaterThanOrEqualTo(4));

            musicInst.Stop(OaeStopMode.Immediate);
            musicInst.Release();
        }

        [Test]
        public void V1_07_Spatial3DAttributes_And_BuiltinParameters_SyncWithNative()
        {
            var listenerAttrs = new Oae3DAttributes
            {
                position = new OaeVec3(0f, 0f, 0f),
                velocity = new OaeVec3(0f, 0f, 0f),
                forward = new OaeVec3(0f, 0f, 1f),
                up = new OaeVec3(0f, 1f, 0f)
            };
            _system.SetListener3DAttributes(listenerAttrs);
            var readListener = _system.GetListener3DAttributes();
            Assert.That(readListener.forward.z, Is.EqualTo(1.0f).Within(0.001f));

            using var inst = _system.GetEvent("event:/playground/emitter_3d").CreateInstance();
            inst.Set3DAttributes(new Oae3DAttributes
            {
                position = new OaeVec3(3f, 4f, 0f),
                velocity = new OaeVec3(0f, 6f, 8f),
                forward = new OaeVec3(-0.6f, -0.8f, 0f),
                up = new OaeVec3(0f, 1f, 0f)
            });
            inst.SetSpatializerSettings(new OaeSpatializerSettings
            {
                enabled = 1,
                rolloff = (int)OaeRolloffType.Linear,
                min_distance = 1.0f,
                max_distance = 20.0f,
                sound_size = 2.0f,
                min_extent_deg = 45.0f,
                cone_inner_angle_deg = 60.0f,
                cone_outer_angle_deg = 120.0f,
                cone_outer_gain = 0.25f
            });

            var settings = inst.GetSpatializerSettings();
            Assert.That(settings.enabled, Is.EqualTo(1));
            Assert.That(settings.sound_size, Is.EqualTo(2.0f).Within(0.001f));
            Assert.That(settings.min_extent_deg, Is.EqualTo(45.0f).Within(0.001f));

            inst.GetParameter("Distance", out float dist, out _);
            inst.GetParameter("Direction", out float dir, out _);
            inst.GetParameter("Elevation", out float elev, out _);
            inst.GetParameter("EventConeAngle", out float cone, out _);
            inst.GetParameter("Speed", out float speed, out _);

            Assert.That(dist, Is.EqualTo(5.0f).Within(0.01f));
            Assert.That(dir, Is.EqualTo(90.0f).Within(0.05f));
            Assert.That(elev, Is.EqualTo(53.13f).Within(0.1f));
            Assert.That(cone, Is.EqualTo(0.0f).Within(0.1f));
            Assert.That(speed, Is.EqualTo(10.0f).Within(0.01f));
        }
    }
}
