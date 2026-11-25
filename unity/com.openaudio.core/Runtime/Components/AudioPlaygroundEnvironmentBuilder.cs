#if UNITY_5_3_OR_NEWER
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace OpenAudio
{
    /// <summary>
    /// Holds runtime references to procedural materials, textures, lights, and animated props for cleanup and live feedback.
    /// </summary>
    public sealed class AudioPlaygroundEnvironment
    {
        private readonly List<Texture2D> m_Textures = new List<Texture2D>();
        private readonly List<Material> m_Materials = new List<Material>();
        private readonly List<Light> m_ArenaLights = new List<Light>();
        private readonly List<Renderer> m_ArenaBeacons = new List<Renderer>();
        private readonly List<Transform> m_Fireflies = new List<Transform>();

        /// <summary>Root GameObject of the procedural 3D environment.</summary>
        public GameObject RoomRoot { get; internal set; }

        /// <summary>StudioEventEmitter mounted on the Station 2 3D radio/campfire totem.</summary>
        public StudioEventEmitter SceneEmitter3D { get; internal set; }

        internal Light EmitterLight { get; set; }
        internal Transform EmitterCore { get; set; }
        internal Light ForestLight { get; set; }
        internal Light CaveLight { get; set; }
        internal Light DialogueLight { get; set; }
        internal Material DialogueScreenMaterial { get; set; }
        internal Material ArenaBeaconMaterial { get; set; }

        internal void TrackTexture(Texture2D texture)
        {
            if (texture != null) m_Textures.Add(texture);
        }

        internal void TrackMaterial(Material material)
        {
            if (material != null) m_Materials.Add(material);
        }

        internal void AddArenaLight(Light light)
        {
            if (light != null) m_ArenaLights.Add(light);
        }

        internal void AddArenaBeacon(Renderer renderer)
        {
            if (renderer != null) m_ArenaBeacons.Add(renderer);
        }

        internal void AddFirefly(Transform firefly)
        {
            if (firefly != null) m_Fireflies.Add(firefly);
        }

        /// <summary>
        /// Animates lights, beacons, and ambient props based on active playground station states.
        /// </summary>
        public void UpdateVisuals(float timeSeconds, bool combatActive, bool forestActive, bool caveActive, bool voLoaded)
        {
            if (EmitterLight != null)
            {
                float flicker = 0.88f + 0.12f * Mathf.Sin(timeSeconds * 7.5f) + 0.06f * Mathf.Sin(timeSeconds * 19.0f);
                EmitterLight.intensity = 3.2f * flicker;
            }

            if (EmitterCore != null)
            {
                float pulse = 1.0f + 0.08f * Mathf.Sin(timeSeconds * 5.0f);
                EmitterCore.localScale = new Vector3(0.38f * pulse, 0.38f * pulse, 0.38f * pulse);
            }

            Color targetArenaColor = combatActive
                ? new Color(1.0f, 0.22f, 0.12f)
                : new Color(0.22f, 0.55f, 0.95f);
            float arenaIntensity = combatActive ? (3.2f + 0.8f * Mathf.Sin(timeSeconds * 6.0f)) : 1.4f;

            for (int i = 0; i < m_ArenaLights.Count; i++)
            {
                if (m_ArenaLights[i] != null)
                {
                    m_ArenaLights[i].color = Color.Lerp(m_ArenaLights[i].color, targetArenaColor, Time.deltaTime * 5.0f);
                    m_ArenaLights[i].intensity = Mathf.Lerp(m_ArenaLights[i].intensity, arenaIntensity, Time.deltaTime * 5.0f);
                }
            }

            if (ArenaBeaconMaterial != null)
            {
                Color emission = targetArenaColor * (combatActive ? 2.4f : 0.9f);
                ArenaBeaconMaterial.SetColor("_EmissionColor", emission);
            }

            if (ForestLight != null)
            {
                float targetForest = forestActive ? 3.0f : 1.3f;
                ForestLight.intensity = Mathf.Lerp(ForestLight.intensity, targetForest, Time.deltaTime * 4.0f);
            }

            for (int i = 0; i < m_Fireflies.Count; i++)
            {
                Transform f = m_Fireflies[i];
                if (f != null)
                {
                    Vector3 p = f.localPosition;
                    p.y = 1.3f + 0.45f * Mathf.Sin(timeSeconds * (1.4f + i * 0.23f) + i * 1.7f);
                    f.localPosition = p;
                }
            }

            if (CaveLight != null)
            {
                float targetCave = caveActive ? 3.8f : 1.6f;
                CaveLight.intensity = Mathf.Lerp(CaveLight.intensity, targetCave, Time.deltaTime * 4.0f);
            }

            if (DialogueLight != null)
            {
                float targetVo = voLoaded ? (2.8f + 0.5f * Mathf.Sin(timeSeconds * 4.0f)) : 1.1f;
                DialogueLight.intensity = Mathf.Lerp(DialogueLight.intensity, targetVo, Time.deltaTime * 5.0f);
            }

            if (DialogueScreenMaterial != null)
            {
                Color screenEmission = voLoaded
                    ? new Color(0.15f, 0.95f, 1.0f) * 2.0f
                    : new Color(0.08f, 0.35f, 0.48f) * 0.8f;
                DialogueScreenMaterial.SetColor("_EmissionColor", screenEmission);
            }
        }

        /// <summary>
        /// Releases procedurally allocated Texture2D and Material objects.
        /// </summary>
        public void DisposeResources()
        {
            for (int i = 0; i < m_Materials.Count; i++)
            {
                if (m_Materials[i] != null) Object.Destroy(m_Materials[i]);
            }
            m_Materials.Clear();

            for (int i = 0; i < m_Textures.Count; i++)
            {
                if (m_Textures[i] != null) Object.Destroy(m_Textures[i]);
            }
            m_Textures.Clear();
        }
    }

    /// <summary>
    /// Constructs an atmospheric, lit 3D acoustic pavilion with procedural PBR textures and detailed stations.
    /// </summary>
    public static class AudioPlaygroundEnvironmentBuilder
    {
        /// <summary>
        /// Builds the complete 3D Audio Playground showcase room, lighting, textures, signage, and Station 2 emitter.
        /// </summary>
        public static AudioPlaygroundEnvironment Build(string emitter3DEventPath, Vector3 emitterPosition)
        {
            var env = new AudioPlaygroundEnvironment();
            var root = new GameObject("AudioPlayground_RoomRoot");
            env.RoomRoot = root;

            ConfigureAtmosphereAndSun(env);

            Material floorMat = CreateConcreteMaterial(env, 256, new Color(0.18f, 0.20f, 0.23f), new Color(0.10f, 0.12f, 0.14f), 0.14f, 0.42f, new Vector2(12f, 12f), 0.75f);
            Material stoneWallMat = CreateStoneMaterial(env, 256, new Color(0.21f, 0.23f, 0.27f), new Color(0.13f, 0.15f, 0.18f), 0.08f, 0.28f, new Vector2(6f, 2f), 1.15f);
            Material trimMetalMat = CreateSolidMaterial(env, new Color(0.24f, 0.27f, 0.32f), 0.85f, 0.74f);
            Material warmLampMat = CreateEmissiveMaterial(env, new Color(1.0f, 0.88f, 0.65f), new Color(1.0f, 0.72f, 0.36f) * 3.4f);

            BuildCourtyardShell(root.transform, floorMat, stoneWallMat, trimMetalMat, warmLampMat);
            BuildStation1Footsteps(env, root.transform, trimMetalMat);
            BuildStation2EmitterTotem(env, root.transform, emitter3DEventPath, emitterPosition, trimMetalMat, stoneWallMat);
            BuildStation3CombatArena(env, root.transform, stoneWallMat, trimMetalMat);
            BuildStation4ForestGrove(env, root.transform, stoneWallMat);
            BuildStation5CaveAndDialogue(env, root.transform, stoneWallMat, trimMetalMat);
            BuildReflectionProbe(root.transform);

            return env;
        }

        private static void ConfigureAtmosphereAndSun(AudioPlaygroundEnvironment env)
        {
            Light[] lights =
#if UNITY_2023_1_OR_NEWER
                Object.FindObjectsByType<Light>(FindObjectsSortMode.None);
#else
                Object.FindObjectsOfType<Light>();
#endif
            Light sun = null;
            for (int i = 0; i < lights.Length; i++)
            {
                if (lights[i].type == LightType.Directional)
                {
                    sun = lights[i];
                    break;
                }
            }

            if (sun != null)
            {
                sun.useColorTemperature = true;
                sun.colorTemperature = 4400.0f;
                sun.color = new Color(1.0f, 0.92f, 0.82f);
                sun.intensity = 1.65f;
                sun.shadows = LightShadows.Soft;
                sun.shadowStrength = 0.82f;
                sun.shadowBias = 0.02f;
                sun.shadowNormalBias = 0.3f;
                sun.transform.rotation = Quaternion.Euler(24.0f, -42.0f, 0.0f);
                RenderSettings.sun = sun;
            }

            Shader skyShader = Shader.Find("Skybox/Procedural");
            if (skyShader != null)
            {
                var skyMat = new Material(skyShader);
                skyMat.SetFloat("_SunDisk", 2.0f);
                skyMat.SetFloat("_SunSize", 0.045f);
                skyMat.SetFloat("_SunSizeConvergence", 6.0f);
                skyMat.SetFloat("_AtmosphereThickness", 1.35f);
                skyMat.SetColor("_SkyTint", new Color(0.36f, 0.48f, 0.72f));
                skyMat.SetColor("_GroundColor", new Color(0.11f, 0.12f, 0.15f));
                skyMat.SetFloat("_Exposure", 0.78f);
                env.TrackMaterial(skyMat);
                RenderSettings.skybox = skyMat;
                RenderSettings.ambientMode = AmbientMode.Skybox;
                RenderSettings.ambientIntensity = 0.95f;
                RenderSettings.reflectionIntensity = 1.1f;
                DynamicGI.UpdateEnvironment();
            }
            else
            {
                RenderSettings.ambientMode = AmbientMode.Trilight;
                RenderSettings.ambientSkyColor = new Color(0.28f, 0.36f, 0.48f);
                RenderSettings.ambientEquatorColor = new Color(0.20f, 0.21f, 0.24f);
                RenderSettings.ambientGroundColor = new Color(0.10f, 0.09f, 0.11f);
            }

            RenderSettings.fog = true;
            RenderSettings.fogMode = FogMode.ExponentialSquared;
            RenderSettings.fogColor = new Color(0.10f, 0.13f, 0.19f);
            RenderSettings.fogDensity = 0.012f;

            Camera cam = Camera.main;
            if (cam != null)
            {
                cam.clearFlags = skyShader != null ? CameraClearFlags.Skybox : CameraClearFlags.SolidColor;
                cam.backgroundColor = new Color(0.08f, 0.11f, 0.16f, 1.0f);
                cam.allowHDR = true;
                cam.allowMSAA = true;
                if (cam.GetComponent<AudioPlaygroundPostProcess>() == null)
                {
                    cam.gameObject.AddComponent<AudioPlaygroundPostProcess>();
                }
            }
        }

        private static void BuildReflectionProbe(Transform parent)
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null || Application.isBatchMode)
                return;

            var go = new GameObject("Courtyard_ReflectionProbe");
            go.transform.SetParent(parent, false);
            go.transform.position = new Vector3(0.0f, 3.2f, 5.5f);
            var probe = go.AddComponent<ReflectionProbe>();
            probe.mode = ReflectionProbeMode.Realtime;
            probe.refreshMode = ReflectionProbeRefreshMode.OnAwake;
            probe.timeSlicingMode = ReflectionProbeTimeSlicingMode.NoTimeSlicing;
            probe.resolution = 256;
            probe.hdr = true;
            probe.boxProjection = true;
            probe.size = new Vector3(32.0f, 14.0f, 32.0f);
            probe.intensity = 1.2f;
            probe.RenderProbe();
        }

        private static void BuildCourtyardShell(Transform parent, Material floorMat, Material wallMat, Material metalMat, Material lampMat)
        {
            CreateBox("Floor", parent, new Vector3(0.0f, -0.1f, 5.5f), new Vector3(28.0f, 0.2f, 28.0f), floorMat);

            CreateBox("PerimeterWall_North", parent, new Vector3(0.0f, 1.4f, 19.2f), new Vector3(28.0f, 2.8f, 0.6f), wallMat);
            CreateBox("PerimeterWall_South", parent, new Vector3(0.0f, 1.0f, -8.2f), new Vector3(28.0f, 2.0f, 0.6f), wallMat);
            CreateBox("PerimeterWall_West", parent, new Vector3(-13.8f, 1.2f, 5.5f), new Vector3(0.6f, 2.4f, 28.0f), wallMat);
            CreateBox("PerimeterWall_East", parent, new Vector3(13.8f, 1.2f, 5.5f), new Vector3(0.6f, 2.4f, 28.0f), wallMat);

            CreateBox("WallCap_North", parent, new Vector3(0.0f, 2.86f, 19.2f), new Vector3(28.2f, 0.14f, 0.75f), metalMat);
            CreateBox("WallCap_West", parent, new Vector3(-13.8f, 2.46f, 5.5f), new Vector3(0.75f, 0.14f, 28.2f), metalMat);
            CreateBox("WallCap_East", parent, new Vector3(13.8f, 2.46f, 5.5f), new Vector3(0.75f, 0.14f, 28.2f), metalMat);

            Vector3[] bollards = new[]
            {
                new Vector3(-6.2f, 0.0f, -3.2f),
                new Vector3(6.2f, 0.0f, -3.2f),
                new Vector3(-3.2f, 0.0f, 4.2f),
                new Vector3(3.2f, 0.0f, 4.2f),
                new Vector3(-4.2f, 0.0f, 10.6f),
                new Vector3(4.2f, 0.0f, 10.6f)
            };

            for (int i = 0; i < bollards.Length; i++)
            {
                var post = new GameObject($"PathBollard_{i}");
                post.transform.SetParent(parent, false);
                post.transform.position = bollards[i];
                CreateCylinder("Post", post.transform, new Vector3(0.0f, 0.45f, 0.0f), new Vector3(0.16f, 0.45f, 0.16f), metalMat);
                CreateCylinder("LampHead", post.transform, new Vector3(0.0f, 0.94f, 0.0f), new Vector3(0.18f, 0.08f, 0.18f), lampMat);
                CreatePointLight("BollardLight", post.transform, new Vector3(0.0f, 1.15f, 0.0f), new Color(1.0f, 0.80f, 0.54f), 1.25f, 7.5f);
            }
        }

        private static void BuildStation1Footsteps(AudioPlaygroundEnvironment env, Transform parent, Material metalMat)
        {
            Material woodMat = CreateWoodMaterial(env, 256, 0.04f, 0.44f, new Vector2(1.5f, 2.5f), 0.9f);
            Material concreteMat = CreateConcreteMaterial(env, 256, new Color(0.56f, 0.57f, 0.58f), new Color(0.38f, 0.39f, 0.41f), 0.05f, 0.30f, new Vector2(2f, 3f), 0.85f);
            Material grassMat = CreateGrassMaterial(env, 256, 0.0f, 0.14f, new Vector2(3f, 4f), 0.7f);
            Material curbMat = CreateSolidMaterial(env, new Color(0.30f, 0.32f, 0.35f), 0.18f, 0.42f);

            var woodPad = CreateBox("Station1_Surface_Wood", parent, new Vector3(-4.0f, 0.04f, 0.0f), new Vector3(3.4f, 0.08f, 5.2f), woodMat);
            CreateBox("WoodBorderL", woodPad.transform, new Vector3(-0.52f, 0.15f, 0.0f), new Vector3(0.05f, 1.3f, 1.02f), curbMat);
            CreateBox("WoodBorderR", woodPad.transform, new Vector3(0.52f, 0.15f, 0.0f), new Vector3(0.05f, 1.3f, 1.02f), curbMat);

            var concretePad = CreateBox("Station1_Surface_Concrete", parent, new Vector3(0.0f, 0.04f, 0.0f), new Vector3(3.4f, 0.08f, 5.2f), concreteMat);
            CreateBox("ConcreteBorderL", concretePad.transform, new Vector3(-0.52f, 0.15f, 0.0f), new Vector3(0.05f, 1.3f, 1.02f), metalMat);
            CreateBox("ConcreteBorderR", concretePad.transform, new Vector3(0.52f, 0.15f, 0.0f), new Vector3(0.05f, 1.3f, 1.02f), metalMat);

            var grassPad = CreateBox("Station1_Surface_Grass", parent, new Vector3(4.0f, 0.04f, 0.0f), new Vector3(3.4f, 0.08f, 5.2f), grassMat);
            CreateBox("GrassBorderL", grassPad.transform, new Vector3(-0.52f, 0.15f, 0.0f), new Vector3(0.05f, 1.3f, 1.02f), curbMat);
            CreateBox("GrassBorderR", grassPad.transform, new Vector3(0.52f, 0.15f, 0.0f), new Vector3(0.05f, 1.3f, 1.02f), curbMat);

            CreateSign(parent, "Sign_Station1", "ESTACAO 1: PASSOS\n[1] Madeira | [2] Concreto | [3] Grama", new Vector3(0.0f, 2.1f, -2.5f), new Color(0.95f, 0.90f, 0.78f));
            CreatePointLight("Station1_GalleryLight", parent, new Vector3(0.0f, 3.6f, 0.0f), new Color(1.0f, 0.92f, 0.80f), 1.8f, 12.0f);
        }

        private static void BuildStation2EmitterTotem(AudioPlaygroundEnvironment env, Transform parent, string eventPath, Vector3 emitterPos, Material metalMat, Material stoneMat)
        {
            Material grillMat = CreateSolidMaterial(env, new Color(0.08f, 0.09f, 0.11f), 0.35f, 0.25f);
            Material amberGlowMat = CreateEmissiveMaterial(env, new Color(1.0f, 0.62f, 0.16f), new Color(1.0f, 0.52f, 0.10f) * 2.8f);
            Material coneRingMat = CreateEmissiveMaterial(env, new Color(1.0f, 0.80f, 0.28f), new Color(1.0f, 0.72f, 0.20f) * 1.4f);

            var pedestal = CreateCylinder("Station2_Pedestal", parent, new Vector3(emitterPos.x, 0.25f, emitterPos.z), new Vector3(2.2f, 0.25f, 2.2f), stoneMat);
            CreateCylinder("Station2_PedestalTrim", pedestal.transform, new Vector3(0.0f, 1.05f, 0.0f), new Vector3(0.92f, 0.18f, 0.92f), metalMat);

            var totemGo = new GameObject("Station2_Emitter3DTotem");
            totemGo.transform.SetParent(parent, false);
            totemGo.transform.position = emitterPos;
            totemGo.transform.rotation = Quaternion.Euler(0.0f, 180.0f, 0.0f);

            CreateBox("RadioCabinet", totemGo.transform, new Vector3(0.0f, 0.0f, 0.0f), new Vector3(0.95f, 1.35f, 0.75f), metalMat);
            CreateBox("SpeakerGrill", totemGo.transform, new Vector3(0.0f, 0.08f, 0.39f), new Vector3(0.72f, 0.82f, 0.04f), grillMat);
            var core = CreateSphere("AcousticCore", totemGo.transform, new Vector3(0.0f, 0.10f, 0.42f), new Vector3(0.38f, 0.38f, 0.38f), amberGlowMat);
            CreateCylinder("Antenna", totemGo.transform, new Vector3(0.28f, 1.15f, -0.12f), new Vector3(0.04f, 0.55f, 0.04f), metalMat);
            CreateSphere("AntennaTip", totemGo.transform, new Vector3(0.28f, 1.72f, -0.12f), new Vector3(0.11f, 0.11f, 0.11f), amberGlowMat);

            CreateBox("ConeGuide_Inner", totemGo.transform, new Vector3(0.0f, -0.58f, 1.35f), new Vector3(0.14f, 0.04f, 1.9f), coneRingMat);
            CreateBox("ConeGuide_Left", totemGo.transform, new Vector3(-0.62f, -0.58f, 1.30f), new Vector3(0.08f, 0.04f, 1.9f), coneRingMat).transform.localRotation = Quaternion.Euler(0.0f, -28.0f, 0.0f);
            CreateBox("ConeGuide_Right", totemGo.transform, new Vector3(0.62f, -0.58f, 1.30f), new Vector3(0.08f, 0.04f, 1.9f), coneRingMat).transform.localRotation = Quaternion.Euler(0.0f, 28.0f, 0.0f);

            env.EmitterCore = core.transform;
            env.EmitterLight = CreatePointLight("Station2_BeaconLight", totemGo.transform, new Vector3(0.0f, 0.45f, 0.9f), new Color(1.0f, 0.62f, 0.22f), 3.6f, 11.0f, LightShadows.Soft);

            var emitter = totemGo.AddComponent<StudioEventEmitter>();
            emitter.EventPath = eventPath;
            emitter.PlayTrigger = EmitterPlayTrigger.Manual;
            emitter.StopTrigger = EmitterStopTrigger.OnDestroy;
            emitter.SpatialMode = EmitterSpatialMode.Spatial3D;
            emitter.MinDistance = 1.5f;
            emitter.MaxDistance = 22.0f;
            emitter.Rolloff = OaeRolloffType.Inverse;
            emitter.SoundSize = 1.2f;
            emitter.MinExtent = 15.0f;
            emitter.ConeInnerAngle = 45.0f;
            emitter.ConeOuterAngle = 115.0f;
            emitter.ConeOuterGain = 0.04f;
            env.SceneEmitter3D = emitter;

            CreateSign(parent, "Sign_Station2", "ESTACAO 2: EMISSOR 3D + CONE\nContorne para ouvir frente vs costas", new Vector3(emitterPos.x, 3.2f, emitterPos.z), new Color(1.0f, 0.80f, 0.42f));
        }

        private static void BuildStation3CombatArena(AudioPlaygroundEnvironment env, Transform parent, Material stoneMat, Material metalMat)
        {
            Material arenaFloorMat = CreateConcreteMaterial(env, 256, new Color(0.30f, 0.13f, 0.14f), new Color(0.16f, 0.07f, 0.08f), 0.28f, 0.55f, new Vector2(3f, 3f), 0.95f);
            Material beaconMat = CreateEmissiveMaterial(env, new Color(0.25f, 0.60f, 1.0f), new Color(0.22f, 0.55f, 0.95f) * 2.2f);
            env.ArenaBeaconMaterial = beaconMat;

            Vector3 center = new Vector3(-7.5f, 0.05f, 6.5f);
            CreateBox("Station3_CombatArena", parent, center, new Vector3(6.2f, 0.10f, 6.2f), arenaFloorMat);
            CreateBox("ArenaCurb_N", parent, center + new Vector3(0.0f, 0.08f, 3.15f), new Vector3(6.4f, 0.18f, 0.22f), metalMat);
            CreateBox("ArenaCurb_S", parent, center + new Vector3(0.0f, 0.08f, -3.15f), new Vector3(6.4f, 0.18f, 0.22f), metalMat);

            Vector3[] corners = new[]
            {
                new Vector3(-2.85f, 0.0f, -2.85f),
                new Vector3(2.85f, 0.0f, -2.85f),
                new Vector3(-2.85f, 0.0f, 2.85f),
                new Vector3(2.85f, 0.0f, 2.85f)
            };

            for (int i = 0; i < corners.Length; i++)
            {
                Vector3 p = center + corners[i];
                CreateBox($"ArenaPillar_{i}", parent, p + new Vector3(0.0f, 1.25f, 0.0f), new Vector3(0.55f, 2.5f, 0.55f), stoneMat);
                var crystal = CreateSphere($"ArenaBeacon_{i}", parent, p + new Vector3(0.0f, 2.78f, 0.0f), new Vector3(0.42f, 0.58f, 0.42f), beaconMat);
                env.AddArenaBeacon(crystal.GetComponent<Renderer>());
            }

            env.AddArenaLight(CreatePointLight("Station3_ArenaLightA", parent, center + new Vector3(0.0f, 3.4f, 0.0f), new Color(0.22f, 0.55f, 0.95f), 1.6f, 11.0f));
            CreateSign(parent, "Sign_Station3", "ESTACAO 3: ARENA ADAPTATIVA\nEntre ou pressione [C] (CombatIntensity)", center + new Vector3(0.0f, 3.6f, 0.0f), new Color(1.0f, 0.55f, 0.48f));
        }

        private static void BuildStation4ForestGrove(AudioPlaygroundEnvironment env, Transform parent, Material stoneMat)
        {
            Material mossMat = CreateGrassMaterial(env, 256, 0.0f, 0.12f, new Vector2(4f, 4f), 0.8f);
            Material barkMat = CreateWoodMaterial(env, 128, 0.0f, 0.14f, new Vector2(1f, 3f), 1.2f);
            Material leafDarkMat = CreateSolidMaterial(env, new Color(0.10f, 0.30f, 0.13f), 0.0f, 0.16f);
            Material leafBrightMat = CreateSolidMaterial(env, new Color(0.17f, 0.44f, 0.19f), 0.0f, 0.20f);
            Material fireflyMat = CreateEmissiveMaterial(env, new Color(0.85f, 1.0f, 0.45f), new Color(0.75f, 1.0f, 0.30f) * 4.2f);

            Vector3 center = new Vector3(7.5f, 0.05f, 6.5f);
            CreateBox("Station4_ForestGrove", parent, center, new Vector3(6.4f, 0.10f, 6.4f), mossMat);

            Vector3[] treeOffsets = new[]
            {
                new Vector3(-2.1f, 0.0f, -1.8f),
                new Vector3(2.0f, 0.0f, -1.5f),
                new Vector3(-1.7f, 0.0f, 2.0f),
                new Vector3(2.2f, 0.0f, 1.9f),
                new Vector3(0.2f, 0.0f, 2.4f)
            };

            for (int i = 0; i < treeOffsets.Length; i++)
            {
                float h = 2.6f + (i % 3) * 0.55f;
                Vector3 pos = center + treeOffsets[i];
                var tree = new GameObject($"ForestTree_{i}");
                tree.transform.SetParent(parent, false);
                tree.transform.position = pos;

                CreateCylinder("Trunk", tree.transform, new Vector3(0.0f, h * 0.5f, 0.0f), new Vector3(0.28f, h * 0.5f, 0.28f), barkMat);
                CreateSphere("CanopyLow", tree.transform, new Vector3(0.0f, h + 0.35f, 0.0f), new Vector3(1.85f, 1.25f, 1.85f), leafDarkMat);
                CreateSphere("CanopyTop", tree.transform, new Vector3(0.15f, h + 1.05f, -0.1f), new Vector3(1.35f, 1.05f, 1.35f), leafBrightMat);
            }

            CreateSphere("ForestBoulder_1", parent, center + new Vector3(-0.9f, 0.32f, -0.4f), new Vector3(0.95f, 0.58f, 0.80f), stoneMat);
            CreateSphere("ForestBoulder_2", parent, center + new Vector3(1.2f, 0.24f, 0.6f), new Vector3(0.75f, 0.45f, 0.65f), stoneMat);

            for (int i = 0; i < 6; i++)
            {
                float angle = i * (Mathf.PI * 2.0f / 6.0f);
                var orb = CreateSphere($"ForestFirefly_{i}", parent, center + new Vector3(Mathf.Cos(angle) * 2.1f, 1.5f, Mathf.Sin(angle) * 2.1f), new Vector3(0.09f, 0.09f, 0.09f), fireflyMat);
                env.AddFirefly(orb.transform);
            }

            env.ForestLight = CreatePointLight("Station4_ForestLight", parent, center + new Vector3(0.0f, 3.3f, 0.0f), new Color(0.45f, 0.95f, 0.55f), 1.5f, 11.0f);
            CreateSign(parent, "Sign_Station4", "ESTACAO 4: BOSQUE SCATTERER\nEntre ou pressione [F] (Graos 3D)", center + new Vector3(0.0f, 4.1f, 0.0f), new Color(0.55f, 0.96f, 0.62f));
        }

        private static void BuildStation5CaveAndDialogue(AudioPlaygroundEnvironment env, Transform parent, Material stoneMat, Material metalMat)
        {
            Material caveFloorMat = CreateStoneMaterial(env, 256, new Color(0.13f, 0.17f, 0.25f), new Color(0.07f, 0.10f, 0.16f), 0.20f, 0.62f, new Vector2(3f, 3f), 1.05f);
            Material screenMat = CreateEmissiveMaterial(env, new Color(0.10f, 0.65f, 0.85f), new Color(0.08f, 0.45f, 0.65f) * 1.8f);
            env.DialogueScreenMaterial = screenMat;

            Vector3 center = new Vector3(0.0f, 0.05f, 14.2f);
            CreateBox("Station5_CaveTunnel", parent, center, new Vector3(6.4f, 0.10f, 6.4f), caveFloorMat);

            CreateBox("CaveWall_Left", parent, center + new Vector3(-3.1f, 1.85f, 0.0f), new Vector3(0.45f, 3.6f, 6.4f), stoneMat);
            CreateBox("CaveWall_Right", parent, center + new Vector3(3.1f, 1.85f, 0.0f), new Vector3(0.45f, 3.6f, 6.4f), stoneMat);
            CreateBox("CaveBackWall", parent, center + new Vector3(0.0f, 1.85f, 3.1f), new Vector3(6.6f, 3.6f, 0.45f), stoneMat);
            CreateBox("Station5_CaveArch", parent, center + new Vector3(0.0f, 3.65f, 0.0f), new Vector3(6.6f, 0.42f, 6.6f), stoneMat);

            for (int i = -1; i <= 1; i++)
            {
                float zOff = i * 2.2f;
                CreateBox($"CaveRib_L_{i}", parent, center + new Vector3(-2.78f, 1.8f, zOff), new Vector3(0.25f, 3.4f, 0.35f), metalMat);
                CreateBox($"CaveRib_R_{i}", parent, center + new Vector3(2.78f, 1.8f, zOff), new Vector3(0.25f, 3.4f, 0.35f), metalMat);
                CreateBox($"CaveRib_Top_{i}", parent, center + new Vector3(0.0f, 3.35f, zOff), new Vector3(5.8f, 0.25f, 0.35f), metalMat);
            }

            var voTotem = new GameObject("Station5_DialogueTotem");
            voTotem.transform.SetParent(parent, false);
            voTotem.transform.position = new Vector3(1.9f, 0.1f, 13.6f);
            voTotem.transform.rotation = Quaternion.Euler(0.0f, -25.0f, 0.0f);

            CreateBox("KioskBase", voTotem.transform, new Vector3(0.0f, 0.55f, 0.0f), new Vector3(0.9f, 1.1f, 0.7f), metalMat);
            var screen = CreateBox("KioskScreen", voTotem.transform, new Vector3(0.0f, 1.45f, -0.05f), new Vector3(0.85f, 0.65f, 0.12f), screenMat);
            screen.transform.localRotation = Quaternion.Euler(-16.0f, 0.0f, 0.0f);

            env.CaveLight = CreatePointLight("Station5_CaveLight", parent, center + new Vector3(0.0f, 2.8f, 0.0f), new Color(0.28f, 0.56f, 1.0f), 2.2f, 11.0f, LightShadows.Soft);
            env.DialogueLight = CreatePointLight("Station5_DialogueLight", voTotem.transform, new Vector3(0.0f, 1.75f, -0.6f), new Color(0.22f, 0.90f, 1.0f), 1.4f, 6.5f);

            CreateSign(parent, "Sign_Station5", "ESTACAO 5: TUNEL SNAPSHOT & VO\nEntre ou [V] Cave | [E] Dialogo Programmer", center + new Vector3(0.0f, 4.35f, -3.0f), new Color(0.42f, 0.88f, 1.0f));
        }

        private static GameObject CreateBox(string name, Transform parent, Vector3 localPos, Vector3 scale, Material mat)
        {
            var go = GameObject.CreatePrimitive(PrimitiveType.Cube);
            go.name = name;
            go.transform.SetParent(parent, false);
            go.transform.localPosition = localPos;
            go.transform.localScale = scale;
            ApplyMaterial(go, mat);
            return go;
        }

        private static GameObject CreateCylinder(string name, Transform parent, Vector3 localPos, Vector3 scale, Material mat)
        {
            var go = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            go.name = name;
            go.transform.SetParent(parent, false);
            go.transform.localPosition = localPos;
            go.transform.localScale = scale;
            ApplyMaterial(go, mat);
            return go;
        }

        private static GameObject CreateSphere(string name, Transform parent, Vector3 localPos, Vector3 scale, Material mat)
        {
            var go = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            go.name = name;
            go.transform.SetParent(parent, false);
            go.transform.localPosition = localPos;
            go.transform.localScale = scale;
            ApplyMaterial(go, mat);
            return go;
        }

        private static Light CreatePointLight(string name, Transform parent, Vector3 localPos, Color color, float intensity, float range, LightShadows shadows = LightShadows.None)
        {
            var go = new GameObject(name);
            go.transform.SetParent(parent, false);
            go.transform.localPosition = localPos;
            var light = go.AddComponent<Light>();
            light.type = LightType.Point;
            light.color = color;
            light.intensity = intensity;
            light.range = range;
            light.shadows = shadows;
            if (shadows != LightShadows.None)
            {
                light.shadowStrength = 0.75f;
                light.shadowBias = 0.03f;
                light.shadowNormalBias = 0.3f;
            }
            return light;
        }

        private static void CreateSign(Transform parent, string name, string text, Vector3 position, Color color)
        {
            var go = new GameObject(name);
            go.transform.SetParent(parent, false);
            go.transform.position = position;
            var tm = go.AddComponent<TextMesh>();
            tm.text = text;
            tm.fontSize = 36;
            tm.characterSize = 0.08f;
            tm.anchor = TextAnchor.MiddleCenter;
            tm.alignment = TextAlignment.Center;
            tm.color = color;
        }

        private static void ApplyMaterial(GameObject go, Material mat)
        {
            var r = go.GetComponent<Renderer>();
            if (r != null && mat != null)
            {
                r.sharedMaterial = mat;
            }
        }

        private static Shader FindLitShader()
        {
            Shader s = Shader.Find("Standard");
            if (s != null) return s;
            s = Shader.Find("Legacy Shaders/Diffuse");
            if (s != null) return s;
            return Shader.Find("Sprites/Default");
        }

        private static Material CreateSolidMaterial(AudioPlaygroundEnvironment env, Color color, float metallic, float smoothness)
        {
            Shader shader = FindLitShader();
            if (shader == null) return null;
            var mat = new Material(shader);
            mat.color = color;
            if (mat.HasProperty("_Metallic")) mat.SetFloat("_Metallic", metallic);
            if (mat.HasProperty("_Glossiness")) mat.SetFloat("_Glossiness", smoothness);
            env.TrackMaterial(mat);
            return mat;
        }

        private static Material CreateEmissiveMaterial(AudioPlaygroundEnvironment env, Color baseColor, Color emissionColor)
        {
            Material mat = CreateSolidMaterial(env, baseColor, 0.2f, 0.8f);
            if (mat != null && mat.HasProperty("_EmissionColor"))
            {
                mat.EnableKeyword("_EMISSION");
                mat.SetColor("_EmissionColor", emissionColor);
            }
            return mat;
        }

        private static Material CreateTexturedMaterial(AudioPlaygroundEnvironment env, Texture2D albedo, Texture2D normalMap, float metallic, float smoothness, Vector2 tiling, float bumpScale)
        {
            Material mat = CreateSolidMaterial(env, Color.white, metallic, smoothness);
            if (mat != null)
            {
                if (albedo != null)
                {
                    mat.mainTexture = albedo;
                    mat.mainTextureScale = tiling;
                }
                if (normalMap != null && mat.HasProperty("_BumpMap"))
                {
                    mat.EnableKeyword("_NORMALMAP");
                    mat.SetTexture("_BumpMap", normalMap);
                    mat.SetTextureScale("_BumpMap", tiling);
                    if (mat.HasProperty("_BumpScale")) mat.SetFloat("_BumpScale", bumpScale);
                }
            }
            return mat;
        }

        private static Texture2D BuildNormalMapFromHeight(AudioPlaygroundEnvironment env, float[] height, int size, float strength)
        {
            var normalTex = new Texture2D(size, size, TextureFormat.RGBA32, true, true);
            normalTex.wrapMode = TextureWrapMode.Repeat;
            var pixels = new Color[size * size];

            for (int y = 0; y < size; y++)
            {
                int ym = (y - 1 + size) % size;
                int yp = (y + 1) % size;
                for (int x = 0; x < size; x++)
                {
                    int xm = (x - 1 + size) % size;
                    int xp = (x + 1) % size;

                    float hL = height[y * size + xm];
                    float hR = height[y * size + xp];
                    float hD = height[ym * size + x];
                    float hU = height[yp * size + x];

                    float dx = (hL - hR) * strength;
                    float dy = (hD - hU) * strength;
                    Vector3 n = new Vector3(dx, dy, 1.0f).normalized;
                    pixels[y * size + x] = new Color(n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, n.z * 0.5f + 0.5f, 1.0f);
                }
            }

            normalTex.SetPixels(pixels);
            normalTex.Apply(true);
            env.TrackTexture(normalTex);
            return normalTex;
        }

        private static Material CreateWoodMaterial(AudioPlaygroundEnvironment env, int size, float metallic, float smoothness, Vector2 tiling, float bumpScale)
        {
            var tex = new Texture2D(size, size, TextureFormat.RGBA32, true);
            tex.wrapMode = TextureWrapMode.Repeat;
            var pixels = new Color[size * size];
            var heights = new float[size * size];
            int planks = 6;
            int plankW = size / planks;

            for (int y = 0; y < size; y++)
            {
                for (int x = 0; x < size; x++)
                {
                    int plankIdx = x / plankW;
                    int localX = x % plankW;
                    bool isSeam = (localX < 2 || y % (size / 2) < 2);
                    float seam = isSeam ? 0.55f : 1.0f;
                    float grain = Mathf.Sin((y * 0.08f + plankIdx * 1.9f) + Mathf.Sin(x * 0.28f) * 1.4f) * 0.5f + 0.5f;
                    float fine = Hash2D(x, y) * 0.08f;
                    float tint = 0.88f + 0.14f * ((plankIdx % 3) * 0.5f);
                    Color baseWood = new Color(0.46f, 0.28f, 0.14f);
                    Color darkWood = new Color(0.31f, 0.18f, 0.09f);
                    Color c = Color.Lerp(darkWood, baseWood, grain) * tint * seam;
                    c.r = Mathf.Clamp01(c.r + fine);
                    c.g = Mathf.Clamp01(c.g + fine * 0.7f);
                    c.b = Mathf.Clamp01(c.b + fine * 0.4f);
                    c.a = 1.0f;
                    pixels[y * size + x] = c;
                    heights[y * size + x] = isSeam ? 0.12f : (0.65f + grain * 0.25f + fine);
                }
            }

            tex.SetPixels(pixels);
            tex.Apply(true);
            env.TrackTexture(tex);
            Texture2D normalMap = BuildNormalMapFromHeight(env, heights, size, 2.8f);
            return CreateTexturedMaterial(env, tex, normalMap, metallic, smoothness, tiling, bumpScale);
        }

        private static Material CreateConcreteMaterial(AudioPlaygroundEnvironment env, int size, Color lightTone, Color groutTone, float metallic, float smoothness, Vector2 tiling, float bumpScale)
        {
            var tex = new Texture2D(size, size, TextureFormat.RGBA32, true);
            tex.wrapMode = TextureWrapMode.Repeat;
            var pixels = new Color[size * size];
            var heights = new float[size * size];
            int half = size / 2;

            for (int y = 0; y < size; y++)
            {
                for (int x = 0; x < size; x++)
                {
                    bool isGrout = (x % half < 3) || (y % half < 3);
                    float n = (Hash2D(x, y) - 0.5f) * 0.08f + (Hash2D(x / 4, y / 4) - 0.5f) * 0.06f;
                    Color c = isGrout ? groutTone : lightTone;
                    c.r = Mathf.Clamp01(c.r + n);
                    c.g = Mathf.Clamp01(c.g + n);
                    c.b = Mathf.Clamp01(c.b + n);
                    c.a = 1.0f;
                    pixels[y * size + x] = c;
                    heights[y * size + x] = isGrout ? 0.15f : (0.72f + n * 1.4f);
                }
            }

            tex.SetPixels(pixels);
            tex.Apply(true);
            env.TrackTexture(tex);
            Texture2D normalMap = BuildNormalMapFromHeight(env, heights, size, 2.4f);
            return CreateTexturedMaterial(env, tex, normalMap, metallic, smoothness, tiling, bumpScale);
        }

        private static Material CreateGrassMaterial(AudioPlaygroundEnvironment env, int size, float metallic, float smoothness, Vector2 tiling, float bumpScale)
        {
            var tex = new Texture2D(size, size, TextureFormat.RGBA32, true);
            tex.wrapMode = TextureWrapMode.Repeat;
            var pixels = new Color[size * size];
            var heights = new float[size * size];

            for (int y = 0; y < size; y++)
            {
                for (int x = 0; x < size; x++)
                {
                    float patch = Hash2D(x / 8, y / 8);
                    float blade = Hash2D(x, y);
                    Color deep = new Color(0.11f, 0.28f, 0.10f);
                    Color lush = new Color(0.22f, 0.46f, 0.16f);
                    Color dry = new Color(0.30f, 0.42f, 0.15f);
                    Color c = Color.Lerp(deep, lush, blade);
                    c = Color.Lerp(c, dry, patch * 0.35f);
                    c.a = 1.0f;
                    pixels[y * size + x] = c;
                    heights[y * size + x] = 0.35f + blade * 0.45f + patch * 0.20f;
                }
            }

            tex.SetPixels(pixels);
            tex.Apply(true);
            env.TrackTexture(tex);
            Texture2D normalMap = BuildNormalMapFromHeight(env, heights, size, 2.0f);
            return CreateTexturedMaterial(env, tex, normalMap, metallic, smoothness, tiling, bumpScale);
        }

        private static Material CreateStoneMaterial(AudioPlaygroundEnvironment env, int size, Color stoneA, Color stoneB, float metallic, float smoothness, Vector2 tiling, float bumpScale)
        {
            var tex = new Texture2D(size, size, TextureFormat.RGBA32, true);
            tex.wrapMode = TextureWrapMode.Repeat;
            var pixels = new Color[size * size];
            var heights = new float[size * size];

            for (int y = 0; y < size; y++)
            {
                int row = y / 32;
                int offset = (row % 2) * 32;
                for (int x = 0; x < size; x++)
                {
                    bool mortar = (y % 32 < 2) || ((x + offset) % 64 < 2);
                    float blockTint = Hash2D((x + offset) / 64, row) * 0.16f - 0.08f;
                    float grit = (Hash2D(x, y) - 0.5f) * 0.07f;
                    Color c = mortar ? (stoneB * 0.65f) : Color.Lerp(stoneB, stoneA, 0.5f + blockTint);
                    c.r = Mathf.Clamp01(c.r + grit);
                    c.g = Mathf.Clamp01(c.g + grit);
                    c.b = Mathf.Clamp01(c.b + grit);
                    c.a = 1.0f;
                    pixels[y * size + x] = c;
                    heights[y * size + x] = mortar ? 0.10f : (0.70f + blockTint + grit * 1.5f);
                }
            }

            tex.SetPixels(pixels);
            tex.Apply(true);
            env.TrackTexture(tex);
            Texture2D normalMap = BuildNormalMapFromHeight(env, heights, size, 3.2f);
            return CreateTexturedMaterial(env, tex, normalMap, metallic, smoothness, tiling, bumpScale);
        }

        private static float Hash2D(int x, int y)
        {
            int n = x * 374761393 + y * 668265263;
            n = (n ^ (n >> 13)) * 1274126177;
            return ((n ^ (n >> 16)) & 0x7FFFFFFF) / 2147483647.0f;
        }
    }
}
#endif
