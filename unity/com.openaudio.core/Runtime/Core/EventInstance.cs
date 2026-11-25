using System;
using System.Runtime.InteropServices;

namespace OpenAudio
{
    public sealed class EventInstance : IDisposable
    {
        private ulong m_Handle;
        private uint m_ActiveCallbackMask = 0;

        public ulong Handle => m_Handle;
        public bool IsValid => m_Handle != 0;

        public event Action OnStarted;
        public event Action OnStopped;
        public event Action<OaeTimelineMarkerProperties> OnTimelineMarker;
        public event Action<OaeTimelineBeatProperties> OnTimelineBeat;
        public event Action OnStolen;
        public event Action<OaeEventSoundProperties> OnSoundPlayed;
        public event Action<OaeEventSoundProperties> OnSoundStopped;

        internal EventInstance(ulong handle)
        {
            m_Handle = handle;
            if (m_Handle != 0)
            {
                // Register trampoline callback for all event types
                OaeNative.RegisterInstanceCallback(m_Handle, DispatchCallback);
                m_ActiveCallbackMask = (uint)OaeEventCallbackType.All;
                OaeNative.oae_event_instance_set_callback(m_Handle, OaeNative.StaticCallback, m_ActiveCallbackMask, IntPtr.Zero);
            }
        }

        private void DispatchCallback(uint typeMask, ulong instanceHandle, IntPtr properties)
        {
            if (instanceHandle != m_Handle)
                return;

            var type = (OaeEventCallbackType)typeMask;

            if ((type & OaeEventCallbackType.Started) != 0)
            {
                OnStarted?.Invoke();
            }

            if ((type & OaeEventCallbackType.Stopped) != 0)
            {
                OnStopped?.Invoke();
            }

            if ((type & OaeEventCallbackType.Marker) != 0 && properties != IntPtr.Zero)
            {
                var marker = Marshal.PtrToStructure<OaeTimelineMarkerProperties>(properties);
                OnTimelineMarker?.Invoke(marker);
            }

            if ((type & OaeEventCallbackType.Beat) != 0 && properties != IntPtr.Zero)
            {
                var beat = Marshal.PtrToStructure<OaeTimelineBeatProperties>(properties);
                OnTimelineBeat?.Invoke(beat);
            }

            if ((type & OaeEventCallbackType.Stolen) != 0)
            {
                OnStolen?.Invoke();
            }

            if ((type & OaeEventCallbackType.SoundPlayed) != 0 && properties != IntPtr.Zero)
            {
                var snd = Marshal.PtrToStructure<OaeEventSoundProperties>(properties);
                OnSoundPlayed?.Invoke(snd);
            }

            if ((type & OaeEventCallbackType.SoundStopped) != 0 && properties != IntPtr.Zero)
            {
                var snd = Marshal.PtrToStructure<OaeEventSoundProperties>(properties);
                OnSoundStopped?.Invoke(snd);
            }
        }

        public void Start()
        {
            if (m_Handle == 0) return;
            var res = OaeNative.oae_event_instance_start(m_Handle);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return;
            }
            OaeException.Check(res, "oae_event_instance_start");
        }

        public void Stop(OaeStopMode mode = OaeStopMode.AllowFadeout)
        {
            if (m_Handle == 0) return;
            var res = OaeNative.oae_event_instance_stop(m_Handle, (int)mode);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return;
            }
            OaeException.Check(res, "oae_event_instance_stop");
        }

        public void KeyOff()
        {
            if (m_Handle == 0) return;
            var res = OaeNative.oae_event_instance_key_off(m_Handle);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return;
            }
            OaeException.Check(res, "oae_event_instance_key_off");
        }

        public void Release()
        {
            if (m_Handle != 0)
            {
                OaeNative.UnregisterInstanceCallback(m_Handle);
                OaeNative.oae_event_instance_set_callback(m_Handle, null, 0, IntPtr.Zero);
                OaeNative.oae_event_instance_release(m_Handle);
                m_Handle = 0;
            }
        }

        public void Dispose()
        {
            Release();
            GC.SuppressFinalize(this);
        }

        ~EventInstance()
        {
            if (m_Handle != 0)
            {
                OaeNative.UnregisterInstanceCallback(m_Handle);
                OaeNative.oae_event_instance_release(m_Handle);
                m_Handle = 0;
            }
        }

        public OaePlaybackState PlaybackState
        {
            get
            {
                if (m_Handle == 0) return OaePlaybackState.Stopped;
                var res = OaeNative.oae_event_instance_get_playback_state(m_Handle, out int state);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return OaePlaybackState.Stopped;
                }
                OaeException.Check(res, "oae_event_instance_get_playback_state");
                return (OaePlaybackState)state;
            }
        }

        public float Volume
        {
            get
            {
                if (m_Handle == 0) return 0f;
                var res = OaeNative.oae_event_instance_get_volume(m_Handle, out float vol);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return 0f;
                }
                OaeException.Check(res, "oae_event_instance_get_volume");
                return vol;
            }
            set
            {
                if (m_Handle == 0) return;
                var res = OaeNative.oae_event_instance_set_volume(m_Handle, value);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return;
                }
                OaeException.Check(res, "oae_event_instance_set_volume");
            }
        }

        public float Pitch
        {
            get
            {
                if (m_Handle == 0) return 1f;
                var res = OaeNative.oae_event_instance_get_pitch(m_Handle, out float pitch);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return 1f;
                }
                OaeException.Check(res, "oae_event_instance_get_pitch");
                return pitch;
            }
            set
            {
                if (m_Handle == 0) return;
                var res = OaeNative.oae_event_instance_set_pitch(m_Handle, value);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return;
                }
                OaeException.Check(res, "oae_event_instance_set_pitch");
            }
        }

        public float Pan
        {
            get
            {
                if (m_Handle == 0) return 0f;
                var res = OaeNative.oae_event_instance_get_pan(m_Handle, out float pan);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return 0f;
                }
                OaeException.Check(res, "oae_event_instance_get_pan");
                return pan;
            }
            set
            {
                if (m_Handle == 0) return;
                var res = OaeNative.oae_event_instance_set_pan(m_Handle, value);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return;
                }
                OaeException.Check(res, "oae_event_instance_set_pan");
            }
        }

        public int TimelinePositionMs
        {
            get
            {
                if (m_Handle == 0) return 0;
                var res = OaeNative.oae_event_instance_get_timeline_position_ms(m_Handle, out int pos);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return 0;
                }
                OaeException.Check(res, "oae_event_instance_get_timeline_position_ms");
                return pos;
            }
            set
            {
                if (m_Handle == 0) return;
                var res = OaeNative.oae_event_instance_set_timeline_position_ms(m_Handle, value);
                if (res == OaeResult.ERR_INVALID_HANDLE)
                {
                    m_Handle = 0;
                    return;
                }
                OaeException.Check(res, "oae_event_instance_set_timeline_position_ms");
            }
        }

        public void SetParameter(string name, float value, bool ignoreSeekSpeed = false)
        {
            if (m_Handle == 0) return;
            var res = OaeNative.oae_event_instance_set_parameter_by_name(m_Handle, name, value, ignoreSeekSpeed ? 1 : 0);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return;
            }
            OaeException.Check(res, "oae_event_instance_set_parameter_by_name");
        }

        public void SetParameterWithLabel(string name, string label, bool ignoreSeekSpeed = false)
        {
            if (m_Handle == 0) return;
            var res = OaeNative.oae_event_instance_set_parameter_by_name_with_label(m_Handle, name, label, ignoreSeekSpeed ? 1 : 0);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return;
            }
            OaeException.Check(res, "oae_event_instance_set_parameter_by_name_with_label");
        }

        public void GetParameter(string name, out float value, out float finalValue)
        {
            if (m_Handle == 0)
            {
                value = 0f;
                finalValue = 0f;
                return;
            }
            var res = OaeNative.oae_event_instance_get_parameter_by_name(m_Handle, name, out value, out finalValue);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                value = 0f;
                finalValue = 0f;
                return;
            }
            OaeException.Check(res, "oae_event_instance_get_parameter_by_name");
        }

        public Bus GetTrackBus(uint trackIndex)
        {
            OaeException.Check(OaeNative.oae_event_instance_get_track_bus(m_Handle, trackIndex, out ulong busHandle), "oae_event_instance_get_track_bus");
            return new Bus(busHandle);
        }

        public void Set3DAttributes(Oae3DAttributes attributes)
        {
            if (m_Handle == 0) return;
            var res = OaeNative.oae_event_instance_set_3d_attributes(m_Handle, ref attributes);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return;
            }
            OaeException.Check(res, "oae_event_instance_set_3d_attributes");
        }

        public Oae3DAttributes Get3DAttributes()
        {
            if (m_Handle == 0) return Oae3DAttributes.Identity;
            var res = OaeNative.oae_event_instance_get_3d_attributes(m_Handle, out var attributes);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return Oae3DAttributes.Identity;
            }
            OaeException.Check(res, "oae_event_instance_get_3d_attributes");
            return attributes;
        }

        public void SetSpatializerSettings(OaeSpatializerSettings settings)
        {
            if (m_Handle == 0) return;
            var res = OaeNative.oae_event_instance_set_spatializer_settings(m_Handle, ref settings);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return;
            }
            OaeException.Check(res, "oae_event_instance_set_spatializer_settings");
        }

        public OaeSpatializerSettings GetSpatializerSettings()
        {
            if (m_Handle == 0) return OaeSpatializerSettings.Default;
            var res = OaeNative.oae_event_instance_get_spatializer_settings(m_Handle, out var settings);
            if (res == OaeResult.ERR_INVALID_HANDLE)
            {
                m_Handle = 0;
                return OaeSpatializerSettings.Default;
            }
            OaeException.Check(res, "oae_event_instance_get_spatializer_settings");
            return settings;
        }

        public EventDescription Description
        {
            get
            {
                OaeException.Check(OaeNative.oae_event_instance_get_description(m_Handle, out ulong descHandle), "oae_event_instance_get_description");
                return new EventDescription(descHandle);
            }
        }
    }
}
