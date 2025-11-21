#!/usr/bin/env python3
"""
Session Compression Scheduler
=============================

Periodic scheduler that extracts and processes sessions from SQLite to DuckDB.

This script runs every 5 minutes to:
1. Query uncompressed raw events from SQLite
2. Detect interaction sessions from raw events
3. Compress sessions (basic summary generation)
4. Store compressed content to DuckDB
5. Mark raw events as compressed

Features:
- Periodic execution (configurable interval)
- Session detection based on time gaps and app changes
- Basic content compression
- Error handling with detailed logging
- Graceful shutdown on SIGINT/SIGTERM

Usage:
    python SessionCompressionScheduler.py
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
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple

# ============================================================================
# CONFIGURATION
# ============================================================================

# Execution interval (seconds)
COMPRESSION_INTERVAL = 300  # 5 minutes

# Compression delay: wait for this many seconds after last event before compressing
COMPRESSION_DELAY = 60  # 1 minute

# Session detection thresholds
IDLE_THRESHOLD_SECONDS = 300  # 5 minutes - session break on idle gap
MIN_SESSION_DURATION_SECONDS = 3  # Ignore very short sessions

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
# DATABASE SCHEMA INITIALIZATION
# ============================================================================

def init_sqlite_schema(conn: sqlite3.Connection):
    """Initialize SQLite schema for raw events."""
    conn.execute("""
        CREATE TABLE IF NOT EXISTS raw_events (
            event_id TEXT PRIMARY KEY,
            timestamp TIMESTAMP NOT NULL,
            device_id TEXT NOT NULL,
            
            -- App context
            app_name TEXT NOT NULL,
            window_title TEXT,
            url TEXT,
            
            -- Content
            screen_content TEXT,
            screen_content_hash TEXT,
            
            -- Interaction signals
            mouse_events TEXT,
            interaction_count INTEGER DEFAULT 0,
            dwell_time_seconds INTEGER DEFAULT 0,
            
            -- Audio/Camera
            voice_transcription TEXT,
            camera_description TEXT,
            
            -- System info
            battery_percent INTEGER,
            is_charging BOOLEAN,
            network_type TEXT,
            location_lat REAL,
            location_lon REAL,
            cpu_usage REAL,
            memory_usage REAL,
            
            -- Classification (assigned during compression)
            content_type TEXT,
            domain TEXT,
            
            -- Session linking
            session_id TEXT,
            
            -- Status
            compressed BOOLEAN DEFAULT FALSE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    
    # Create indexes
    conn.execute("CREATE INDEX IF NOT EXISTS idx_timestamp ON raw_events(timestamp)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_compressed ON raw_events(compressed)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_session ON raw_events(session_id)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_device ON raw_events(device_id)")
    
    conn.commit()


def init_duckdb_schema(conn):
    """Initialize DuckDB schema for compressed content."""
    # Sessions table
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sessions (
            session_id VARCHAR PRIMARY KEY,
            device_id VARCHAR NOT NULL,
            
            -- Session boundaries
            start_time TIMESTAMP NOT NULL,
            end_time TIMESTAMP NOT NULL,
            duration_seconds INTEGER,
            
            -- Classification
            app_name VARCHAR NOT NULL,
            content_type VARCHAR NOT NULL,
            domain VARCHAR NOT NULL,
            
            -- Engagement metrics
            engagement_score DOUBLE,
            interaction_count INTEGER,
            total_dwell_time INTEGER,
            
            -- Strong features (for project/topic detection)
            window_title TEXT,
            url TEXT,
            url_host TEXT,
            strong_keys_json TEXT,
            title_fingerprint TEXT,
            
            -- Interaction
            input_events INTEGER,
            copy_select_count INTEGER,
            idle_seconds INTEGER,
            
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    
    # Add missing columns if table already exists (migration)
    # Try to add columns directly (will fail if they exist, but that's ok)
    columns_to_add = [
        ('window_title', 'TEXT'),
        ('url', 'TEXT'),
        ('url_host', 'TEXT'),
        ('strong_keys_json', 'TEXT'),
        ('title_fingerprint', 'TEXT'),
        ('input_events', 'INTEGER'),
        ('copy_select_count', 'INTEGER'),
        ('idle_seconds', 'INTEGER'),
    ]
    
    for col_name, col_type in columns_to_add:
        try:
            conn.execute(f"ALTER TABLE sessions ADD COLUMN {col_name} {col_type}")
            logger.info(f"Added missing column: {col_name}")
        except Exception:
            # Column already exists, ignore
            pass
    
    # Compressed content table
    conn.execute("""
        CREATE TABLE IF NOT EXISTS compressed_content (
            content_id VARCHAR PRIMARY KEY,
            session_id VARCHAR NOT NULL,
            device_id VARCHAR NOT NULL,
            
            -- Content classification
            content_type VARCHAR NOT NULL,
            title VARCHAR,
            url VARCHAR,
            
            -- Compressed content
            summary TEXT,
            key_points TEXT,
            
            -- Entities
            extracted_entities VARCHAR,  -- JSON
            
            -- Engagement
            engagement_score DOUBLE,
            
            -- Timestamp
            timestamp TIMESTAMP NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    
    # Add missing columns to compressed_content table if it already exists (migration)
    columns_to_add_compressed = [
        ('extracted_entities', 'VARCHAR'),
    ]
    
    for col_name, col_type in columns_to_add_compressed:
        try:
            conn.execute(f"ALTER TABLE compressed_content ADD COLUMN {col_name} {col_type}")
            logger.info(f"Added missing column to compressed_content: {col_name}")
        except Exception:
            # Column already exists, ignore
            pass


# ============================================================================
# CONTENT CLASSIFICATION
# ============================================================================

def classify_content(app_name: str, url: Optional[str] = None, window_title: Optional[str] = None) -> Tuple[str, str]:
    """
    Classify content type and domain from app name, URL, and window title.
    
    Returns:
        Tuple of (content_type, domain)
    """
    app_lower = app_name.lower()
    
    # Email/Communication
    if 'outlook' in app_lower or 'mail' in app_lower:
        return ('email', 'WORK')
    if 'teams' in app_lower or 'slack' in app_lower or 'discord' in app_lower:
        return ('chat', 'WORK')
    
    # Code/Development
    if any(code_app in app_lower for code_app in ['code.exe', 'idea64.exe', 'pycharm', 'devenv.exe', 'terminal']):
        return ('code', 'WORK')
    
    # Documents
    if any(doc_app in app_lower for doc_app in ['word', 'excel', 'powerpoint', 'notion', 'acrobat']):
        return ('document', 'WORK')
    
    # Browser - check URL and title
    if any(browser in app_lower for browser in ['chrome', 'edge', 'firefox']):
        if url:
            url_lower = url.lower()
            # Development
            if any(dev_site in url_lower for dev_site in ['github.com', 'gitlab.com', 'stackoverflow.com']):
                return ('code', 'WORK')
            # Social
            if any(social in url_lower for social in ['linkedin.com', 'twitter.com', 'facebook.com']):
                return ('social', 'ENTERTAINMENT')
            # General web
            return ('web_page', 'WORK')
        return ('web_page', 'WORK')
    
    # Meetings
    if any(meeting in app_lower for meeting in ['zoom', 'teams', 'meet']):
        return ('meeting', 'WORK')
    
    # Default
    return ('unknown', 'WORK')


# ============================================================================
# SESSION DETECTION
# ============================================================================

def detect_sessions(raw_events: List[Dict]) -> List[List[Dict]]:
    """
    Detect interaction session boundaries from raw events.
    
    Session breaks occur on:
    - Idle gap >= IDLE_THRESHOLD_SECONDS
    - App name change
    - Content type change
    - Domain change
    
    Args:
        raw_events: List of raw event dictionaries
        
    Returns:
        List of sessions, where each session is a list of events
    """
    if not raw_events:
        return []
    
    sessions = []
    current_session = [raw_events[0]]
    
    for i in range(1, len(raw_events)):
        prev_event = raw_events[i-1]
        curr_event = raw_events[i]
        
        # Calculate time gap
        prev_time = datetime.fromisoformat(prev_event['timestamp'])
        curr_time = datetime.fromisoformat(curr_event['timestamp'])
        gap_seconds = (curr_time - prev_time).total_seconds()
        
        # Check for session boundaries
        should_break = False
        
        # 1. Idle threshold
        if gap_seconds >= IDLE_THRESHOLD_SECONDS:
            should_break = True
            logger.debug(f"Session break: idle gap {gap_seconds}s")
        
        # 2. App change
        if prev_event['app_name'] != curr_event['app_name']:
            should_break = True
            logger.debug(f"Session break: app change {prev_event['app_name']} -> {curr_event['app_name']}")
        
        # 3. Content type change
        prev_type = prev_event.get('content_type')
        curr_type = curr_event.get('content_type')
        if prev_type and curr_type and prev_type != curr_type:
            should_break = True
            logger.debug(f"Session break: content type change {prev_type} -> {curr_type}")
        
        # 4. Domain change
        prev_domain = prev_event.get('domain')
        curr_domain = curr_event.get('domain')
        if prev_domain and curr_domain and prev_domain != curr_domain:
            should_break = True
            logger.debug(f"Session break: domain change {prev_domain} -> {curr_domain}")
        
        # 5. Browser tab/window change
        if prev_event.get('window_title') != curr_event.get('window_title'):
            if curr_event['app_name'] in ['chrome.exe', 'msedge.exe', 'firefox.exe']:
                should_break = True
                logger.debug(f"Session break: browser tab change")
        
        if should_break:
            # Save current session and start new one
            if len(current_session) > 0:
                sessions.append(current_session)
            current_session = [curr_event]
        else:
            # Continue current session
            current_session.append(curr_event)
    
    # Don't forget last session
    if len(current_session) > 0:
        sessions.append(current_session)
    
    # Filter out very short sessions
    filtered_sessions = []
    for session in sessions:
        if len(session) > 0:
            start = datetime.fromisoformat(session[0]['timestamp'])
            end = datetime.fromisoformat(session[-1]['timestamp'])
            duration = (end - start).total_seconds()
            if duration >= MIN_SESSION_DURATION_SECONDS:
                filtered_sessions.append(session)
    
    logger.info(f"Detected {len(filtered_sessions)} sessions from {len(raw_events)} raw events")
    return filtered_sessions


def extract_high_attention_content(session_events: List[Dict]) -> Dict:
    """
    Extract high-attention content from session events.
    
    Returns:
        Dictionary with copied_content, selected_text, clicked_elements
    """
    copied_content = []
    selected_text = []
    clicked_elements = []
    
    for event in session_events:
        mouse_events_str = event.get('mouse_events', '[]')
        if not mouse_events_str:
            continue
        
        try:
            mouse_events = json.loads(mouse_events_str) if isinstance(mouse_events_str, str) else mouse_events_str
            for me in mouse_events:
                event_type = me.get('eventType', '')
                content = me.get('content', '') or me.get('text', '') or me.get('value', '')
                
                if not content:
                    continue
                
                if 'Copy' in event_type or 'copy' in event_type.lower():
                    copied_content.append(content)
                elif 'Selection' in event_type or 'TextSelection' in event_type or 'select' in event_type.lower():
                    selected_text.append(content)
                elif 'Click' in event_type or 'click' in event_type.lower():
                    clicked_elements.append(content)
        except (json.JSONDecodeError, TypeError):
            pass
    
    # Deduplicate while preserving order
    def dedupe(items):
        seen = set()
        result = []
        for item in items:
            if item and item not in seen:
                seen.add(item)
                result.append(item)
        return result
    
    return {
        'copied_content': dedupe(copied_content),
        'selected_text': dedupe(selected_text),
        'clicked_elements': dedupe(clicked_elements)
    }


def extract_entities(session_events: List[Dict], high_attention: Dict) -> Dict:
    """
    Extract key entities: numbers, dates, URLs, emails from session content.
    
    Priority: High-attention content > full screen content
    
    Args:
        session_events: List of session event dictionaries
        high_attention: Dictionary with copied_content, selected_text, etc.
        
    Returns:
        Dictionary with entities: numbers, dates, urls, emails
    """
    # Combine all text, prioritizing high-attention content
    priority_text = " ".join(
        high_attention.get('copied_content', []) + 
        high_attention.get('selected_text', [])
    )
    
    # Get screen content from events
    all_screen_content = " ".join([
        e.get('screen_content', '') or '' 
        for e in session_events
    ])
    
    # Search priority text first, then full content (limit to avoid regex timeout)
    combined_text = priority_text + " " + all_screen_content[:10000]
    
    # Extract entities using regex patterns
    entities = {
        'numbers': re.findall(r'\$?\d{1,3}(?:,\d{3})*(?:\.\d+)?[MKB%]?', combined_text),
        'dates': re.findall(
            r'\b\d{1,2}[/-]\d{1,2}[/-]\d{2,4}\b|'
            r'\b(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]* \d{1,2},? \d{4}\b|'
            r'\bQ[1-4] \d{4}\b',
            combined_text
        ),
        'urls': re.findall(r'https?://[^\s<>"{}|\\^`\[\]]+', combined_text),
        'emails': re.findall(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', combined_text),
    }
    
    # Deduplicate and limit to top 15 per type
    entities = {k: list(set(v))[:15] for k, v in entities.items()}
    
    return entities


def extract_url_host(url: Optional[str]) -> Optional[str]:
    """
    Extract host from URL.
    
    Args:
        url: URL string
        
    Returns:
        Host string or None
    """
    if not url:
        return None
    
    try:
        from urllib.parse import urlparse
        parsed = urlparse(url)
        host = parsed.netloc
        # Remove www. prefix
        if host.startswith('www.'):
            host = host[4:]
        return host if host else None
    except Exception:
        return None


def extract_strong_keys(session_events: List[Dict]) -> Dict:
    """
    Extract strong keys for project/topic detection from URL and window title.
    
    Returns:
        Dictionary with repo, issue, doc_id, meeting_id, etc.
    """
    strong_keys = {}
    
    if not session_events:
        return strong_keys
    
    first_event = session_events[0]
    url = first_event.get('url', '')
    window_title = first_event.get('window_title', '') or ''
    
    # Extract from URL
    if url:
        url_lower = url.lower()
        
        # GitHub repo: github.com/user/repo
        github_repo_match = re.search(r'github\.com/([^/]+)/([^/?]+)', url_lower)
        if github_repo_match:
            strong_keys['repo'] = f"github.com/{github_repo_match.group(1)}/{github_repo_match.group(2)}"
            
            # GitHub issue: github.com/user/repo/issues/123
            issue_match = re.search(r'/issues/(\d+)', url_lower)
            if issue_match:
                strong_keys['issue'] = issue_match.group(1)
            
            # GitHub PR: github.com/user/repo/pull/123
            pr_match = re.search(r'/pull/(\d+)', url_lower)
            if pr_match:
                strong_keys['pr'] = pr_match.group(1)
        
        # GitLab repo: gitlab.com/user/repo
        gitlab_repo_match = re.search(r'gitlab\.com/([^/]+)/([^/?]+)', url_lower)
        if gitlab_repo_match:
            strong_keys['repo'] = f"gitlab.com/{gitlab_repo_match.group(1)}/{gitlab_repo_match.group(2)}"
        
        # Google Docs: docs.google.com/document/d/DOC_ID
        docs_match = re.search(r'docs\.google\.com/document/d/([a-zA-Z0-9_-]+)', url_lower)
        if docs_match:
            strong_keys['doc_id'] = docs_match.group(1)
        
        # Google Sheets: docs.google.com/spreadsheets/d/SHEET_ID
        sheets_match = re.search(r'docs\.google\.com/spreadsheets/d/([a-zA-Z0-9_-]+)', url_lower)
        if sheets_match:
            strong_keys['sheet_id'] = sheets_match.group(1)
    
    # Extract from window title
    if window_title:
        # Meeting IDs from common meeting apps
        # Zoom: "Meeting Title - Zoom Meeting"
        if 'zoom' in window_title.lower():
            # Try to extract meeting ID if present
            zoom_id_match = re.search(r'(\d{3}[\s-]?\d{3}[\s-]?\d{4})', window_title)
            if zoom_id_match:
                strong_keys['meeting_id'] = zoom_id_match.group(1).replace(' ', '').replace('-', '')
        
        # Teams meeting
        if 'teams' in window_title.lower() and 'meeting' in window_title.lower():
            strong_keys['meeting_type'] = 'teams'
    
    return strong_keys


def generate_title_fingerprint(window_title: Optional[str]) -> Optional[str]:
    """
    Generate a fingerprint for window title using hash.
    Simple implementation using MD5 hash (can be upgraded to SimHash later).
    
    Args:
        window_title: Window title string
        
    Returns:
        Hash fingerprint string or None
    """
    if not window_title:
        return None
    
    # Normalize: remove common suffixes, lowercase
    normalized = window_title.lower()
    # Remove common browser/app suffixes
    normalized = re.sub(r'\s*-\s*(google chrome|microsoft edge|firefox|mozilla firefox)$', '', normalized)
    normalized = re.sub(r'\s*-\s*(visual studio code|code)$', '', normalized)
    normalized = re.sub(r'\s*-\s*(outlook|word|excel|powerpoint)$', '', normalized)
    
    # Generate hash (first 16 chars of MD5 for shorter fingerprint)
    hash_obj = hashlib.md5(normalized.encode('utf-8'))
    return hash_obj.hexdigest()[:16]


def calculate_idle_seconds(session_events: List[Dict], idle_threshold: int = 300) -> int:
    """
    Calculate total idle time within a session.
    Idle time is defined as gaps between events >= idle_threshold seconds.
    
    Args:
        session_events: List of session event dictionaries
        idle_threshold: Threshold in seconds for considering a gap as idle
        
    Returns:
        Total idle seconds
    """
    if len(session_events) < 2:
        return 0
    
    total_idle = 0
    
    for i in range(1, len(session_events)):
        prev_time = datetime.fromisoformat(session_events[i-1]['timestamp'])
        curr_time = datetime.fromisoformat(session_events[i]['timestamp'])
        gap_seconds = (curr_time - prev_time).total_seconds()
        
        # Only count gaps >= threshold as idle
        if gap_seconds >= idle_threshold:
            total_idle += int(gap_seconds)
    
    return total_idle


def calculate_engagement(session_events: List[Dict]) -> Dict:
    """
    Calculate engagement metrics for a session.
    
    Returns:
        Dictionary with engagement_score, interaction_count, total_dwell_time
    """
    total_interactions = sum(e.get('interaction_count', 0) for e in session_events)
    total_dwell = sum(e.get('dwell_time_seconds', 0) for e in session_events)
    
    # Parse mouse events to detect copies/selections
    has_copied = False
    has_selected = False
    copied_count = 0
    selection_count = 0
    
    for event in session_events:
        mouse_events_str = event.get('mouse_events', '[]')
        if not mouse_events_str:
            continue
        
        try:
            mouse_events = json.loads(mouse_events_str) if isinstance(mouse_events_str, str) else mouse_events_str
            for me in mouse_events:
                event_type = me.get('eventType', '')
                if 'Copy' in event_type:
                    has_copied = True
                    copied_count += 1
                elif 'Selection' in event_type or 'TextSelection' in event_type:
                    has_selected = True
                    selection_count += 1
        except (json.JSONDecodeError, TypeError):
            pass
    
    # Calculate engagement score
    score = 0.0
    
    if has_copied:
        score += 0.4  # Strongest signal
    if has_selected:
        score += 0.2
    if total_interactions > 5:
        score += 0.2
    if total_dwell > 30:
        score += 0.2
    
    # Boost for multiple copies/selections
    if copied_count > 3:
        score = min(score + 0.1, 1.0)
    
    return {
        'engagement_score': min(score, 1.0),
        'interaction_count': total_interactions,
        'total_dwell_time': total_dwell,
        'has_copied': has_copied,
        'has_selected': has_selected,
        'copied_count': copied_count,
        'selection_count': selection_count,
        'copy_select_count': copied_count + selection_count  # Total copy/select operations
    }


def generate_summary(session_events: List[Dict], engagement: Dict) -> Tuple[str, List[str]]:
    """
    Generate a basic summary for a session.
    
    Returns:
        Tuple of (summary, key_points)
    """
    if not session_events:
        return ("No content", [])
    
    # Get first event for context
    first_event = session_events[0]
    app_name = first_event.get('app_name', 'unknown')
    window_title = first_event.get('window_title') or ''
    content_type = first_event.get('content_type', 'unknown')
    
    # Build summary
    summary_parts = []
    
    # Add app/context info
    summary_parts.append(f"User activity in {app_name}")
    if window_title:
        summary_parts.append(f"Window: {window_title[:100] if len(window_title) > 100 else window_title}")
    
    # Add engagement info
    if engagement['has_copied']:
        summary_parts.append(f"User copied {engagement['copied_count']} items")
    if engagement['has_selected']:
        summary_parts.append(f"User selected text {engagement['selection_count']} times")
    
    # Add duration
    start = datetime.fromisoformat(session_events[0]['timestamp'])
    end = datetime.fromisoformat(session_events[-1]['timestamp'])
    duration_minutes = (end - start).total_seconds() / 60
    summary_parts.append(f"Duration: {duration_minutes:.1f} minutes")
    
    summary = ". ".join(summary_parts) + "."
    
    # Extract key points (simplified)
    key_points = []
    if window_title:
        key_points.append(f"Context: {window_title[:80]}")
    if engagement['has_copied']:
        key_points.append(f"Copied {engagement['copied_count']} items")
    if engagement['interaction_count'] > 10:
        key_points.append(f"High interaction: {engagement['interaction_count']} interactions")
    
    return (summary, key_points[:5])  # Limit to 5 key points


# ============================================================================
# COMPRESSION SCHEDULER
# ============================================================================

class SessionCompressionScheduler:
    """Periodic scheduler for session compression from SQLite to DuckDB."""
    
    def __init__(
        self,
        device_id: str,
        database_dir: Path,
        compression_interval: int = 300,
        compression_delay: int = 60
    ):
        """Initialize the compression scheduler."""
        self.device_id = device_id
        self.database_dir = database_dir
        self.compression_interval = compression_interval
        self.compression_delay = compression_delay
        self.running = False
        
        # Database paths
        self.sqlite_path = database_dir / device_id / "raw_events.db"
        self.duckdb_path = database_dir / device_id / "compressed_context.duckdb"
        
        # Database connections
        self.sqlite_conn: Optional[sqlite3.Connection] = None
        self.duckdb_conn: Optional[duckdb.DuckDBPyConnection] = None
        
        logger.info(f"CompressionScheduler initialized for device: {device_id}")
    
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
            
            # Connect to DuckDB
            self.duckdb_conn = duckdb.connect(str(self.duckdb_path))
            init_duckdb_schema(self.duckdb_conn)
            
            logger.info("✓ Database connections initialized successfully")
            logger.info(f"  SQLite: {self.sqlite_path}")
            logger.info(f"  DuckDB: {self.duckdb_path}")
            
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
                    # Truncate window_title to 1-2KB (use 2048 chars as max)
                    window_title = window_title_raw[:2048] if window_title_raw else None
                    
                    url = session_events[0].get('url') or None
                    url_host = extract_url_host(url)
                    
                    strong_keys = extract_strong_keys(session_events)
                    strong_keys_json = json.dumps(strong_keys) if strong_keys else None
                    
                    title_fingerprint = generate_title_fingerprint(window_title_raw)
                    
                    # Calculate input_events (total mouse/keyboard events)
                    input_events = engagement['interaction_count']
                    
                    # Get copy_select_count from engagement
                    copy_select_count = engagement.get('copy_select_count', 0)
                    
                    # Calculate idle_seconds
                    idle_seconds = calculate_idle_seconds(session_events, IDLE_THRESHOLD_SECONDS)
                    
                    # Store session metadata with all new fields
                    # For existing records, new fields will remain NULL (not updated)
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
                            -- New fields are not updated for existing records (remain NULL)
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
                    # compressed_content table has 12 columns: content_id, session_id, device_id,
                    # content_type, title, url, summary, key_points, extracted_entities,
                    # engagement_score, timestamp, created_at (with DEFAULT)
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
    
    def _generate_session_id(self, first_event: Dict) -> str:
        """Generate unique session ID from first event."""
        timestamp = first_event['timestamp']
        device_id = first_event['device_id']
        app_name = first_event['app_name']
        
        unique_str = f"{device_id}_{timestamp}_{app_name}"
        return hashlib.md5(unique_str.encode()).hexdigest()
    
    def run_periodic(self):
        """Run periodic compression loop."""
        self.running = True
        last_compression_time = 0
        
        logger.info("=" * 60)
        logger.info("Session Compression Scheduler")
        logger.info("=" * 60)
        logger.info(f"Device ID: {self.device_id}")
        logger.info(f"Compression interval: {self.compression_interval}s ({self.compression_interval/60:.1f} minutes)")
        logger.info(f"Compression delay: {self.compression_delay}s")
        logger.info(f"SQLite: {self.sqlite_path}")
        logger.info(f"DuckDB: {self.duckdb_path}")
        logger.info("=" * 60)
        logger.info("")
        
        try:
            while self.running:
                current_time = time.time()
                
                # Check if it's time to run compression
                if current_time - last_compression_time >= self.compression_interval:
                    # Execute compression
                    result = self.run_compression()
                    
                    # Log statistics
                    if result['success']:
                        uncompressed = self.get_uncompressed_count()
                        logger.info(
                            f"Status: {uncompressed} uncompressed events remaining | "
                            f"Last run: {result['events_compressed']} events compressed"
                        )
                    else:
                        logger.warning(f"Compression failed: {result.get('error', 'Unknown error')}")
                    
                    last_compression_time = current_time
                
                # Sleep for a short interval before checking again
                time.sleep(10)  # Check every 10 seconds
                
        except KeyboardInterrupt:
            logger.info("")
            logger.info("Received interrupt signal, shutting down...")
            self.stop()
        except Exception as e:
            logger.error(f"Scheduler error: {e}", exc_info=True)
            self.stop()
    
    def stop(self):
        """Stop the scheduler and close database connections."""
        self.running = False
        
        if self.sqlite_conn:
            self.sqlite_conn.close()
            logger.info("✓ SQLite connection closed")
        
        if self.duckdb_conn:
            self.duckdb_conn.close()
            logger.info("✓ DuckDB connection closed")
        
        logger.info("Scheduler stopped")


# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

def main():
    """Main entry point for the compression scheduler."""
    
    print("Starting Session Compression Scheduler...")
    print(f"Current directory: {Path.cwd()}")
    
    scheduler = SessionCompressionScheduler(
        device_id=DEVICE_ID,
        database_dir=DATABASE_DIR,
        compression_interval=COMPRESSION_INTERVAL,
        compression_delay=COMPRESSION_DELAY
    )
    
    # Register signal handlers for graceful shutdown
    def signal_handler(sig, frame):
        logger.info("Received signal, shutting down gracefully...")
        scheduler.stop()
        sys.exit(0)
    
    # Windows only supports SIGINT
    if sys.platform == 'win32':
        signal.signal(signal.SIGINT, signal_handler)
    else:
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
    
    try:
        # Initialize connections
        scheduler.initialize()
        
        # Start periodic compression
        scheduler.run_periodic()
        
    except Exception as e:
        logger.error(f"Failed to start scheduler: {e}", exc_info=True)
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
