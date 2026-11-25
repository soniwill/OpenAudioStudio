using System;
using System.Collections.Concurrent;
using System.Runtime.InteropServices;

#if !UNITY_2017_1_OR_NEWER
namespace AOT
{
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class MonoPInvokeCallbackAttribute : Attribute
    {
        public MonoPInvokeCallbackAttribute(Type type) { }
    }
}
#endif

namespace OpenAudio
{
    public static class OaeNative
    {
        public const string DllName = "oae_audio";

        #region Struct Sizes Verification

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_get_struct_sizes")]
        public static extern OaeResult oae_get_struct_sizes(out OaeStructSizes out_sizes);

        public static void VerifyStructSizes()
        {
            var res = oae_get_struct_sizes(out var sizes);
            if (res != OaeResult.OK)
                throw new OaeException(res, "Failed to retrieve native struct sizes");

            int sInit = Marshal.SizeOf<OaeInitSettings>();
            unsafe
            {
                int sMeter = sizeof(OaeMeter);
                if (sMeter != (int)sizes.meter)
                    throw new InvalidOperationException($"Struct size mismatch for OaeMeter: C#={sMeter}, Native={sizes.meter}");
            }

            int sMarker = Marshal.SizeOf<OaeTimelineMarkerProperties>();
            int sBeat = Marshal.SizeOf<OaeTimelineBeatProperties>();
            int sSound = Marshal.SizeOf<OaeEventSoundProperties>();

            if (sInit != (int)sizes.init_settings)
                throw new InvalidOperationException($"Struct size mismatch for OaeInitSettings: C#={sInit}, Native={sizes.init_settings}");
            if (sMarker != (int)sizes.timeline_marker_properties)
                throw new InvalidOperationException($"Struct size mismatch for OaeTimelineMarkerProperties: C#={sMarker}, Native={sizes.timeline_marker_properties}");
            if (sBeat != (int)sizes.timeline_beat_properties)
                throw new InvalidOperationException($"Struct size mismatch for OaeTimelineBeatProperties: C#={sBeat}, Native={sizes.timeline_beat_properties}");
            if (sSound != (int)sizes.event_sound_properties)
                throw new InvalidOperationException($"Struct size mismatch for OaeEventSoundProperties: C#={sSound}, Native={sizes.event_sound_properties}");
        }

        #endregion

        #region Core System Lifecycle (1..8)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_get_version")]
        public static extern uint oae_get_version();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_result_string")]
        public static extern IntPtr oae_result_string(OaeResult result);

        public static string GetResultString(OaeResult result)
        {
            IntPtr ptr = oae_result_string(result);
            return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) : result.ToString();
        }

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_init_settings_default")]
        public static extern OaeResult oae_init_settings_default(out OaeInitSettings settings);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_create")]
        public static extern OaeResult oae_system_create(out ulong out_system);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_init")]
        public static extern OaeResult oae_system_init(ulong system, ref OaeInitSettings settings);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_update")]
        public static extern OaeResult oae_system_update(ulong system);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_render")]
        public static extern OaeResult oae_system_render(ulong system, [Out] float[] out_buffer, uint frames);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_render")]
        public static extern unsafe OaeResult oae_system_render(ulong system, float* out_buffer, uint frames);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_release")]
        public static extern OaeResult oae_system_release(ulong system);

        #endregion

        #region Sounds (9..12)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_sound_create_from_file")]
        public static extern OaeResult oae_sound_create_from_file(ulong system, string path, uint flags, out ulong out_sound);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_sound_create_from_memory")]
        public static extern OaeResult oae_sound_create_from_memory(ulong system, byte[] data, UIntPtr size, uint flags, out ulong out_sound);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_sound_create_from_memory")]
        public static extern OaeResult oae_sound_create_from_memory(ulong system, IntPtr data, UIntPtr size, uint flags, out ulong out_sound);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_sound_release")]
        public static extern OaeResult oae_sound_release(ulong sound);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_sound_get_length_ms")]
        public static extern OaeResult oae_sound_get_length_ms(ulong sound, out uint out_ms);

        #endregion

        #region Channels (13..23)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_play_sound")]
        public static extern OaeResult oae_system_play_sound(ulong system, ulong sound, ulong bus, int paused, out ulong out_channel);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_stop")]
        public static extern OaeResult oae_channel_stop(ulong channel);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_set_paused")]
        public static extern OaeResult oae_channel_set_paused(ulong channel, int paused);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_get_paused")]
        public static extern OaeResult oae_channel_get_paused(ulong channel, out int out_paused);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_set_volume")]
        public static extern OaeResult oae_channel_set_volume(ulong channel, float volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_get_volume")]
        public static extern OaeResult oae_channel_get_volume(ulong channel, out float out_volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_set_pitch")]
        public static extern OaeResult oae_channel_set_pitch(ulong channel, float pitch);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_get_pitch")]
        public static extern OaeResult oae_channel_get_pitch(ulong channel, out float out_pitch);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_is_playing")]
        public static extern OaeResult oae_channel_is_playing(ulong channel, out int out_is_playing);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_get_position_ms")]
        public static extern OaeResult oae_channel_get_position_ms(ulong channel, out uint out_ms);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_set_position_ms")]
        public static extern OaeResult oae_channel_set_position_ms(ulong channel, uint ms);

        #endregion

        #region Buses (24..34)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_get_master_bus")]
        public static extern OaeResult oae_system_get_master_bus(ulong system, out ulong out_bus);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_bus_create")]
        public static extern OaeResult oae_bus_create(ulong system, string name, ulong parent, out ulong out_bus);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_system_get_bus")]
        public static extern OaeResult oae_system_get_bus(ulong system, string path, out ulong out_bus);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_set_volume")]
        public static extern OaeResult oae_bus_set_volume(ulong bus, float volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_volume")]
        public static extern OaeResult oae_bus_get_volume(ulong bus, out float out_volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_set_mute")]
        public static extern OaeResult oae_bus_set_mute(ulong bus, int mute);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_mute")]
        public static extern OaeResult oae_bus_get_mute(ulong bus, out int out_mute);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_set_paused")]
        public static extern OaeResult oae_bus_set_paused(ulong bus, int paused);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_paused")]
        public static extern OaeResult oae_bus_get_paused(ulong bus, out int out_paused);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_stop_all")]
        public static extern OaeResult oae_bus_stop_all(ulong bus);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_release")]
        public static extern OaeResult oae_bus_release(ulong bus);

        #endregion

        #region Voice Priority & Virtualization (35..38)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_set_priority")]
        public static extern OaeResult oae_channel_set_priority(ulong channel, int priority);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_get_priority")]
        public static extern OaeResult oae_channel_get_priority(ulong channel, out int out_priority);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_is_virtual")]
        public static extern OaeResult oae_channel_is_virtual(ulong channel, out int out_virtual);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_get_channels_playing")]
        public static extern OaeResult oae_system_get_channels_playing(ulong system, out int total, out int real);

        #endregion

        #region Timing & Panning Controls (39..50)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_set_pan")]
        public static extern OaeResult oae_channel_set_pan(ulong channel, float pan);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_get_pan")]
        public static extern OaeResult oae_channel_get_pan(ulong channel, out float out_pan);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_set_pan")]
        public static extern OaeResult oae_bus_set_pan(ulong bus, float pan);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_pan")]
        public static extern OaeResult oae_bus_get_pan(ulong bus, out float out_pan);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_fade_to")]
        public static extern OaeResult oae_channel_fade_to(ulong channel, float target, uint duration_ms, int curve);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_fade_to")]
        public static extern OaeResult oae_bus_fade_to(ulong bus, float target, uint duration_ms, int curve);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_get_fade_gain")]
        public static extern OaeResult oae_channel_get_fade_gain(ulong channel, out float out_gain);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_fade_gain")]
        public static extern OaeResult oae_bus_get_fade_gain(ulong bus, out float out_gain);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_channel_stop_with_fade")]
        public static extern OaeResult oae_channel_stop_with_fade(ulong channel, uint duration_ms);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_get_dsp_clock")]
        public static extern OaeResult oae_system_get_dsp_clock(ulong system, out ulong out_frames);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_play_sound_at")]
        public static extern OaeResult oae_system_play_sound_at(ulong system, ulong sound, ulong bus, ulong dsp_clock, out ulong out_channel);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_play_sound_delayed")]
        public static extern OaeResult oae_system_play_sound_delayed(ulong system, ulong sound, ulong bus, uint delay_ms, out ulong out_channel);

        #endregion

        #region Bus Effects & Meter (51..61)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_add_effect")]
        public static extern OaeResult oae_bus_add_effect(ulong bus, int type, int index, out ulong out_effect);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_add_effect_ex")]
        public static extern OaeResult oae_bus_add_effect_ex(ulong bus, int type, int position, int index, out ulong out_effect);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_set_param")]
        public static extern OaeResult oae_effect_set_param(ulong effect, int param_id, float value);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_get_param")]
        public static extern OaeResult oae_effect_get_param(ulong effect, int param_id, out float out_value);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_set_bypass")]
        public static extern OaeResult oae_effect_set_bypass(ulong effect, int bypass);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_get_bypass")]
        public static extern OaeResult oae_effect_get_bypass(ulong effect, out int out_bypass);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_set_wet_dry")]
        public static extern OaeResult oae_effect_set_wet_dry(ulong effect, float wet, float dry);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_get_wet_dry")]
        public static extern OaeResult oae_effect_get_wet_dry(ulong effect, out float out_wet, out float out_dry);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_set_sidechain")]
        public static extern OaeResult oae_effect_set_sidechain(ulong effect, ulong source_bus);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_effect_release")]
        public static extern OaeResult oae_effect_release(ulong effect);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_meter")]
        public static extern OaeResult oae_bus_get_meter(ulong bus, out OaeMeter out_meter);

        #endregion

        #region Studio Banks (62..64)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_studio_load_bank_memory")]
        public static extern OaeResult oae_studio_load_bank_memory(ulong system, byte[] bytes, UIntPtr size, out ulong out_bank);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_studio_load_bank_memory")]
        public static extern OaeResult oae_studio_load_bank_memory(ulong system, IntPtr bytes, UIntPtr size, out ulong out_bank);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_load_bank_file")]
        public static extern OaeResult oae_studio_load_bank_file(ulong system, string path, out ulong out_bank);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_studio_unload_bank")]
        public static extern OaeResult oae_studio_unload_bank(ulong system, ulong bank);

        #endregion

        #region VCAs (65..67)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_get_vca")]
        public static extern OaeResult oae_studio_get_vca(ulong system, string path, out ulong out_vca);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_vca_set_volume")]
        public static extern OaeResult oae_vca_set_volume(ulong vca, float volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_vca_get_volume")]
        public static extern OaeResult oae_vca_get_volume(ulong vca, out float out_volume);

        #endregion

        #region Events & Instances (68..84)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_get_event")]
        public static extern OaeResult oae_studio_get_event(ulong system, string path, out ulong out_description);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_play_one_shot")]
        public static extern OaeResult oae_studio_play_one_shot(ulong system, string path);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_description_create_instance")]
        public static extern OaeResult oae_event_description_create_instance(ulong description, out ulong out_instance);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_description_get_length_ms")]
        public static extern OaeResult oae_event_description_get_length_ms(ulong description, out uint out_length_ms);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_description_is_oneshot")]
        public static extern OaeResult oae_event_description_is_oneshot(ulong description, out int out_is_oneshot);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_description_get_instance_count")]
        public static extern OaeResult oae_event_description_get_instance_count(ulong description, out int out_count);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_start")]
        public static extern OaeResult oae_event_instance_start(ulong instance);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_stop")]
        public static extern OaeResult oae_event_instance_stop(ulong instance, int mode);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_release")]
        public static extern OaeResult oae_event_instance_release(ulong instance);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_playback_state")]
        public static extern OaeResult oae_event_instance_get_playback_state(ulong instance, out int out_state);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_description")]
        public static extern OaeResult oae_event_instance_get_description(ulong instance, out ulong out_description);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_set_volume")]
        public static extern OaeResult oae_event_instance_set_volume(ulong instance, float volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_volume")]
        public static extern OaeResult oae_event_instance_get_volume(ulong instance, out float out_volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_set_pitch")]
        public static extern OaeResult oae_event_instance_set_pitch(ulong instance, float pitch);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_pitch")]
        public static extern OaeResult oae_event_instance_get_pitch(ulong instance, out float out_pitch);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_set_pan")]
        public static extern OaeResult oae_event_instance_set_pan(ulong instance, float pan);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_pan")]
        public static extern OaeResult oae_event_instance_get_pan(ulong instance, out float out_pan);

        #endregion

        #region Callbacks, Timeline & Parameters (85..94)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_set_callback")]
        public static extern OaeResult oae_event_instance_set_callback(ulong instance, OaeEventCallback callback, uint mask, IntPtr user_data);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_key_off")]
        public static extern OaeResult oae_event_instance_key_off(ulong instance);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_timeline_position_ms")]
        public static extern OaeResult oae_event_instance_get_timeline_position_ms(ulong instance, out int out_position_ms);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_set_timeline_position_ms")]
        public static extern OaeResult oae_event_instance_set_timeline_position_ms(ulong instance, int position_ms);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_event_instance_set_parameter_by_name")]
        public static extern OaeResult oae_event_instance_set_parameter_by_name(ulong instance, string name, float value, int ignore_seek_speed);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_event_instance_set_parameter_by_name_with_label")]
        public static extern OaeResult oae_event_instance_set_parameter_by_name_with_label(ulong instance, string name, string label, int ignore_seek_speed);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_event_instance_get_parameter_by_name")]
        public static extern OaeResult oae_event_instance_get_parameter_by_name(ulong instance, string name, out float out_value, out float out_final_value);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_set_global_parameter_by_name")]
        public static extern OaeResult oae_studio_set_global_parameter_by_name(ulong system, string name, float value, int ignore_seek_speed);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_set_global_parameter_by_name_with_label")]
        public static extern OaeResult oae_studio_set_global_parameter_by_name_with_label(ulong system, string name, string label, int ignore_seek_speed);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_get_global_parameter_by_name")]
        public static extern OaeResult oae_studio_get_global_parameter_by_name(ulong system, string name, out float out_value, out float out_final_value);

        #endregion

        #region Async Bank & Sample Data Hardening (95..100)
        // Note: 95 is oae_get_struct_sizes declared above in Struct Sizes Verification

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_studio_load_bank_file_ex")]
        public static extern OaeResult oae_studio_load_bank_file_ex(ulong system, string path, uint flags, out ulong out_bank);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_studio_load_bank_memory_ex")]
        public static extern OaeResult oae_studio_load_bank_memory_ex(ulong system, byte[] bytes, UIntPtr size, uint flags, out ulong out_bank);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_studio_load_bank_memory_ex")]
        public static extern OaeResult oae_studio_load_bank_memory_ex(ulong system, IntPtr bytes, UIntPtr size, uint flags, out ulong out_bank);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_studio_get_bank_loading_state")]
        public static extern OaeResult oae_studio_get_bank_loading_state(ulong system, ulong bank, out int out_state);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_description_load_sample_data")]
        public static extern OaeResult oae_event_description_load_sample_data(ulong description);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_description_unload_sample_data")]
        public static extern OaeResult oae_event_description_unload_sample_data(ulong description);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi, EntryPoint = "oae_bus_create_return")]
        public static extern OaeResult oae_bus_create_return(ulong system, string name, ulong parent, out ulong out_bus);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_add_send")]
        public static extern OaeResult oae_bus_add_send(ulong source, ulong target, float level, int position);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_remove_send")]
        public static extern OaeResult oae_bus_remove_send(ulong source, ulong target);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_set_send_level")]
        public static extern OaeResult oae_bus_set_send_level(ulong source, ulong target, float level);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_send_level")]
        public static extern OaeResult oae_bus_get_send_level(ulong source, ulong target, out float out_level);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_set_send_mute")]
        public static extern OaeResult oae_bus_set_send_mute(ulong source, ulong target, int mute);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_bus_get_send_mute")]
        public static extern OaeResult oae_bus_get_send_mute(ulong source, ulong target, out int out_mute);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_track_bus")]
        public static extern OaeResult oae_event_instance_get_track_bus(ulong instance, uint track_index, out ulong out_bus);

        #endregion

        #region 3D Spatialization (101..106)

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_set_listener_3d_attributes")]
        public static extern OaeResult oae_system_set_listener_3d_attributes(ulong system, ref Oae3DAttributes attributes);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_system_get_listener_3d_attributes")]
        public static extern OaeResult oae_system_get_listener_3d_attributes(ulong system, out Oae3DAttributes out_attributes);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_set_3d_attributes")]
        public static extern OaeResult oae_event_instance_set_3d_attributes(ulong instance, ref Oae3DAttributes attributes);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_3d_attributes")]
        public static extern OaeResult oae_event_instance_get_3d_attributes(ulong instance, out Oae3DAttributes out_attributes);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_set_spatializer_settings")]
        public static extern OaeResult oae_event_instance_set_spatializer_settings(ulong instance, ref OaeSpatializerSettings settings);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "oae_event_instance_get_spatializer_settings")]
        public static extern OaeResult oae_event_instance_get_spatializer_settings(ulong instance, out OaeSpatializerSettings out_settings);

        #endregion

        #region Static Callback Trampoline (AOT / IL2CPP)

        public delegate void ManagedEventCallback(uint type, ulong instanceHandle, IntPtr properties);
        private static readonly ConcurrentDictionary<ulong, ManagedEventCallback> s_CallbackTable = new ConcurrentDictionary<ulong, ManagedEventCallback>();

        public static readonly OaeEventCallback StaticCallback = NativeCallbackTrampoline;

        [AOT.MonoPInvokeCallback(typeof(OaeEventCallback))]
        private static OaeResult NativeCallbackTrampoline(uint type, ulong instance, IntPtr properties, IntPtr userData)
        {
            if (s_CallbackTable.TryGetValue(instance, out var handler) && handler != null)
            {
                try
                {
                    handler(type, instance, properties);
                }
                catch (Exception ex)
                {
                    // Do not let exceptions escape to native unmanaged code
#if UNITY_5_3_OR_NEWER
                    UnityEngine.Debug.LogException(ex);
#else
                    Console.Error.WriteLine($"[OpenAudio] Exception in callback trampoline: {ex}");
#endif
                }
            }
            return OaeResult.OK;
        }

        public static void RegisterInstanceCallback(ulong instanceHandle, ManagedEventCallback handler)
        {
            if (instanceHandle != 0 && handler != null)
            {
                s_CallbackTable[instanceHandle] = handler;
            }
        }

        public static void UnregisterInstanceCallback(ulong instanceHandle)
        {
            if (instanceHandle != 0)
            {
                s_CallbackTable.TryRemove(instanceHandle, out _);
            }
        }

        #endregion
    }
}
