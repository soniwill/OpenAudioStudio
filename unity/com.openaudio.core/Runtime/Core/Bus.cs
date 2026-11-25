using System;

namespace OpenAudio
{
    public sealed class Bus
    {
        private ulong m_Handle;

        public ulong Handle => m_Handle;
        public bool IsValid => m_Handle != 0;

        internal Bus(ulong handle)
        {
            m_Handle = handle;
        }

        public float Volume
        {
            get
            {
                OaeException.Check(OaeNative.oae_bus_get_volume(m_Handle, out float vol), "oae_bus_get_volume");
                return vol;
            }
            set
            {
                OaeException.Check(OaeNative.oae_bus_set_volume(m_Handle, value), "oae_bus_set_volume");
            }
        }

        public float Pan
        {
            get
            {
                OaeException.Check(OaeNative.oae_bus_get_pan(m_Handle, out float pan), "oae_bus_get_pan");
                return pan;
            }
            set
            {
                OaeException.Check(OaeNative.oae_bus_set_pan(m_Handle, value), "oae_bus_set_pan");
            }
        }

        public bool Mute
        {
            get
            {
                OaeException.Check(OaeNative.oae_bus_get_mute(m_Handle, out int mute), "oae_bus_get_mute");
                return mute != 0;
            }
            set
            {
                OaeException.Check(OaeNative.oae_bus_set_mute(m_Handle, value ? 1 : 0), "oae_bus_set_mute");
            }
        }

        public bool Paused
        {
            get
            {
                OaeException.Check(OaeNative.oae_bus_get_paused(m_Handle, out int paused), "oae_bus_get_paused");
                return paused != 0;
            }
            set
            {
                OaeException.Check(OaeNative.oae_bus_set_paused(m_Handle, value ? 1 : 0), "oae_bus_set_paused");
            }
        }

        public float FadeGain
        {
            get
            {
                OaeException.Check(OaeNative.oae_bus_get_fade_gain(m_Handle, out float gain), "oae_bus_get_fade_gain");
                return gain;
            }
        }

        public void FadeTo(float targetVolume, uint durationMs, OaeFadeCurve curve = OaeFadeCurve.Linear)
        {
            OaeException.Check(OaeNative.oae_bus_fade_to(m_Handle, targetVolume, durationMs, (int)curve), "oae_bus_fade_to");
        }

        public void StopAll()
        {
            OaeException.Check(OaeNative.oae_bus_stop_all(m_Handle), "oae_bus_stop_all");
        }

        public OaeMeter GetMeter()
        {
            OaeException.Check(OaeNative.oae_bus_get_meter(m_Handle, out var meter), "oae_bus_get_meter");
            return meter;
        }

        public ulong AddEffect(OaeEffectType type, int index = -1, OaeEffectPosition position = OaeEffectPosition.PostFader)
        {
            OaeException.Check(OaeNative.oae_bus_add_effect_ex(m_Handle, (int)type, (int)position, index, out ulong effectHandle), "oae_bus_add_effect_ex");
            return effectHandle;
        }

        public void AddSend(Bus target, float level, OaeEffectPosition position = OaeEffectPosition.PostFader)
        {
            OaeException.Check(OaeNative.oae_bus_add_send(m_Handle, target != null ? target.Handle : 0, level, (int)position), "oae_bus_add_send");
        }

        public void RemoveSend(Bus target)
        {
            OaeException.Check(OaeNative.oae_bus_remove_send(m_Handle, target != null ? target.Handle : 0), "oae_bus_remove_send");
        }

        public void SetSendLevel(Bus target, float level)
        {
            OaeException.Check(OaeNative.oae_bus_set_send_level(m_Handle, target != null ? target.Handle : 0, level), "oae_bus_set_send_level");
        }

        public float GetSendLevel(Bus target)
        {
            OaeException.Check(OaeNative.oae_bus_get_send_level(m_Handle, target != null ? target.Handle : 0, out float level), "oae_bus_get_send_level");
            return level;
        }

        public void SetSendMute(Bus target, bool muted)
        {
            OaeException.Check(OaeNative.oae_bus_set_send_mute(m_Handle, target != null ? target.Handle : 0, muted ? 1 : 0), "oae_bus_set_send_mute");
        }

        public bool GetSendMute(Bus target)
        {
            OaeException.Check(OaeNative.oae_bus_get_send_mute(m_Handle, target != null ? target.Handle : 0, out int muted), "oae_bus_get_send_mute");
            return muted != 0;
        }

        public void Release()
        {
            if (m_Handle != 0)
            {
                OaeNative.oae_bus_release(m_Handle);
                m_Handle = 0;
            }
        }
    }
}
