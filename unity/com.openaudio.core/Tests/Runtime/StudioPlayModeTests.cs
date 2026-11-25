using System.Collections;
using NUnit.Framework;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
using UnityEngine.TestTools;
#endif

namespace OpenAudio.Tests
{
    [TestFixture]
    public class StudioPlayModeTests
    {
#if UNITY_5_3_OR_NEWER
        [UnityTest]
        public IEnumerator OaeAudioManager_InitializesAndUpdatesInScene()
        {
            var go = new GameObject("Test_OaeAudioManager");
            var manager = go.AddComponent<OaeAudioManager>();

            yield return null;

            Assert.That(manager.IsInitialized, Is.True, "OaeAudioManager should initialize on scene start");
            Assert.That(manager.System, Is.Not.Null);
            Assert.That(manager.MasterBus, Is.Not.Null);

            // Run a few frames
            ulong startClock = manager.System.GetDspClock();
            for (int i = 0; i < 5; ++i)
            {
                yield return null;
            }

            Object.DestroyImmediate(go);
            yield return null;

            Assert.That(manager.System, Is.Null, "OaeAudioManager should release system on destroy");
        }

        [UnityTest]
        public IEnumerator StudioEventEmitter_CanAttachAndPlay()
        {
            var mgrGo = new GameObject("OaeAudioManager_Host");
            var manager = mgrGo.AddComponent<OaeAudioManager>();
            yield return null;

            var emitterGo = new GameObject("Emitter_Host");
            var emitter = emitterGo.AddComponent<StudioEventEmitter>();
            emitter.PlayTrigger = EmitterPlayTrigger.Manual;
            emitter.StopTrigger = EmitterStopTrigger.Manual;
            emitter.EventPath = "event:/test/dummy";

            // Unity Test Framework requires expecting LogError calls
            LogAssert.Expect(LogType.Error, new System.Text.RegularExpressions.Regex(".*Failed to play event.*"));
            Assert.DoesNotThrow(() => emitter.Play());
            yield return null;

            emitter.Stop();
            yield return null;

            Object.DestroyImmediate(emitterGo);
            Object.DestroyImmediate(mgrGo);
            yield return null;
        }
#else
        [Test]
        public void Standalone_PlayMode_Placeholder()
        {
            Assert.Pass("PlayMode scene tests require Unity Engine runtime environment");
        }
#endif
    }
}
