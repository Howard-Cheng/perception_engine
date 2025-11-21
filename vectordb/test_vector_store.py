#!/usr/bin/env python3
"""
Vector Store Test Script
=========================

Test script for vector database functionality using Qdrant and embeddings.

This script tests:
1. VectorStore initialization
2. Single and batch storage
3. Semantic search
4. Search with metadata filters
5. Filter-only search
6. Point deletion

Usage:
    python test_vector_store.py
"""

import logging
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from vectordb import VectorStore

# ============================================================================
# CONFIGURATION
# ============================================================================

# Test collection name
TEST_COLLECTION = "test_collection"

# Qdrant connection settings
# Option 1: Local file mode (no server needed) - RECOMMENDED
QDRANT_PATH = Path("./qdrant_storage")
QDRANT_URL = None  # Not used in local file mode

# Option 2: Remote server mode (requires Qdrant server running)
# Uncomment these and comment out QDRANT_PATH above:
# QDRANT_URL = "http://localhost:6333"
# QDRANT_PATH = None

# Logging configuration
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


# ============================================================================
# TEST FUNCTIONS
# ============================================================================

def test_initialization():
    """Test VectorStore initialization."""
    logger.info("=" * 60)
    logger.info("Test 1: VectorStore Initialization")
    logger.info("=" * 60)
    
    try:
        # Initialize with local file mode (no server needed)
        if QDRANT_PATH:
            store = VectorStore(
                collection_name=TEST_COLLECTION,
                qdrant_path=QDRANT_PATH,
                recreate_collection=True  # Clean start for testing
            )
        else:
            # Remote server mode
            store = VectorStore(
                collection_name=TEST_COLLECTION,
                qdrant_url=QDRANT_URL,
                recreate_collection=True  # Clean start for testing
            )
        logger.info("✓ VectorStore initialized successfully")
        
        # Get collection info
        info = store.get_collection_info()
        if info:
            logger.info(f"  Collection: {info['name']}")
            logger.info(f"  Vector size: {info['config']['vector_size']}")
            logger.info(f"  Distance: {info['config']['distance']}")
            logger.info(f"  Points count: {info['points_count']}")
        
        return store
    except Exception as e:
        logger.error(f"✗ Initialization failed: {e}", exc_info=True)
        raise


def test_single_storage(store: VectorStore):
    """Test storing single text with metadata."""
    logger.info("=" * 60)
    logger.info("Test 2: Single Text Storage")
    logger.info("=" * 60)
    
    try:
        # Store single text
        store.store(
            point_id="doc1",
            text="Machine learning is a branch of artificial intelligence that focuses on enabling computers to learn from data.",
            metadata={
                "type": "article",
                "category": "AI",
                "author": "John Smith",
                "date": "2024-01-15"
            }
        )
        logger.info("✓ Stored document 1")
        
        store.store(
            point_id="doc2",
            text="Python is a high-level programming language widely used in data science and web development.",
            metadata={
                "type": "tutorial",
                "category": "Programming",
                "author": "Jane Doe",
                "date": "2024-01-16"
            }
        )
        logger.info("✓ Stored document 2")
        
        store.store(
            point_id="doc3",
            text="Deep learning uses neural networks to simulate the learning process of the human brain.",
            metadata={
                "type": "article",
                "category": "AI",
                "author": "Bob Johnson",
                "date": "2024-01-17"
            }
        )
        logger.info("✓ Stored document 3")
        
        # Check collection info
        info = store.get_collection_info()
        logger.info(f"  Total points: {info['points_count']}")
        
    except Exception as e:
        logger.error(f"✗ Single storage failed: {e}", exc_info=True)
        raise


def test_batch_storage(store: VectorStore):
    """Test batch storage."""
    logger.info("=" * 60)
    logger.info("Test 3: Batch Storage")
    logger.info("=" * 60)
    
    try:
        items = [
            {
                "id": "doc4",
                "text": "Vector databases can efficiently store and retrieve high-dimensional vector data.",
                "metadata": {
                    "type": "article",
                    "category": "Database",
                    "author": "Alice Williams",
                    "date": "2024-01-18"
                }
            },
            {
                "id": "doc5",
                "text": "Natural language processing technology enables computers to understand and generate human language.",
                "metadata": {
                    "type": "article",
                    "category": "NLP",
                    "author": "Charlie Brown",
                    "date": "2024-01-19"
                }
            },
            {
                "id": "doc6",
                "text": "Web development includes frontend and backend technologies for building internet applications.",
                "metadata": {
                    "type": "tutorial",
                    "category": "Programming",
                    "author": "David Lee",
                    "date": "2024-01-20"
                }
            }
        ]
        
        store.store_batch(items)
        logger.info(f"✓ Stored {len(items)} documents in batch")
        
        # Check collection info
        info = store.get_collection_info()
        logger.info(f"  Total points: {info['points_count']}")
        
    except Exception as e:
        logger.error(f"✗ Batch storage failed: {e}", exc_info=True)
        raise


def test_semantic_search(store: VectorStore):
    """Test semantic search."""
    logger.info("=" * 60)
    logger.info("Test 4: Semantic Search")
    logger.info("=" * 60)
    
    try:
        # Search for AI-related content
        query = "artificial intelligence and machine learning"
        logger.info(f"Query: {query}")
        
        results = store.search(query, limit=3)
        logger.info(f"  Found {len(results)} results:")
        
        for i, result in enumerate(results, 1):
            logger.info(f"  {i}. ID: {result['id']}, Score: {result['score']:.4f}")
            if 'payload' in result:
                metadata = result['payload']
                logger.info(f"     Type: {metadata.get('type')}, Category: {metadata.get('category')}")
        
    except Exception as e:
        logger.error(f"✗ Semantic search failed: {e}", exc_info=True)
        raise


def test_search_with_filter(store: VectorStore):
    """Test search with metadata filters."""
    logger.info("=" * 60)
    logger.info("Test 5: Search with Metadata Filter")
    logger.info("=" * 60)
    
    try:
        # Search with exact match filter
        query = "programming"
        logger.info(f"Query: {query}")
        logger.info("Filter: type = 'tutorial'")
        
        results = store.search_with_filter(
            query,
            filter_conditions={"type": "tutorial"},
            limit=5
        )
        logger.info(f"  Found {len(results)} results:")
        
        for i, result in enumerate(results, 1):
            logger.info(f"  {i}. ID: {result['id']}, Score: {result['score']:.4f}")
            if 'payload' in result:
                metadata = result['payload']
                logger.info(f"     Type: {metadata.get('type')}, Category: {metadata.get('category')}")
        
        # Search with multiple filters
        logger.info("\nFilter: category = 'AI' AND type = 'article'")
        results = store.search_with_filter(
            "deep learning",
            filter_conditions={
                "category": "AI",
                "type": "article"
            },
            limit=5
        )
        logger.info(f"  Found {len(results)} results:")
        
        for i, result in enumerate(results, 1):
            logger.info(f"  {i}. ID: {result['id']}, Score: {result['score']:.4f}")
        
    except Exception as e:
        logger.error(f"✗ Search with filter failed: {e}", exc_info=True)
        raise


def test_filter_only_search(store: VectorStore):
    """Test filter-only search (no semantic similarity)."""
    logger.info("=" * 60)
    logger.info("Test 6: Filter-Only Search")
    logger.info("=" * 60)
    
    try:
        # Search by filter only
        logger.info("Filter: category = 'AI'")
        results = store.search_by_filter_only(
            filter_conditions={"category": "AI"},
            limit=10
        )
        logger.info(f"  Found {len(results)} results:")
        
        for i, result in enumerate(results, 1):
            logger.info(f"  {i}. ID: {result['id']}")
            if 'payload' in result:
                metadata = result['payload']
                logger.info(f"     Type: {metadata.get('type')}, Category: {metadata.get('category')}, Author: {metadata.get('author')}")
        
    except Exception as e:
        logger.error(f"✗ Filter-only search failed: {e}", exc_info=True)
        raise


def test_range_filter(store: VectorStore):
    """Test range filter (if metadata has numeric values)."""
    logger.info("=" * 60)
    logger.info("Test 7: Range Filter (Example)")
    logger.info("=" * 60)
    
    try:
        # Note: This is just an example of how to use range filters
        # In our test data, dates are strings, so this won't work directly
        # But we can demonstrate the syntax
        
        logger.info("Example: Range filter syntax")
        logger.info("  filter_conditions={'score': {'gte': 0.5, 'lt': 0.9}}")
        logger.info("  filter_conditions={'age': {'gt': 18, 'lte': 65}}")
        logger.info("  Supported operators: gt, gte, lt, lte")
        logger.info("✓ Range filter syntax documented")
        
    except Exception as e:
        logger.error(f"✗ Range filter test failed: {e}", exc_info=True)
        raise


def test_deletion(store: VectorStore):
    """Test point deletion."""
    logger.info("=" * 60)
    logger.info("Test 8: Point Deletion")
    logger.info("=" * 60)
    
    try:
        # Delete by ID
        logger.info("Deleting doc1 by ID...")
        store.delete(["doc1"])
        logger.info("✓ Deleted doc1")
        
        # Verify deletion
        info = store.get_collection_info()
        logger.info(f"  Points after deletion: {info['points_count']}")
        
        # Delete by filter
        logger.info("Deleting all documents with type='tutorial'...")
        store.delete_by_filter({"type": "tutorial"})
        logger.info("✓ Deleted documents by filter")
        
        # Verify deletion
        info = store.get_collection_info()
        logger.info(f"  Points after filter deletion: {info['points_count']}")
        
    except Exception as e:
        logger.error(f"✗ Deletion failed: {e}", exc_info=True)
        raise


def test_text_filter(store: VectorStore):
    """Test text match filter (substring search)."""
    logger.info("=" * 60)
    logger.info("Test 9: Text Match Filter")
    logger.info("=" * 60)
    
    try:
        # Search with text match filter (substring search in metadata)
        logger.info("Filter: author contains 'John'")
        results = store.search_by_filter_only(
            filter_conditions={"author": {"text": "John"}},
            limit=10
        )
        logger.info(f"  Found {len(results)} results:")
        
        for i, result in enumerate(results, 1):
            logger.info(f"  {i}. ID: {result['id']}")
            if 'payload' in result:
                metadata = result['payload']
                logger.info(f"     Author: {metadata.get('author')}")
        
    except Exception as e:
        logger.error(f"✗ Text filter test failed: {e}", exc_info=True)
        raise


# ============================================================================
# MAIN
# ============================================================================

def main():
    """Run all tests."""
    logger.info("Starting Vector Store Tests")
    logger.info("=" * 60)
    
    try:
        # Initialize
        store = test_initialization()
        
        # Storage tests
        test_single_storage(store)
        test_batch_storage(store)
        
        # Search tests
        test_semantic_search(store)
        test_search_with_filter(store)
        test_filter_only_search(store)
        test_range_filter(store)
        test_text_filter(store)
        
        # Deletion tests
        test_deletion(store)
        
        logger.info("=" * 60)
        logger.info("✓ All tests completed successfully!")
        logger.info("=" * 60)
        
    except Exception as e:
        logger.error("=" * 60)
        logger.error(f"✗ Tests failed: {e}", exc_info=True)
        logger.error("=" * 60)
        sys.exit(1)


if __name__ == "__main__":
    main()

