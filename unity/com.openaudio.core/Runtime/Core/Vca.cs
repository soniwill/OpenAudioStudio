using System;

namespace OpenAudio
{
    public sealed class Vca
    {
        private readonly ulong m_Handle;

        public ulong Handle => m_Handle;
        public bool IsValid => m_Handle != 0;

        internal Vca(ulong handle)
        {
            m_Handle = handle;
        }

        public float Volume
        {
            get
            {
                OaeException.Check(OaeNative.oae_vca_get_volume(m_Handle, out float vol), "oae_vca_get_volume");
                return vol;
            }
            set
            {
                OaeException.Check(OaeNative.oae_vca_set_volume(m_Handle, value), "oae_vca_set_volume");
            }
        }
    }
}
