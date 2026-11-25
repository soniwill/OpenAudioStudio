using System;
using System.Collections;
using System.IO;
using NUnit.Framework;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.TestTools;
#endif

namespace OpenAudio.Tests
{
    /// <summary>
    /// End-to-end tests covering all 6 Audio Playground stations in headless .NET 8 and Unity Runtime.
    /// </summary>
    [TestFixture]
    public class AudioPlaygroundTests
    {
        private OaeSystem _system;
        private Bank _playgroundBank;
        private string _playgroundBankPath;
        private string _voBankPath;

        /// <summary>
        /// Initializes a NoDevice OaeSystem and loads playground.bank for each test.
        /// </summary>
        [SetUp]
        public void SetUp()
        {
            _system = OaeSystem.Create();
            _system.Init(48000, 2, 64, OaeInitFlags.NoDevice);

            _playgroundBankPath = FindBankPath("playground.bank");
            _voBankPath = FindBankPath("playground_vo.bank");

            Assert.That(File.Exists(_playgroundBankPath), Is.True, $"playground.bank not found at {_playgroundBankPath}");
            Assert.That(File.Exists(_voBankPath), Is.True, $"playground_vo.bank not found at {_voBankPath}");

            _playgroundBank = _system.LoadBankFile(_playgroundBankPath);
        }

        /// <summary>
        /// Unloads test banks and releases the OaeSystem instance.
        /// </summary>
        [TearDown]
        public void TearDown()
        {
            _playgroundBank?.Unload();
            _system?.Dispose();
        }

        internal static string FindBankPath(string fileName)
        {
#if UNITY_5_3_OR_NEWER
            return Path.Combine(UnityEngine.Application.streamingAssetsPath, fileName);
#else
            string startDir = AppDomain.CurrentDomain.BaseDirectory;
            string direct = Path.Combine(startDir, fileName);
            if (File.Exists(direct)) return direct;

            var dir = new DirectoryInfo(startDir);
            for (int i = 0; i < 8 && dir != null; i++)
            {
                string candidate = Path.Combine(dir.FullName, "example_project", fileName);
                if (File.Exists(candidate)) return candidate;
                dir = dir.Parent;
            }
            return direct;
#endif
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

        /// <summary>
        /// Station 1: Validates SurfaceType labeled parameter updates and multi-instrument footstep playback.
        /// </summary>
        [Test]
        public void PG_01_Station1_Footsteps_VarySurfaceTypeAndPlayMultiInstrument()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);

            foreach (PlaygroundSurface surface in new[] { PlaygroundSurface.Wood, PlaygroundSurface.Concrete, PlaygroundSurface.Grass })
            {
                bool triggered = ctrl.SelectSurfaceAndStep(surface);
                Assert.That(triggered, Is.True, $"Footstep failed on surface {surface}");
                _system.GetGlobalParameter("SurfaceType", out float paramValue, out _);
                Assert.That(paramValue, Is.EqualTo((float)surface).Within(0.01f));
                _system.Update();
                float peak = RenderPeak(_system, 2400);
                Assert.That(peak, Is.GreaterThan(0.005f), $"Expected audible footstep output on surface {surface}");
            }

            Assert.That(ctrl.CurrentSurface, Is.EqualTo(PlaygroundSurface.Grass));
            Assert.That(ctrl.TotalFootstepsTriggered, Is.EqualTo(3));
            ctrl.ReleaseAllInstances();
        }

        /// <summary>
        /// Station 2: Validates 3D emitter/listener attributes, directional cone attenuation, and built-in parameters.
        /// </summary>
        [Test]
        public void PG_02_Station2_Emitter3D_UpdatesSpatialAttributesConesAndBuiltinParameters()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);

            ctrl.UpdateEmitter3D(8.0f);
            ctrl.SetPlayerTransform(0.0f, 1.2f, 3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f);
            _system.Update();
            ctrl.Refresh3DBuiltinParameters();

            Assert.That(ctrl.LastDistance, Is.EqualTo(5.0f).Within(0.05f));
            Assert.That(ctrl.LastDirection, Is.EqualTo(0.0f).Within(0.5f));
            Assert.That(ctrl.LastElevation, Is.EqualTo(0.0f).Within(0.5f));
            Assert.That(ctrl.LastEventConeAngle, Is.EqualTo(0.0f).Within(0.5f));
            Assert.That(ctrl.LastSpeed, Is.EqualTo(4.0f).Within(0.05f));
            float frontPeak = RenderPeak(_system, 4800);
            Assert.That(frontPeak, Is.GreaterThan(0.01f));

            ctrl.SetPlayerTransform(0.0f, 1.2f, 13.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
            _system.Update();
            ctrl.Refresh3DBuiltinParameters();

            Assert.That(ctrl.LastDistance, Is.EqualTo(5.0f).Within(0.05f));
            Assert.That(ctrl.LastEventConeAngle, Is.EqualTo(180.0f).Within(0.5f));
            float rearPeak = RenderPeak(_system, 4800);
            Assert.That(frontPeak, Is.GreaterThan(rearPeak * 1.5f));

            ctrl.ReleaseAllInstances();
        }

        /// <summary>
        /// Station 3: Validates CombatIntensity parameter transitions between Exploration and Combat timeline regions.
        /// </summary>
        [Test]
        public void PG_03_Station3_CombatArena_SwitchesAdaptiveMusicTimeline()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);

            ctrl.SetCombatZoneActive(false);
            RenderPeak(_system, 4800);
            _system.Update();

            Assert.That(ctrl.IsCombatZoneActive, Is.False);
            Assert.That(ctrl.CombatIntensity, Is.EqualTo(0.0f).Within(0.001f));
            Assert.That(ctrl.AdaptiveMusicTimelinePositionMs, Is.LessThan(4000));

            ctrl.SetCombatZoneActive(true);
            RenderPeak(_system, 4800);
            _system.Update();
            float combatPeak = RenderPeak(_system, 4800);
            _system.Update();

            Assert.That(ctrl.IsCombatZoneActive, Is.True);
            Assert.That(ctrl.CombatIntensity, Is.EqualTo(1.0f).Within(0.001f));
            Assert.That(ctrl.AdaptiveMusicTimelinePositionMs, Is.GreaterThanOrEqualTo(5000));
            Assert.That(combatPeak, Is.GreaterThan(0.005f));

            ctrl.SetCombatZoneActive(false);
            RenderPeak(_system, 4800);
            _system.Update();
            Assert.That(ctrl.AdaptiveMusicTimelinePositionMs, Is.LessThan(4000));

            ctrl.ReleaseAllInstances();
        }

        /// <summary>
        /// Station 4: Validates stochastic grain spawning in the Forest Scatterer instrument.
        /// </summary>
        [Test]
        public void PG_04_Station4_ForestScatterer_SpawnsStochasticGrains()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);

            ctrl.SetForestZoneActive(true);
            Assert.That(ctrl.IsForestZoneActive, Is.True);

            float maxPeak = 0.0f;
            for (int i = 0; i < 6; i++)
            {
                float blockPeak = RenderPeak(_system, 4800);
                if (blockPeak > maxPeak) maxPeak = blockPeak;
                _system.Update();
            }

            Assert.That(maxPeak, Is.GreaterThan(0.005f));

            ctrl.SetForestZoneActive(false);
            _system.Update();
            Assert.That(ctrl.IsForestZoneActive, Is.False);

            ctrl.ReleaseAllInstances();
        }

        /// <summary>
        /// Station 5: Validates cave snapshot activation, on-demand VO bank loading, and Programmer Instrument playback.
        /// </summary>
        [Test]
        public void PG_05_Station5_CaveTunnelAndDialogue_ActivateSnapshotLoadVoBankAndPlayProgrammerSound()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);

            ctrl.SetCaveZoneActive(true);
            Assert.That(ctrl.IsCaveZoneActive, Is.True);
            Assert.That(ctrl.IsVoBankLoaded, Is.False);

            foreach (string key in new[] { "vo_welcome", "vo_spatial_test", "vo_ducking_active" })
            {
                bool ok = ctrl.TriggerProgrammerDialogue(key, _voBankPath);
                Assert.That(ok, Is.True, $"Failed to trigger Programmer dialogue key '{key}'");
                Assert.That(ctrl.IsVoBankLoaded, Is.True);
                Assert.That(ctrl.LastDialogueKey, Is.EqualTo(key));

                _system.Update();
                float peak = RenderPeak(_system, 24000);
                Assert.That(peak, Is.GreaterThan(0.005f), $"Expected audible dialogue output for key '{key}'");
            }

            ctrl.ReleaseAllInstances();
            Assert.That(ctrl.IsVoBankLoaded, Is.False);
        }

        /// <summary>
        /// Station 6: Validates Master, Music, and SFX VCA linear gains and 2D UI click playback.
        /// </summary>
        [Test]
        public void PG_06_Station6_HudVcasAndUiClick_ApplyVcaLevelsAndPlay2DSfx()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);

            ctrl.SetVcaLevel("Master", 0.75f);
            ctrl.SetVcaLevel("Music", 0.45f);
            ctrl.SetVcaLevel("Sfx", 0.65f);

            Assert.That(ctrl.MasterVcaLevel, Is.EqualTo(0.75f).Within(0.001f));
            Assert.That(ctrl.MusicVcaLevel, Is.EqualTo(0.45f).Within(0.001f));
            Assert.That(ctrl.SfxVcaLevel, Is.EqualTo(0.65f).Within(0.001f));

            Assert.That(_system.GetVca("vca:/playground_master").Volume, Is.EqualTo(0.75f).Within(0.001f));
            Assert.That(_system.GetVca("vca:/playground_music").Volume, Is.EqualTo(0.45f).Within(0.001f));
            Assert.That(_system.GetVca("vca:/playground_sfx").Volume, Is.EqualTo(0.65f).Within(0.001f));

            ctrl.TriggerUiClick();
            Assert.That(ctrl.UiClicksTriggered, Is.EqualTo(1));
            _system.Update();
            Assert.That(RenderPeak(_system, 2400), Is.GreaterThan(0.005f));

            ctrl.ReleaseAllInstances();
        }

        /// <summary>
        /// Verifies SmoothStep camera move cues update player pose, trigger Wood footsteps, activate Combat Arena, and cancel cleanly.
        /// </summary>
        [Test]
        public void PG_09_CameraMoveCue_SmoothStepUpdatesPoseFootstepsAndZones()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);
            ctrl.SetPlayerTransform(0.0f, 1.6f, -4.0f, 0.0f, 0.0f, 1.0f);

            ctrl.BeginCameraMoveCue(-7.5f, 1.6f, 6.5f, -45.0f, 8.0f, 1.6f);
            Assert.That(ctrl.IsCameraCueActive, Is.True);

            for (int i = 0; i < 4; i++)
            {
                ctrl.StepCameraCue(0.2f);
                _system.Update();
            }

            Assert.That(ctrl.IsCameraCueActive, Is.True);
            Assert.That(ctrl.PlayerPosX, Is.EqualTo(-3.75f).Within(0.05f));
            Assert.That(ctrl.PlayerPosZ, Is.EqualTo(1.25f).Within(0.05f));
            Assert.That(ctrl.CurrentSurface, Is.EqualTo(PlaygroundSurface.Wood));
            Assert.That(ctrl.TotalFootstepsTriggered, Is.GreaterThanOrEqualTo(2));
            Assert.That(RenderPeak(_system, 2048), Is.GreaterThan(0.002f));

            for (int i = 0; i < 4; i++)
            {
                ctrl.StepCameraCue(0.2f);
                _system.Update();
            }

            Assert.That(ctrl.IsCameraCueActive, Is.False);
            Assert.That(ctrl.PlayerPosX, Is.EqualTo(-7.5f).Within(0.01f));
            Assert.That(ctrl.PlayerPosZ, Is.EqualTo(6.5f).Within(0.01f));
            Assert.That(ctrl.IsCombatZoneActive, Is.True);
            Assert.That(ctrl.CombatIntensity, Is.EqualTo(1.0f).Within(0.001f));

            ctrl.BeginCameraMoveCue(7.5f, 1.6f, 6.5f, 45.0f, 5.0f, 2.0f);
            Assert.That(ctrl.IsCameraCueActive, Is.True);
            ctrl.StepCameraCue(0.25f);
            ctrl.CancelCameraCue();
            Assert.That(ctrl.IsCameraCueActive, Is.False);
            float pausedX = ctrl.PlayerPosX;
            ctrl.StepCameraCue(0.5f);
            Assert.That(ctrl.PlayerPosX, Is.EqualTo(pausedX).Within(0.0001f));

            ctrl.ReleaseAllInstances();
        }

        /// <summary>
        /// Verifies 360-degree camera orbit cue around Station2_Emitter3DTotem sweeps EventConeAngle, Distance, and Speed.
        /// </summary>
        [Test]
        public void PG_10_CameraOrbitCue_OrbitsTotem360AndUpdatesBuiltin3DParams()
        {
            var ctrl = new AudioPlaygroundController();
            ctrl.Initialize(_system);
            ctrl.UpdateEmitter3D();

            ctrl.BeginCameraOrbitCue(0.0f, 1.2f, 8.0f, 3.6f, 1.6f, 4.0f, 180.0f, 360.0f);
            Assert.That(ctrl.IsCameraCueActive, Is.True);

            ctrl.StepCameraCue(0.05f);
            _system.Update();
            ctrl.Refresh3DBuiltinParameters();
            float frontCone = ctrl.LastEventConeAngle;
            Assert.That(frontCone, Is.LessThan(15.0f));
            Assert.That(ctrl.LastDistance, Is.EqualTo(3.62f).Within(0.15f));

            for (int i = 0; i < 19; i++)
            {
                ctrl.StepCameraCue(0.1f);
                _system.Update();
            }
            ctrl.Refresh3DBuiltinParameters();
            float rearCone = ctrl.LastEventConeAngle;
            Assert.That(rearCone, Is.GreaterThan(165.0f));
            Assert.That(ctrl.LastSpeed, Is.GreaterThan(0.5f));

            for (int i = 0; i < 21; i++)
            {
                ctrl.StepCameraCue(0.1f);
                _system.Update();
            }
            ctrl.Refresh3DBuiltinParameters();
            Assert.That(ctrl.IsCameraCueActive, Is.False);
            Assert.That(ctrl.LastEventConeAngle, Is.LessThan(15.0f));

            ctrl.ReleaseAllInstances();
        }
    }

#if UNITY_5_3_OR_NEWER
    /// <summary>
    /// Unity PlayMode tests verifying procedural 3D room construction and AudioPlayground scene loading.
    /// </summary>
    [TestFixture]
    public class AudioPlaygroundPlayModeTests
    {
        private static void CleanupPlaygroundSingletons(GameObject roomRoot)
        {
            if (roomRoot != null)
            {
                UnityEngine.Object.DestroyImmediate(roomRoot);
            }
            if (OaeAudioManager.Instance != null)
            {
                UnityEngine.Object.DestroyImmediate(OaeAudioManager.Instance.gameObject);
            }
        }

        /// <summary>
        /// Verifies procedural room geometry, StudioListener, StudioEventEmitter, and all 6 stations in Unity PlayMode.
        /// </summary>
        [UnityTest]
        public IEnumerator PG_07_UnityRuntime_BuildsProcedural3DRoomAndStations()
        {
            if (OaeAudioManager.Instance != null)
            {
                UnityEngine.Object.DestroyImmediate(OaeAudioManager.Instance.gameObject);
                yield return null;
            }

            var mgrGo = new GameObject("PG_OaeAudioManager");
            mgrGo.AddComponent<OaeAudioManager>();
            yield return null;

            Assert.That(OaeAudioManager.Instance != null && OaeAudioManager.Instance.IsInitialized, Is.True);
            string bankPath = Path.Combine(UnityEngine.Application.streamingAssetsPath, "playground.bank");
            var bank = OaeAudioManager.Instance.System.LoadBankFile(bankPath);

            var ctrlGo = new GameObject("PG_Controller");
            var ctrl = ctrlGo.AddComponent<AudioPlaygroundController>();

            yield return null;
            yield return null;

            Assert.That(ctrl.RoomRoot, Is.Not.Null);
            Assert.That(StudioListener.CurrentListener, Is.Not.Null);
            Assert.That(ctrl.SceneEmitter3D, Is.Not.Null);

            Assert.That(ctrl.RoomRoot.transform.Find("Station1_Surface_Wood"), Is.Not.Null);
            Assert.That(ctrl.RoomRoot.transform.Find("Station2_Emitter3DTotem"), Is.Not.Null);
            Assert.That(ctrl.RoomRoot.transform.Find("Station3_CombatArena"), Is.Not.Null);
            Assert.That(ctrl.RoomRoot.transform.Find("Station4_ForestGrove"), Is.Not.Null);
            Assert.That(ctrl.RoomRoot.transform.Find("Station5_CaveTunnel"), Is.Not.Null);
            Assert.That(ctrl.RoomRoot.transform.Find("Station5_DialogueTotem"), Is.Not.Null);

            Assert.That(ctrl.SelectSurfaceAndStep(PlaygroundSurface.Concrete), Is.True);
            ctrl.SetCombatZoneActive(true);
            ctrl.SetForestZoneActive(true);
            ctrl.SetCaveZoneActive(true);
            Assert.That(ctrl.TriggerProgrammerDialogue("vo_welcome"), Is.True);
            ctrl.SetVcaLevel("Music", 0.5f);
            ctrl.TriggerUiClick();
            yield return null;

            Assert.That(ctrl.IsCombatZoneActive, Is.True);
            Assert.That(ctrl.IsForestZoneActive, Is.True);
            Assert.That(ctrl.IsCaveZoneActive, Is.True);
            Assert.That(ctrl.IsVoBankLoaded, Is.True);

            var roomRoot = ctrl.RoomRoot;
            UnityEngine.Object.DestroyImmediate(ctrlGo);
            bank.Unload();
            CleanupPlaygroundSingletons(roomRoot);
            yield return null;
        }

        /// <summary>
        /// Verifies the serialized AudioPlayground.unity scene opens and initializes cleanly in PlayMode.
        /// </summary>
        [UnityTest]
        public IEnumerator PG_08_AudioPlaygroundScene_LoadsAndInitializesInPlayMode()
        {
            if (OaeAudioManager.Instance != null)
            {
                UnityEngine.Object.DestroyImmediate(OaeAudioManager.Instance.gameObject);
                yield return null;
            }

            var loadOp = SceneManager.LoadSceneAsync("AudioPlayground", LoadSceneMode.Single);
            Assert.That(loadOp, Is.Not.Null, "AudioPlayground scene must be registered in EditorBuildSettings.");
            while (!loadOp.isDone)
            {
                yield return null;
            }

            yield return null;
            yield return null;

            Assert.That(OaeAudioManager.Instance, Is.Not.Null);
            Assert.That(OaeAudioManager.Instance.IsInitialized, Is.True);

#if UNITY_2023_1_OR_NEWER
            var ctrl = UnityEngine.Object.FindFirstObjectByType<AudioPlaygroundController>();
            var overlay = UnityEngine.Object.FindFirstObjectByType<OaeDiagnosticsOverlay>();
            var loader = UnityEngine.Object.FindFirstObjectByType<StudioBankLoader>();
#else
            var ctrl = UnityEngine.Object.FindObjectOfType<AudioPlaygroundController>();
            var overlay = UnityEngine.Object.FindObjectOfType<OaeDiagnosticsOverlay>();
            var loader = UnityEngine.Object.FindObjectOfType<StudioBankLoader>();
#endif

            Assert.That(ctrl, Is.Not.Null);
            Assert.That(overlay, Is.Not.Null);
            Assert.That(loader, Is.Not.Null);
            Assert.That(loader.IsLoaded, Is.True);
            Assert.That(ctrl.RoomRoot, Is.Not.Null);
            Assert.That(StudioListener.CurrentListener, Is.Not.Null);

            var roomRoot = ctrl.RoomRoot;
            UnityEngine.Object.DestroyImmediate(ctrl.gameObject);
            CleanupPlaygroundSingletons(roomRoot);
            yield return null;
        }
    }
#endif
}
