using System;
using System.Runtime.InteropServices;

namespace OpenAudio
{
    public enum OaeResult
    {
        OK = 0,
        ERR_INVALID_PARAM = 1,
        ERR_INVALID_HANDLE = 2,
        ERR_NOT_INITIALIZED = 3,
        ERR_ALREADY_INITIALIZED = 4,
        ERR_OUT_OF_MEMORY = 5,
        ERR_FILE_NOT_FOUND = 6,
        ERR_FORMAT = 7,
        ERR_DEVICE = 8,
        ERR_NOT_FOUND = 9,
        ERR_ALREADY_EXISTS = 10,
        ERR_INVALID_STATE = 11,
        ERR_INTERNAL = 100
    }

    [Flags]
    public enum OaeInitFlags : uint
    {
        Default = 0,
        NoDevice = 1 << 0
    }

    [Flags]
    public enum OaeSoundFlags : uint
    {
        Default = 0,
        Loop = 1 << 0,
        Stream = 1 << 1
    }

    public enum OaeFadeCurve
    {
        Linear = 0,
        Log = 1,
        SCurve = 2
    }

    public enum OaeEffectType
    {
        Limiter = 0,
        Compressor = 1,
        Eq3 = 2,
        Lowpass = 3,
        Highpass = 4,
        Reverb = 5,
        Delay = 6
    }

    public enum OaeEffectPosition
    {
        PostFader = 0,
        PreFader = 1
    }

    public enum OaeLimiterParam
    {
        CeilingDb = 0,
        ReleaseMs = 1,
        InputGainDb = 2
    }

    public enum OaeCompressorParam
    {
        ThresholdDb = 0,
        Ratio = 1,
        AttackMs = 2,
        ReleaseMs = 3,
        MakeupDb = 4
    }

    public enum OaeEq3Param
    {
        LowDb = 0,
        MidDb = 1,
        HighDb = 2,
        LowXoverHz = 3,
        HighXoverHz = 4
    }

    public enum OaeFilterParam
    {
        CutoffHz = 0,
        Q = 1
    }

    public enum OaeReverbParam
    {
        RoomSize = 0,
        Damping = 1,
        Width = 2
    }

    public enum OaeDelayParam
    {
        TimeMs = 0,
        Feedback = 1
    }

    public enum OaePlaybackState
    {
        Playing = 0,
        Sustaining = 1,
        Stopped = 2,
        Starting = 3,
        Stopping = 4
    }

    public enum OaeEventKind
    {
        Event = 0,
        Snapshot = 1
    }

    public enum OaeSnapshotMode
    {
        Override = 0,
        Blending = 1
    }

    public enum OaeStopMode
    {
        AllowFadeout = 0,
        Immediate = 1
    }

    public enum OaeRolloffType
    {
        Linear = 0,
        Inverse = 1,
        LinearSquared = 2
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeVec3
    {
        public float x;
        public float y;
        public float z;

        public OaeVec3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Oae3DAttributes
    {
        public OaeVec3 position;
        public OaeVec3 velocity;
        public OaeVec3 forward;
        public OaeVec3 up;

        public static Oae3DAttributes Identity => new Oae3DAttributes
        {
            position = new OaeVec3(0f, 0f, 0f),
            velocity = new OaeVec3(0f, 0f, 0f),
            forward = new OaeVec3(0f, 0f, 1f),
            up = new OaeVec3(0f, 1f, 0f)
        };
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeSpatializerSettings
    {
        public int enabled;
        public int rolloff;
        public float min_distance;
        public float max_distance;
        public float sound_size;
        public float min_extent_deg;
        public float cone_inner_angle_deg;
        public float cone_outer_angle_deg;
        public float cone_outer_gain;

        public static OaeSpatializerSettings Default => new OaeSpatializerSettings
        {
            enabled = 0,
            rolloff = (int)OaeRolloffType.Linear,
            min_distance = 1.0f,
            max_distance = 20.0f,
            sound_size = 0.0f,
            min_extent_deg = 0.0f,
            cone_inner_angle_deg = 360.0f,
            cone_outer_angle_deg = 360.0f,
            cone_outer_gain = 1.0f
        };
    }

    [Flags]
    public enum OaeEventCallbackType : uint
    {
        Started = 1 << 0,
        Stopped = 1 << 1,
        Marker = 1 << 2,
        Beat = 1 << 3,
        Stolen = 1 << 4,
        SoundPlayed = 1 << 5,
        SoundStopped = 1 << 6,
        CreateProgrammerSound = 1 << 7,
        DestroyProgrammerSound = 1 << 8,
        All = 0x1FF
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeProgrammerSoundProperties
    {
        public IntPtr name;
        public ulong sound;
    }

    public enum OaeBankLoadingState
    {
        Loading = 0,
        Loaded = 1,
        Failed = 2
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeInitSettings
    {
        public uint sample_rate;
        public uint channels;
        public uint max_physical_voices;
        public uint flags;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct OaeMeter
    {
        public const int MaxChannels = 8;
        public uint channel_count;
        public uint frame_count;
        public fixed float peak[MaxChannels];
        public fixed float rms[MaxChannels];
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeTimelineMarkerProperties
    {
        public ulong dsp_clock;
        public IntPtr name;
        public int position_ms;
        public uint marker_index;

        public string Name => Marshal.PtrToStringAnsi(name);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeTimelineBeatProperties
    {
        public ulong dsp_clock;
        public int bar;
        public int beat;
        public int position_ms;
        public float tempo;
        public int time_signature_upper;
        public int time_signature_lower;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeEventSoundProperties
    {
        public ulong dsp_clock;
        public ulong sound;
        public ulong channel;
        public uint instrument_index;
        public uint nested_depth;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OaeStructSizes
    {
        public uint init_settings;
        public uint meter;
        public uint timeline_marker_properties;
        public uint timeline_beat_properties;
        public uint event_sound_properties;
    }

    public delegate OaeResult OaeEventCallback(
        uint type,
        ulong instance,
        IntPtr properties,
        IntPtr userData);
}
