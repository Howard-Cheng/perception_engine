#!/usr/bin/env python3
"""
SQLite Data Collector for Perception Engine
============================================

This script polls the Perception Engine HTTP API and writes raw events
to a SQLite database (Layer 0 - raw events storage).

Features:
- Polls context API every 5 seconds
- Writes to SQLite with proper schema
- Handles errors gracefully
- Supports content deduplication via hash
- Uses English logging and output

Usage:
    python sqlite_data_collector.py
"""

import sqlite3
import requests
import time
import json
import hashlib
import logging
from datetime import datetime
from pathlib import Path
from typing import Dict, Optional

# ============================================================================
# CONFIGURATION
# ============================================================================

API_URL = "http://localhost:8777/context"
POLL_INTERVAL = 5  # seconds
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

def init_database(db_path: Path) -> sqlite3.Connection:
    """
    Initialize SQLite database with Layer 0 raw events schema.
    
    Args:
        db_path: Path to SQLite database file
        
    Returns:
        SQLite connection object
    """
    db_path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path)
    
    # Create raw_events table
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
    
    # Create indexes for performance
    conn.execute("CREATE INDEX IF NOT EXISTS idx_timestamp ON raw_events(timestamp)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_compressed ON raw_events(compressed)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_session ON raw_events(session_id)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_device ON raw_events(device_id)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_app_name ON raw_events(app_name)")
    
    conn.commit()
    logger.info(f"Database initialized: {db_path}")
    
    return conn


# ============================================================================
# DATA TRANSFORMATION
# ============================================================================

def transform_api_data(api_data: Dict, device_id: str) -> Dict:
    """
    Transform API response data into database event format.
    
    Args:
        api_data: Raw data from Perception Engine API
        device_id: Device identifier
        
    Returns:
        Dictionary ready for database insertion
    """
    # Extract mouse events (convert to JSON if exists)
    mouse_events = api_data.get('mouseEvents', [])
    if isinstance(mouse_events, list):
        mouse_events_json = json.dumps(mouse_events)
    else:
        mouse_events_json = json.dumps([])
    
    # Calculate interaction count from mouse events
    interaction_count = len(mouse_events) if mouse_events else 0
    
    # Extract screen content
    screen_content = api_data.get('activeAppContent', '')
    
    # Compute content hash for deduplication
    content_hash = None
    if screen_content:
        content_hash = hashlib.md5(screen_content.encode()).hexdigest()
    
    # Build event data structure
    event_data = {
        'timestamp': api_data.get('timestamp', datetime.now().isoformat()),
        'device_id': device_id,
        'app_name': api_data.get('activeApp', 'unknown.exe'),
        'window_title': api_data.get('activeWindowTitle'),
        'url': api_data.get('browserUrl'),
        'screen_content': screen_content,
        'screen_content_hash': content_hash,
        'mouse_events': mouse_events_json,
        'interaction_count': interaction_count,
        'dwell_time_seconds': POLL_INTERVAL,  # Time since last poll
        'voice_transcription': api_data.get('voiceTranscription'),
        'camera_description': api_data.get('cameraDescription'),
        'battery_percent': api_data.get('battery'),
        'is_charging': api_data.get('isCharging', False),
        'network_type': api_data.get('networkType'),
        'location_lat': api_data.get('locationLat'),
        'location_lon': api_data.get('locationLon'),
        'cpu_usage': api_data.get('cpuUsage'),
        'memory_usage': api_data.get('memoryUsage'),
    }
    
    return event_data


# ============================================================================
# DATA INGESTION
# ============================================================================

def generate_event_id(event_data: Dict) -> str:
    """
    Generate unique event ID from event data.
    
    Args:
        event_data: Event data dictionary
        
    Returns:
        MD5 hash as event ID
    """
    timestamp = event_data['timestamp']
    app_name = event_data['app_name']
    window_title = event_data.get('window_title', '')
    
    unique_str = f"{timestamp}_{app_name}_{window_title}"
    return hashlib.md5(unique_str.encode()).hexdigest()


def ingest_event(conn: sqlite3.Connection, event_data: Dict) -> str:
    """
    Insert a single event into the database.
    
    Args:
        conn: SQLite connection
        event_data: Event data dictionary
        
    Returns:
        Generated event ID
    """
    event_id = generate_event_id(event_data)
    
    conn.execute("""
        INSERT OR REPLACE INTO raw_events (
            event_id, timestamp, device_id, app_name, window_title, url,
            screen_content, screen_content_hash, mouse_events, interaction_count,
            dwell_time_seconds, voice_transcription, camera_description,
            battery_percent, is_charging, network_type, location_lat, location_lon,
            cpu_usage, memory_usage, compressed, created_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, FALSE, CURRENT_TIMESTAMP)
    """, [
        event_id,
        event_data['timestamp'],
        event_data['device_id'],
        event_data['app_name'],
        event_data.get('window_title'),
        event_data.get('url'),
        event_data.get('screen_content'),
        event_data.get('screen_content_hash'),
        event_data['mouse_events'],
        event_data['interaction_count'],
        event_data['dwell_time_seconds'],
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
    
    conn.commit()
    return event_id


# ============================================================================
# STATISTICS
# ============================================================================

def get_statistics(conn: sqlite3.Connection) -> Dict:
    """
    Get database statistics.
    
    Args:
        conn: SQLite connection
        
    Returns:
        Dictionary with statistics
    """
    cursor = conn.cursor()
    
    # Total events
    cursor.execute("SELECT COUNT(*) FROM raw_events")
    total_events = cursor.fetchone()[0]
    
    # Events today
    cursor.execute("""
        SELECT COUNT(*) FROM raw_events 
        WHERE DATE(timestamp) = DATE('now')
    """)
    events_today = cursor.fetchone()[0]
    
    # Compressed events
    cursor.execute("SELECT COUNT(*) FROM raw_events WHERE compressed = TRUE")
    compressed_events = cursor.fetchone()[0]
    
    # Most active app today
    cursor.execute("""
        SELECT app_name, COUNT(*) as count 
        FROM raw_events 
        WHERE DATE(timestamp) = DATE('now')
        GROUP BY app_name 
        ORDER BY count DESC 
        LIMIT 1
    """)
    result = cursor.fetchone()
    top_app = result[0] if result else "N/A"
    top_app_count = result[1] if result else 0
    
    return {
        'total_events': total_events,
        'events_today': events_today,
        'compressed_events': compressed_events,
        'top_app': top_app,
        'top_app_count': top_app_count
    }


# ============================================================================
# MAIN COLLECTION LOOP
# ============================================================================

def main():
    """Main data collection loop"""
    
    # Initialize database
    db_path = DATABASE_DIR / DEVICE_ID / "raw_events.db"
    conn = init_database(db_path)
    
    logger.info("=" * 60)
    logger.info("Perception Engine SQLite Data Collector")
    logger.info("=" * 60)
    logger.info(f"Device ID: {DEVICE_ID}")
    logger.info(f"Database: {db_path}")
    logger.info(f"API URL: {API_URL}")
    logger.info(f"Poll Interval: {POLL_INTERVAL}s")
    logger.info("=" * 60)
    
    # Check initial connection
    try:
        response = requests.get(API_URL, timeout=3)
        if response.status_code == 200:
            logger.info("✓ Successfully connected to Perception Engine API")
        else:
            logger.warning(f"⚠ API returned status code: {response.status_code}")
    except requests.exceptions.RequestException as e:
        logger.error(f"✗ Cannot connect to Perception Engine API: {e}")
        logger.error("  Make sure PerceptionEngine.exe is running on port 8777")
        return
    
    logger.info("")
    logger.info("Starting data collection... (Press Ctrl+C to stop)")
    logger.info("")
    
    # Statistics counters
    success_count = 0
    error_count = 0
    last_stats_time = time.time()
    
    try:
        while True:
            try:
                # Fetch data from API
                response = requests.get(API_URL, timeout=3)
                
                if response.status_code == 200:
                    api_data = response.json()
                    
                    # Transform to database format
                    event_data = transform_api_data(api_data, DEVICE_ID)
                    
                    # Insert into database
                    event_id = ingest_event(conn, event_data)
                    
                    success_count += 1
                    
                    # Log event
                    app_name = event_data['app_name']
                    cpu = event_data.get('cpu_usage', 0) or 0
                    mem = event_data.get('memory_usage', 0) or 0
                    
                    logger.info(
                        f"✓ Collected: {app_name:<25} | "
                        f"CPU: {cpu:>5.1f}% | "
                        f"Mem: {mem:>5.1f}% | "
                        f"Events: {success_count}"
                    )
                    
                else:
                    error_count += 1
                    logger.warning(f"⚠ API returned status {response.status_code}")
                
            except requests.exceptions.RequestException as e:
                error_count += 1
                logger.error(f"✗ Connection error: {e}")
                logger.info("  Retrying in 10 seconds...")
                time.sleep(10)
                continue
                
            except json.JSONDecodeError as e:
                error_count += 1
                logger.error(f"✗ JSON decode error: {e}")
                
            except Exception as e:
                error_count += 1
                logger.error(f"✗ Unexpected error: {e}", exc_info=True)
            
            # Print statistics every 5 minutes
            if time.time() - last_stats_time > 300:
                stats = get_statistics(conn)
                logger.info("")
                logger.info("--- Database Statistics ---")
                logger.info(f"Total events: {stats['total_events']}")
                logger.info(f"Events today: {stats['events_today']}")
                logger.info(f"Compressed: {stats['compressed_events']}")
                logger.info(f"Top app today: {stats['top_app']} ({stats['top_app_count']} events)")
                logger.info("---------------------------")
                logger.info("")
                last_stats_time = time.time()
            
            # Wait for next poll
            time.sleep(POLL_INTERVAL)
            
    except KeyboardInterrupt:
        logger.info("")
        logger.info("=" * 60)
        logger.info("Shutting down...")
        
        # Print final statistics
        stats = get_statistics(conn)
        logger.info("")
        logger.info("Final Statistics:")
        logger.info(f"  Total events collected: {success_count}")
        logger.info(f"  Errors encountered: {error_count}")
        logger.info(f"  Total in database: {stats['total_events']}")
        logger.info(f"  Events today: {stats['events_today']}")
        logger.info("")
        
        conn.close()
        logger.info("✓ Database closed")
        logger.info("=" * 60)


if __name__ == "__main__":
    main()