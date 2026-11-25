#if UNITY_5_3_OR_NEWER
using UnityEngine;

namespace OpenAudio
{
    /// <summary>
    /// Built-in RP HDR post-processing stack with multi-pass cinematic bloom, ACES filmic tone mapping, split-tone color grading, and vignette.
    /// </summary>
    [DisallowMultipleComponent]
    [RequireComponent(typeof(Camera))]
    [ImageEffectAllowedInSceneView]
    public sealed class AudioPlaygroundPostProcess : MonoBehaviour
    {
        [Header("Tone Mapping & Exposure")]
        [SerializeField, Range(0.4f, 2.5f)] private float m_Exposure = 1.08f;
        [SerializeField, Range(0.8f, 1.35f)] private float m_Contrast = 1.06f;
        [SerializeField, Range(0.7f, 1.4f)] private float m_Saturation = 1.08f;

        [Header("HDR Cinematic Bloom")]
        [SerializeField, Range(0.3f, 2.5f)] private float m_BloomThreshold = 0.95f;
        [SerializeField, Range(0.1f, 1.0f)] private float m_BloomSoftKnee = 0.55f;
        [SerializeField, Range(0.0f, 1.5f)] private float m_BloomIntensity = 0.42f;

        [Header("Lens")]
        [SerializeField, Range(0.0f, 0.7f)] private float m_VignetteIntensity = 0.30f;
        [SerializeField, Range(0.1f, 0.75f)] private float m_VignetteSmoothness = 0.48f;
        [SerializeField, Range(0.0f, 0.015f)] private float m_ChromaticAberration = 0.0028f;

        private Material m_Material;

        private void OnEnable()
        {
            var cam = GetComponent<Camera>();
            if (cam != null)
            {
                cam.allowHDR = true;
                cam.allowMSAA = true;
            }
            EnsureMaterial();
        }

        private void OnDisable()
        {
            ReleaseMaterial();
        }

        private void OnDestroy()
        {
            ReleaseMaterial();
        }

        private void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            if (source == null || !EnsureMaterial())
            {
                Graphics.Blit(source, destination);
                return;
            }

            m_Material.SetFloat("_BloomThreshold", m_BloomThreshold);
            m_Material.SetFloat("_BloomSoftKnee", m_BloomSoftKnee);
            m_Material.SetFloat("_BloomIntensity", m_BloomIntensity);
            m_Material.SetFloat("_Exposure", m_Exposure);
            m_Material.SetFloat("_VignetteIntensity", m_VignetteIntensity);
            m_Material.SetFloat("_VignetteSmoothness", m_VignetteSmoothness);
            m_Material.SetFloat("_ChromaticAberration", m_ChromaticAberration);
            m_Material.SetFloat("_Contrast", m_Contrast);
            m_Material.SetFloat("_Saturation", m_Saturation);

            RenderTextureFormat hdrFormat = SystemInfo.SupportsRenderTextureFormat(RenderTextureFormat.DefaultHDR)
                ? RenderTextureFormat.DefaultHDR
                : RenderTextureFormat.Default;

            int halfW = Mathf.Max(2, source.width / 2);
            int halfH = Mathf.Max(2, source.height / 2);
            int quarterW = Mathf.Max(2, source.width / 4);
            int quarterH = Mathf.Max(2, source.height / 4);

            RenderTexture rtBright = RenderTexture.GetTemporary(halfW, halfH, 0, hdrFormat);
            RenderTexture rtBlurA = RenderTexture.GetTemporary(quarterW, quarterH, 0, hdrFormat);
            RenderTexture rtBlurB = RenderTexture.GetTemporary(quarterW, quarterH, 0, hdrFormat);

            try
            {
                Graphics.Blit(source, rtBright, m_Material, 0);
                Graphics.Blit(rtBright, rtBlurA, m_Material, 1);
                Graphics.Blit(rtBlurA, rtBlurB, m_Material, 2);
                Graphics.Blit(rtBlurB, rtBlurA, m_Material, 1);
                Graphics.Blit(rtBlurA, rtBlurB, m_Material, 2);

                m_Material.SetTexture("_BloomTex", rtBlurB);
                Graphics.Blit(source, destination, m_Material, 3);
            }
            finally
            {
                RenderTexture.ReleaseTemporary(rtBright);
                RenderTexture.ReleaseTemporary(rtBlurA);
                RenderTexture.ReleaseTemporary(rtBlurB);
            }
        }

        private bool EnsureMaterial()
        {
            if (m_Material != null)
                return true;

            Shader shader = Resources.Load<Shader>("Shaders/AudioPlaygroundPostProcess");
            if (shader == null)
            {
                shader = Shader.Find("Hidden/OpenAudio/PlaygroundPostProcess");
            }
            if (shader == null || !shader.isSupported)
                return false;

            m_Material = new Material(shader)
            {
                hideFlags = HideFlags.HideAndDontSave
            };
            return true;
        }

        private void ReleaseMaterial()
        {
            if (m_Material != null)
            {
                DestroyImmediate(m_Material);
                m_Material = null;
            }
        }
    }
}
#endif
