using System;

namespace PerceptionEngine.SDK.Exceptions
{
    /// <summary>
    /// Exception thrown when communication with Perception Engine fails.
    /// </summary>
    public class PerceptionEngineException : Exception
    {
        public PerceptionEngineException(string message) : base(message)
        {
        }

        public PerceptionEngineException(string message, Exception innerException)
            : base(message, innerException)
        {
        }
    }
}
