using System;

namespace OpenAudio
{
    public class OaeException : Exception
    {
        public OaeResult Result { get; }

        public OaeException(OaeResult result) 
            : base($"OpenAudio error: {result} ({(int)result})")
        {
            Result = result;
        }

        public OaeException(OaeResult result, string message) 
            : base($"OpenAudio error: {result} ({(int)result}) - {message}")
        {
            Result = result;
        }

        public static void Check(OaeResult result, string context = null)
        {
            if (result != OaeResult.OK)
            {
                if (string.IsNullOrEmpty(context))
                    throw new OaeException(result);
                else
                    throw new OaeException(result, context);
            }
        }
    }
}
