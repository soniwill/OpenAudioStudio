using System;
using System.Collections.Generic;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
using UnityEngine.Playables;
#if OAE_UNITY_TIMELINE
using UnityEngine.Timeline;
#endif
#endif

namespace OpenAudio
{
    public enum OaeTimelineStopMode
    {
        AllowFadeout = 0,
        Immediate = 1,
        None = 2
    }

    /// <summary>
    /// Platform-independent controller for a Unity Timeline audio event clip.
    /// Manages EventInstance creation, parameter updates, scrubbing, and stop-on-exit.
    /// </summary>
    public sealed class OaeTimelineClipController : IDisposable
    {
        private EventInstance m_Instance;
        private bool m_Entered;

        public bool IsActive => m_Entered && m_Instance != null && m_Instance.IsValid;
        public EventInstance Instance => m_Instance;

        public void OnClipEnter(OaeSystem system, string eventPath, float volume = 1f)
        {
            if (m_Entered)
                return;

            m_Entered = true;
            if (system == null || !system.IsValid || string.IsNullOrEmpty(eventPath))
                return;

            var desc = system.GetEvent(eventPath);
            m_Instance = desc.CreateInstance();
            m_Instance.Volume = volume;
            m_Instance.Start();
        }

        public void OnClipEvaluate(IReadOnlyList<KeyValuePair<string, float>> parameters)
        {
            if (!IsActive || parameters == null)
                return;

            for (int i = 0; i < parameters.Count; i++)
            {
                m_Instance.SetParameter(parameters[i].Key, parameters[i].Value, ignoreSeekSpeed: true);
            }
        }

        public void OnClipExit(OaeTimelineStopMode stopMode)
        {
            if (!m_Entered)
                return;

            m_Entered = false;
            if (m_Instance != null && m_Instance.IsValid)
            {
                if (stopMode == OaeTimelineStopMode.AllowFadeout)
                    m_Instance.Stop(OaeStopMode.AllowFadeout);
                else if (stopMode == OaeTimelineStopMode.Immediate)
                    m_Instance.Stop(OaeStopMode.Immediate);
            }
            Dispose();
        }

        public void Dispose()
        {
            if (m_Instance != null)
            {
                m_Instance.Dispose();
                m_Instance = null;
            }
            m_Entered = false;
        }
    }

#if UNITY_5_3_OR_NEWER && OAE_UNITY_TIMELINE
    [Serializable]
    public struct OaeTimelineParameterCurve
    {
        public string parameterName;
        public AnimationCurve curve;
    }

    [TrackColor(0.15f, 0.72f, 0.85f)]
    [TrackClipType(typeof(OaeEventClip))]
    public class OaeEventTrack : TrackAsset
    {
    }

    [Serializable]
    public class OaeEventClip : PlayableAsset, ITimelineClipAsset
    {
        [EventRef]
        public string eventPath;
        [Range(0f, 2f)]
        public float volume = 1f;
        public OaeTimelineStopMode stopMode = OaeTimelineStopMode.AllowFadeout;
        public List<OaeTimelineParameterCurve> parameterCurves = new List<OaeTimelineParameterCurve>();

        public ClipCaps clipCaps => ClipCaps.Blending | ClipCaps.ClipIn | ClipCaps.SpeedMultiplier;

        public override Playable CreatePlayable(PlayableGraph graph, GameObject owner)
        {
            var playable = ScriptPlayable<OaeEventPlayableBehaviour>.Create(graph);
            var behaviour = playable.GetBehaviour();
            behaviour.eventPath = eventPath;
            behaviour.volume = volume;
            behaviour.stopMode = stopMode;
            behaviour.parameterCurves = parameterCurves;
            return playable;
        }
    }

    public class OaeEventPlayableBehaviour : PlayableBehaviour
    {
        public string eventPath;
        public float volume = 1f;
        public OaeTimelineStopMode stopMode = OaeTimelineStopMode.AllowFadeout;
        public List<OaeTimelineParameterCurve> parameterCurves;

        private readonly OaeTimelineClipController m_Controller = new OaeTimelineClipController();
        private readonly List<KeyValuePair<string, float>> m_EvalBuffer = new List<KeyValuePair<string, float>>();

        public override void OnBehaviourPlay(Playable playable, FrameData info)
        {
            var sys = OaeAudioManager.Instance != null ? OaeAudioManager.Instance.System : null;
            m_Controller.OnClipEnter(sys, eventPath, volume);
        }

        public override void ProcessFrame(Playable playable, FrameData info, object playerData)
        {
            if (!m_Controller.IsActive || parameterCurves == null || parameterCurves.Count == 0)
                return;

            float t = (float)playable.GetTime();
            m_EvalBuffer.Clear();
            for (int i = 0; i < parameterCurves.Count; i++)
            {
                var pc = parameterCurves[i];
                if (!string.IsNullOrEmpty(pc.parameterName) && pc.curve != null)
                {
                    m_EvalBuffer.Add(new KeyValuePair<string, float>(pc.parameterName, pc.curve.Evaluate(t)));
                }
            }
            m_Controller.OnClipEvaluate(m_EvalBuffer);
        }

        public override void OnBehaviourPause(Playable playable, FrameData info)
        {
            m_Controller.OnClipExit(stopMode);
        }

        public override void OnPlayableDestroy(Playable playable)
        {
            m_Controller.OnClipExit(OaeTimelineStopMode.Immediate);
            m_Controller.Dispose();
        }
    }
#endif
}
