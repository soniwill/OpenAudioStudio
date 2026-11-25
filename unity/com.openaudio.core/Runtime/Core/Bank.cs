using System;

namespace OpenAudio
{
    public sealed class Bank : IDisposable
    {
        private ulong m_Handle;
        private readonly ulong m_SystemHandle;

        public ulong Handle => m_Handle;
        public bool IsValid => m_Handle != 0;

        internal Bank(ulong handle, ulong systemHandle)
        {
            m_Handle = handle;
            m_SystemHandle = systemHandle;
        }

        public OaeBankLoadingState LoadingState
        {
            get
            {
                if (m_Handle == 0)
                    return OaeBankLoadingState.Failed;
                OaeException.Check(OaeNative.oae_studio_get_bank_loading_state(m_SystemHandle, m_Handle, out int state), "oae_studio_get_bank_loading_state");
                return (OaeBankLoadingState)state;
            }
        }

        public bool IsLoaded => LoadingState == OaeBankLoadingState.Loaded;

        public void Unload()
        {
            if (m_Handle != 0)
            {
                OaeNative.oae_studio_unload_bank(m_SystemHandle, m_Handle);
                m_Handle = 0;
            }
        }

        public void Dispose()
        {
            Unload();
            GC.SuppressFinalize(this);
        }

        ~Bank()
        {
            if (m_Handle != 0)
            {
                OaeNative.oae_studio_unload_bank(m_SystemHandle, m_Handle);
                m_Handle = 0;
            }
        }
    }
}
