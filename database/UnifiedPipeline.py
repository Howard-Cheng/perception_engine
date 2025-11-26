#!/usr/bin/env python3
"""
Unified Pipeline for Session Compression and Work Session Aggregation
======================================================================

This unified program combines SessionCompressionScheduler and WorkSessionAggregator
into a single process to avoid database file locking issues.

This script runs every 5 minutes to:
1. Query uncompressed raw events from SQLite
2. Detect interaction sessions from raw events
3. Compress sessions (basic summary generation)
4. Store compressed content to DuckDB
5. Mark raw events as compressed
6. Query unaggregated interaction sessions from DuckDB
7. Group sessions into work sessions using weighted scoring
8. Store work session metadata to DuckDB
9. Update sessions with work_session_id

Features:
- Single DuckDB connection (no file locking issues)
- Periodic execution (configurable interval)
- Weighted scoring system with adaptive time threshold
- Optional lightweight embedding support
- Error handling with detailed logging
- Graceful shutdown on SIGINT/SIGTERM

Usage:
    python UnifiedPipeline.py
"""

import sqlite3
import duckdb
import time
import signal
import sys
import json
import hashlib
import logging
import re
import math
import numpy as np
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple
from collections import Counter

# Try to import sentence-transformers, fallback gracefully if not available
try:
    from sentence_transformers import SentenceTransformer
    EMBEDDING_AVAILABLE = True
except ImportError:
    EMBEDDING_AVAILABLE = False

# Import functions from SessionCompressionScheduler
import SessionCompressionScheduler as compression_module

# Import functions from WorkSessionAggregator
import WorkSessionAggregator as aggregation_module

# Re-export commonly used functions and constants
init_sqlite_schema = compression_module.init_sqlite_schema
init_duckdb_schema = compression_module.init_duckdb_schema
classify_content = compression_module.classify_content
detect_sessions = compression_module.detect_sessions
extract_high_attention_content = compression_module.extract_high_attention_content
extract_entities = compression_module.extract_entities
extract_url_host = compression_module.extract_url_host
extract_strong_keys = compression_module.extract_strong_keys
generate_title_fingerprint = compression_module.generate_title_fingerprint
calculate_idle_seconds = compression_module.calculate_idle_seconds
calculate_engagement = compression_module.calculate_engagement
generate_summary = compression_module.generate_summary
IDLE_THRESHOLD_SECONDS = compression_module.IDLE_THRESHOLD_SECONDS
MIN_SESSION_DURATION_SECONDS = compression_module.MIN_SESSION_DURATION_SECONDS

# Import work session aggregation functions
calculate_weighted_jaccard = aggregation_module.calculate_weighted_jaccard
calculate_adaptive_time_threshold = aggregation_module.calculate_adaptive_time_threshold
calculate_time_similarity = aggregation_module.calculate_time_similarity
calculate_title_similarity = aggregation_module.calculate_title_similarity
calculate_merge_score = aggregation_module.calculate_merge_score
check_project_conflict = aggregation_module.check_project_conflict
detect_work_sessions = aggregation_module.detect_work_sessions
generate_work_session_summary = aggregation_module.generate_work_session_summary
# _build_embedding_text is used internally by calculate_merge_score, no need to import
DEFAULT_TIME_THRESHOLD_SECONDS = aggregation_module.DEFAULT_TIME_THRESHOLD_SECONDS
MIN_TIME_THRESHOLD_SECONDS = aggregation_module.MIN_TIME_THRESHOLD_SECONDS
MAX_TIME_THRESHOLD_SECONDS = aggregation_module.MAX_TIME_THRESHOLD_SECONDS
WEIGHT_TIME = aggregation_module.WEIGHT_TIME
WEIGHT_DOMAIN = aggregation_module.WEIGHT_DOMAIN
WEIGHT_APP = aggregation_module.WEIGHT_APP
WEIGHT_ENTITY = aggregation_module.WEIGHT_ENTITY
WEIGHT_TITLE = aggregation_module.WEIGHT_TITLE
WEIGHT_TIME_EMBED = aggregation_module.WEIGHT_TIME_EMBED
WEIGHT_DOMAIN_EMBED = aggregation_module.WEIGHT_DOMAIN_EMBED
WEIGHT_APP_EMBED = aggregation_module.WEIGHT_APP_EMBED
WEIGHT_ENTITY_EMBED = aggregation_module.WEIGHT_ENTITY_EMBED
WEIGHT_TITLE_EMBED = aggregation_module.WEIGHT_TITLE_EMBED
WEIGHT_EMBED_EMBED = aggregation_module.WEIGHT_EMBED_EMBED
MERGE_THRESHOLD = aggregation_module.MERGE_THRESHOLD
MAX_CROSS_DAY_HOURS = aggregation_module.MAX_CROSS_DAY_HOURS

# Also need to ensure work_sessions table exists
def init_work_sessions_schema(conn):
    """Initialize work_sessions table schema."""
    # Work sessions table
    conn.execute("""
        CREATE TABLE IF NOT EXISTS work_sessions (
            work_session_id VARCHAR PRIMARY KEY,
            device_id VARCHAR NOT NULL,
            
            start_time TIMESTAMP NOT NULL,
            end_time TIMESTAMP NOT NULL,
            
            -- Project inference
            project_name VARCHAR,
            project_confidence DOUBLE,
            
            -- Aggregated info
            domain VARCHAR NOT NULL,
            subdomain VARCHAR,
            session_ids VARCHAR[],
            
            -- Summary
            activity_summary VARCHAR,
            key_accomplishments VARCHAR[],
            entities_involved VARCHAR,  -- JSON
            
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    
    # Features for merge table (for learning and auditing)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS features_for_merge (
            sid_left VARCHAR NOT NULL,
            sid_right VARCHAR NOT NULL,
            delta_t DOUBLE,
            f_time DOUBLE,
            same_domain BOOLEAN,
            same_app BOOLEAN,
            jaccard_w DOUBLE,
            title_hit BOOLEAN,
            embed_cos DOUBLE,
            score DOUBLE,
            decision BOOLEAN,
            reason VARCHAR,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (sid_left, sid_right)
        )
    """)
    
    # Ensure sessions table has work_session_id column
    try:
        conn.execute("ALTER TABLE sessions ADD COLUMN work_session_id VARCHAR")
    except Exception:
        # Column already exists, ignore
        pass

# ============================================================================
# CONFIGURATION
# ============================================================================

# Execution interval (seconds)
EXECUTION_INTERVAL = 300  # 5 minutes

# Compression delay: wait for this many seconds after last event before compressing
COMPRESSION_DELAY = 60  # 1 minute

# Database paths
DATABASE_DIR = Path("./perception_data")
DEVICE_ID = "pc_001"

# Logging configuration
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


# ============================================================================
# UNIFIED PIPELINE
# ============================================================================

class UnifiedPipeline:
    """Unified pipeline for session compression and work session aggregation."""
    
    def __init__(
        self,
        device_id: str,
        database_dir: Path,
        execution_interval: int = 300,
        compression_delay: int = 60,
        use_embedding: bool = True
    ):
        """Initialize the unified pipeline."""
        self.device_id = device_id
        self.database_dir = database_dir
        self.execution_interval = execution_interval
        self.compression_delay = compression_delay
        self.running = False
        self.use_embedding = use_embedding and EMBEDDING_AVAILABLE
        
        # Database paths
        self.sqlite_path = database_dir / device_id / "raw_events.db"
        self.duckdb_path = database_dir / device_id / "compressed_context.duckdb"
        
        # Database connections
        self.sqlite_conn: Optional[sqlite3.Connection] = None
        self.duckdb_conn: Optional[duckdb.DuckDBPyConnection] = None
        
        # Embedding model (lazy load)
        self.embed_model = None
        
        logger.info(f"UnifiedPipeline initialized for device: {device_id}")
        if self.use_embedding:
            logger.info("Embedding model will be loaded on first use")
    
    def initialize(self):
        """Initialize database connections and schemas."""
        try:
            # Ensure directories exist
            self.sqlite_path.parent.mkdir(parents=True, exist_ok=True)
            self.duckdb_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Connect to SQLite
            self.sqlite_conn = sqlite3.connect(str(self.sqlite_path))
            self.sqlite_conn.row_factory = sqlite3.Row
            init_sqlite_schema(self.sqlite_conn)
            
            # Connect to DuckDB (single connection for both operations)
            self.duckdb_conn = duckdb.connect(str(self.duckdb_path))
            init_duckdb_schema(self.duckdb_conn)
            init_work_sessions_schema(self.duckdb_conn)
            
            logger.info("✓ Database connections initialized successfully")
            logger.info(f"  SQLite: {self.sqlite_path}")
            logger.info(f"  DuckDB: {self.duckdb_path}")
            
            # Load embedding model if enabled
            if self.use_embedding and EMBEDDING_AVAILABLE:
                try:
                    logger.info("Loading embedding model (all-MiniLM-L6-v2)...")
                    self.embed_model = SentenceTransformer('all-MiniLM-L6-v2')
                    logger.info("✓ Embedding model loaded successfully")
                except Exception as e:
                    logger.warning(f"Failed to load embedding model: {e}")
                    logger.warning("Continuing without embedding features")
                    self.use_embedding = False
                    self.embed_model = None
            
        except Exception as e:
            logger.error(f"✗ Initialization failed: {e}", exc_info=True)
            raise
    
    def get_uncompressed_count(self) -> int:
        """Get count of uncompressed raw events."""
        cursor = self.sqlite_conn.execute("""
            SELECT COUNT(*) FROM raw_events
            WHERE compressed = FALSE
        """)
        return cursor.fetchone()[0]
    
    def get_unaggregated_count(self) -> int:
        """Get count of unaggregated interaction sessions."""
        cursor = self.duckdb_conn.execute("""
            SELECT COUNT(*) FROM sessions
            WHERE work_session_id IS NULL
        """)
        return cursor.fetchone()[0]
    
    def run_compression(self) -> dict:
        """Execute a single compression run."""
        start_time = time.time()
        
        try:
            # Check uncompressed events count
            uncompressed_count = self.get_uncompressed_count()
            
            if uncompressed_count == 0:
                logger.debug("No uncompressed events found")
                return {
                    'success': True,
                    'sessions_compressed': 0,
                    'events_compressed': 0,
                    'duration': 0,
                    'uncompressed_remaining': 0
                }
            
            logger.info(f"Starting compression: {uncompressed_count} uncompressed events")
            
            # Query uncompressed events
            cursor = self.sqlite_conn.execute("""
                SELECT * FROM raw_events
                WHERE compressed = FALSE
                ORDER BY timestamp ASC
            """)
            
            raw_events = []
            for row in cursor.fetchall():
                event = dict(row)
                # Classify content
                content_type, domain = classify_content(
                    event['app_name'],
                    event.get('url'),
                    event.get('window_title')
                )
                event['content_type'] = content_type
                event['domain'] = domain
                raw_events.append(event)
            
            if not raw_events:
                logger.info("No raw events to compress")
                return {
                    'success': True,
                    'sessions_compressed': 0,
                    'events_compressed': 0,
                    'duration': 0,
                    'uncompressed_remaining': 0
                }
            
            # Detect sessions
            sessions = detect_sessions(raw_events)
            
            logger.info(f"Detected {len(sessions)} sessions to compress")
            
            # Compress each session
            sessions_compressed = 0
            events_compressed = 0
            
            for session_events in sessions:
                try:
                    # Generate session ID
                    session_id = self._generate_session_id(session_events[0])
                    
                    # Calculate engagement
                    engagement = calculate_engagement(session_events)
                    
                    # Extract high-attention content
                    high_attention = extract_high_attention_content(session_events)
                    
                    # Extract entities
                    entities = extract_entities(session_events, high_attention)
                    
                    # Generate summary
                    summary, key_points = generate_summary(session_events, engagement)
                    
                    # Get session metadata
                    start_time_str = session_events[0]['timestamp']
                    end_time_str = session_events[-1]['timestamp']
                    start_dt = datetime.fromisoformat(start_time_str)
                    end_dt = datetime.fromisoformat(end_time_str)
                    duration = int((end_dt - start_dt).total_seconds())
                    
                    content_type = session_events[0]['content_type']
                    domain = session_events[0]['domain']
                    app_name = session_events[0]['app_name']
                    
                    # Extract new fields
                    window_title_raw = session_events[0].get('window_title') or ''
                    window_title = window_title_raw[:2048] if window_title_raw else None
                    
                    url = session_events[0].get('url') or None
                    url_host = extract_url_host(url)
                    
                    strong_keys = extract_strong_keys(session_events)
                    strong_keys_json = json.dumps(strong_keys) if strong_keys else None
                    
                    title_fingerprint = generate_title_fingerprint(window_title_raw)
                    
                    # Calculate input_events
                    input_events = engagement['interaction_count']
                    copy_select_count = engagement.get('copy_select_count', 0)
                    idle_seconds = calculate_idle_seconds(session_events, IDLE_THRESHOLD_SECONDS)
                    
                    # Store session metadata
                    self.duckdb_conn.execute("""
                        INSERT INTO sessions 
                        (session_id, device_id, start_time, end_time, duration_seconds, 
                         app_name, content_type, domain, engagement_score, 
                         interaction_count, total_dwell_time,
                         window_title, url, url_host, strong_keys_json, title_fingerprint,
                         input_events, copy_select_count, idle_seconds) 
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        ON CONFLICT (session_id) DO UPDATE SET
                            device_id = EXCLUDED.device_id,
                            start_time = EXCLUDED.start_time,
                            end_time = EXCLUDED.end_time,
                            duration_seconds = EXCLUDED.duration_seconds,
                            app_name = EXCLUDED.app_name,
                            content_type = EXCLUDED.content_type,
                            domain = EXCLUDED.domain,
                            engagement_score = EXCLUDED.engagement_score,
                            interaction_count = EXCLUDED.interaction_count,
                            total_dwell_time = EXCLUDED.total_dwell_time
                    """, [
                        session_id,
                        self.device_id,
                        start_time_str,
                        end_time_str,
                        duration,
                        app_name,
                        content_type,
                        domain,
                        engagement['engagement_score'],
                        engagement['interaction_count'],
                        engagement['total_dwell_time'],
                        window_title,
                        url,
                        url_host,
                        strong_keys_json,
                        title_fingerprint,
                        input_events,
                        copy_select_count,
                        idle_seconds
                    ])
                    
                    # Store compressed content
                    content_id = f"content_{session_id}"
                    window_title = session_events[0].get('window_title') or ''
                    title = window_title[:200] if window_title else ''
                    url = session_events[0].get('url') or ''
                    key_points_str = json.dumps(key_points)
                    entities_str = json.dumps(entities) if entities else None
                    
                    self.duckdb_conn.execute("""
                        INSERT INTO compressed_content 
                        (content_id, session_id, device_id, content_type, title, url, 
                         summary, key_points, extracted_entities, engagement_score, timestamp) 
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        ON CONFLICT (content_id) DO UPDATE SET
                            session_id = EXCLUDED.session_id,
                            device_id = EXCLUDED.device_id,
                            content_type = EXCLUDED.content_type,
                            title = EXCLUDED.title,
                            url = EXCLUDED.url,
                            summary = EXCLUDED.summary,
                            key_points = EXCLUDED.key_points,
                            extracted_entities = EXCLUDED.extracted_entities,
                            engagement_score = EXCLUDED.engagement_score,
                            timestamp = EXCLUDED.timestamp
                    """, [
                        content_id,
                        session_id,
                        self.device_id,
                        content_type,
                        title,
                        url,
                        summary,
                        key_points_str,
                        entities_str,
                        engagement['engagement_score'],
                        start_time_str
                    ])
                    
                    # Mark raw events as compressed
                    event_ids = [e['event_id'] for e in session_events]
                    placeholders = ','.join(['?' for _ in event_ids])
                    
                    self.sqlite_conn.execute(f"""
                        UPDATE raw_events
                        SET compressed = TRUE, session_id = ?
                        WHERE event_id IN ({placeholders})
                    """, [session_id] + event_ids)
                    
                    self.sqlite_conn.commit()
                    
                    sessions_compressed += 1
                    events_compressed += len(session_events)
                    
                except Exception as e:
                    logger.error(f"Error compressing session: {e}", exc_info=True)
                    continue
            
            # Calculate statistics
            duration = time.time() - start_time
            remaining_uncompressed = self.get_uncompressed_count()
            
            logger.info(
                f"✓ Compression completed: {sessions_compressed} sessions, "
                f"{events_compressed} events compressed, "
                f"duration: {duration:.2f}s"
            )
            
            return {
                'success': True,
                'sessions_compressed': sessions_compressed,
                'events_compressed': events_compressed,
                'duration': duration,
                'uncompressed_remaining': remaining_uncompressed
            }
            
        except Exception as e:
            logger.error(f"✗ Compression failed: {e}", exc_info=True)
            return {
                'success': False,
                'error': str(e),
                'duration': time.time() - start_time
            }
    
    def run_aggregation(self) -> dict:
        """Execute a single aggregation run."""
        start_time = time.time()
        
        try:
            # Check unaggregated sessions count
            unaggregated_count = self.get_unaggregated_count()
            
            if unaggregated_count == 0:
                logger.debug("No unaggregated sessions found")
                return {
                    'success': True,
                    'work_sessions_created': 0,
                    'sessions_aggregated': 0,
                    'duration': 0,
                    'unaggregated_remaining': 0
                }
            
            logger.info(f"Starting aggregation: {unaggregated_count} unaggregated sessions")
            
            # Query unaggregated sessions with their compressed content
            cursor = self.duckdb_conn.execute("""
                SELECT 
                    s.session_id,
                    s.device_id,
                    s.start_time,
                    s.end_time,
                    s.app_name,
                    s.content_type,
                    s.domain,
                    s.engagement_score,
                    s.interaction_count,
                    s.total_dwell_time,
                    s.window_title,
                    s.strong_keys_json,
                    s.title_fingerprint,
                    cc.extracted_entities,
                    cc.metadata
                FROM sessions s
                LEFT JOIN compressed_content cc ON s.session_id = cc.session_id
                WHERE s.work_session_id IS NULL
                ORDER BY s.start_time ASC
                LIMIT 200
            """)
            
            interaction_sessions = []
            for row in cursor.fetchall():
                session = dict(row)
                
                # Parse metadata to extract project_name if available
                metadata_str = session.get('metadata')
                if metadata_str:
                    try:
                        if isinstance(metadata_str, str):
                            metadata = json.loads(metadata_str)
                        else:
                            metadata = metadata_str
                        session['project_name'] = metadata.get('project_name')
                    except (json.JSONDecodeError, TypeError):
                        session['project_name'] = None
                else:
                    session['project_name'] = None
                
                interaction_sessions.append(session)
            
            if not interaction_sessions:
                logger.info("No interaction sessions to aggregate")
                return {
                    'success': True,
                    'work_sessions_created': 0,
                    'sessions_aggregated': 0,
                    'duration': 0,
                    'unaggregated_remaining': 0
                }
            
            # Detect work sessions using weighted scoring
            work_sessions = detect_work_sessions(
                interaction_sessions,
                embed_model=self.embed_model if self.use_embedding else None,
                conn=self.duckdb_conn
            )
            
            logger.info(f"Detected {len(work_sessions)} work sessions to create")
            
            # Create work sessions
            work_sessions_created = 0
            sessions_aggregated = 0
            
            for work_session_sessions in work_sessions:
                try:
                    # Generate work session ID
                    work_session_id = self._generate_work_session_id(work_session_sessions[0])
                    
                    # Get work session metadata
                    start_time_str = work_session_sessions[0]['start_time']
                    end_time_str = work_session_sessions[-1]['end_time']
                    
                    # Determine domain
                    domain = work_session_sessions[0].get('domain', 'WORK')
                    
                    # Extract project name
                    project_names = [s.get('project_name') for s in work_session_sessions if s.get('project_name')]
                    project_name = project_names[0] if project_names else None
                    project_confidence = len(project_names) / len(work_session_sessions) if project_names else 0.0
                    
                    # Collect all session IDs
                    session_ids = [s['session_id'] for s in work_session_sessions]
                    
                    # Generate summary
                    summary, key_accomplishments = generate_work_session_summary(work_session_sessions)
                    
                    # Collect all entities
                    all_entities = {}
                    for session in work_session_sessions:
                        entities_data = session.get('extracted_entities')
                        if entities_data:
                            if isinstance(entities_data, str):
                                try:
                                    entities_data = json.loads(entities_data)
                                except (json.JSONDecodeError, TypeError):
                                    entities_data = {}
                            if isinstance(entities_data, dict):
                                for entity_type, entity_list in entities_data.items():
                                    if entity_type not in all_entities:
                                        all_entities[entity_type] = []
                                    if isinstance(entity_list, list):
                                        all_entities[entity_type].extend(entity_list)
                    
                    # Deduplicate entities
                    for entity_type in all_entities:
                        all_entities[entity_type] = list(set(all_entities[entity_type]))[:20]
                    
                    entities_json = json.dumps(all_entities) if all_entities else None
                    
                    # Store work session
                    self.duckdb_conn.execute("""
                        INSERT INTO work_sessions 
                        (work_session_id, device_id, start_time, end_time, 
                         project_name, project_confidence, domain, session_ids,
                         activity_summary, key_accomplishments, entities_involved) 
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        ON CONFLICT (work_session_id) DO UPDATE SET
                            device_id = EXCLUDED.device_id,
                            start_time = EXCLUDED.start_time,
                            end_time = EXCLUDED.end_time,
                            project_name = EXCLUDED.project_name,
                            project_confidence = EXCLUDED.project_confidence,
                            domain = EXCLUDED.domain,
                            session_ids = EXCLUDED.session_ids,
                            activity_summary = EXCLUDED.activity_summary,
                            key_accomplishments = EXCLUDED.key_accomplishments,
                            entities_involved = EXCLUDED.entities_involved
                    """, [
                        work_session_id,
                        self.device_id,
                        start_time_str,
                        end_time_str,
                        project_name,
                        project_confidence,
                        domain,
                        session_ids,
                        summary,
                        key_accomplishments,
                        entities_json
                    ])
                    
                    # Update sessions with work_session_id
                    placeholders = ','.join(['?' for _ in session_ids])
                    self.duckdb_conn.execute(f"""
                        UPDATE sessions
                        SET work_session_id = ?
                        WHERE session_id IN ({placeholders})
                    """, [work_session_id] + session_ids)
                    
                    work_sessions_created += 1
                    sessions_aggregated += len(session_ids)
                    
                except Exception as e:
                    logger.error(f"Error creating work session: {e}", exc_info=True)
                    continue
            
            # Calculate statistics
            duration = time.time() - start_time
            remaining_unaggregated = self.get_unaggregated_count()
            
            logger.info(
                f"✓ Aggregation completed: {work_sessions_created} work sessions created, "
                f"{sessions_aggregated} sessions aggregated, "
                f"duration: {duration:.2f}s"
            )
            
            return {
                'success': True,
                'work_sessions_created': work_sessions_created,
                'sessions_aggregated': sessions_aggregated,
                'duration': duration,
                'unaggregated_remaining': remaining_unaggregated
            }
            
        except Exception as e:
            logger.error(f"✗ Aggregation failed: {e}", exc_info=True)
            return {
                'success': False,
                'error': str(e),
                'duration': time.time() - start_time
            }
    
    def run_pipeline(self) -> dict:
        """Execute both compression and aggregation in sequence."""
        pipeline_start = time.time()
        
        # Step 1: Run compression
        compression_result = self.run_compression()
        
        # Step 2: Run aggregation (only if compression succeeded)
        aggregation_result = self.run_aggregation()
        
        pipeline_duration = time.time() - pipeline_start
        
        return {
            'compression': compression_result,
            'aggregation': aggregation_result,
            'total_duration': pipeline_duration
        }
    
    def _generate_session_id(self, first_event: Dict) -> str:
        """Generate unique session ID from first event."""
        timestamp = first_event['timestamp']
        device_id = first_event['device_id']
        app_name = first_event['app_name']
        
        unique_str = f"{device_id}_{timestamp}_{app_name}"
        return hashlib.md5(unique_str.encode()).hexdigest()
    
    def _generate_work_session_id(self, first_session: Dict) -> str:
        """Generate unique work session ID from first session."""
        timestamp = first_session['start_time']
        device_id = first_session['device_id']
        domain = first_session.get('domain', 'WORK')
        
        unique_str = f"{device_id}_{timestamp}_{domain}_work"
        return hashlib.md5(unique_str.encode()).hexdigest()
    
    def run_periodic(self):
        """Run periodic pipeline loop."""
        self.running = True
        last_execution_time = 0
        
        logger.info("=" * 60)
        logger.info("Unified Pipeline")
        logger.info("=" * 60)
        logger.info(f"Device ID: {self.device_id}")
        logger.info(f"Execution interval: {self.execution_interval}s ({self.execution_interval/60:.1f} minutes)")
        logger.info(f"SQLite: {self.sqlite_path}")
        logger.info(f"DuckDB: {self.duckdb_path}")
        logger.info("=" * 60)
        logger.info("")
        
        try:
            while self.running:
                current_time = time.time()
                
                # Check if it's time to run pipeline
                if current_time - last_execution_time >= self.execution_interval:
                    # Execute pipeline (compression + aggregation)
                    result = self.run_pipeline()
                    
                    # Log statistics
                    if result['compression']['success']:
                        uncompressed = self.get_uncompressed_count()
                        logger.info(
                            f"Compression: {uncompressed} uncompressed events remaining | "
                            f"Last run: {result['compression']['events_compressed']} events compressed"
                        )
                    else:
                        logger.warning(f"Compression failed: {result['compression'].get('error', 'Unknown error')}")
                    
                    if result['aggregation']['success']:
                        unaggregated = self.get_unaggregated_count()
                        logger.info(
                            f"Aggregation: {unaggregated} unaggregated sessions remaining | "
                            f"Last run: {result['aggregation']['sessions_aggregated']} sessions aggregated into {result['aggregation']['work_sessions_created']} work sessions"
                        )
                    else:
                        logger.warning(f"Aggregation failed: {result['aggregation'].get('error', 'Unknown error')}")
                    
                    logger.info(f"Total pipeline duration: {result['total_duration']:.2f}s")
                    
                    last_execution_time = current_time
                
                # Sleep for a short interval before checking again
                time.sleep(10)  # Check every 10 seconds
                
        except KeyboardInterrupt:
            logger.info("")
            logger.info("Received interrupt signal, shutting down...")
            self.stop()
        except Exception as e:
            logger.error(f"Pipeline error: {e}", exc_info=True)
            self.stop()
    
    def stop(self):
        """Stop the pipeline and close database connections."""
        self.running = False
        
        if self.sqlite_conn:
            self.sqlite_conn.close()
            logger.info("✓ SQLite connection closed")
        
        if self.duckdb_conn:
            self.duckdb_conn.close()
            logger.info("✓ DuckDB connection closed")
        
        logger.info("Pipeline stopped")


# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

def main():
    """Main entry point for the unified pipeline."""
    
    print("Starting Unified Pipeline...")
    print(f"Current directory: {Path.cwd()}")
    
    pipeline = UnifiedPipeline(
        device_id=DEVICE_ID,
        database_dir=DATABASE_DIR,
        execution_interval=EXECUTION_INTERVAL,
        compression_delay=COMPRESSION_DELAY,
        use_embedding=True
    )
    
    # Register signal handlers for graceful shutdown
    def signal_handler(sig, frame):
        logger.info("Received signal, shutting down gracefully...")
        pipeline.stop()
        sys.exit(0)
    
    # Windows only supports SIGINT
    if sys.platform == 'win32':
        signal.signal(signal.SIGINT, signal_handler)
    else:
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
    
    try:
        # Initialize connections
        pipeline.initialize()
        
        # Start periodic pipeline
        pipeline.run_periodic()
        
    except Exception as e:
        logger.error(f"Failed to start pipeline: {e}", exc_info=True)
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

