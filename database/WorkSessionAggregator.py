#!/usr/bin/env python3
"""
Work Session Aggregator
========================

Periodic scheduler that groups interaction sessions into work sessions using
weighted scoring system.

This script runs every 5 minutes to:
1. Query unaggregated interaction sessions from DuckDB
2. Group sessions into work sessions based on weighted scoring:
   - Time similarity (exponential decay with adaptive threshold)
   - Domain/app consistency
   - Weighted entity Jaccard (by content_type)
   - Title/strong_keys similarity
   - Embedding similarity (optional, all-MiniLM-L6-v2)
3. Store work session metadata to DuckDB
4. Log merge features for learning/auditing
5. Update sessions with work_session_id

Features:
- Periodic execution (configurable interval)
- Weighted scoring system with adaptive time threshold
- Project-based hard constraints (no cross-project merging)
- Optional lightweight embedding support
- Feature logging for future learning
- Error handling with detailed logging
- Graceful shutdown on SIGINT/SIGTERM

Usage:
    python WorkSessionAggregator.py
"""

import duckdb
import time
import signal
import sys
import json
import hashlib
import logging
import math
import re
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

# ============================================================================
# CONFIGURATION
# ============================================================================

# Execution interval (seconds)
AGGREGATION_INTERVAL = 300  # 5 minutes

# Work session detection thresholds (adaptive)
DEFAULT_TIME_THRESHOLD_SECONDS = 900  # 15 minutes default
MIN_TIME_THRESHOLD_SECONDS = 300  # 5 minutes minimum
MAX_TIME_THRESHOLD_SECONDS = 2700  # 45 minutes maximum

# Scoring weights (default, without embedding)
WEIGHT_TIME = 0.25
WEIGHT_DOMAIN = 0.1
WEIGHT_APP = 0.1
WEIGHT_ENTITY = 0.4
WEIGHT_TITLE = 0.15
WEIGHT_EMBED = 0.0  # Will be set if embedding is available

# Scoring weights (with embedding)
WEIGHT_TIME_EMBED = 0.20
WEIGHT_DOMAIN_EMBED = 0.08
WEIGHT_APP_EMBED = 0.08
WEIGHT_ENTITY_EMBED = 0.28
WEIGHT_TITLE_EMBED = 0.11
WEIGHT_EMBED_EMBED = 0.25  # Embedding weight when available

# Merge threshold
MERGE_THRESHOLD = 0.58  # Default merge threshold

# Hard constraints
MAX_CROSS_DAY_HOURS = 8  # Maximum hours between sessions in same work session

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

def init_duckdb_schema(conn):
    """Initialize DuckDB schema for work sessions."""
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
    # This is a no-op if column already exists
    try:
        conn.execute("ALTER TABLE sessions ADD COLUMN work_session_id VARCHAR")
    except Exception:
        # Column already exists, ignore
        pass


# ============================================================================
# ENTITY WEIGHTS BY CONTENT TYPE
# ============================================================================

# Entity type weights by content_type
# Format: {content_type: {entity_type: weight}}
ENTITY_WEIGHTS = {
    'code': {
        'code_identifiers': 1.0,  # Files/classes/functions/repo/issue
        'urls': 0.4,
        'title_ngrams': 0.6,
        'numbers': 0.3,
        'dates': 0.3,
        'emails': 0.3,
    },
    'web_page': {
        'code_identifiers': 0.5,
        'urls': 1.0,
        'title_ngrams': 0.8,
        'numbers': 0.4,
        'dates': 0.4,
        'emails': 0.6,
    },
    'document': {
        'code_identifiers': 0.7,
        'urls': 0.5,
        'title_ngrams': 1.0,
        'numbers': 0.5,
        'dates': 0.5,
        'emails': 0.7,
    },
    'meeting': {
        'code_identifiers': 0.3,
        'urls': 0.5,
        'title_ngrams': 0.6,
        'numbers': 0.3,
        'dates': 0.3,
        'emails': 1.0,  # People/organizations/meeting IDs
    },
}

# Default weights if content_type not found
DEFAULT_ENTITY_WEIGHTS = {
    'code_identifiers': 0.5,
    'urls': 0.6,
    'title_ngrams': 0.7,
    'numbers': 0.4,
    'dates': 0.4,
    'emails': 0.5,
}


# ============================================================================
# WORK SESSION DETECTION
# ============================================================================

def calculate_weighted_jaccard(session1: Dict, session2: Dict) -> float:
    """
    Calculate weighted Jaccard similarity between two sessions.
    
    Args:
        session1: First session dict with extracted_entities and content_type
        session2: Second session dict with extracted_entities and content_type
        
    Returns:
        Weighted Jaccard similarity [0, 1]
    """
    # Get content types
    content_type1 = session1.get('content_type', 'unknown')
    content_type2 = session2.get('content_type', 'unknown')
    
    # Use the first session's content_type for weights (or average if different)
    weights = ENTITY_WEIGHTS.get(content_type1, DEFAULT_ENTITY_WEIGHTS)
    if content_type1 != content_type2:
        # Average weights if different content types
        weights2 = ENTITY_WEIGHTS.get(content_type2, DEFAULT_ENTITY_WEIGHTS)
        weights = {k: (weights.get(k, 0) + weights2.get(k, 0)) / 2 
                  for k in set(weights.keys()) | set(weights2.keys())}
    
    # Extract entities from both sessions
    entities1 = {}
    entities2 = {}
    
    # Parse extracted_entities
    entities1_data = session1.get('extracted_entities')
    if entities1_data:
        if isinstance(entities1_data, str):
            try:
                entities1_data = json.loads(entities1_data)
            except (json.JSONDecodeError, TypeError):
                entities1_data = {}
        if isinstance(entities1_data, dict):
            entities1 = entities1_data
    
    entities2_data = session2.get('extracted_entities')
    if entities2_data:
        if isinstance(entities2_data, str):
            try:
                entities2_data = json.loads(entities2_data)
            except (json.JSONDecodeError, TypeError):
                entities2_data = {}
        if isinstance(entities2_data, dict):
            entities2 = entities2_data
    
    # Map entity types to weights
    entity_type_mapping = {
        'urls': 'urls',
        'emails': 'emails',
        'numbers': 'numbers',
        'dates': 'dates',
    }
    
    # Calculate weighted intersection and union
    weighted_intersection = 0.0
    weighted_union = 0.0
    
    # Process each entity type
    all_entity_types = set(entities1.keys()) | set(entities2.keys())
    
    for entity_type in all_entity_types:
        # Map to weight key
        weight_key = entity_type_mapping.get(entity_type, 'title_ngrams')
        weight = weights.get(weight_key, 0.5)
        
        set1 = set(entities1.get(entity_type, []))
        set2 = set(entities2.get(entity_type, []))
        
        intersection = len(set1 & set2)
        union = len(set1 | set2)
        
        if union > 0:
            weighted_intersection += weight * intersection
            weighted_union += weight * union
    
    # Also check strong_keys for code identifiers
    strong_keys1 = session1.get('strong_keys_json')
    strong_keys2 = session2.get('strong_keys_json')
    
    if strong_keys1 and strong_keys2:
        try:
            if isinstance(strong_keys1, str):
                strong_keys1 = json.loads(strong_keys1)
            if isinstance(strong_keys2, str):
                strong_keys2 = json.loads(strong_keys2)
            
            # Check for repo, issue, pr, doc_id matches
            code_weight = weights.get('code_identifiers', 1.0)
            
            # Check repo match
            if strong_keys1.get('repo') and strong_keys2.get('repo'):
                if strong_keys1['repo'] == strong_keys2['repo']:
                    weighted_intersection += code_weight * 2
                    weighted_union += code_weight * 2
                else:
                    weighted_union += code_weight * 2
            
            # Check issue/PR/doc_id matches
            for key in ['issue', 'pr', 'doc_id', 'sheet_id']:
                if strong_keys1.get(key) and strong_keys2.get(key):
                    if strong_keys1[key] == strong_keys2[key]:
                        weighted_intersection += code_weight
                        weighted_union += code_weight
                    else:
                        weighted_union += code_weight
        
        except (json.JSONDecodeError, TypeError):
            pass
    
    if weighted_union == 0:
        return 0.0
    
    return weighted_intersection / weighted_union


def calculate_entity_overlap(session1: Dict, session2: Dict) -> float:
    """
    Calculate entity overlap between two sessions using simple Jaccard similarity.
    (Kept for backward compatibility, use calculate_weighted_jaccard for new code)
    
    Args:
        session1: First session dict with extracted_entities
        session2: Second session dict with extracted_entities
        
    Returns:
        Overlap ratio [0, 1]
    """
    # Extract entities from both sessions
    entities1 = set()
    entities2 = set()
    
    # Parse extracted_entities if it's a JSON string
    entities1_data = session1.get('extracted_entities')
    if entities1_data:
        if isinstance(entities1_data, str):
            try:
                entities1_data = json.loads(entities1_data)
            except (json.JSONDecodeError, TypeError):
                entities1_data = {}
        if isinstance(entities1_data, dict):
            for entity_list in entities1_data.values():
                if isinstance(entity_list, list):
                    entities1.update(entity_list)
    
    entities2_data = session2.get('extracted_entities')
    if entities2_data:
        if isinstance(entities2_data, str):
            try:
                entities2_data = json.loads(entities2_data)
            except (json.JSONDecodeError, TypeError):
                entities2_data = {}
        if isinstance(entities2_data, dict):
            for entity_list in entities2_data.values():
                if isinstance(entity_list, list):
                    entities2.update(entity_list)
    
    if not entities1 or not entities2:
        return 0.0
    
    # Jaccard similarity
    intersection = len(entities1 & entities2)
    union = len(entities1 | entities2)
    
    return intersection / union if union > 0 else 0.0


def calculate_adaptive_time_threshold(interaction_sessions: List[Dict]) -> float:
    """
    Calculate adaptive time threshold using bimodal valley method.
    
    Args:
        interaction_sessions: List of session metadata dicts sorted by start_time
        
    Returns:
        Adaptive time threshold in seconds
    """
    if len(interaction_sessions) < 2:
        return DEFAULT_TIME_THRESHOLD_SECONDS
    
    # Calculate gaps between consecutive sessions
    gaps = []
    for i in range(1, len(interaction_sessions)):
        prev_end = datetime.fromisoformat(interaction_sessions[i-1]['end_time'])
        curr_start = datetime.fromisoformat(interaction_sessions[i]['start_time'])
        gap_seconds = (curr_start - prev_end).total_seconds()
        if gap_seconds > 0:  # Only positive gaps
            gaps.append(gap_seconds)
    
    if not gaps:
        return DEFAULT_TIME_THRESHOLD_SECONDS
    
    # Use log of gaps for better distribution
    log_gaps = [math.log(g) for g in gaps if g > 0]
    
    if len(log_gaps) < 10:
        # Not enough data, use default
        return DEFAULT_TIME_THRESHOLD_SECONDS
    
    # Simple histogram-based approach to find valley
    # Find median as a simple approximation of valley
    sorted_gaps = sorted(gaps)
    median_gap = sorted_gaps[len(sorted_gaps) // 2]
    
    # Clamp to valid range
    threshold = max(MIN_TIME_THRESHOLD_SECONDS, 
                    min(MAX_TIME_THRESHOLD_SECONDS, median_gap))
    
    return threshold


def calculate_time_similarity(delta_t: float, tau_t: float) -> float:
    """
    Calculate time similarity using exponential decay.
    
    Args:
        delta_t: Time gap in seconds
        tau_t: Time threshold in seconds
        
    Returns:
        Time similarity [0, 1]
    """
    if tau_t <= 0:
        return 0.0
    return math.exp(-delta_t / tau_t)


def calculate_title_similarity(session1: Dict, session2: Dict) -> float:
    """
    Calculate title similarity based on strong_keys and title_fingerprint.
    
    Args:
        session1: First session dict
        session2: Second session dict
        
    Returns:
        Title similarity [0, 1]
    """
    # Check title_fingerprint match
    fp1 = session1.get('title_fingerprint')
    fp2 = session2.get('title_fingerprint')
    
    if fp1 and fp2 and fp1 == fp2:
        return 1.0
    
    # Check strong_keys matches
    strong_keys1 = session1.get('strong_keys_json')
    strong_keys2 = session2.get('strong_keys_json')
    
    if not strong_keys1 or not strong_keys2:
        return 0.0
    
    try:
        if isinstance(strong_keys1, str):
            strong_keys1 = json.loads(strong_keys1)
        if isinstance(strong_keys2, str):
            strong_keys2 = json.loads(strong_keys2)
        
        # Check for repo, doc_id, meeting_id matches
        matches = 0
        total = 0
        
        for key in ['repo', 'doc_id', 'sheet_id', 'meeting_id']:
            if strong_keys1.get(key) or strong_keys2.get(key):
                total += 1
                if strong_keys1.get(key) == strong_keys2.get(key):
                    matches += 1
        
        if total == 0:
            return 0.0
        
        return matches / total
    
    except (json.JSONDecodeError, TypeError):
        return 0.0


def calculate_merge_score(
    session1: Dict,
    session2: Dict,
    delta_t: float,
    tau_t: float,
    embed_model=None
) -> Tuple[float, Dict]:
    """
    Calculate weighted merge score between two sessions.
    
    Args:
        session1: First session dict
        session2: Second session dict
        delta_t: Time gap in seconds
        tau_t: Time threshold in seconds
        embed_model: Optional embedding model
        
    Returns:
        Tuple of (score, features_dict)
    """
    features = {}
    
    # 1. Time similarity
    f_time = calculate_time_similarity(delta_t, tau_t)
    features['f_time'] = f_time
    
    # 2. Domain consistency
    f_domain = 1.0 if session1.get('domain') == session2.get('domain') else 0.0
    features['f_domain'] = f_domain
    
    # 3. App consistency
    f_app = 1.0 if session1.get('app_name') == session2.get('app_name') else 0.0
    features['f_app'] = f_app
    
    # 4. Weighted Jaccard
    jaccard_w = calculate_weighted_jaccard(session1, session2)
    features['jaccard_w'] = jaccard_w
    
    # 5. Title similarity
    f_title = calculate_title_similarity(session1, session2)
    features['f_title'] = f_title
    
    # 6. Embedding similarity (if available)
    f_embed = 0.0
    if embed_model and EMBEDDING_AVAILABLE:
        try:
            # Build text for embedding
            text1 = _build_embedding_text(session1)
            text2 = _build_embedding_text(session2)
            
            if text1 and text2:
                emb1 = embed_model.encode(text1, convert_to_numpy=True)
                emb2 = embed_model.encode(text2, convert_to_numpy=True)
                
                # Cosine similarity
                dot_product = np.dot(emb1, emb2)
                norm1 = np.linalg.norm(emb1)
                norm2 = np.linalg.norm(emb2)
                
                if norm1 > 0 and norm2 > 0:
                    f_embed = dot_product / (norm1 * norm2)
                    f_embed = max(0.0, min(1.0, f_embed))  # Clamp to [0, 1]
        except Exception as e:
            logger.debug(f"Embedding calculation failed: {e}")
    
    features['f_embed'] = f_embed
    
    # Calculate weighted score
    if embed_model and EMBEDDING_AVAILABLE:
        # Use embedding-aware weights
        weights = {
            'time': WEIGHT_TIME_EMBED,
            'domain': WEIGHT_DOMAIN_EMBED,
            'app': WEIGHT_APP_EMBED,
            'entity': WEIGHT_ENTITY_EMBED,
            'title': WEIGHT_TITLE_EMBED,
            'embed': WEIGHT_EMBED_EMBED,
        }
    else:
        # Use default weights (without embedding)
        weights = {
            'time': WEIGHT_TIME,
            'domain': WEIGHT_DOMAIN,
            'app': WEIGHT_APP,
            'entity': WEIGHT_ENTITY,
            'title': WEIGHT_TITLE,
            'embed': 0.0,
        }
    
    # Normalize weights to ensure they sum to 1.0
    total = sum(weights.values())
    if total > 0:
        weights = {k: v / total for k, v in weights.items()}
    
    score = (
        weights['time'] * f_time +
        weights['domain'] * f_domain +
        weights['app'] * f_app +
        weights['entity'] * jaccard_w +
        weights['title'] * f_title +
        weights['embed'] * f_embed
    )
    
    features['score'] = score
    features['delta_t'] = delta_t
    
    return score, features


def _build_embedding_text(session: Dict) -> str:
    """Build text for embedding from session data."""
    parts = []
    
    # Add window title
    window_title = session.get('window_title', '')
    if window_title:
        parts.append(window_title[:200])  # Limit length
    
    # Add summary from compressed_content if available
    # (This would need to be joined in the query)
    
    # Add key entities
    entities = session.get('extracted_entities')
    if entities:
        if isinstance(entities, str):
            try:
                entities = json.loads(entities)
            except (json.JSONDecodeError, TypeError):
                entities = {}
        if isinstance(entities, dict):
            for entity_list in entities.values():
                if isinstance(entity_list, list):
                    parts.extend([str(e) for e in entity_list[:5]])  # Top 5 per type
    
    # Add strong_keys
    strong_keys = session.get('strong_keys_json')
    if strong_keys:
        if isinstance(strong_keys, str):
            try:
                strong_keys = json.loads(strong_keys)
            except (json.JSONDecodeError, TypeError):
                strong_keys = {}
        if isinstance(strong_keys, dict):
            parts.extend([str(v) for v in strong_keys.values() if v])
    
    text = ' '.join(parts)
    return text[:300]  # Limit to 300 chars


def check_project_conflict(session1: Dict, session2: Dict) -> bool:
    """
    Check if there's a strong project conflict between two sessions.
    
    Args:
        session1: First session dict
        session2: Second session dict
        
    Returns:
        True if there's a strong conflict (should not merge)
    """
    # Check strong_keys for repo/doc_id conflicts
    strong_keys1 = session1.get('strong_keys_json')
    strong_keys2 = session2.get('strong_keys_json')
    
    if not strong_keys1 or not strong_keys2:
        return False
    
    try:
        if isinstance(strong_keys1, str):
            strong_keys1 = json.loads(strong_keys1)
        if isinstance(strong_keys2, str):
            strong_keys2 = json.loads(strong_keys2)
        
        # If both have repo and they're different, it's a conflict
        if strong_keys1.get('repo') and strong_keys2.get('repo'):
            if strong_keys1['repo'] != strong_keys2['repo']:
                return True
        
        # If both have doc_id and they're different, it's a conflict
        if strong_keys1.get('doc_id') and strong_keys2.get('doc_id'):
            if strong_keys1['doc_id'] != strong_keys2['doc_id']:
                return True
        
        # If both have sheet_id and they're different, it's a conflict
        if strong_keys1.get('sheet_id') and strong_keys2.get('sheet_id'):
            if strong_keys1['sheet_id'] != strong_keys2['sheet_id']:
                return True
    
    except (json.JSONDecodeError, TypeError):
        pass
    
    return False


def detect_work_sessions(
    interaction_sessions: List[Dict],
    embed_model=None,
    conn=None
) -> List[List[Dict]]:
    """
    Group interaction sessions into higher-level work sessions using weighted scoring.
    
    Args:
        interaction_sessions: List of session metadata dicts sorted by start_time
        embed_model: Optional embedding model for semantic similarity
        conn: Optional DuckDB connection for logging features
        
    Returns:
        List of work sessions, where each is a list of interaction session dicts
    """
    if not interaction_sessions:
        return []
    
    # Calculate adaptive time threshold
    tau_t = calculate_adaptive_time_threshold(interaction_sessions)
    logger.debug(f"Adaptive time threshold: {tau_t:.1f} seconds ({tau_t/60:.1f} minutes)")
    
    work_sessions = []
    current_work_session = [interaction_sessions[0]]
    
    for i in range(1, len(interaction_sessions)):
        prev_session = interaction_sessions[i-1]
        curr_session = interaction_sessions[i]
        
        # Calculate time gap between sessions
        prev_end = datetime.fromisoformat(prev_session['end_time'])
        curr_start = datetime.fromisoformat(curr_session['start_time'])
        delta_t = (curr_start - prev_end).total_seconds()
        
        # Hard constraints
        should_break = False
        reason = None
        
        # 1. Cross-day constraint (max 8 hours)
        if delta_t > MAX_CROSS_DAY_HOURS * 3600:
            should_break = True
            reason = f"cross_day_gap_{delta_t/3600:.1f}h"
            logger.debug(f"Work session break: cross-day gap {delta_t/3600:.1f} hours")
        
        # 2. Project conflict (hard constraint)
        if not should_break and check_project_conflict(prev_session, curr_session):
            should_break = True
            reason = "project_conflict"
            logger.debug("Work session break: project conflict")
        
        # 3. Domain completely different with weak entity/embedding similarity
        if not should_break:
            prev_domain = prev_session.get('domain', '')
            curr_domain = curr_session.get('domain', '')
            if prev_domain != curr_domain:
                # Check if entity/embedding similarity is weak
                jaccard_w = calculate_weighted_jaccard(prev_session, curr_session)
                if jaccard_w < 0.2:
                    should_break = True
                    reason = f"domain_change_weak_sim_{jaccard_w:.2f}"
                    logger.debug(f"Work session break: domain change with weak similarity {jaccard_w:.2f}")
        
        # If hard constraints not met, calculate merge score
        if not should_break:
            score, features = calculate_merge_score(
                prev_session, curr_session, delta_t, tau_t, embed_model
            )
            
            # Log features for learning (only for near-neighbors, limit to 10 per session)
            if conn and i <= 10:  # Only log first 10 pairs per batch
                try:
                    conn.execute("""
                        INSERT INTO features_for_merge
                        (sid_left, sid_right, delta_t, f_time, same_domain, same_app,
                         jaccard_w, title_hit, embed_cos, score, decision, reason)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        ON CONFLICT (sid_left, sid_right) DO UPDATE SET
                            delta_t = EXCLUDED.delta_t,
                            f_time = EXCLUDED.f_time,
                            same_domain = EXCLUDED.same_domain,
                            same_app = EXCLUDED.same_app,
                            jaccard_w = EXCLUDED.jaccard_w,
                            title_hit = EXCLUDED.title_hit,
                            embed_cos = EXCLUDED.embed_cos,
                            score = EXCLUDED.score,
                            decision = EXCLUDED.decision,
                            reason = EXCLUDED.reason
                    """, [
                        prev_session['session_id'],
                        curr_session['session_id'],
                        features['delta_t'],
                        features['f_time'],
                        features['f_domain'] == 1.0,
                        features['f_app'] == 1.0,
                        features['jaccard_w'],
                        features['f_title'] > 0.5,
                        features['f_embed'],
                        features['score'],
                        score >= MERGE_THRESHOLD,
                        reason or "score_based"
                    ])
                except Exception as e:
                    logger.debug(f"Failed to log features: {e}")
            
            # Check if score meets threshold
            if score >= MERGE_THRESHOLD:
                # Continue current work session
                current_work_session.append(curr_session)
            else:
                # Break work session
                should_break = True
                reason = f"score_below_threshold_{score:.3f}"
                logger.debug(f"Work session break: score {score:.3f} < threshold {MERGE_THRESHOLD}")
        
        if should_break:
            # Save current work session and start new one
            if len(current_work_session) > 0:
                work_sessions.append(current_work_session)
            current_work_session = [curr_session]
    
    # Don't forget last work session
    if len(current_work_session) > 0:
        work_sessions.append(current_work_session)
    
    logger.info(f"Detected {len(work_sessions)} work sessions from {len(interaction_sessions)} interaction sessions")
    return work_sessions


def generate_work_session_summary(work_session_sessions: List[Dict]) -> Tuple[str, List[str]]:
    """
    Generate a summary for a work session.
    
    Args:
        work_session_sessions: List of interaction sessions in the work session
        
    Returns:
        Tuple of (summary, key_accomplishments)
    """
    if not work_session_sessions:
        return ("No activity", [])
    
    # Get session count and time span
    start_time = datetime.fromisoformat(work_session_sessions[0]['start_time'])
    end_time = datetime.fromisoformat(work_session_sessions[-1]['end_time'])
    duration_minutes = (end_time - start_time).total_seconds() / 60
    
    # Count unique apps and content types
    apps = set(s.get('app_name', '') for s in work_session_sessions)
    content_types = set(s.get('content_type', '') for s in work_session_sessions)
    domain = work_session_sessions[0].get('domain', 'UNKNOWN')
    
    # Build summary
    summary_parts = []
    summary_parts.append(f"Work session in {domain} domain")
    summary_parts.append(f"{len(work_session_sessions)} interaction sessions")
    summary_parts.append(f"Duration: {duration_minutes:.1f} minutes")
    summary_parts.append(f"Apps: {', '.join(sorted(apps)[:3])}")
    if len(apps) > 3:
        summary_parts.append(f"and {len(apps) - 3} more")
    
    summary = ". ".join(summary_parts) + "."
    
    # Extract key accomplishments
    key_accomplishments = []
    
    # Add project name if available
    project_names = set(s.get('project_name') for s in work_session_sessions if s.get('project_name'))
    if project_names:
        key_accomplishments.append(f"Project: {', '.join(sorted(project_names))}")
    
    # Add content types
    if content_types:
        key_accomplishments.append(f"Activities: {', '.join(sorted(content_types))}")
    
    # Add high engagement sessions
    high_engagement = [s for s in work_session_sessions if s.get('engagement_score', 0) >= 0.7]
    if high_engagement:
        key_accomplishments.append(f"{len(high_engagement)} high-engagement sessions")
    
    return (summary, key_accomplishments[:5])  # Limit to 5 accomplishments


# ============================================================================
# WORK SESSION AGGREGATOR
# ============================================================================

class WorkSessionAggregator:
    """Periodic scheduler for aggregating interaction sessions into work sessions."""
    
    def __init__(
        self,
        device_id: str,
        database_dir: Path,
        aggregation_interval: int = 300,
        use_embedding: bool = True
    ):
        """Initialize the work session aggregator."""
        self.device_id = device_id
        self.database_dir = database_dir
        self.aggregation_interval = aggregation_interval
        self.running = False
        self.use_embedding = use_embedding and EMBEDDING_AVAILABLE
        
        # Database path
        self.duckdb_path = database_dir / device_id / "compressed_context.duckdb"
        
        # Database connection
        self.duckdb_conn: Optional[duckdb.DuckDBPyConnection] = None
        
        # Embedding model (lazy load)
        self.embed_model = None
        
        logger.info(f"WorkSessionAggregator initialized for device: {device_id}")
        if self.use_embedding:
            logger.info("Embedding model will be loaded on first use")
    
    def initialize(self):
        """Initialize database connection and schema."""
        try:
            # Ensure directory exists
            self.duckdb_path.parent.mkdir(parents=True, exist_ok=True)
            
            # Connect to DuckDB
            self.duckdb_conn = duckdb.connect(str(self.duckdb_path))
            init_duckdb_schema(self.duckdb_conn)
            
            logger.info("✓ Database connection initialized successfully")
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
    
    def get_unaggregated_count(self) -> int:
        """Get count of unaggregated interaction sessions."""
        cursor = self.duckdb_conn.execute("""
            SELECT COUNT(*) FROM sessions
            WHERE work_session_id IS NULL
        """)
        return cursor.fetchone()[0]
    
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
                    
                    # Determine domain (should be consistent, but take first)
                    domain = work_session_sessions[0].get('domain', 'WORK')
                    
                    # Extract project name (if consistent across sessions)
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
                        all_entities[entity_type] = list(set(all_entities[entity_type]))[:20]  # Limit to 20 per type
                    
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
    
    def _generate_work_session_id(self, first_session: Dict) -> str:
        """Generate unique work session ID from first session."""
        timestamp = first_session['start_time']
        device_id = first_session['device_id']
        domain = first_session.get('domain', 'WORK')
        
        unique_str = f"{device_id}_{timestamp}_{domain}_work"
        return hashlib.md5(unique_str.encode()).hexdigest()
    
    def run_periodic(self):
        """Run periodic aggregation loop."""
        self.running = True
        last_aggregation_time = 0
        
        logger.info("=" * 60)
        logger.info("Work Session Aggregator")
        logger.info("=" * 60)
        logger.info(f"Device ID: {self.device_id}")
        logger.info(f"Aggregation interval: {self.aggregation_interval}s ({self.aggregation_interval/60:.1f} minutes)")
        logger.info(f"DuckDB: {self.duckdb_path}")
        logger.info("=" * 60)
        logger.info("")
        
        try:
            while self.running:
                current_time = time.time()
                
                # Check if it's time to run aggregation
                if current_time - last_aggregation_time >= self.aggregation_interval:
                    # Execute aggregation
                    result = self.run_aggregation()
                    
                    # Log statistics
                    if result['success']:
                        unaggregated = self.get_unaggregated_count()
                        logger.info(
                            f"Status: {unaggregated} unaggregated sessions remaining | "
                            f"Last run: {result['sessions_aggregated']} sessions aggregated into {result['work_sessions_created']} work sessions"
                        )
                    else:
                        logger.warning(f"Aggregation failed: {result.get('error', 'Unknown error')}")
                    
                    last_aggregation_time = current_time
                
                # Sleep for a short interval before checking again
                time.sleep(10)  # Check every 10 seconds
                
        except KeyboardInterrupt:
            logger.info("")
            logger.info("Received interrupt signal, shutting down...")
            self.stop()
        except Exception as e:
            logger.error(f"Aggregator error: {e}", exc_info=True)
            self.stop()
    
    def stop(self):
        """Stop the aggregator and close database connections."""
        self.running = False
        
        if self.duckdb_conn:
            self.duckdb_conn.close()
            logger.info("✓ DuckDB connection closed")
        
        logger.info("Aggregator stopped")


# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

def main():
    """Main entry point for the work session aggregator."""
    
    print("Starting Work Session Aggregator...")
    print(f"Current directory: {Path.cwd()}")
    
    aggregator = WorkSessionAggregator(
        device_id=DEVICE_ID,
        database_dir=DATABASE_DIR,
        aggregation_interval=AGGREGATION_INTERVAL
    )
    
    # Register signal handlers for graceful shutdown
    def signal_handler(sig, frame):
        logger.info("Received signal, shutting down gracefully...")
        aggregator.stop()
        sys.exit(0)
    
    # Windows only supports SIGINT
    if sys.platform == 'win32':
        signal.signal(signal.SIGINT, signal_handler)
    else:
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
    
    try:
        # Initialize connections
        aggregator.initialize()
        
        # Start periodic aggregation
        aggregator.run_periodic()
        
    except Exception as e:
        logger.error(f"Failed to start aggregator: {e}", exc_info=True)
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

