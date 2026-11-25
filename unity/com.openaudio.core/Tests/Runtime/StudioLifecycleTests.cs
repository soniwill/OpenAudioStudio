using System;
using NUnit.Framework;

namespace OpenAudio.Tests
{
    [TestFixture]
    public class StudioLifecycleTests
    {
        private OaeSystem m_System;

        [SetUp]
        public void SetUp()
        {
            m_System = OaeSystem.Create();
            m_System.Init(sampleRate: 48000, channels: 2, maxVoices: 32, flags: OaeInitFlags.NoDevice);
        }

        [TearDown]
        public void TearDown()
        {
            if (m_System != null && m_System.IsValid)
            {
                m_System.Release();
                m_System = null;
            }
        }

        [Test]
        public void MasterBus_CanGetAndSetProperties()
        {
            var master = m_System.GetMasterBus();
            Assert.That(master.IsValid, Is.True);

            master.Volume = 0.75f;
            Assert.That(master.Volume, Is.EqualTo(0.75f).Within(1e-4f));

            master.Mute = true;
            Assert.That(master.Mute, Is.True);
            master.Mute = false;
            Assert.That(master.Mute, Is.False);

            master.Paused = true;
            Assert.That(master.Paused, Is.True);
            master.Paused = false;
            Assert.That(master.Paused, Is.False);
        }

        [Test]
        public void HeadlessRendering_WithSilentBus_RendersZeroes()
        {
            float[] buffer = new float[1024];
            m_System.Update();
            m_System.Render(buffer, 512);

            float peak = 0.0f;
            foreach (float sample in buffer)
            {
                peak = Math.Max(peak, Math.Abs(sample));
            }

            Assert.That(peak, Is.EqualTo(0.0f).Within(1e-5f));
        }

        [Test]
        public void DspClock_AdvancesOverUpdates()
        {
            ulong clock1 = m_System.GetDspClock();
            float[] buffer = new float[512];
            m_System.Render(buffer, 256);
            m_System.Update();
            ulong clock2 = m_System.GetDspClock();

            Assert.That(clock2, Is.EqualTo(clock1 + 256));
        }

        [Test]
        public void GlobalParameter_CanSetAndGet()
        {
            m_System.SetGlobalParameter("MusicIntensity", 0.8f, ignoreSeekSpeed: true);
            m_System.GetGlobalParameter("MusicIntensity", out float val, out float finalVal);
            Assert.That(val, Is.EqualTo(0.8f).Within(1e-4f));
            Assert.That(finalVal, Is.EqualTo(0.8f).Within(1e-4f));
        }
    }
}
