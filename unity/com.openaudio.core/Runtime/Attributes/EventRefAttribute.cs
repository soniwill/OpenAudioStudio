using System;
#if UNITY_5_3_OR_NEWER
using UnityEngine;
#endif

namespace OpenAudio
{
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, AllowMultiple = false, Inherited = true)]
#if UNITY_5_3_OR_NEWER
    public class EventRefAttribute : PropertyAttribute
#else
    public class EventRefAttribute : Attribute
#endif
    {
        public EventRefAttribute()
        {
        }
    }
}
