using System;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using PerceptionEngine.SDK.Exceptions;
using PerceptionEngine.SDK.Models;

namespace PerceptionEngine.SDK
{
    /// <summary>
    /// Client for interacting with the Perception Engine HTTP API.
    /// </summary>
    public class PerceptionEngineClient : IDisposable
    {
        private readonly HttpClient _httpClient;
        private readonly string _baseUrl;
        private readonly JsonSerializerOptions _jsonOptions;
        private bool _disposed;

        /// <summary>
        /// Creates a new Perception Engine client.
        /// </summary>
        /// <param name="baseUrl">Base URL of the Perception Engine (e.g., "http://localhost:8777")</param>
        /// <param name="timeout">HTTP request timeout. Default: 30 seconds</param>
        public PerceptionEngineClient(string baseUrl = "http://localhost:8777", TimeSpan? timeout = null)
        {
            _baseUrl = baseUrl.TrimEnd('/');
            _httpClient = new HttpClient
            {
                Timeout = timeout ?? TimeSpan.FromSeconds(30)
            };

            _jsonOptions = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true,
                NumberHandling = System.Text.Json.Serialization.JsonNumberHandling.AllowReadingFromString
            };
        }

        /// <summary>
        /// Gets the current perception context from the engine.
        /// </summary>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <returns>The current perception context</returns>
        /// <exception cref="PerceptionEngineException">Thrown when the request fails</exception>
        public async Task<PerceptionContext> GetContextAsync(CancellationToken cancellationToken = default)
        {
            try
            {
                var response = await _httpClient.GetAsync($"{_baseUrl}/context", cancellationToken);
                response.EnsureSuccessStatusCode();

                var json = await response.Content.ReadAsStringAsync(cancellationToken);
                var context = JsonSerializer.Deserialize<PerceptionContext>(json, _jsonOptions);

                if (context == null)
                {
                    throw new PerceptionEngineException("Failed to deserialize response from Perception Engine");
                }

                return context;
            }
            catch (HttpRequestException ex)
            {
                throw new PerceptionEngineException(
                    $"Failed to connect to Perception Engine at {_baseUrl}. Is it running?", ex);
            }
            catch (TaskCanceledException ex)
            {
                throw new PerceptionEngineException(
                    "Request to Perception Engine timed out", ex);
            }
            catch (JsonException ex)
            {
                throw new PerceptionEngineException(
                    "Failed to parse response from Perception Engine", ex);
            }
        }

        /// <summary>
        /// Checks if the Perception Engine is running and responding.
        /// </summary>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <returns>True if the engine is healthy, false otherwise</returns>
        public async Task<bool> IsHealthyAsync(CancellationToken cancellationToken = default)
        {
            try
            {
                var response = await _httpClient.GetAsync($"{_baseUrl}/context", cancellationToken);
                return response.IsSuccessStatusCode;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Waits for the Perception Engine to become available.
        /// </summary>
        /// <param name="timeout">Maximum time to wait. Default: 30 seconds</param>
        /// <param name="pollInterval">How often to check. Default: 1 second</param>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <returns>True if engine became available, false if timeout</returns>
        public async Task<bool> WaitForHealthyAsync(
            TimeSpan? timeout = null,
            TimeSpan? pollInterval = null,
            CancellationToken cancellationToken = default)
        {
            var maxWait = timeout ?? TimeSpan.FromSeconds(30);
            var interval = pollInterval ?? TimeSpan.FromSeconds(1);
            var endTime = DateTime.UtcNow + maxWait;

            while (DateTime.UtcNow < endTime)
            {
                if (await IsHealthyAsync(cancellationToken))
                {
                    return true;
                }

                await Task.Delay(interval, cancellationToken);
            }

            return false;
        }

        /// <summary>
        /// Gets the engine mode (Full or Screen-Only).
        /// </summary>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <returns>The current engine mode</returns>
        public async Task<EngineMode> GetEngineModeAsync(CancellationToken cancellationToken = default)
        {
            var context = await GetContextAsync(cancellationToken);
            return context.GetEngineMode();
        }

        /// <summary>
        /// Disposes the HTTP client.
        /// </summary>
        public void Dispose()
        {
            if (!_disposed)
            {
                _httpClient?.Dispose();
                _disposed = true;
            }
            GC.SuppressFinalize(this);
        }
    }
}
