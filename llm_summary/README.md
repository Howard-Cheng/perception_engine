# LLM Summary Module

Module for local LLM inference using llama.cpp, supporting Phi-4 model for text summarization and Q&A.

## Features

- ✅ Load local GGUF format models
- ✅ Text generation and conversation
- ✅ Text summarization
- ✅ Q&A functionality
- ✅ Database reading (DuckDB and SQLite)
- ✅ Structured data parsing
- ✅ Baseline raw data reading for testing

## Installation

```bash
# Basic installation (CPU version)
pip install llama-cpp-python

# For GPU support (CUDA)
pip install llama-cpp-python[server]
```

## Quick Start

### Basic Usage

```python
from llm_summary import LLMClient, MODEL_PATH

# Create client (model loads automatically on first use)
client = LLMClient()

# Generate text
response = client.generate("Please introduce artificial intelligence.", max_tokens=200)
print(response)

# Text summarization
summary = client.summarize("Very long text content...")

# Q&A
answer = client.answer_question("What is machine learning?", context="Relevant context...")
```

### Database Reading

```python
from pathlib import Path
from llm_summary import LLMClient

client = LLMClient()

# Read from DuckDB (parsed structured data)
duckdb_path = Path("path/to/compressed_context.duckdb")
parsed_data = client.read_from_database(duckdb_path, limit=10)
for item in parsed_data:
    print(item['summary'])

# Read from SQLite (parsed structured data)
sqlite_path = Path("path/to/raw_events.db")
parsed_data = client.read_from_database(sqlite_path, limit=10)

# Baseline: Read raw data without parsing (for testing)
raw_data = client.read_from_database_raw(duckdb_path, limit=10)
```

### Custom Model Path

```python
from pathlib import Path
from llm_summary import LLMClient

# Use global MODEL_PATH
print(f"Default model path: {MODEL_PATH}")

# Or specify custom path
custom_path = Path("path/to/your/model.gguf")
client = LLMClient(model_path=custom_path)
```

### GPU Support

```python
# Enable GPU acceleration (requires CUDA)
client = LLMClient(n_gpu_layers=35)  # Set number of GPU layers
```

## Configuration

### Global Model Path

Model path is defined as a global variable in `llm_client.py`:

```python
MODEL_PATH = Path(__file__).parent.parent.parent / "models" / "phi4-aitc" / "Phi4_FP16-3.8B-Q41-g32d-1027-v1.3.1.gguf"
```

You can directly modify this variable to change the default model path.

### Default Parameters

- `temperature`: 0.7 (temperature parameter, controls randomness)
- `max_tokens`: 512 (maximum tokens to generate)
- `top_p`: 0.9 (nucleus sampling)
- `top_k`: 40 (top-k sampling)
- `repeat_penalty`: 1.1 (repetition penalty)
- `n_gpu_layers`: 35 (default GPU acceleration, set to 0 for CPU only)

## API Reference

### LLMClient

#### `__init__(model_path=None, n_ctx=2048, n_threads=None, n_gpu_layers=35, verbose=True)`

Initialize LLM client.

- `model_path`: Path to model file, None uses global MODEL_PATH
- `n_ctx`: Context window size
- `n_threads`: Number of CPU threads, None for auto-detection
- `n_gpu_layers`: Number of GPU layers, 0 for CPU only, >0 for GPU acceleration
- `verbose`: Whether to output verbose information

#### `generate(prompt, temperature=0.7, max_tokens=512, ...)`

Generate text response.

#### `chat(messages, temperature=0.7, max_tokens=512, ...)`

Conversational generation, supports multi-turn dialogue.

#### `summarize(text, max_tokens=200)`

Summarize text.

#### `answer_question(question, context=None)`

Answer a question, optionally with context.

#### `read_from_database(database_path, query=None, table=None, limit=None)`

Read data from database and parse into structured dictionaries.

- Returns: List of structured dictionaries with key-value pairs

#### `read_from_database_raw(database_path, query=None, table=None, limit=None)`

Read raw data from database without parsing (baseline function for testing).

- Returns: List of raw row dictionaries

#### `process_database_content(database_path, operation="summarize", **kwargs)`

Read data from database and process with LLM.

## Data Format

### Parsed Data Structure

**DuckDB (compressed_content table):**
```python
{
    'id': 'content_id',
    'type': 'content_type',
    'title': 'title',
    'url': 'url',
    'session_id': 'session_id',
    'device_id': 'device_id',
    'summary': 'summary',
    'key_points': ['list', 'of', 'key', 'points'],
    'copied_content': ['copied', 'text', 'items'],
    'selected_text': ['selected', 'text', 'items'],
    'engagement_score': 0.85,
    'timestamp': '2024-01-01 12:00:00',
    'metadata': {'key': 'value'},
    'extracted_entities': {'emails': [], 'urls': [], ...}
}
```

**SQLite (raw_events table):**
```python
{
    'id': 'event_id',
    'timestamp': '2024-01-01 12:00:00',
    'device_id': 'device_id',
    'app_name': 'app_name',
    'window_title': 'window_title',
    'url': 'url',
    'screen_content': 'screen_content',
    'mouse_events': [{'eventType': '...', ...}],
    'interaction_count': 10,
    'dwell_time_seconds': 30,
    'content_type': 'email',
    'domain': 'WORK',
    ...
}
```

## Notes

1. **First-time model loading is slow**: Model file is large (~3.8GB), first load takes some time
2. **Memory usage**: Model loading consumes significant memory, recommend at least 8GB RAM
3. **GPU support**: For GPU acceleration, need CUDA version of llama-cpp-python
4. **Model format**: Currently supports GGUF format models
5. **CUDA version**: Default configuration supports CUDA 12.4

## Examples

Run example code:

```bash
cd perception_engine/llm_summary
python example_usage.py
```
