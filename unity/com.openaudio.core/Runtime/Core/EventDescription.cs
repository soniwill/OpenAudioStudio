using System;

namespace OpenAudio
{
    public sealed class EventDescription
    {
        private readonly ulong m_Handle;

        public ulong Handle => m_Handle;
        public bool IsValid => m_Handle != 0;

        internal EventDescription(ulong handle)
        {
            m_Handle = handle;
        }

        public EventInstance CreateInstance()
        {
            OaeException.Check(OaeNative.oae_event_description_create_instance(m_Handle, out ulong instanceHandle), "oae_event_description_create_instance");
            return new EventInstance(instanceHandle);
        }

        public uint LengthMs
        {
            get
            {
                OaeException.Check(OaeNative.oae_event_description_get_length_ms(m_Handle, out uint len), "oae_event_description_get_length_ms");
                return len;
            }
        }

        public bool IsOneShot
        {
            get
            {
                OaeException.Check(OaeNative.oae_event_description_is_oneshot(m_Handle, out int oneShot), "oae_event_description_is_oneshot");
                return oneShot != 0;
            }
        }

        public int InstanceCount
        {
            get
            {
                OaeException.Check(OaeNative.oae_event_description_get_instance_count(m_Handle, out int count), "oae_event_description_get_instance_count");
                return count;
            }
        }

        public void LoadSampleData()
        {
            OaeException.Check(OaeNative.oae_event_description_load_sample_data(m_Handle), "oae_event_description_load_sample_data");
        }

        public void UnloadSampleData()
        {
            OaeException.Check(OaeNative.oae_event_description_unload_sample_data(m_Handle), "oae_event_description_unload_sample_data");
        }
    }
}
