using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using NUnit.Framework;

namespace OpenAudio.Tests
{
    [TestFixture]
    public class Epic9Tests
    {
        private static OaeSystem CreateHeadlessSystem()
        {
            var sys = OaeSystem.Create();
            sys.Init(48000, 2, 16, OaeInitFlags.NoDevice);
            return sys;
        }

        [Test]
        public void T18_BusSendsAndReturns_WorkAndRejectCycles()
        {
            using var sys = CreateHeadlessSystem();
            var sfx = sys.CreateBus("sfx");
            var reverb = sys.CreateReturnBus("reverb");
            Assert.That(reverb.IsValid, Is.True);

            var resolved = sys.GetBus("return:/reverb");
            Assert.That(resolved.Handle, Is.EqualTo(reverb.Handle));

            sfx.AddSend(reverb, 0.5f, OaeEffectPosition.PostFader);
            Assert.That(sfx.GetSendLevel(reverb), Is.EqualTo(0.5f).Within(0.001f));

            sfx.SetSendLevel(reverb, 0.25f);
            Assert.That(sfx.GetSendLevel(reverb), Is.EqualTo(0.25f).Within(0.001f));

            sfx.SetSendMute(reverb, true);
            Assert.That(sfx.GetSendMute(reverb), Is.True);

            // Cycle rejection
            Assert.That(() => reverb.AddSend(sfx, 0.5f), Throws.TypeOf<OaeException>());
            Assert.That(() => sfx.AddSend(sfx, 0.5f), Throws.TypeOf<OaeException>());

            sfx.RemoveSend(reverb);
            reverb.Release();
            sfx.Release();
        }

        [Test]
        public void T23_TimelineClipController_ManagesLifecycleAndParameters()
        {
            string bankPath = AudioPlaygroundTests.FindBankPath("playground.bank");
            if (!File.Exists(bankPath)) Assert.Ignore("playground.bank not found for T23 test.");

            using var sys = CreateHeadlessSystem();
            using var bank = sys.LoadBankFile(bankPath);

            using var controller = new OaeTimelineClipController();
            controller.OnClipEnter(sys, "event:/playground/adaptive_music", 0.8f);
            Assert.That(controller.IsActive, Is.True);

            controller.OnClipEvaluate(new[] { new KeyValuePair<string, float>("CombatIntensity", 0.5f) });
            sys.Update();

            controller.OnClipExit(OaeTimelineStopMode.Immediate);
            Assert.That(controller.IsActive, Is.False);
        }

        [Test]
        public async Task T24_BankReferenceManager_DeduplicatesLoadsAndValidatesCrc32()
        {
            string bankPath = AudioPlaygroundTests.FindBankPath("playground.bank");
            if (!File.Exists(bankPath)) Assert.Ignore("playground.bank not found for T24 test.");

            byte[] bytes = File.ReadAllBytes(bankPath);
            uint crc = OaeBankReferenceManager.ComputeCrc32(bytes);

            using var sys = CreateHeadlessSystem();
            using var mgr = new OaeBankReferenceManager(sys);

            int loaderCalls = 0;
            OaeBankPayloadLoader loader = async key =>
            {
                loaderCalls++;
                await Task.Delay(10);
                return new OaeBankPayload { BankBytes = bytes, ExpectedCrc32 = crc };
            };

            var t1 = mgr.AcquireBankAsync("playground_bundle_key", loader);
            var t2 = mgr.AcquireBankAsync("playground_bundle_key", loader);
            var b1 = await t1;
            var b2 = await t2;

            Assert.That(loaderCalls, Is.EqualTo(1));
            Assert.That(b2, Is.SameAs(b1));
            Assert.That(mgr.GetRefCount("playground_bundle_key"), Is.EqualTo(2));

            mgr.ReleaseBank("playground_bundle_key");
            Assert.That(mgr.GetRefCount("playground_bundle_key"), Is.EqualTo(1));
            Assert.That(b1.IsValid, Is.True);

            mgr.ReleaseBank("playground_bundle_key");
            Assert.That(mgr.GetRefCount("playground_bundle_key"), Is.EqualTo(0));
            Assert.That(b1.IsValid, Is.False);

            // CRC mismatch must throw
            Assert.That(async () =>
            {
                await mgr.AcquireBankAsync("bad_crc", _ => Task.FromResult(new OaeBankPayload
                {
                    BankBytes = bytes,
                    ExpectedCrc32 = crc ^ 0x1u
                }));
            }, Throws.TypeOf<InvalidOperationException>());
        }
    }
}
