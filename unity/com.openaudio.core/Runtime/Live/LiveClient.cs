using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace OpenAudio
{
    public enum LiveConnectionState
    {
        Disconnected,
        Connecting,
        Connected
    }

    public sealed class LiveClient : IDisposable
    {
        public const int DefaultPort = 7450;
        public const int ProtocolVersion = 1;

        private readonly OaeSystem m_System;
        private readonly string m_ClientName;
        private readonly ConcurrentQueue<string> m_IncomingLines = new ConcurrentQueue<string>();
        private readonly object m_SendLock = new object();

        private readonly Dictionary<string, EventInstance> m_RemoteInstances = new Dictionary<string, EventInstance>();
        private readonly Dictionary<string, EventInstance> m_RemoteSnapshots = new Dictionary<string, EventInstance>();
        private readonly Dictionary<string, float> m_CachedLocalParams = new Dictionary<string, float>();

        private TcpClient m_TcpClient;
        private NetworkStream m_Stream;
        private Thread m_WorkerThread;
        private volatile bool m_Running;
        private volatile LiveConnectionState m_State = LiveConnectionState.Disconnected;
        private volatile bool m_HandshakeCompleted;
        private volatile bool m_CrcMatched = true;
        private uint m_StudioBankCrc32;
        private string m_HandshakeStatus = "disconnected";
        private bool m_Disposed;

        public LiveConnectionState State => m_State;
        public bool IsConnected => m_State == LiveConnectionState.Connected && m_HandshakeCompleted;
        public bool IsCrcMatched => m_CrcMatched;
        public uint StudioBankCrc32 => m_StudioBankCrc32;
        public string HandshakeStatus => m_HandshakeStatus;
        public string LastCurveUpdateJson { get; private set; }

        public event Action<bool> OnHandshakeCompleted;
        public event Action OnDisconnected;
        public event Action<string> OnCurveUpdated;

        public LiveClient(OaeSystem system, string clientName = "Unity")
        {
            m_System = system ?? throw new ArgumentNullException(nameof(system));
            m_ClientName = string.IsNullOrEmpty(clientName) ? "Unity" : clientName;
            m_System.GlobalParameterChanged += HandleLocalGlobalParameterChanged;
        }

        public void ConnectAsync(string host = "127.0.0.1", int port = DefaultPort, uint bankCrc32 = 0)
        {
            if (m_Disposed) throw new ObjectDisposedException(nameof(LiveClient));
            Disconnect();

            m_Running = true;
            m_State = LiveConnectionState.Connecting;
            m_HandshakeCompleted = false;
            m_CrcMatched = true;
            m_StudioBankCrc32 = 0;
            m_HandshakeStatus = "connecting";

            m_WorkerThread = new Thread(() => ConnectionLoop(host, port, bankCrc32))
            {
                IsBackground = true,
                Name = "OAE-LiveClient"
            };
            m_WorkerThread.Start();
        }

        public void Disconnect()
        {
            m_Running = false;
            m_HandshakeCompleted = false;
            m_State = LiveConnectionState.Disconnected;

            lock (m_SendLock)
            {
                try { m_Stream?.Close(); } catch { }
                try { m_TcpClient?.Close(); } catch { }
                m_Stream = null;
                m_TcpClient = null;
            }

            if (m_WorkerThread != null && m_WorkerThread.IsAlive && Thread.CurrentThread != m_WorkerThread)
            {
                m_WorkerThread.Join(500);
            }
            m_WorkerThread = null;

            ReleaseRemoteInstances();
        }

        public void Update()
        {
            while (m_IncomingLines.TryDequeue(out string line))
            {
                ProcessMessageOnMainThread(line);
            }
        }

        public bool SendParameterChanged(string name, float value, bool isGlobal = true, string eventPath = "")
        {
            if (!IsConnected || string.IsNullOrEmpty(name)) return false;

            string json = string.Format(
                CultureInfo.InvariantCulture,
                "{{\"type\":\"param_set\",\"name\":\"{0}\",\"value\":{1},\"is_global\":{2},\"event_path\":\"{3}\"}}",
                EscapeJson(name),
                value,
                isGlobal ? "true" : "false",
                EscapeJson(eventPath ?? string.Empty));
            return SendLine(json);
        }

        private void HandleLocalGlobalParameterChanged(string name, float value)
        {
            SendParameterChanged(name, value, isGlobal: true);
        }

        private void ConnectionLoop(string host, int port, uint bankCrc32)
        {
            try
            {
                var client = new TcpClient();
                client.NoDelay = true;
                var ar = client.BeginConnect(host, port, null, null);
                while (!ar.IsCompleted)
                {
                    if (!m_Running)
                    {
                        try { client.Close(); } catch { }
                        return;
                    }
                    ar.AsyncWaitHandle.WaitOne(50);
                }
                client.EndConnect(ar);

                if (!m_Running)
                {
                    try { client.Close(); } catch { }
                    return;
                }

                lock (m_SendLock)
                {
                    m_TcpClient = client;
                    m_Stream = client.GetStream();
                }

                m_State = LiveConnectionState.Connected;

                string hello = string.Format(
                    CultureInfo.InvariantCulture,
                    "{{\"type\":\"hello\",\"version\":{0},\"client\":\"{1}\",\"bank_crc32\":{2}}}",
                    ProtocolVersion,
                    EscapeJson(m_ClientName),
                    bankCrc32);
                SendLine(hello);

                var buffer = new byte[4096];
                var sb = new StringBuilder();
                while (m_Running && client.Connected)
                {
                    int bytesRead;
                    try
                    {
                        bytesRead = m_Stream.Read(buffer, 0, buffer.Length);
                    }
                    catch
                    {
                        break;
                    }

                    if (bytesRead <= 0)
                        break;

                    sb.Append(Encoding.UTF8.GetString(buffer, 0, bytesRead));
                    string accumulated = sb.ToString();
                    int nl;
                    int start = 0;
                    while ((nl = accumulated.IndexOf('\n', start)) >= 0)
                    {
                        string line = accumulated.Substring(start, nl - start).TrimEnd('\r');
                        if (!string.IsNullOrEmpty(line))
                        {
                            if (TryExtractString(line, "type") == "ping")
                            {
                                long ts = TryExtractInt64(line, "ts_ms", 0);
                                SendLine(string.Format(CultureInfo.InvariantCulture, "{{\"type\":\"pong\",\"ts_ms\":{0}}}", ts));
                            }
                            else
                            {
                                m_IncomingLines.Enqueue(line);
                            }
                        }
                        start = nl + 1;
                    }

                    if (start > 0)
                    {
                        sb.Clear();
                        if (start < accumulated.Length)
                            sb.Append(accumulated.Substring(start));
                    }
                }
            }
            catch
            {
                // Connection refused or lost; transition cleanly to Disconnected.
            }
            finally
            {
                bool wasConnected = m_State == LiveConnectionState.Connected;
                m_HandshakeCompleted = false;
                m_State = LiveConnectionState.Disconnected;
                lock (m_SendLock)
                {
                    try { m_Stream?.Close(); } catch { }
                    try { m_TcpClient?.Close(); } catch { }
                    m_Stream = null;
                    m_TcpClient = null;
                }
                if (wasConnected)
                {
                    m_IncomingLines.Enqueue("{\"type\":\"_internal_disconnected\"}");
                }
            }
        }

        private bool SendLine(string jsonLine)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(jsonLine + "\n");
            lock (m_SendLock)
            {
                if (m_Stream == null || m_TcpClient == null || !m_TcpClient.Connected)
                    return false;
                try
                {
                    m_Stream.Write(bytes, 0, bytes.Length);
                    m_Stream.Flush();
                    return true;
                }
                catch
                {
                    return false;
                }
            }
        }

        private void ProcessMessageOnMainThread(string line)
        {
            string type = TryExtractString(line, "type");
            if (string.IsNullOrEmpty(type)) return;

            try
            {
                switch (type)
                {
                    case "_internal_disconnected":
                        ReleaseRemoteInstances();
                        m_HandshakeStatus = "disconnected";
                        OnDisconnected?.Invoke();
                        break;

                    case "hello_ack":
                        m_HandshakeStatus = TryExtractString(line, "status") ?? "connected";
                        m_CrcMatched = TryExtractBool(line, "crc_match", true);
                        m_StudioBankCrc32 = TryExtractUInt32(line, "studio_bank_crc32", 0);
                        m_HandshakeCompleted = (m_HandshakeStatus == "connected");
                        OnHandshakeCompleted?.Invoke(m_CrcMatched);
                        break;

                    case "bus_volume":
                    {
                        string path = TryExtractString(line, "path");
                        float volumeDb = TryExtractFloat(line, "volume_db", 0.0f);
                        if (!string.IsNullOrEmpty(path) && m_System.IsValid)
                        {
                            var bus = m_System.GetBus(path);
                            bus.Volume = DbToLinear(volumeDb);
                            bus.Release();
                        }
                        break;
                    }

                    case "bus_pan":
                    {
                        string path = TryExtractString(line, "path");
                        float pan = TryExtractFloat(line, "pan", 0.0f);
                        if (!string.IsNullOrEmpty(path) && m_System.IsValid)
                        {
                            var bus = m_System.GetBus(path);
                            bus.Pan = pan;
                            bus.Release();
                        }
                        break;
                    }

                    case "bus_mute":
                    {
                        string path = TryExtractString(line, "path");
                        bool mute = TryExtractBool(line, "mute", false);
                        if (!string.IsNullOrEmpty(path) && m_System.IsValid)
                        {
                            var bus = m_System.GetBus(path);
                            bus.Mute = mute;
                            bus.Release();
                        }
                        break;
                    }

                    case "vca_volume":
                    {
                        string path = TryExtractString(line, "path");
                        float volumeDb = TryExtractFloat(line, "volume_db", 0.0f);
                        if (!string.IsNullOrEmpty(path) && m_System.IsValid)
                        {
                            var vca = m_System.GetVca(path);
                            vca.Volume = DbToLinear(volumeDb);
                        }
                        break;
                    }

                    case "param_set":
                    {
                        string name = TryExtractString(line, "name");
                        float val = TryExtractFloat(line, "value", 0.0f);
                        bool isGlobal = TryExtractBool(line, "is_global", true);
                        string eventPath = TryExtractString(line, "event_path") ?? string.Empty;
                        if (!string.IsNullOrEmpty(name) && m_System.IsValid)
                        {
                            if (isGlobal)
                            {
                                m_System.SetGlobalParameterInternal(name, val, ignoreSeekSpeed: true, notifyLive: false);
                            }
                            else
                            {
                                m_CachedLocalParams[name] = val;
                                if (!string.IsNullOrEmpty(eventPath) && m_RemoteInstances.TryGetValue(eventPath, out var inst) && inst.IsValid)
                                {
                                    inst.SetParameter(name, val, ignoreSeekSpeed: true);
                                }
                            }
                        }
                        break;
                    }

                    case "curve_update":
                        LastCurveUpdateJson = line;
                        OnCurveUpdated?.Invoke(line);
                        break;

                    case "event_play":
                    {
                        string eventPath = TryExtractString(line, "event_path");
                        double posMs = TryExtractFloat(line, "position_ms", 0.0f);
                        if (!string.IsNullOrEmpty(eventPath) && m_System.IsValid)
                        {
                            if (m_RemoteInstances.TryGetValue(eventPath, out var existing))
                            {
                                if (existing.IsValid)
                                {
                                    existing.Stop(OaeStopMode.Immediate);
                                    existing.Release();
                                }
                                m_RemoteInstances.Remove(eventPath);
                            }

                            var desc = m_System.GetEvent(eventPath);
                            var inst = desc.CreateInstance();
                            foreach (var kv in m_CachedLocalParams)
                            {
                                try { inst.SetParameter(kv.Key, kv.Value, ignoreSeekSpeed: true); } catch { }
                            }
                            if (posMs > 0.0)
                            {
                                inst.TimelinePositionMs = (int)posMs;
                            }
                            inst.Start();
                            m_RemoteInstances[eventPath] = inst;
                        }
                        break;
                    }

                    case "event_stop":
                    {
                        string eventPath = TryExtractString(line, "event_path");
                        bool immediate = TryExtractBool(line, "immediate", false);
                        if (!string.IsNullOrEmpty(eventPath) && m_RemoteInstances.TryGetValue(eventPath, out var inst))
                        {
                            if (inst.IsValid)
                            {
                                inst.Stop(immediate ? OaeStopMode.Immediate : OaeStopMode.AllowFadeout);
                                inst.Release();
                            }
                            m_RemoteInstances.Remove(eventPath);
                        }
                        break;
                    }

                    case "event_key_off":
                    {
                        string eventPath = TryExtractString(line, "event_path");
                        if (!string.IsNullOrEmpty(eventPath) && m_RemoteInstances.TryGetValue(eventPath, out var inst) && inst.IsValid)
                        {
                            inst.KeyOff();
                        }
                        break;
                    }

                    case "snapshot_toggle":
                    {
                        string snapPath = TryExtractString(line, "snapshot_path");
                        bool active = TryExtractBool(line, "active", false);
                        if (!string.IsNullOrEmpty(snapPath) && m_System.IsValid)
                        {
                            if (active)
                            {
                                if (!m_RemoteSnapshots.ContainsKey(snapPath))
                                {
                                    var desc = m_System.GetEvent(snapPath);
                                    var inst = desc.CreateInstance();
                                    inst.Start();
                                    m_RemoteSnapshots[snapPath] = inst;
                                }
                            }
                            else if (m_RemoteSnapshots.TryGetValue(snapPath, out var inst))
                            {
                                if (inst.IsValid)
                                {
                                    inst.Stop(OaeStopMode.AllowFadeout);
                                    inst.Release();
                                }
                                m_RemoteSnapshots.Remove(snapPath);
                            }
                        }
                        break;
                    }
                }
            }
            catch
            {
                // Ignore transient errors for missing buses/events in partial banks during live authoring.
            }
        }

        private void ReleaseRemoteInstances()
        {
            foreach (var inst in m_RemoteInstances.Values)
            {
                try
                {
                    if (inst.IsValid)
                    {
                        inst.Stop(OaeStopMode.Immediate);
                        inst.Release();
                    }
                }
                catch { }
            }
            m_RemoteInstances.Clear();

            foreach (var inst in m_RemoteSnapshots.Values)
            {
                try
                {
                    if (inst.IsValid)
                    {
                        inst.Stop(OaeStopMode.Immediate);
                        inst.Release();
                    }
                }
                catch { }
            }
            m_RemoteSnapshots.Clear();
        }

        private static float DbToLinear(float db)
        {
            return db <= -80.0f ? 0.0f : (float)Math.Pow(10.0, db / 20.0);
        }

        private static string EscapeJson(string s)
        {
            return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private static int FindKeyValueStart(string json, string key)
        {
            string pattern = "\"" + key + "\"";
            int idx = json.IndexOf(pattern, StringComparison.Ordinal);
            if (idx < 0) return -1;
            int colon = json.IndexOf(':', idx + pattern.Length);
            if (colon < 0) return -1;
            int pos = colon + 1;
            while (pos < json.Length && char.IsWhiteSpace(json[pos])) pos++;
            return pos < json.Length ? pos : -1;
        }

        private static string TryExtractString(string json, string key)
        {
            int pos = FindKeyValueStart(json, key);
            if (pos < 0 || json[pos] != '"') return null;
            pos++;
            var sb = new StringBuilder();
            while (pos < json.Length)
            {
                char c = json[pos++];
                if (c == '\\' && pos < json.Length)
                {
                    sb.Append(json[pos++]);
                }
                else if (c == '"')
                {
                    return sb.ToString();
                }
                else
                {
                    sb.Append(c);
                }
            }
            return null;
        }

        private static string ExtractToken(string json, int pos)
        {
            int end = pos;
            while (end < json.Length && json[end] != ',' && json[end] != '}' && !char.IsWhiteSpace(json[end]))
            {
                end++;
            }
            return json.Substring(pos, end - pos);
        }

        private static float TryExtractFloat(string json, string key, float fallback)
        {
            int pos = FindKeyValueStart(json, key);
            if (pos < 0) return fallback;
            string token = ExtractToken(json, pos);
            return float.TryParse(token, NumberStyles.Float, CultureInfo.InvariantCulture, out float v) ? v : fallback;
        }

        private static uint TryExtractUInt32(string json, string key, uint fallback)
        {
            int pos = FindKeyValueStart(json, key);
            if (pos < 0) return fallback;
            string token = ExtractToken(json, pos);
            return uint.TryParse(token, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint v) ? v : fallback;
        }

        private static long TryExtractInt64(string json, string key, long fallback)
        {
            int pos = FindKeyValueStart(json, key);
            if (pos < 0) return fallback;
            string token = ExtractToken(json, pos);
            return long.TryParse(token, NumberStyles.Integer, CultureInfo.InvariantCulture, out long v) ? v : fallback;
        }

        private static bool TryExtractBool(string json, string key, bool fallback)
        {
            int pos = FindKeyValueStart(json, key);
            if (pos < 0) return fallback;
            string token = ExtractToken(json, pos);
            if (token == "true") return true;
            if (token == "false") return false;
            return fallback;
        }

        public void Dispose()
        {
            if (m_Disposed) return;
            m_Disposed = true;
            m_System.GlobalParameterChanged -= HandleLocalGlobalParameterChanged;
            Disconnect();
            GC.SuppressFinalize(this);
        }
    }
}
