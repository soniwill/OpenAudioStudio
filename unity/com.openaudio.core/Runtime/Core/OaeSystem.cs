using System;

namespace OpenAudio
{
    public sealed class OaeSystem : IDisposable
    {
        private ulong m_Handle;

        public ulong Handle => m_Handle;
        public bool IsValid => m_Handle != 0;

        public event Action<string, float> GlobalParameterChanged;

        private OaeSystem(ulong handle)
        {
            m_Handle = handle;
        }

        public static OaeSystem Create()
        {
            OaeException.Check(OaeNative.oae_system_create(out ulong handle), "oae_system_create");
            return new OaeSystem(handle);
        }

        public void Init(uint sampleRate = 48000, uint channels = 2, uint maxVoices = 64, OaeInitFlags flags = OaeInitFlags.Default)
        {
            var settings = new OaeInitSettings
            {
                sample_rate = sampleRate,
                channels = channels,
                max_physical_voices = maxVoices,
                flags = (uint)flags
            };

            OaeException.Check(OaeNative.oae_system_init(m_Handle, ref settings), "oae_system_init");
        }

        public void Update()
        {
            OaeException.Check(OaeNative.oae_system_update(m_Handle), "oae_system_update");
        }

        public void Render(float[] buffer, uint frames)
        {
            OaeException.Check(OaeNative.oae_system_render(m_Handle, buffer, frames), "oae_system_render");
        }

        public unsafe void Render(float* buffer, uint frames)
        {
            OaeException.Check(OaeNative.oae_system_render(m_Handle, buffer, frames), "oae_system_render");
        }

        public Bus GetMasterBus()
        {
            OaeException.Check(OaeNative.oae_system_get_master_bus(m_Handle, out ulong busHandle), "oae_system_get_master_bus");
            return new Bus(busHandle);
        }

        public Bus GetBus(string path)
        {
            OaeException.Check(OaeNative.oae_system_get_bus(m_Handle, path, out ulong busHandle), "oae_system_get_bus");
            return new Bus(busHandle);
        }

        public Bus CreateBus(string name, Bus parent = null)
        {
            ulong parentHandle = parent != null ? parent.Handle : 0;
            OaeException.Check(OaeNative.oae_bus_create(m_Handle, name, parentHandle, out ulong busHandle), "oae_bus_create");
            return new Bus(busHandle);
        }

        public Bus CreateReturnBus(string name, Bus parent = null)
        {
            ulong parentHandle = parent != null ? parent.Handle : 0;
            OaeException.Check(OaeNative.oae_bus_create_return(m_Handle, name, parentHandle, out ulong busHandle), "oae_bus_create_return");
            return new Bus(busHandle);
        }

        public Vca GetVca(string path)
        {
            OaeException.Check(OaeNative.oae_studio_get_vca(m_Handle, path, out ulong vcaHandle), "oae_studio_get_vca");
            return new Vca(vcaHandle);
        }

        public Bank LoadBankFile(string path)
        {
            OaeException.Check(OaeNative.oae_studio_load_bank_file(m_Handle, path, out ulong bankHandle), "oae_studio_load_bank_file");
            return new Bank(bankHandle, m_Handle);
        }

        public Bank LoadBankFileAsync(string path)
        {
            const uint OAE_LOAD_BANK_NONBLOCKING = 1u;
            OaeException.Check(OaeNative.oae_studio_load_bank_file_ex(m_Handle, path, OAE_LOAD_BANK_NONBLOCKING, out ulong bankHandle), "oae_studio_load_bank_file_ex");
            return new Bank(bankHandle, m_Handle);
        }

        public Bank LoadBankMemory(byte[] data, bool nonBlocking = false)
        {
            if (nonBlocking)
            {
                const uint OAE_LOAD_BANK_NONBLOCKING = 1u;
                OaeException.Check(OaeNative.oae_studio_load_bank_memory_ex(m_Handle, data, (UIntPtr)data.Length, OAE_LOAD_BANK_NONBLOCKING, out ulong bankHandle), "oae_studio_load_bank_memory_ex");
                return new Bank(bankHandle, m_Handle);
            }
            else
            {
                OaeException.Check(OaeNative.oae_studio_load_bank_memory(m_Handle, data, (UIntPtr)data.Length, out ulong bankHandle), "oae_studio_load_bank_memory");
                return new Bank(bankHandle, m_Handle);
            }
        }

        public EventDescription GetEvent(string path)
        {
            OaeException.Check(OaeNative.oae_studio_get_event(m_Handle, path, out ulong descHandle), "oae_studio_get_event");
            return new EventDescription(descHandle);
        }

        public void PlayOneShot(string path)
        {
            OaeException.Check(OaeNative.oae_studio_play_one_shot(m_Handle, path), "oae_studio_play_one_shot");
        }

        public void SetGlobalParameter(string name, float value, bool ignoreSeekSpeed = false)
        {
            SetGlobalParameterInternal(name, value, ignoreSeekSpeed, notifyLive: true);
        }

        internal void SetGlobalParameterInternal(string name, float value, bool ignoreSeekSpeed, bool notifyLive)
        {
            OaeException.Check(OaeNative.oae_studio_set_global_parameter_by_name(m_Handle, name, value, ignoreSeekSpeed ? 1 : 0), "oae_studio_set_global_parameter_by_name");
            if (notifyLive)
            {
                GlobalParameterChanged?.Invoke(name, value);
            }
        }

        public void SetGlobalParameterWithLabel(string name, string label, bool ignoreSeekSpeed = false)
        {
            OaeException.Check(OaeNative.oae_studio_set_global_parameter_by_name_with_label(m_Handle, name, label, ignoreSeekSpeed ? 1 : 0), "oae_studio_set_global_parameter_by_name_with_label");
        }

        public void GetGlobalParameter(string name, out float value, out float finalValue)
        {
            OaeException.Check(OaeNative.oae_studio_get_global_parameter_by_name(m_Handle, name, out value, out finalValue), "oae_studio_get_global_parameter_by_name");
        }

        public void SetListener3DAttributes(Oae3DAttributes attributes)
        {
            OaeException.Check(OaeNative.oae_system_set_listener_3d_attributes(m_Handle, ref attributes), "oae_system_set_listener_3d_attributes");
        }

        public Oae3DAttributes GetListener3DAttributes()
        {
            OaeException.Check(OaeNative.oae_system_get_listener_3d_attributes(m_Handle, out var attributes), "oae_system_get_listener_3d_attributes");
            return attributes;
        }

        public void GetChannelsPlaying(out int total, out int real)
        {
            OaeException.Check(OaeNative.oae_system_get_channels_playing(m_Handle, out total, out real), "oae_system_get_channels_playing");
        }

        public ulong GetDspClock()
        {
            OaeException.Check(OaeNative.oae_system_get_dsp_clock(m_Handle, out ulong clock), "oae_system_get_dsp_clock");
            return clock;
        }

        public void Release()
        {
            if (m_Handle != 0)
            {
                OaeNative.oae_system_release(m_Handle);
                m_Handle = 0;
            }
        }

        public void Dispose()
        {
            Release();
            GC.SuppressFinalize(this);
        }

        ~OaeSystem()
        {
            Release();
        }
    }
}
