#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace OpenAudio.Editor
{
    /// <summary>
    /// Native Unity Editor tutorial director loading Season 1 JSON cues, highlighting Inspector fields via Highlighter,
    /// and triggering smooth SmoothStep camera moves/orbits in PlayMode without external packages.
    /// </summary>
    public class PlaygroundTutorialDirectorWindow : EditorWindow
    {
        [Serializable]
        private class EpisodeCueStep
        {
            public string id;
            public string targetApp;
            public int durationMs;
            public string caption;
            public string studioSelectType;
            public string studioSelectPath;
            public string studioHighlightId;
            public string studioHighlightLabel;
            public string studioHighlightColor;
            public float studioDimOpacity;
            public string studioTransportAction;
            public string studioControlId;
            public float studioControlValue;
            public string unityMode;
            public string unityGameObject;
            public string unityInspectorLabel;
            public string unityCameraCue;
            public float unityDurationSec;
        }

        [Serializable]
        private class EpisodeCueManifest
        {
            public string episodeId;
            public string title;
            public string studioProject;
            public string unityScene;
            public EpisodeCueStep[] steps;
        }

        private sealed class LoadedEpisode
        {
            public string FilePath;
            public EpisodeCueManifest Manifest;
        }

        private readonly List<LoadedEpisode> m_Episodes = new List<LoadedEpisode>();
        private string[] m_EpisodeLabels = Array.Empty<string>();
        private int m_SelectedEpisodeIndex = 0;
        private int m_SelectedStepIndex = 0;
        private Vector2 m_ScrollPos;
        private string m_CuesDirectory = string.Empty;
        private string m_StatusNote = "Selecione um episodio e clique em um passo para destacar o alvo.";
        private bool m_ShowAllSteps = false;

        /// <summary>Opens the OpenAudio Playground Tutorial Director window.</summary>
        [MenuItem("OpenAudio/Tutorial Director")]
        public static void ShowWindow()
        {
            var win = GetWindow<PlaygroundTutorialDirectorWindow>("Tutorial Director");
            win.minSize = new Vector2(420.0f, 440.0f);
            win.Show();
        }

        private void OnEnable()
        {
            ReloadManifests();
        }

        private void OnDisable()
        {
            StopHighlight();
        }

        private void OnDestroy()
        {
            StopHighlight();
        }

        private void OnGUI()
        {
            EditorGUILayout.Space(6.0f);
            EditorGUILayout.LabelField("Audio Playground — Tutorial Director", EditorStyles.boldLabel);

            using (new EditorGUILayout.HorizontalScope())
            {
                if (m_EpisodeLabels.Length > 0)
                {
                    int nextEp = EditorGUILayout.Popup("Episodio", m_SelectedEpisodeIndex, m_EpisodeLabels);
                    if (nextEp != m_SelectedEpisodeIndex)
                    {
                        m_SelectedEpisodeIndex = nextEp;
                        m_SelectedStepIndex = 0;
                        StopHighlight();
                        m_StatusNote = "Episodio alterado. Execute um passo para iniciar o destaque.";
                    }
                }
                else
                {
                    EditorGUILayout.LabelField("Nenhum cue JSON encontrado.");
                }

                if (GUILayout.Button("Recarregar", GUILayout.Width(90.0f)))
                {
                    ReloadManifests();
                }
            }

            if (!string.IsNullOrEmpty(m_CuesDirectory))
            {
                EditorGUILayout.LabelField("Cues:", m_CuesDirectory, EditorStyles.miniLabel);
            }

            m_ShowAllSteps = EditorGUILayout.ToggleLeft("Exibir também passos exclusivos do Studio", m_ShowAllSteps);

            var currentEp = GetCurrentEpisode();
            if (currentEp == null || currentEp.Manifest.steps == null || currentEp.Manifest.steps.Length == 0)
            {
                EditorGUILayout.HelpBox(
                    "Nenhum manifesto de cue valido encontrado em docs/tutorials/playground/cues/.",
                    MessageType.Warning);
                return;
            }

            var visibleIndices = CollectVisibleStepIndices(currentEp.Manifest);
            if (visibleIndices.Count == 0)
            {
                EditorGUILayout.HelpBox(
                    "Este episodio nao possui passos Unity (ative o toggle acima para inspecionar passos do Studio).",
                    MessageType.Info);
                return;
            }

            m_SelectedStepIndex = Mathf.Clamp(m_SelectedStepIndex, 0, visibleIndices.Count - 1);
            var activeStep = currentEp.Manifest.steps[visibleIndices[m_SelectedStepIndex]];

            EditorGUILayout.Space(6.0f);
            EditorGUILayout.HelpBox(
                $"[{activeStep.id}] {activeStep.caption}\nAlvo: {activeStep.targetApp} | Modo: {(string.IsNullOrEmpty(activeStep.unityMode) ? "-" : activeStep.unityMode)} | GameObject: {(string.IsNullOrEmpty(activeStep.unityGameObject) ? "-" : activeStep.unityGameObject)}\nInspector: {(string.IsNullOrEmpty(activeStep.unityInspectorLabel) ? "-" : activeStep.unityInspectorLabel)} | Camera Cue: {(string.IsNullOrEmpty(activeStep.unityCameraCue) ? "-" : activeStep.unityCameraCue)}",
                MessageType.Info);

            using (new EditorGUILayout.HorizontalScope())
            {
                if (GUILayout.Button("Anterior"))
                {
                    m_SelectedStepIndex = Mathf.Max(0, m_SelectedStepIndex - 1);
                    ExecuteStep(currentEp.Manifest.steps[visibleIndices[m_SelectedStepIndex]]);
                }
                if (GUILayout.Button("Repetir Passo"))
                {
                    ExecuteStep(activeStep);
                }
                if (GUILayout.Button("Próximo"))
                {
                    m_SelectedStepIndex = Mathf.Min(visibleIndices.Count - 1, m_SelectedStepIndex + 1);
                    ExecuteStep(currentEp.Manifest.steps[visibleIndices[m_SelectedStepIndex]]);
                }
                if (GUILayout.Button("Parar Destaque"))
                {
                    StopHighlight();
                    m_StatusNote = "Destaque do Inspector encerrado.";
                }
            }

            EditorGUILayout.Space(4.0f);
            EditorGUILayout.LabelField("Passos do Episodio", EditorStyles.boldLabel);
            m_ScrollPos = EditorGUILayout.BeginScrollView(m_ScrollPos, GUILayout.ExpandHeight(true));
            for (int i = 0; i < visibleIndices.Count; i++)
            {
                var step = currentEp.Manifest.steps[visibleIndices[i]];
                bool isCurrent = (i == m_SelectedStepIndex);
                GUIStyle style = isCurrent ? EditorStyles.miniButtonMid : EditorStyles.miniButton;
                Color prevBg = GUI.backgroundColor;
                if (isCurrent)
                {
                    GUI.backgroundColor = new Color(0.25f, 0.78f, 0.75f, 1.0f);
                }
                string label = $"{step.id} [{step.targetApp}] — {step.caption}";
                if (GUILayout.Button(label, style))
                {
                    m_SelectedStepIndex = i;
                    ExecuteStep(step);
                }
                GUI.backgroundColor = prevBg;
            }
            EditorGUILayout.EndScrollView();

            EditorGUILayout.Space(6.0f);
            EditorGUILayout.LabelField("Atalhos de Câmera (PlayMode)", EditorStyles.boldLabel);
            using (new EditorGUILayout.HorizontalScope())
            {
                if (GUILayout.Button("Spawn")) TriggerCameraCue("spawn", 2.0f);
                if (GUILayout.Button("Wood")) TriggerCameraCue("station1_wood", 2.0f);
                if (GUILayout.Button("Concrete")) TriggerCameraCue("station1_concrete", 1.8f);
                if (GUILayout.Button("Grass")) TriggerCameraCue("station1_grass", 1.8f);
            }
            using (new EditorGUILayout.HorizontalScope())
            {
                if (GUILayout.Button("Totem Frente")) TriggerCameraCue("station2_front", 2.2f);
                if (GUILayout.Button("Órbita 360°")) TriggerCameraCue("station2_orbit_360", 6.0f);
                if (GUILayout.Button("Arena Combate")) TriggerCameraCue("station3_combat", 2.5f);
                if (GUILayout.Button("Bosque")) TriggerCameraCue("station4_forest", 2.5f);
            }
            using (new EditorGUILayout.HorizontalScope())
            {
                if (GUILayout.Button("Caverna")) TriggerCameraCue("station5_cave", 2.5f);
                if (GUILayout.Button("Kiosk VO")) TriggerCameraCue("station5_vo_kiosk", 1.5f);
                if (GUILayout.Button("Cancelar Cue")) TriggerCameraCue("cancel", 0.0f);
            }

            if (!string.IsNullOrEmpty(m_StatusNote))
            {
                EditorGUILayout.Space(4.0f);
                EditorGUILayout.LabelField(m_StatusNote, EditorStyles.wordWrappedMiniLabel);
            }
        }

        private void ReloadManifests()
        {
            StopHighlight();
            m_Episodes.Clear();
            m_CuesDirectory = ResolveCuesDirectory();

            if (!string.IsNullOrEmpty(m_CuesDirectory) && Directory.Exists(m_CuesDirectory))
            {
                string[] files = Directory.GetFiles(m_CuesDirectory, "*.json", SearchOption.TopDirectoryOnly);
                Array.Sort(files, StringComparer.OrdinalIgnoreCase);

                foreach (string file in files)
                {
                    try
                    {
                        string json = File.ReadAllText(file);
                        var manifest = JsonUtility.FromJson<EpisodeCueManifest>(json);
                        if (manifest != null && !string.IsNullOrEmpty(manifest.episodeId))
                        {
                            m_Episodes.Add(new LoadedEpisode
                            {
                                FilePath = file,
                                Manifest = manifest
                            });
                        }
                    }
                    catch (Exception ex)
                    {
                        Debug.LogWarning($"[TutorialDirector] Falha ao carregar '{file}': {ex.Message}");
                    }
                }
            }

            m_EpisodeLabels = new string[m_Episodes.Count];
            for (int i = 0; i < m_Episodes.Count; i++)
            {
                var m = m_Episodes[i].Manifest;
                m_EpisodeLabels[i] = $"{m.episodeId} — {m.title}";
            }

            m_SelectedEpisodeIndex = Mathf.Clamp(m_SelectedEpisodeIndex, 0, Mathf.Max(0, m_Episodes.Count - 1));
            m_SelectedStepIndex = 0;
            Repaint();
        }

        private static string ResolveCuesDirectory()
        {
            var dir = new DirectoryInfo(Application.dataPath);
            for (int i = 0; i < 6 && dir != null; i++)
            {
                string candidate = Path.Combine(dir.FullName, "docs", "tutorials", "playground", "cues");
                if (Directory.Exists(candidate))
                {
                    return candidate;
                }
                dir = dir.Parent;
            }
            return string.Empty;
        }

        private LoadedEpisode GetCurrentEpisode()
        {
            if (m_SelectedEpisodeIndex < 0 || m_SelectedEpisodeIndex >= m_Episodes.Count)
            {
                return null;
            }
            return m_Episodes[m_SelectedEpisodeIndex];
        }

        private List<int> CollectVisibleStepIndices(EpisodeCueManifest manifest)
        {
            var indices = new List<int>();
            if (manifest.steps == null) return indices;

            for (int i = 0; i < manifest.steps.Length; i++)
            {
                var s = manifest.steps[i];
                bool relevant = m_ShowAllSteps
                    || string.Equals(s.targetApp, "unity", StringComparison.OrdinalIgnoreCase)
                    || string.Equals(s.targetApp, "both", StringComparison.OrdinalIgnoreCase);
                if (relevant)
                {
                    indices.Add(i);
                }
            }
            return indices;
        }

        private void ExecuteStep(EpisodeCueStep step)
        {
            if (step == null) return;
            StopHighlight();

            if (!string.IsNullOrEmpty(step.unityGameObject))
            {
                GameObject target = FindSceneGameObject(step.unityGameObject);
                if (target != null)
                {
                    Selection.activeGameObject = target;
                    EditorGUIUtility.PingObject(target);
                }
                else
                {
                    m_StatusNote = $"GameObject '{step.unityGameObject}' nao encontrado na cena atual.";
                    Debug.LogWarning($"[TutorialDirector] {m_StatusNote}");
                }
            }

            if (!string.IsNullOrEmpty(step.unityInspectorLabel))
            {
                EditorApplication.ExecuteMenuItem("Window/General/Inspector");
                string labelToHighlight = step.unityInspectorLabel;
                string targetName = step.unityGameObject;
                EditorApplication.delayCall += () =>
                {
                    bool ok = Highlighter.Highlight("Inspector", labelToHighlight);
                    if (!ok)
                    {
                        Debug.LogWarning($"[TutorialDirector] Highlighter nao encontrou o rotulo '{labelToHighlight}' no Inspector de '{targetName}'.");
                    }
                };
                m_StatusNote = $"Destaque no Inspector: '{step.unityInspectorLabel}' ({step.unityGameObject}).";
            }
            else
            {
                m_StatusNote = $"Passo ativo: {step.id}";
            }

            if (!string.IsNullOrEmpty(step.unityCameraCue))
            {
                TriggerCameraCue(step.unityCameraCue, step.unityDurationSec);
            }
        }

        private void TriggerCameraCue(string cueName, float durationSec)
        {
            if (!EditorApplication.isPlaying)
            {
                m_StatusNote = $"Cue de camera '{cueName}' requer PlayMode ativo na cena AudioPlayground.";
                return;
            }

#if UNITY_2023_1_OR_NEWER
            var ctrl = UnityEngine.Object.FindFirstObjectByType<AudioPlaygroundController>();
#else
            var ctrl = UnityEngine.Object.FindObjectOfType<AudioPlaygroundController>();
#endif
            if (ctrl == null)
            {
                m_StatusNote = "AudioPlaygroundController nao encontrado na cena em PlayMode.";
                return;
            }

            float dur = durationSec > 0.0f ? durationSec : 2.5f;
            switch (cueName)
            {
                case "spawn":
                    ctrl.BeginCameraMoveCue(0.0f, 1.6f, -4.0f, 0.0f, 5.0f, dur);
                    break;
                case "station1_wood":
                    ctrl.BeginCameraMoveCue(-4.0f, 1.6f, 0.0f, 0.0f, 10.0f, dur);
                    break;
                case "station1_concrete":
                    ctrl.BeginCameraMoveCue(0.0f, 1.6f, 0.0f, 0.0f, 10.0f, dur);
                    break;
                case "station1_grass":
                    ctrl.BeginCameraMoveCue(4.0f, 1.6f, 0.0f, 0.0f, 10.0f, dur);
                    break;
                case "station2_front":
                    ctrl.BeginCameraMoveCue(0.0f, 1.6f, 4.5f, 0.0f, 5.0f, dur);
                    break;
                case "station2_orbit_360":
                    ctrl.BeginCameraOrbitCue(0.0f, 1.2f, 8.0f, 3.6f, 1.6f, durationSec > 0.0f ? durationSec : 6.0f, 180.0f, 360.0f);
                    break;
                case "station3_combat":
                    ctrl.BeginCameraMoveCue(-7.5f, 1.6f, 6.5f, -45.0f, 5.0f, dur);
                    break;
                case "station3_exit":
                    ctrl.BeginCameraMoveCue(-2.0f, 1.6f, 4.0f, 0.0f, 5.0f, dur);
                    break;
                case "station4_forest":
                    ctrl.BeginCameraMoveCue(7.5f, 1.6f, 6.5f, 45.0f, 5.0f, dur);
                    break;
                case "station4_exit":
                    ctrl.BeginCameraMoveCue(2.0f, 1.6f, 4.0f, 0.0f, 5.0f, dur);
                    break;
                case "station5_cave":
                    ctrl.BeginCameraMoveCue(0.0f, 1.6f, 14.2f, 0.0f, 4.0f, dur);
                    break;
                case "station5_vo_kiosk":
                    ctrl.BeginCameraMoveCue(0.2f, 1.6f, 13.6f, 90.0f, 10.0f, dur);
                    break;
                case "station5_exit":
                    ctrl.BeginCameraMoveCue(0.0f, 1.6f, 9.5f, 180.0f, 5.0f, dur);
                    break;
                case "cancel":
                    ctrl.CancelCameraCue();
                    break;
                default:
                    m_StatusNote = $"Cue de camera desconhecido: '{cueName}'.";
                    return;
            }

            m_StatusNote = $"Camera cue executado: {cueName} ({dur:0.0}s).";
        }

        private static GameObject FindSceneGameObject(string objectName)
        {
            var direct = GameObject.Find(objectName);
            if (direct != null) return direct;

            for (int s = 0; s < SceneManager.sceneCount; s++)
            {
                var scene = SceneManager.GetSceneAt(s);
                if (!scene.isLoaded) continue;
                foreach (var root in scene.GetRootGameObjects())
                {
                    var match = FindInHierarchy(root.transform, objectName);
                    if (match != null) return match.gameObject;
                }
            }
            return null;
        }

        private static Transform FindInHierarchy(Transform parent, string targetName)
        {
            if (string.Equals(parent.name, targetName, StringComparison.Ordinal))
            {
                return parent;
            }
            for (int i = 0; i < parent.childCount; i++)
            {
                var found = FindInHierarchy(parent.GetChild(i), targetName);
                if (found != null) return found;
            }
            return null;
        }

        private static void StopHighlight()
        {
            Highlighter.Stop();
        }
    }
}
#endif
