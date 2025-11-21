"""
LLM Client using llama.cpp
==========================

Load and run local GGUF models using llama-cpp-python library.
"""

import os
import json
import sqlite3
from pathlib import Path
from typing import Optional, List, Dict, Any, Union
import logging

logger = logging.getLogger(__name__)

# ============================================================================
# Global Configuration
# ============================================================================

# Model path (global variable for easy modification)
MODEL_PATH = Path(__file__).parent.parent / "models" / "phi4-aitc" / "Phi4_FP16-3.8B-Q41-g32d-1027-v1.3.1.gguf"

# Default inference parameters
DEFAULT_TEMPERATURE = 0.7
DEFAULT_MAX_TOKENS = 512
DEFAULT_TOP_P = 0.9
DEFAULT_TOP_K = 40
DEFAULT_REPEAT_PENALTY = 1.1


# ============================================================================
# LLM Client Class
# ============================================================================

class LLMClient:
    """
    LLM client using llama.cpp.
    
    Supports:
    - Loading GGUF models
    - Text generation and Q&A
    - Reading from databases (DuckDB and SQLite)
    """
    
    def __init__(
        self,
        model_path: Optional[Path] = None,
        n_ctx: int = 2048,
        n_threads: Optional[int] = None,
        n_gpu_layers: int = 35,  # Default to GPU acceleration (set to 0 for CPU only)
        verbose: bool = True
    ):
        """
        Initialize LLM client.
        
        Args:
            model_path: Path to model file, None uses global MODEL_PATH
            n_ctx: Context window size
            n_threads: Number of CPU threads, None for auto-detection
            n_gpu_layers: Number of GPU layers, 0 for CPU only, >0 for GPU acceleration
            verbose: Whether to output verbose information
        """
        self.model_path = Path(model_path) if model_path else MODEL_PATH
        self.n_ctx = n_ctx
        self.n_threads = n_threads
        self.n_gpu_layers = n_gpu_layers
        self.verbose = verbose
        
        # Lazy model loading
        self._llm = None
        self._model_loaded = False
        
        # Verify model file exists
        if not self.model_path.exists():
            raise FileNotFoundError(
                f"Model file not found: {self.model_path}\n"
                f"Please ensure the model file is downloaded to the correct location."
            )
        
        logger.info(f"LLM Client initialized, model path: {self.model_path}")
    
    def _load_model(self):
        """Lazy load model (load on first use)."""
        if self._model_loaded:
            return
        
        try:
            # Dynamically import llama-cpp-python
            try:
                from llama_cpp import Llama
            except ImportError:
                raise ImportError(
                    "llama-cpp-python library not installed.\n"
                    "Please run: pip install llama-cpp-python\n"
                    "For GPU support: pip install llama-cpp-python[server]"
                )
            
            logger.info(f"Loading model: {self.model_path}")
            
            # Load model
            self._llm = Llama(
                model_path=str(self.model_path),
                n_ctx=self.n_ctx,
                n_threads=self.n_threads,
                n_gpu_layers=self.n_gpu_layers,
                verbose=self.verbose
            )
            
            self._model_loaded = True
            logger.info("✓ Model loaded successfully")
            
        except Exception as e:
            logger.error(f"Failed to load model: {e}", exc_info=True)
            raise
    
    def generate(
        self,
        prompt: str,
        temperature: float = DEFAULT_TEMPERATURE,
        max_tokens: int = DEFAULT_MAX_TOKENS,
        top_p: float = DEFAULT_TOP_P,
        top_k: int = DEFAULT_TOP_K,
        repeat_penalty: float = DEFAULT_REPEAT_PENALTY,
        stop: Optional[List[str]] = None,
        **kwargs
    ) -> str:
        """
        Generate text response.
        
        Args:
            prompt: Input prompt
            temperature: Temperature parameter (0.0-2.0), higher = more random
            max_tokens: Maximum tokens to generate
            top_p: Nucleus sampling parameter
            top_k: Top-k sampling parameter
            repeat_penalty: Repetition penalty coefficient
            stop: List of stop words
            **kwargs: Other llama.cpp parameters
        
        Returns:
            Generated text
        """
        if not self._model_loaded:
            self._load_model()
        
        try:
            # Call model generation
            response = self._llm(
                prompt,
                temperature=temperature,
                max_tokens=max_tokens,
                top_p=top_p,
                top_k=top_k,
                repeat_penalty=repeat_penalty,
                stop=stop,
                **kwargs
            )
            
            # Extract generated text
            if isinstance(response, dict):
                text = response.get('choices', [{}])[0].get('text', '')
            else:
                text = str(response)
            
            return text.strip()
            
        except Exception as e:
            logger.error(f"Text generation failed: {e}", exc_info=True)
            raise
    
    def chat(
        self,
        messages: List[Dict[str, str]],
        temperature: float = DEFAULT_TEMPERATURE,
        max_tokens: int = DEFAULT_MAX_TOKENS,
        **kwargs
    ) -> str:
        """
        Conversational generation (supports multi-turn dialogue).
        
        Args:
            messages: List of messages, format: [{"role": "user", "content": "..."}, ...]
            temperature: Temperature parameter
            max_tokens: Maximum tokens to generate
            **kwargs: Other parameters
        
        Returns:
            Generated response
        """
        if not self._model_loaded:
            self._load_model()
        
        try:
            # Build conversation format prompt
            prompt = self._format_messages(messages)
            
            # Generate response
            return self.generate(
                prompt,
                temperature=temperature,
                max_tokens=max_tokens,
                **kwargs
            )
            
        except Exception as e:
            logger.error(f"Conversation generation failed: {e}", exc_info=True)
            raise
    
    def _format_messages(self, messages: List[Dict[str, str]]) -> str:
        """
        Format message list into model-acceptable prompt.
        
        Args:
            messages: List of messages
        
        Returns:
            Formatted prompt
        """
        # Phi-4 uses ChatML format
        formatted = []
        for msg in messages:
            role = msg.get("role", "user")
            content = msg.get("content", "")
            
            if role == "system":
                formatted.append(f"<|system|>\n{content}<|end|>\n")
            elif role == "user":
                formatted.append(f"<|user|>\n{content}<|end|>\n")
            elif role == "assistant":
                formatted.append(f"<|assistant|>\n{content}<|end|>\n")
        
        # Add assistant start marker
        formatted.append("<|assistant|>\n")
        
        return "".join(formatted)
    
    def summarize(self, text: str, max_tokens: int = 200) -> str:
        """
        Summarize text.
        
        Args:
            text: Text to summarize
            max_tokens: Maximum length of summary
        
        Returns:
            Summary text
        """
        prompt = f"""Please summarize the following text concisely:

{text}

Summary:"""
        
        return self.generate(
            prompt,
            max_tokens=max_tokens,
            temperature=0.3  # Lower temperature for more deterministic summaries
        )
    
    def answer_question(self, question: str, context: Optional[str] = None) -> str:
        """
        Answer a question.
        
        Args:
            question: Question to answer
            context: Optional context information
        
        Returns:
            Answer
        """
        if context:
            prompt = f"""Answer the question based on the following context:

Context:
{context}

Question: {question}

Answer:"""
        else:
            prompt = f"""Please answer the following question:

Question: {question}

Answer:"""
        
        return self.generate(prompt, temperature=DEFAULT_TEMPERATURE)
    
    # ========================================================================
    # Database Reading Interface
    # ========================================================================
    
    def _detect_database_type(self, database_path: Union[str, Path]) -> str:
        """
        Detect database type from file extension.
        
        Args:
            database_path: Path to database file
        
        Returns:
            'duckdb' or 'sqlite'
        """
        path = Path(database_path)
        ext = path.suffix.lower()
        
        if ext == '.duckdb':
            return 'duckdb'
        elif ext in ['.db', '.sqlite', '.sqlite3']:
            return 'sqlite'
        else:
            # Try to detect by attempting connection
            try:
                import duckdb
                conn = duckdb.connect(str(path))
                conn.close()
                return 'duckdb'
            except:
                try:
                    conn = sqlite3.connect(str(path))
                    conn.close()
                    return 'sqlite'
                except:
                    raise ValueError(f"Cannot determine database type for: {database_path}")
    
    def read_from_database_raw(
        self,
        database_path: Union[str, Path],
        query: Optional[str] = None,
        table: Optional[str] = None,
        limit: Optional[int] = None
    ) -> List[Dict[str, Any]]:
        """
        Read data from database without parsing (baseline function for testing).
        
        Args:
            database_path: Path to database file (DuckDB or SQLite)
            query: Custom SQL query (optional)
            table: Table name (if using default query)
            limit: Limit number of rows returned
        
        Returns:
            List of raw row dictionaries
        """
        db_path = Path(database_path)
        if not db_path.exists():
            raise FileNotFoundError(f"Database file not found: {db_path}")
        
        db_type = self._detect_database_type(db_path)
        
        if db_type == 'duckdb':
            return self._read_duckdb_raw(db_path, query, table, limit)
        else:
            return self._read_sqlite_raw(db_path, query, table, limit)
    
    def read_from_database(
        self,
        database_path: Union[str, Path],
        query: Optional[str] = None,
        table: Optional[str] = None,
        limit: Optional[int] = None
    ) -> List[Dict[str, Any]]:
        """
        Read data from database and parse into structured dictionaries.
        
        Args:
            database_path: Path to database file (DuckDB or SQLite)
            query: Custom SQL query (optional)
            table: Table name (if using default query)
            limit: Limit number of rows returned
        
        Returns:
            List of structured dictionaries with key-value pairs
        """
        # Get raw data
        raw_data = self.read_from_database_raw(database_path, query, table, limit)
        
        # Parse into structured format
        db_path = Path(database_path)
        db_type = self._detect_database_type(db_path)
        
        if db_type == 'duckdb':
            return self._parse_duckdb_data(raw_data, table)
        else:
            return self._parse_sqlite_data(raw_data, table)
    
    def _read_duckdb_raw(
        self,
        db_path: Path,
        query: Optional[str],
        table: Optional[str],
        limit: Optional[int]
    ) -> List[Dict[str, Any]]:
        """Read raw data from DuckDB."""
        try:
            import duckdb
        except ImportError:
            raise ImportError("duckdb library not installed. Please run: pip install duckdb")
        
        conn = duckdb.connect(str(db_path))
        
        try:
            # Build query
            if query:
                sql = query
            elif table:
                sql = f"SELECT * FROM {table}"
                if limit:
                    sql += f" LIMIT {limit}"
            else:
                # Default: read from compressed_content table
                sql = "SELECT * FROM compressed_content"
                if limit:
                    sql += f" LIMIT {limit}"
            
            # Execute query
            result = conn.execute(sql).fetchall()
            columns = conn.execute(sql).description
            
            # Convert to list of dictionaries
            data = []
            for row in result:
                row_dict = {}
                for i, col in enumerate(columns):
                    col_name = col[0] if isinstance(col, tuple) else col
                    row_dict[col_name] = row[i]
                data.append(row_dict)
            
            return data
            
        finally:
            conn.close()
    
    def _read_sqlite_raw(
        self,
        db_path: Path,
        query: Optional[str],
        table: Optional[str],
        limit: Optional[int]
    ) -> List[Dict[str, Any]]:
        """Read raw data from SQLite."""
        conn = sqlite3.connect(str(db_path))
        conn.row_factory = sqlite3.Row
        
        try:
            # Build query
            if query:
                sql = query
            elif table:
                sql = f"SELECT * FROM {table}"
                if limit:
                    sql += f" LIMIT {limit}"
            else:
                # Default: read from raw_events table
                sql = "SELECT * FROM raw_events"
                if limit:
                    sql += f" LIMIT {limit}"
            
            # Execute query
            cursor = conn.execute(sql)
            rows = cursor.fetchall()
            
            # Convert to list of dictionaries
            data = []
            for row in rows:
                row_dict = dict(row)
                data.append(row_dict)
            
            return data
            
        finally:
            conn.close()
    
    def _parse_duckdb_data(
        self,
        raw_data: List[Dict[str, Any]],
        table: Optional[str]
    ) -> List[Dict[str, Any]]:
        """
        Parse DuckDB data into structured format.
        
        For compressed_content table, extracts:
        - Basic info: content_id, session_id, content_type, title, url
        - Content: summary, key_points, copied_content, selected_text
        - Metadata: engagement_score, timestamp, extracted_entities
        """
        parsed_data = []
        
        for row in raw_data:
            parsed = {}
            
            # Basic identification
            parsed['id'] = row.get('content_id') or row.get('session_id', '')
            parsed['type'] = row.get('content_type', 'unknown')
            parsed['title'] = row.get('title', '')
            parsed['url'] = row.get('url', '')
            parsed['session_id'] = row.get('session_id', '')
            parsed['device_id'] = row.get('device_id', '')
            
            # Content
            parsed['summary'] = row.get('summary', '')
            parsed['key_points'] = self._parse_array_field(row.get('key_points'))
            parsed['copied_content'] = self._parse_array_field(row.get('copied_content'))
            parsed['selected_text'] = self._parse_array_field(row.get('selected_text'))
            
            # Metadata
            parsed['engagement_score'] = row.get('engagement_score', 0.0)
            parsed['timestamp'] = str(row.get('timestamp', ''))
            
            # Parse JSON fields
            metadata = row.get('metadata')
            if metadata:
                parsed['metadata'] = self._parse_json_field(metadata)
            else:
                parsed['metadata'] = {}
            
            extracted_entities = row.get('extracted_entities')
            if extracted_entities:
                parsed['extracted_entities'] = self._parse_json_field(extracted_entities)
            else:
                parsed['extracted_entities'] = {}
            
            parsed_data.append(parsed)
        
        return parsed_data
    
    def _parse_sqlite_data(
        self,
        raw_data: List[Dict[str, Any]],
        table: Optional[str]
    ) -> List[Dict[str, Any]]:
        """
        Parse SQLite data into structured format.
        
        For raw_events table, extracts:
        - Basic info: event_id, timestamp, device_id, app_name, window_title, url
        - Content: screen_content, voice_transcription, camera_description
        - Interactions: mouse_events, interaction_count, dwell_time_seconds
        - System: battery_percent, cpu_usage, memory_usage, network_type, location
        - Classification: content_type, domain, session_id
        """
        parsed_data = []
        
        for row in raw_data:
            parsed = {}
            
            # Basic identification
            parsed['id'] = row.get('event_id', '')
            parsed['timestamp'] = str(row.get('timestamp', ''))
            parsed['device_id'] = row.get('device_id', '')
            parsed['app_name'] = row.get('app_name', '')
            parsed['window_title'] = row.get('window_title', '')
            parsed['url'] = row.get('url', '')
            
            # Content
            parsed['screen_content'] = row.get('screen_content', '')
            parsed['screen_content_hash'] = row.get('screen_content_hash', '')
            parsed['voice_transcription'] = row.get('voice_transcription', '')
            parsed['camera_description'] = row.get('camera_description', '')
            
            # Interactions
            mouse_events = row.get('mouse_events', '[]')
            parsed['mouse_events'] = self._parse_json_field(mouse_events)
            parsed['interaction_count'] = row.get('interaction_count', 0)
            parsed['dwell_time_seconds'] = row.get('dwell_time_seconds', 0)
            
            # System info
            parsed['battery_percent'] = row.get('battery_percent')
            parsed['is_charging'] = bool(row.get('is_charging', False))
            parsed['cpu_usage'] = row.get('cpu_usage')
            parsed['memory_usage'] = row.get('memory_usage')
            parsed['network_type'] = row.get('network_type', '')
            parsed['location_lat'] = row.get('location_lat')
            parsed['location_lon'] = row.get('location_lon')
            
            # Classification
            parsed['content_type'] = row.get('content_type', '')
            parsed['domain'] = row.get('domain', '')
            parsed['session_id'] = row.get('session_id', '')
            parsed['compressed'] = bool(row.get('compressed', False))
            
            parsed_data.append(parsed)
        
        return parsed_data
    
    def _parse_array_field(self, field: Any) -> List[str]:
        """Parse array field (DuckDB arrays or JSON arrays)."""
        if field is None:
            return []
        if isinstance(field, list):
            return [str(item) for item in field]
        if isinstance(field, str):
            try:
                parsed = json.loads(field)
                if isinstance(parsed, list):
                    return [str(item) for item in parsed]
            except:
                pass
        return [str(field)]
    
    def _parse_json_field(self, field: Any) -> Dict[str, Any]:
        """Parse JSON field."""
        if field is None:
            return {}
        if isinstance(field, dict):
            return field
        if isinstance(field, str):
            try:
                return json.loads(field)
            except:
                return {}
        return {}
    
    def process_database_content(
        self,
        database_path: Union[str, Path],
        operation: str = "summarize",
        **kwargs
    ) -> List[Dict[str, Any]]:
        """
        Read data from database and process with LLM.
        
        Args:
            database_path: Path to database file
            operation: Operation type ("summarize", "answer", "extract", etc.)
            **kwargs: Other parameters
        
        Returns:
            List of processed results
        """
        # Read data
        data = self.read_from_database(database_path, **kwargs)
        
        # Process with LLM based on operation
        results = []
        for item in data:
            if operation == "summarize":
                # Summarize the content
                text_to_summarize = item.get('summary', '') or item.get('screen_content', '')
                if text_to_summarize:
                    summary = self.summarize(text_to_summarize)
                    item['llm_summary'] = summary
            elif operation == "answer":
                # Answer questions about the content
                question = kwargs.get('question', 'What is this about?')
                context = item.get('summary', '') or item.get('screen_content', '')
                answer = self.answer_question(question, context)
                item['llm_answer'] = answer
            # Add more operations as needed
            
            results.append(item)
        
        return results
    
    def __del__(self):
        """Cleanup resources."""
        if self._model_loaded and self._llm:
            try:
                # llama-cpp-python automatically cleans up resources
                pass
            except:
                pass
