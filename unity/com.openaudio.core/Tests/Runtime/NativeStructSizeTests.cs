using System;
using System.Runtime.InteropServices;
using NUnit.Framework;

namespace OpenAudio.Tests
{
    [TestFixture]
    public class NativeStructSizeTests
    {
        [Test]
        public void StructSizes_MatchNativeOaeStructSizesExactly()
        {
            var res = OaeNative.oae_get_struct_sizes(out var sizes);
            Assert.That(res, Is.EqualTo(OaeResult.OK), "oae_get_struct_sizes returned an error");

            int sInit = Marshal.SizeOf<OaeInitSettings>();
            Assert.That(sInit, Is.EqualTo((int)sizes.init_settings), "OaeInitSettings size mismatch");

            unsafe
            {
                int sMeter = sizeof(OaeMeter);
                Assert.That(sMeter, Is.EqualTo((int)sizes.meter), "OaeMeter size mismatch");
            }

            int sMarker = Marshal.SizeOf<OaeTimelineMarkerProperties>();
            Assert.That(sMarker, Is.EqualTo((int)sizes.timeline_marker_properties), "OaeTimelineMarkerProperties size mismatch");

            int sBeat = Marshal.SizeOf<OaeTimelineBeatProperties>();
            Assert.That(sBeat, Is.EqualTo((int)sizes.timeline_beat_properties), "OaeTimelineBeatProperties size mismatch");

            int sSound = Marshal.SizeOf<OaeEventSoundProperties>();
            Assert.That(sSound, Is.EqualTo((int)sizes.event_sound_properties), "OaeEventSoundProperties size mismatch");
        }

        [Test]
        public void VerifyStructSizes_DoesNotThrow()
        {
            Assert.DoesNotThrow(() => OaeNative.VerifyStructSizes());
        }

        [Test]
        public void NativeVersion_ReturnsExpectedFormat()
        {
            uint version = OaeNative.oae_get_version();
            Assert.That(version, Is.EqualTo(1u), "Expected API version 1");
        }
    }
}
