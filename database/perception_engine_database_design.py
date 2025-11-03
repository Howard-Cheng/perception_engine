"""
Perception Engine Database Design & Pipeline Implementation
============================================================

Complete implementation of the layered database architecture for cross-device
context collection, compression, and retrieval.

Multi-Layer Storage:
- Layer 0 (SQLite): Raw events, 24hr buffer → 2-5GB/day
- Layer 1 (DuckDB): Compressed sessions → 200-500MB/day (90% reduction!)
- Layer 2 (DuckDB): Work/day aggregations
- Layer 3 (Qdrant Local): Vector search for semantic queries

Session Detection:
- 5min idle threshold (tunable)
- Smart boundaries: app change, content type change, tab switch
- Hierarchical: Interaction → Work → Day sessions

Engagement-Aware Compression:
- Copied text = 0.4 score (strongest signal!)
- High engagement → Keep rich details (500 bytes)
- Low engagement → Minimal summary (100 bytes)
- Content-type specific strategies (email vs code vs web)

Query Optimization:
- Information retrieval: <50ms
- AI context generation: Top-k by engagement
- Day recap: Pre-aggregated summaries
"""

import sqlite3
import duckdb
import json
import re
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass, asdict
from enum import Enum
import hashlib
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


# ============================================================================
# CONFIGURATION
# ============================================================================

@dataclass
class DatabaseConfig:
    """Database paths and configuration"""
    base_dir: Path = Path("./perception_data")  # Relative to exe
    
    # Database files
    sqlite_db: str = "raw_events.db"
    duckdb_db: str = "compressed_context.duckdb"
    qdrant_path: str = "vector_store"
    
    # Retention policies (seconds)
    raw_retention_hours: int = 24
    compressed_retention_days: int = 7
    
    # Session detection thresholds
    idle_threshold_seconds: int = 300  # 5 minutes default
    min_session_duration_seconds: int = 3  # Ignore very short sessions
    
    # Compression settings
    compression_delay_seconds: int = 60  # Wait before compressing active session
    max_tokens_per_content_type: Dict[str, int] = None
    
    # Embedding settings
    embedding_model: str = "sentence-transformers/all-MiniLM-L6-v2" # or the most SOTA model: https://huggingface.co/google/embeddinggemma-300m
    embedding_dimension: int = 384
    
    def __post_init__(self):
        self.base_dir = Path(self.base_dir)
        self.base_dir.mkdir(parents=True, exist_ok=True)
        
        if self.max_tokens_per_content_type is None:
            self.max_tokens_per_content_type = {
                "email": 300,
                "chat": 250,
                "web_article": 200,
                "web_page": 150,
                "code": 400,
                "document": 250,
                "meeting": 400,
                "video": 100,
                "default": 200
            }


@dataclass
class SessionConfig:
    """Session detection configuration"""
    # Idle thresholds
    idle_threshold_seconds: int = 300  # 5 minutes
    
    # Strong break events that force new session
    strong_break_events: List[str] = None
    
    # Session boundary triggers
    domain_change_triggers_new_session: bool = True
    app_type_change_triggers_new_session: bool = True
    content_type_change_triggers_new_session: bool = True
    
    # Work session detection
    work_session_idle_threshold_minutes: int = 15
    work_session_overlap_threshold: float = 0.5  # 50% entity overlap
    
    def __post_init__(self):
        if self.strong_break_events is None:
            self.strong_break_events = [
                'meeting_start',
                'meeting_end', 
                'screen_lock',
                'screen_unlock',
                'sleep',
                'wake',
                'fullscreen_media_start'
            ]


class ContentType(Enum):
    """Content types for classification"""
    EMAIL = "email"
    CHAT = "chat"
    WEB_ARTICLE = "web_article"
    WEB_PAGE = "web_page"
    CODE = "code"
    DOCUMENT = "document"
    MEETING = "meeting"
    VIDEO = "video"
    SOCIAL = "social"
    RESEARCH_PAPER = "research_paper"
    UNKNOWN = "unknown"


class Domain(Enum):
    """Top-level domains for classification"""
    SYSTEM = "SYSTEM"
    WORK = "WORK"
    ENTERTAINMENT = "ENTERTAINMENT"
    LIFE = "LIFE"
    INTERACTION = "INTERACTION"


# ============================================================================
# CONTENT TYPE CLASSIFICATION
# ============================================================================

class ContentClassifier:
    """Classify content type from app name, URL, and window title"""
    
    APP_TO_CONTENT_TYPE = {
        # Communication
        "outlook.exe": ContentType.EMAIL,
        "OUTLOOK.EXE": ContentType.EMAIL,
        "Teams.exe": ContentType.CHAT,
        "slack.exe": ContentType.CHAT,
        "discord.exe": ContentType.CHAT,
        
        # Development
        "Code.exe": ContentType.CODE,
        "code.exe": ContentType.CODE,
        "idea64.exe": ContentType.CODE,
        "pycharm64.exe": ContentType.CODE,
        "devenv.exe": ContentType.CODE,
        "WindowsTerminal.exe": ContentType.CODE,
        
        # Documents
        "WINWORD.EXE": ContentType.DOCUMENT,
        "EXCEL.EXE": ContentType.DOCUMENT,
        "POWERPNT.EXE": ContentType.DOCUMENT,
        "AcroRd32.exe": ContentType.DOCUMENT,
        "Notion.exe": ContentType.DOCUMENT,
        
        # Meetings
        "Zoom.exe": ContentType.MEETING,
        "ms-teams.exe": ContentType.MEETING,
        
        # Browser - requires URL detection
        "chrome.exe": None,  # Requires URL
        "msedge.exe": None,
        "firefox.exe": None,
    }
    
    URL_TO_CONTENT_TYPE = {
        # Development
        "github.com": ContentType.CODE,
        "gitlab.com": ContentType.CODE,
        "stackoverflow.com": ContentType.CODE,
        "stackexchange.com": ContentType.CODE,
        
        # Social/Entertainment
        "linkedin.com": ContentType.SOCIAL,
        "twitter.com": ContentType.SOCIAL,
        "facebook.com": ContentType.SOCIAL,
        "youtube.com": ContentType.VIDEO,
        "netflix.com": ContentType.VIDEO,
        "twitch.tv": ContentType.VIDEO,
        
        # News/Articles
        "wsj.com": ContentType.WEB_ARTICLE,
        "nytimes.com": ContentType.WEB_ARTICLE,
        "techcrunch.com": ContentType.WEB_ARTICLE,
        "medium.com": ContentType.WEB_ARTICLE,
        
        # Research
        "arxiv.org": ContentType.RESEARCH_PAPER,
        "acm.org": ContentType.RESEARCH_PAPER,
        "ieee.org": ContentType.RESEARCH_PAPER,
        
        # Shopping
        "amazon.com": ContentType.WEB_PAGE,
        "jd.com": ContentType.WEB_PAGE,
        "taobao.com": ContentType.WEB_PAGE,
    }
    
    DOMAIN_RULES = {
        # WORK indicators
        ContentType.EMAIL: Domain.WORK,
        ContentType.CHAT: Domain.WORK,
        ContentType.CODE: Domain.WORK,
        ContentType.DOCUMENT: Domain.WORK,
        ContentType.MEETING: Domain.WORK,
        
        # ENTERTAINMENT indicators
        ContentType.VIDEO: Domain.ENTERTAINMENT,
        ContentType.SOCIAL: Domain.ENTERTAINMENT,
        
        # Default to INTERACTION for unknown
        ContentType.UNKNOWN: Domain.INTERACTION,
    }
    
    @classmethod
    def classify(cls, app_name: str, url: Optional[str] = None, 
                 window_title: Optional[str] = None) -> Tuple[ContentType, Domain]:
        """
        Classify content type and domain from available context
        
        Returns:
            (ContentType, Domain) tuple
        """
        # Try app-based classification first
        content_type = cls.APP_TO_CONTENT_TYPE.get(app_name)
        
        # If browser, need URL-based refinement
        if content_type is None and url:
            domain = cls._extract_domain(url)
            content_type = cls.URL_TO_CONTENT_TYPE.get(domain, ContentType.WEB_PAGE)
        
        # Fallback
        if content_type is None:
            content_type = ContentType.UNKNOWN
        
        # Determine domain
        domain = cls.DOMAIN_RULES.get(content_type, Domain.INTERACTION)
        
        return content_type, domain
    
    @staticmethod
    def _extract_domain(url: str) -> str:
        """Extract domain from URL"""
        import urllib.parse
        try:
            parsed = urllib.parse.urlparse(url)
            # Remove www. prefix
            domain = parsed.netloc.replace('www.', '')
            return domain
        except:
            return ""


# ============================================================================
# SCHEMA INITIALIZATION
# ============================================================================

class SchemaManager:
    """Manages database schema creation and migrations"""
    
    @staticmethod
    def init_sqlite(conn: sqlite3.Connection):
        """Initialize Layer 0 raw events schema"""
        
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
                mouse_events TEXT,  -- JSON string
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
        
        # Indexes for performance
        conn.execute("CREATE INDEX IF NOT EXISTS idx_timestamp ON raw_events(timestamp)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_compressed ON raw_events(compressed)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_session ON raw_events(session_id)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_device ON raw_events(device_id)")
        
        conn.commit()
        logger.info("SQLite schema initialized")
    
    @staticmethod
    def init_duckdb(conn):
        """Initialize Layer 1 & 2 compressed/aggregated schema"""
        
        # Layer 1: Sessions
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
                subdomain VARCHAR,
                
                -- Engagement metrics
                engagement_score DOUBLE,
                interaction_count INTEGER,
                total_dwell_time INTEGER,
                
                -- Session hierarchy
                work_session_id VARCHAR,
                day_session_id VARCHAR,
                
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        
        # Layer 1: Compressed content
        conn.execute("""
            CREATE TABLE IF NOT EXISTS compressed_content (
                content_id VARCHAR PRIMARY KEY,
                session_id VARCHAR NOT NULL,
                device_id VARCHAR NOT NULL,
                
                -- Content identification
                content_type VARCHAR NOT NULL,
                title VARCHAR,
                url VARCHAR,
                file_path VARCHAR,
                
                -- Structured extraction (JSON)
                metadata VARCHAR,
                
                -- High-attention content
                copied_content VARCHAR[],
                selected_text VARCHAR[],
                
                -- Entities
                extracted_entities VARCHAR,  -- JSON
                
                -- Summary
                summary VARCHAR NOT NULL,
                key_points VARCHAR[],
                
                -- Retrieval
                engagement_score DOUBLE,
                timestamp TIMESTAMP NOT NULL,
                
                -- Linking
                related_content_ids VARCHAR[]
            )
        """)
        
        # Layer 2: Work sessions
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
        
        # Layer 2: Day sessions
        conn.execute("""
            CREATE TABLE IF NOT EXISTS day_sessions (
                day_session_id VARCHAR PRIMARY KEY,
                device_id VARCHAR NOT NULL,
                date DATE NOT NULL,
                
                start_time TIMESTAMP NOT NULL,
                end_time TIMESTAMP,
                
                -- Aggregation
                work_session_ids VARCHAR[],
                total_active_time_seconds INTEGER,
                
                -- Summary by domain
                work_summary VARCHAR,
                entertainment_summary VARCHAR,
                life_summary VARCHAR,
                
                -- Top entities/projects
                top_projects VARCHAR,  -- JSON
                top_entities VARCHAR,  -- JSON
                
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        
        logger.info("DuckDB schema initialized")


# ============================================================================
# SESSION DETECTION ENGINE
# ============================================================================

class SessionDetector:
    """
    Detects session boundaries from raw events using configurable rules.
    
    Session Types:
    1. Interaction Session: Per app/tab/window continuous interaction
    2. Work Session: Higher-level task/project session
    3. Day Session: Full day from wake to sleep
    """
    
    def __init__(self, config: SessionConfig):
        self.config = config
    
    def detect_interaction_sessions(self, raw_events: List[Dict]) -> List[List[Dict]]:
        """
        Detect interaction session boundaries from chronological raw events.
        
        Boundaries triggered by:
        - Idle gap >= threshold
        - App name change
        - Content type change
        - Domain change
        - Strong break events
        
        Args:
            raw_events: List of raw event dicts sorted by timestamp
            
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
            if gap_seconds >= self.config.idle_threshold_seconds:
                should_break = True
                logger.debug(f"Session break: idle gap {gap_seconds}s")
            
            # 2. App change
            if self.config.app_type_change_triggers_new_session:
                if prev_event['app_name'] != curr_event['app_name']:
                    should_break = True
                    logger.debug(f"Session break: app change {prev_event['app_name']} -> {curr_event['app_name']}")
            
            # 3. Content type change
            if self.config.content_type_change_triggers_new_session:
                if prev_event.get('content_type') != curr_event.get('content_type'):
                    should_break = True
                    logger.debug(f"Session break: content type change")
            
            # 4. Domain change
            if self.config.domain_change_triggers_new_session:
                if prev_event.get('domain') != curr_event.get('domain'):
                    should_break = True
                    logger.debug(f"Session break: domain change")
            
            # 5. Window/tab change (for browser)
            if prev_event['window_title'] != curr_event['window_title']:
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
        sessions = [s for s in sessions if self._get_session_duration(s) >= self.config.min_session_duration_seconds]
        
        logger.info(f"Detected {len(sessions)} interaction sessions from {len(raw_events)} raw events")
        return sessions
    
    def detect_work_sessions(self, interaction_sessions: List[Dict]) -> List[List[Dict]]:
        """
        Group interaction sessions into higher-level work sessions based on:
        - Project/entity overlap
        - Time proximity
        - Domain consistency
        
        Args:
            interaction_sessions: List of session metadata dicts
            
        Returns:
            List of work sessions, where each is a list of interaction session dicts
        """
        if not interaction_sessions:
            return []
        
        work_sessions = []
        current_work_session = [interaction_sessions[0]]
        
        for i in range(1, len(interaction_sessions)):
            prev_session = interaction_sessions[i-1]
            curr_session = interaction_sessions[i]
            
            # Calculate time gap between sessions
            prev_end = datetime.fromisoformat(prev_session['end_time'])
            curr_start = datetime.fromisoformat(curr_session['start_time'])
            gap_minutes = (curr_start - prev_end).total_seconds() / 60
            
            # Check for work session boundary
            should_break = False
            
            # 1. Long idle gap
            if gap_minutes >= self.config.work_session_idle_threshold_minutes:
                should_break = True
            
            # 2. Domain change (WORK -> ENTERTAINMENT)
            if prev_session['domain'] != curr_session['domain']:
                should_break = True
            
            # 3. Project change (if project_name available)
            prev_project = prev_session.get('project_name')
            curr_project = curr_session.get('project_name')
            if prev_project and curr_project and prev_project != curr_project:
                # Check entity overlap
                overlap = self._calculate_entity_overlap(prev_session, curr_session)
                if overlap < self.config.work_session_overlap_threshold:
                    should_break = True
            
            if should_break:
                work_sessions.append(current_work_session)
                current_work_session = [curr_session]
            else:
                current_work_session.append(curr_session)
        
        # Last work session
        if current_work_session:
            work_sessions.append(current_work_session)
        
        logger.info(f"Detected {len(work_sessions)} work sessions from {len(interaction_sessions)} interaction sessions")
        return work_sessions
    
    @staticmethod
    def _get_session_duration(events: List[Dict]) -> float:
        """Calculate session duration in seconds"""
        if not events:
            return 0
        
        start = datetime.fromisoformat(events[0]['timestamp'])
        end = datetime.fromisoformat(events[-1]['timestamp'])
        
        # Add the last event's dwell time
        last_dwell = events[-1].get('dwell_time_seconds', 0)
        
        return (end - start).total_seconds() + last_dwell
    
    @staticmethod
    def _calculate_entity_overlap(session1: Dict, session2: Dict) -> float:
        """
        Calculate entity overlap between two sessions.
        
        Returns:
            Overlap ratio [0, 1]
        """
        # Extract entities from both sessions
        entities1 = set()
        entities2 = set()
        
        if 'extracted_entities' in session1:
            for entity_list in session1['extracted_entities'].values():
                entities1.update(entity_list)
        
        if 'extracted_entities' in session2:
            for entity_list in session2['extracted_entities'].values():
                entities2.update(entity_list)
        
        if not entities1 or not entities2:
            return 0.0
        
        # Jaccard similarity
        intersection = len(entities1 & entities2)
        union = len(entities1 | entities2)
        
        return intersection / union if union > 0 else 0.0


# ============================================================================
# ENGAGEMENT CALCULATOR
# ============================================================================

class EngagementCalculator:
    """Calculate engagement scores from interaction signals"""
    
    @staticmethod
    def calculate(raw_events: List[Dict]) -> Dict:
        """
        Calculate engagement metrics from raw events.
        
        Scoring factors:
        - Copied content: 0.4 (strongest signal)
        - Selected text: 0.2
        - High interaction count: 0.2
        - Long dwell time: 0.2
        
        Returns:
            Dict with engagement_score, interaction_count, has_copied, etc.
        """
        total_interactions = sum(e.get('interaction_count', 0) for e in raw_events)
        total_dwell = sum(e.get('dwell_time_seconds', 0) for e in raw_events)
        
        # Parse mouse events to detect copies/selections
        has_copied = False
        has_selected = False
        copied_count = 0
        selection_count = 0
        
        for event in raw_events:
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
            except json.JSONDecodeError:
                pass
        
        # Calculate engagement score
        score = 0.0
        
        if has_copied:
            score += 0.4
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
            'selection_count': selection_count
        }


# ============================================================================
# CONTENT EXTRACTOR
# ============================================================================

class ContentExtractor:
    """Extract high-attention content and entities from raw events"""
    
    @staticmethod
    def extract_high_attention_content(raw_events: List[Dict]) -> Dict:
        """
        Extract content user specifically interacted with.
        
        Returns:
            Dict with copied_content, selected_text, clicked_elements
        """
        copied_content = []
        selected_text = []
        clicked_elements = []
        
        for event in raw_events:
            mouse_events_str = event.get('mouse_events', '[]')
            if not mouse_events_str:
                continue
            
            try:
                mouse_events = json.loads(mouse_events_str) if isinstance(mouse_events_str, str) else mouse_events_str
                
                for me in mouse_events:
                    event_type = me.get('eventType', '')
                    content = me.get('content', '')
                    
                    # Filter out noise
                    if not content or content in ['[No Content Found]', '[No Text Selected]']:
                        continue
                    
                    if 'Copy' in event_type:
                        copied_content.append(content)
                    elif 'Selection' in event_type or 'TextSelection' in event_type:
                        selected_text.append(content)
                    elif 'Click' in event_type or 'DoubleClick' in event_type:
                        clicked_elements.append(content)
            except (json.JSONDecodeError, TypeError):
                pass
        
        # Deduplicate while preserving order
        def dedupe(items):
            seen = set()
            result = []
            for item in items:
                if item not in seen:
                    seen.add(item)
                    result.append(item)
            return result
        
        return {
            'copied_content': dedupe(copied_content),
            'selected_text': dedupe(selected_text),
            'clicked_elements': dedupe(clicked_elements)
        }
    
    @staticmethod
    def extract_entities(raw_events: List[Dict], high_attention: Dict) -> Dict:
        """
        Extract key entities: numbers, dates, names, URLs, emails.
        
        Priority: High-attention content > full screen content
        """
        # Combine all text, prioritizing high-attention content
        priority_text = " ".join(
            high_attention.get('copied_content', []) + 
            high_attention.get('selected_text', [])
        )
        
        all_screen_content = " ".join([
            e.get('screen_content', '') or '' 
            for e in raw_events
        ])
        
        # Search priority text first, then full content
        combined_text = priority_text + " " + all_screen_content[:10000]  # Limit to avoid regex timeout
        
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
        
        # Dedupe and limit
        entities = {k: list(set(v))[:15] for k, v in entities.items()}
        
        return entities
    
    @staticmethod
    def extract_content_specific_metadata(content_type: ContentType, raw_events: List[Dict]) -> Dict:
        """
        Extract content-type specific metadata.
        
        For emails: sender, subject, participants
        For documents: file_path, doc_type
        For code: file_path, repo_url
        etc.
        """
        metadata = {}
        
        if not raw_events:
            return metadata
        
        # Get first event for context
        first_event = raw_events[0]
        
        if content_type == ContentType.EMAIL:
            # Parse email metadata from window title or content
            window_title = first_event.get('window_title', '')
            # Example: "RE: Q4 Budget - Outlook"
            if ' - ' in window_title:
                parts = window_title.split(' - ')
                metadata['subject'] = parts[0].strip()
            
        elif content_type == ContentType.CODE:
            # Extract file path from window title
            window_title = first_event.get('window_title', '')
            # Example: "main.py - perception_engine - Visual Studio Code"
            if ' - ' in window_title:
                parts = window_title.split(' - ')
                metadata['file_name'] = parts[0].strip()
                if len(parts) > 1:
                    metadata['project_name'] = parts[1].strip()
            
            # Extract repo URL if available
            url = first_event.get('url', '')
            if 'github.com' in url or 'gitlab.com' in url:
                metadata['repo_url'] = url
        
        elif content_type == ContentType.DOCUMENT:
            window_title = first_event.get('window_title', '')
            # Example: "Q4_Report.docx - Word"
            if ' - ' in window_title:
                parts = window_title.split(' - ')
                filename = parts[0].strip()
                metadata['file_name'] = filename
                
                # Detect doc type from extension
                if filename.endswith('.docx') or filename.endswith('.doc'):
                    metadata['doc_type'] = 'word'
                elif filename.endswith('.xlsx') or filename.endswith('.xls'):
                    metadata['doc_type'] = 'excel'
                elif filename.endswith('.pptx') or filename.endswith('.ppt'):
                    metadata['doc_type'] = 'powerpoint'
                elif filename.endswith('.pdf'):
                    metadata['doc_type'] = 'pdf'
        
        elif content_type in [ContentType.WEB_ARTICLE, ContentType.WEB_PAGE]:
            metadata['url'] = first_event.get('url', '')
            metadata['title'] = first_event.get('window_title', '').replace(' - Google Chrome', '').replace(' - Microsoft Edge', '')
        
        elif content_type == ContentType.MEETING:
            window_title = first_event.get('window_title', '')
            metadata['meeting_title'] = window_title
            
            # Get audio transcription if available
            transcriptions = [e.get('voice_transcription', '') for e in raw_events if e.get('voice_transcription')]
            if transcriptions:
                metadata['has_transcription'] = True
                metadata['transcription_count'] = len(transcriptions)
        
        return metadata


# ============================================================================
# LLM COMPRESSION ENGINE
# ============================================================================

class LLMCompressor:
    """
    On-device LLM compression using:
    - Phi-4-mini-instruct for PC
    - Llama 3.2 3B instruct for Android
    """
    
    def __init__(self, model_name: str = "phi-4-mini-instruct", device: str = "pc"):
        """
        Args:
            model_name: Name of the LLM model
            device: 'pc' or 'android'
        """
        self.model_name = model_name
        self.device = device
        
        # In production, initialize actual model here
        # For now, this is a placeholder
        logger.info(f"Initialized LLM compressor: {model_name} on {device}")
    
    def compress(
        self, 
        content: str, 
        high_attention: Dict, 
        content_type: ContentType,
        max_tokens: int = 200
    ) -> Dict:
        """
        Compress content using LLM, focusing on high-attention areas.
        
        Args:
            content: Full screen content
            high_attention: Dict with copied_content, selected_text
            content_type: Type of content
            max_tokens: Maximum tokens for summary
            
        Returns:
            Dict with summary, key_points
        """
        
        # Build prompt based on content type
        prompt = self._build_prompt(content, high_attention, content_type, max_tokens)
        
        # Generate summary (placeholder - replace with actual model inference)
        summary = self._generate_summary(prompt, max_tokens)
        
        # Extract key points
        key_points = self._extract_key_points(summary, high_attention)
        
        return {
            'summary': summary,
            'key_points': key_points
        }
    
    def _build_prompt(
        self, 
        content: str, 
        high_attention: Dict, 
        content_type: ContentType,
        max_tokens: int
    ) -> str:
        """Build compression prompt based on content type"""
        
        # Truncate content if too long (keep first 3000 chars)
        if len(content) > 3000:
            content = content[:3000] + "..."
        
        copied = high_attention.get('copied_content', [])
        selected = high_attention.get('selected_text', [])
        
        prompt = f"""Summarize the following {content_type.value} content in approximately {max_tokens} tokens.

Focus especially on information the user interacted with:
"""
        
        if copied:
            prompt += f"\nUser COPIED (most important):\n"
            for item in copied[:5]:  # Limit to top 5
                prompt += f"- {item}\n"
        
        if selected:
            prompt += f"\nUser SELECTED:\n"
            for item in selected[:5]:
                prompt += f"- {item}\n"
        
        prompt += f"\n\nFull content:\n{content}\n\n"
        prompt += f"""Create a concise summary that:
1. Captures the main topic/purpose
2. Includes key information (especially numbers, dates, decisions)
3. Prioritizes what the user found important based on interactions

Summary:"""
        
        return prompt
    
    def _generate_summary(self, prompt: str, max_tokens: int) -> str:
        """
        Generate summary using LLM.
        
        This is a placeholder - in production, replace with actual model inference.
        """
        # TODO: Replace with actual model inference
        # For now, return a simple extraction
        
        # Simple fallback: extract first sentence and key points
        lines = prompt.split('\n')
        key_info = []
        
        for line in lines:
            if line.startswith('- ') or line.startswith('User '):
                key_info.append(line.strip())
        
        if key_info:
            summary = "User reviewed content with focus on: " + "; ".join(key_info[:3])
        else:
            summary = "User reviewed content."
        
        return summary[:max_tokens * 4]  # Rough token estimate
    
    def _extract_key_points(self, summary: str, high_attention: Dict) -> List[str]:
        """Extract key points from summary and high-attention content"""
        
        key_points = []
        
        # Add copied content as key points
        copied = high_attention.get('copied_content', [])
        key_points.extend(copied[:3])
        
        # Split summary into sentences and take first 2-3
        sentences = [s.strip() for s in summary.split('.') if len(s.strip()) > 20]
        key_points.extend(sentences[:2])
        
        return key_points[:5]  # Max 5 key points


# ============================================================================
# COMPRESSION PIPELINE
# ============================================================================

class CompressionPipeline:
    """
    Main compression pipeline: Layer 0 (Raw) -> Layer 1 (Compressed)
    """
    
    def __init__(
        self, 
        sqlite_conn: sqlite3.Connection,
        duckdb_conn,
        llm_compressor: LLMCompressor,
        config: DatabaseConfig
    ):
        self.sqlite = sqlite_conn
        self.duckdb = duckdb_conn
        self.llm = llm_compressor
        self.config = config
        self.session_detector = SessionDetector(SessionConfig())
        self.engagement_calc = EngagementCalculator()
        self.content_extractor = ContentExtractor()
    
    def compress_ready_sessions(self):
        """
        Find and compress sessions that are ready (completed and not yet compressed).
        
        A session is ready if:
        - Last event was > compression_delay_seconds ago
        - Not yet marked as compressed
        """
        # Find uncompressed events
        cursor = self.sqlite.execute("""
            SELECT * FROM raw_events
            WHERE compressed = FALSE
            ORDER BY timestamp ASC
        """)
        
        raw_events = [dict(row) for row in cursor.fetchall()]
        
        if not raw_events:
            logger.info("No raw events to compress")
            return
        
        logger.info(f"Found {len(raw_events)} uncompressed raw events")
        
        # Classify content types first
        for event in raw_events:
            content_type, domain = ContentClassifier.classify(
                event['app_name'],
                event.get('url'),
                event.get('window_title')
            )
            event['content_type'] = content_type.value
            event['domain'] = domain.value
        
        # Detect session boundaries
        sessions = self.session_detector.detect_interaction_sessions(raw_events)
        
        logger.info(f"Detected {len(sessions)} sessions to compress")
        
        # Compress each session
        for session_events in sessions:
            self._compress_single_session(session_events)
    
    def _compress_single_session(self, session_events: List[Dict]):
        """Compress a single session from raw events to Layer 1"""
        
        if not session_events:
            return
        
        # Generate session ID
        session_id = self._generate_session_id(session_events[0])
        
        # Assign session_id to all events
        for event in session_events:
            event['session_id'] = session_id
        
        # Calculate engagement
        engagement = self.engagement_calc.calculate(session_events)
        
        # Extract high-attention content
        high_attention = self.content_extractor.extract_high_attention_content(session_events)
        
        # Extract entities
        entities = self.content_extractor.extract_entities(session_events, high_attention)
        
        # Get content type and domain
        content_type = ContentType(session_events[0]['content_type'])
        domain = Domain(session_events[0]['domain'])
        
        # Extract content-specific metadata
        metadata = self.content_extractor.extract_content_specific_metadata(
            content_type, 
            session_events
        )
        
        # LLM compression
        full_content = "\n\n".join([
            e.get('screen_content', '') or '' 
            for e in session_events
        ])
        
        max_tokens = self.config.max_tokens_per_content_type.get(
            content_type.value,
            self.config.max_tokens_per_content_type['default']
        )
        
        compressed = self.llm.compress(
            content=full_content,
            high_attention=high_attention,
            content_type=content_type,
            max_tokens=max_tokens
        )
        
        # Store session metadata
        self._store_session(
            session_id=session_id,
            session_events=session_events,
            engagement=engagement,
            content_type=content_type,
            domain=domain
        )
        
        # Store compressed content
        content_id = self._generate_content_id(session_id)
        self._store_compressed_content(
            content_id=content_id,
            session_id=session_id,
            device_id=session_events[0]['device_id'],
            content_type=content_type,
            metadata=metadata,
            high_attention=high_attention,
            entities=entities,
            compressed=compressed,
            engagement=engagement,
            timestamp=session_events[0]['timestamp']
        )
        
        # Mark raw events as compressed
        self._mark_events_compressed(session_events)
        
        logger.info(f"Compressed session {session_id} with {len(session_events)} events")
    
    def _store_session(
        self,
        session_id: str,
        session_events: List[Dict],
        engagement: Dict,
        content_type: ContentType,
        domain: Domain
    ):
        """Store session metadata in Layer 1"""
        
        start_time = session_events[0]['timestamp']
        end_time = session_events[-1]['timestamp']
        
        duration = SessionDetector._get_session_duration(session_events)
        
        self.duckdb.execute("""
            INSERT INTO sessions VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
        """, [
            session_id,
            session_events[0]['device_id'],
            start_time,
            end_time,
            int(duration),
            session_events[0]['app_name'],
            content_type.value,
            domain.value,
            None,  # subdomain - to be inferred
            engagement['engagement_score'],
            engagement['interaction_count'],
            engagement['total_dwell_time'],
            None,  # work_session_id
            None   # day_session_id
        ])
    
    def _store_compressed_content(
        self,
        content_id: str,
        session_id: str,
        device_id: str,
        content_type: ContentType,
        metadata: Dict,
        high_attention: Dict,
        entities: Dict,
        compressed: Dict,
        engagement: Dict,
        timestamp: str
    ):
        """Store compressed content in Layer 1"""
        
        self.duckdb.execute("""
            INSERT INTO compressed_content VALUES (
                ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
            )
        """, [
            content_id,
            session_id,
            device_id,
            content_type.value,
            metadata.get('title') or metadata.get('file_name') or metadata.get('subject'),
            metadata.get('url'),
            metadata.get('file_path'),
            json.dumps(metadata),
            high_attention.get('copied_content', []),
            high_attention.get('selected_text', []),
            json.dumps(entities),
            compressed['summary'],
            compressed['key_points'],
            engagement['engagement_score'],
            timestamp
        ])
    
    def _mark_events_compressed(self, session_events: List[Dict]):
        """Mark raw events as compressed"""
        
        event_ids = [e['event_id'] for e in session_events]
        
        placeholders = ','.join(['?' for _ in event_ids])
        
        self.sqlite.execute(f"""
            UPDATE raw_events
            SET compressed = TRUE, session_id = ?
            WHERE event_id IN ({placeholders})
        """, [session_events[0]['session_id']] + event_ids)
        
        self.sqlite.commit()
    
    @staticmethod
    def _generate_session_id(first_event: Dict) -> str:
        """Generate unique session ID"""
        timestamp = first_event['timestamp']
        device_id = first_event['device_id']
        app_name = first_event['app_name']
        
        unique_str = f"{device_id}_{timestamp}_{app_name}"
        return hashlib.md5(unique_str.encode()).hexdigest()
    
    @staticmethod
    def _generate_content_id(session_id: str) -> str:
        """Generate unique content ID"""
        return f"content_{session_id}"


# ============================================================================
# DATA INGESTION
# ============================================================================

class DataIngestion:
    """Layer 0 data ingestion - write raw events"""
    
    def __init__(self, sqlite_conn: sqlite3.Connection, device_id: str):
        self.sqlite = sqlite_conn
        self.device_id = device_id
    
    def ingest_event(self, event_data: Dict):
        """
        Ingest a single raw event into Layer 0.
        
        Args:
            event_data: Dict with app_name, window_title, screen_content, etc.
        """
        # Generate event ID
        event_id = self._generate_event_id(event_data)
        
        # Compute content hash for deduplication
        screen_content = event_data.get('screen_content', '')
        content_hash = hashlib.md5(screen_content.encode()).hexdigest() if screen_content else None
        
        # Convert mouse events to JSON string
        mouse_events = json.dumps(event_data.get('mouse_events', []))
        
        # Insert into SQLite
        self.sqlite.execute("""
            INSERT OR REPLACE INTO raw_events (
                event_id, timestamp, device_id, app_name, window_title, url,
                screen_content, screen_content_hash, mouse_events, interaction_count,
                dwell_time_seconds, voice_transcription, camera_description,
                battery_percent, is_charging, network_type, location_lat, location_lon,
                cpu_usage, memory_usage, compressed, created_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, FALSE, CURRENT_TIMESTAMP)
        """, [
            event_id,
            event_data.get('timestamp', datetime.now().isoformat()),
            self.device_id,
            event_data['app_name'],
            event_data.get('window_title'),
            event_data.get('url'),
            screen_content,
            content_hash,
            mouse_events,
            event_data.get('interaction_count', 0),
            event_data.get('dwell_time_seconds', 0),
            event_data.get('voice_transcription'),
            event_data.get('camera_description'),
            event_data.get('battery_percent'),
            event_data.get('is_charging'),
            event_data.get('network_type'),
            event_data.get('location_lat'),
            event_data.get('location_lon'),
            event_data.get('cpu_usage'),
            event_data.get('memory_usage')
        ])
        
        self.sqlite.commit()
        logger.debug(f"Ingested event {event_id}")
    
    @staticmethod
    def _generate_event_id(event_data: Dict) -> str:
        """Generate unique event ID"""
        timestamp = event_data.get('timestamp', datetime.now().isoformat())
        app_name = event_data['app_name']
        window_title = event_data.get('window_title', '')
        
        unique_str = f"{timestamp}_{app_name}_{window_title}"
        return hashlib.md5(unique_str.encode()).hexdigest()


# ============================================================================
# QUERY ENGINE
# ============================================================================

class QueryEngine:
    """Query compressed context for information retrieval and AI context"""
    
    def __init__(self, duckdb_conn):
        self.duckdb = duckdb_conn
    
    def search_by_keyword(
        self, 
        keywords: List[str], 
        days_back: int = 1,
        content_types: List[str] = None,
        min_engagement: float = 0.3
    ) -> List[Dict]:
        """
        Keyword-based search in compressed content.
        
        Args:
            keywords: List of keywords to search
            days_back: How many days back to search
            content_types: Filter by content types
            min_engagement: Minimum engagement score
            
        Returns:
            List of matching compressed content dicts
        """
        # Build WHERE clause
        cutoff_date = (datetime.now() - timedelta(days=days_back)).isoformat()
        
        where_clauses = [
            f"timestamp >= '{cutoff_date}'",
            f"engagement_score >= {min_engagement}"
        ]
        
        if content_types:
            types_str = "', '".join(content_types)
            where_clauses.append(f"content_type IN ('{types_str}')")
        
        # Keyword matching in summary, title, and key_points
        keyword_conditions = []
        for keyword in keywords:
            keyword_conditions.append(f"""
                (LOWER(summary) LIKE LOWER('%{keyword}%') OR
                 LOWER(title) LIKE LOWER('%{keyword}%') OR
                 array_to_string(key_points, ' ') LIKE LOWER('%{keyword}%'))
            """)
        
        if keyword_conditions:
            where_clauses.append("(" + " OR ".join(keyword_conditions) + ")")
        
        where_clause = " AND ".join(where_clauses)
        
        query = f"""
            SELECT 
                c.*,
                s.domain,
                s.subdomain,
                s.app_name
            FROM compressed_content c
            JOIN sessions s ON c.session_id = s.session_id
            WHERE {where_clause}
            ORDER BY c.engagement_score DESC, c.timestamp DESC
            LIMIT 20
        """
        
        results = self.duckdb.execute(query).fetchall()
        
        # Convert to dicts
        columns = [desc[0] for desc in self.duckdb.description]
        return [dict(zip(columns, row)) for row in results]
    
    def get_context_for_ai(
        self, 
        query: str, 
        days_back: int = 1, 
        top_k: int = 15
    ) -> str:
        """
        Retrieve relevant context for AI assistant.
        
        This would ideally use vector search, but for now uses keyword matching.
        
        Args:
            query: User's query/task description
            days_back: How many days to look back
            top_k: Number of results
            
        Returns:
            Formatted context string for LLM
        """
        # Extract keywords from query (simple approach)
        keywords = [w for w in query.lower().split() if len(w) > 3]
        
        # Search for relevant content
        results = self.search_by_keyword(keywords, days_back=days_back)[:top_k]
        
        if not results:
            return "No relevant context found."
        
        # Format as context
        context_parts = []
        for item in results:
            context_parts.append(f"""
---
Source: {item['title'] or item['app_name']} ({item['content_type']})
When: {item['timestamp']}
Domain: {item['domain']}
Engagement: {item['engagement_score']:.2f}

Summary: {item['summary']}

Key Points:
{chr(10).join(['- ' + kp for kp in (item['key_points'] or [])])}

Entities: {item['extracted_entities']}
---
            """.strip())
        
        return "\n\n".join(context_parts)
    
    def get_day_recap(self, date: str = None) -> Dict:
        """Get summary of activities for a specific day"""
        
        if not date:
            date = datetime.now().date().isoformat()
        
        # Query all sessions for this day
        query = f"""
            SELECT 
                s.domain,
                s.content_type,
                COUNT(*) as session_count,
                SUM(s.duration_seconds) as total_seconds,
                AVG(s.engagement_score) as avg_engagement
            FROM sessions s
            WHERE DATE(s.start_time) = '{date}'
            GROUP BY s.domain, s.content_type
            ORDER BY total_seconds DESC
        """
        
        results = self.duckdb.execute(query).fetchall()
        
        if not results:
            return {"message": f"No activity found for {date}"}
        
        # Get top content for the day
        top_content_query = f"""
            SELECT 
                c.title,
                c.content_type,
                c.summary,
                c.engagement_score
            FROM compressed_content c
            WHERE DATE(c.timestamp) = '{date}'
            ORDER BY c.engagement_score DESC
            LIMIT 10
        """
        
        top_content = self.duckdb.execute(top_content_query).fetchall()
        
        # Format results
        domain_breakdown = {}
        for row in results:
            domain = row[0]
            if domain not in domain_breakdown:
                domain_breakdown[domain] = {
                    'total_minutes': 0,
                    'session_count': 0,
                    'content_types': {}
                }
            
            domain_breakdown[domain]['session_count'] += row[2]
            domain_breakdown[domain]['total_minutes'] += row[3] / 60
            domain_breakdown[domain]['content_types'][row[1]] = {
                'count': row[2],
                'minutes': row[3] / 60
            }
        
        return {
            'date': date,
            'domain_breakdown': domain_breakdown,
            'top_content': [
                {
                    'title': c[0],
                    'type': c[1],
                    'summary': c[2],
                    'engagement': c[3]
                }
                for c in top_content
            ]
        }


# ============================================================================
# DATA RETENTION & CLEANUP
# ============================================================================

class DataRetention:
    """Manage data retention policies and cleanup"""
    
    def __init__(self, sqlite_conn: sqlite3.Connection, duckdb_conn, config: DatabaseConfig):
        self.sqlite = sqlite_conn
        self.duckdb = duckdb_conn
        self.config = config
    
    def cleanup_old_data(self):
        """Remove old data based on retention policies"""
        
        # Clean up Layer 0 (raw events older than retention period)
        cutoff_time = datetime.now() - timedelta(hours=self.config.raw_retention_hours)
        
        # Only delete if compressed
        deleted = self.sqlite.execute("""
            DELETE FROM raw_events
            WHERE timestamp < ? AND compressed = TRUE
        """, [cutoff_time.isoformat()])
        
        self.sqlite.commit()
        logger.info(f"Cleaned up {deleted.rowcount} old raw events")
        
        # Clean up Layer 1 (compressed content older than retention period)
        compressed_cutoff = datetime.now() - timedelta(days=self.config.compressed_retention_days)
        
        self.duckdb.execute("""
            DELETE FROM compressed_content
            WHERE timestamp < ?
        """, [compressed_cutoff.isoformat()])
        
        logger.info(f"Cleaned up old compressed content")


# ============================================================================
# MAIN ORCHESTRATOR
# ============================================================================

class PerceptionEngine:
    """
    Main orchestrator for Perception Engine database system.
    """
    
    def __init__(self, config: DatabaseConfig, device_id: str):
        self.config = config
        self.device_id = device_id
        
        # Initialize database connections
        self.sqlite_path = self.config.base_dir / self.config.sqlite_db
        self.duckdb_path = self.config.base_dir / self.config.duckdb_db
        
        self.sqlite_conn = sqlite3.connect(str(self.sqlite_path))
        self.sqlite_conn.row_factory = sqlite3.Row
        
        self.duckdb_conn = duckdb.connect(str(self.duckdb_path))
        
        # Initialize schemas
        SchemaManager.init_sqlite(self.sqlite_conn)
        SchemaManager.init_duckdb(self.duckdb_conn)
        
        # Initialize components
        self.llm = LLMCompressor(device="pc")
        self.ingestion = DataIngestion(self.sqlite_conn, device_id)
        self.compression = CompressionPipeline(
            self.sqlite_conn, 
            self.duckdb_conn, 
            self.llm,
            self.config
        )
        self.query_engine = QueryEngine(self.duckdb_conn)
        self.retention = DataRetention(self.sqlite_conn, self.duckdb_conn, self.config)
        
        logger.info(f"Perception Engine initialized for device {device_id}")
    
    def ingest(self, event_data: Dict):
        """Ingest raw event into Layer 0"""
        self.ingestion.ingest_event(event_data)
    
    def run_compression(self):
        """Run compression pipeline (Layer 0 -> Layer 1)"""
        logger.info("Starting compression pipeline...")
        self.compression.compress_ready_sessions()
    
    def search(self, query: str, days_back: int = 1) -> List[Dict]:
        """Search compressed context"""
        keywords = [w for w in query.lower().split() if len(w) > 3]
        return self.query_engine.search_by_keyword(keywords, days_back=days_back)
    
    def get_ai_context(self, task: str, days_back: int = 1) -> str:
        """Get context for AI assistant"""
        return self.query_engine.get_context_for_ai(task, days_back=days_back)
    
    def get_recap(self, date: str = None) -> Dict:
        """Get day recap"""
        return self.query_engine.get_day_recap(date)
    
    def cleanup(self):
        """Run data retention cleanup"""
        logger.info("Running data cleanup...")
        self.retention.cleanup_old_data()
    
    def close(self):
        """Close database connections"""
        self.sqlite_conn.close()
        self.duckdb_conn.close()
        logger.info("Perception Engine closed")


# ============================================================================
# EXAMPLE USAGE
# ============================================================================

if __name__ == "__main__":
    
    # Initialize
    config = DatabaseConfig(
        base_dir=Path("./perception_data"),
        idle_threshold_seconds=300
    )
    
    device_id = "pc_001"
    engine = PerceptionEngine(config, device_id)
    
    # Example 1: Ingest raw events
    print("\n=== Example 1: Ingesting Raw Events ===")
    
    sample_event = {
        'timestamp': datetime.now().isoformat(),
        'app_name': 'chrome.exe',
        'window_title': 'Introducing Avalon - Aqua Voice Blog - Google Chrome',
        'url': 'https://aquavoice.com/blog/avalon',
        'screen_content': 'Introducing Avalon, a speech recognition model...',
        'mouse_events': [
            {
                'timestamp': datetime.now().isoformat(),
                'eventType': 'LeftClick',
                'position': {'x': 100, 'y': 200},
                'content': 'Download button',
                'elementType': 'Button'
            },
            {
                'timestamp': datetime.now().isoformat(),
                'eventType': 'TextSelection',
                'content': 'Avalon outperforms Whisper Large v3 on 7 of 8 test splits',
                'elementType': 'Text'
            }
        ],
        'interaction_count': 5,
        'dwell_time_seconds': 45,
        'battery_percent': 85,
        'is_charging': False,
        'network_type': 'WiFi',
        'cpu_usage': 15.3,
        'memory_usage': 45.2
    }
    
    engine.ingest(sample_event)
    print("✓ Ingested sample event")
    
    # Example 2: Run compression
    print("\n=== Example 2: Running Compression ===")
    engine.run_compression()
    
    # Example 3: Search for information
    print("\n=== Example 3: Information Retrieval ===")
    results = engine.search("Avalon speech recognition", days_back=1)
    print(f"Found {len(results)} results")
    for r in results[:2]:
        print(f"\n- {r['title']}")
        print(f"  Summary: {r['summary'][:100]}...")
        print(f"  Engagement: {r['engagement_score']:.2f}")
    
    # Example 4: Get AI context
    print("\n=== Example 4: Context for AI ===")
    context = engine.get_ai_context("Help me understand speech recognition models", days_back=1)
    print(f"Generated context ({len(context)} chars):\n{context[:300]}...")
    
    # Example 5: Day recap
    print("\n=== Example 5: Day Recap ===")
    recap = engine.get_recap()
    print(json.dumps(recap, indent=2))
    
    # Cleanup
    print("\n=== Cleanup ===")
    engine.cleanup()
    engine.close()
    
    print("\n✓ All examples completed!")
