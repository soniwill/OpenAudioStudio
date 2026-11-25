using System;
using NUnit.Framework;

namespace OpenAudio.Tests
{
    [TestFixture]
    public class MarkerCallbackTests
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
        public void TrampolineRegistration_DoesNotThrow()
        {
            // Verify trampoline dictionary registration and unregistration
            ulong fakeInstance = 0x12345678;
            bool called = false;

            OaeNative.RegisterInstanceCallback(fakeInstance, (type, inst, props) =>
            {
                called = true;
            });

            // Simulate call via static callback pointer
            OaeNative.StaticCallback((uint)OaeEventCallbackType.Started, fakeInstance, IntPtr.Zero, IntPtr.Zero);
            Assert.That(called, Is.True, "Managed callback handler should have been invoked by trampoline");

            OaeNative.UnregisterInstanceCallback(fakeInstance);
            called = false;
            OaeNative.StaticCallback((uint)OaeEventCallbackType.Started, fakeInstance, IntPtr.Zero, IntPtr.Zero);
            Assert.That(called, Is.False, "Managed callback should not be invoked after unregistering");
        }
    }
}
