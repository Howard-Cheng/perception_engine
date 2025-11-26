#!/usr/bin/env python3
"""
Qdrant Client Wrapper
=====================

Wrapper for Qdrant vector database client operations.

Features:
- Collection management (create, delete, list, exists)
- Vector storage (single and batch)
- Vector search with metadata filtering
- Point deletion
- Local Qdrant instance support

Usage:
    from vectordb import QdrantClient
    
    client = QdrantClient(url="http://localhost:6333")
    client.create_collection("my_collection", vector_size=384)
    client.upsert("my_collection", points=[...])
    results = client.search("my_collection", query_vector=[...], limit=10)
"""

from __future__ import annotations

import logging
from typing import List, Dict, Optional, Any, Union
from pathlib import Path

# Try to import qdrant-client
try:
    from qdrant_client import QdrantClient as QdrantClientBase
    from qdrant_client.models import (
        Distance,
        VectorParams,
        PointStruct,
        Filter,
        FieldCondition,
        MatchValue,
        MatchText,
        Range,
        CollectionStatus,
    )
    QDRANT_AVAILABLE = True
except ImportError:
    QDRANT_AVAILABLE = False
    QdrantClientBase = None
    # Create placeholders for type hints
    from enum import Enum
    class Distance(Enum):
        COSINE = "Cosine"
        EUCLID = "Euclid"
        DOT = "Dot"
    
    # Placeholder classes for Filter and FieldCondition
    Filter = None
    FieldCondition = None
    MatchValue = None
    MatchText = None
    Range = None
    VectorParams = None
    PointStruct = None
    CollectionStatus = None

# ============================================================================
# CONFIGURATION
# ============================================================================

# Default Qdrant connection settings
DEFAULT_URL = "http://localhost:6333"
DEFAULT_TIMEOUT = 30.0

# Logging configuration
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


# ============================================================================
# QDRANT CLIENT
# ============================================================================

class QdrantClient:
    """
    Wrapper for Qdrant vector database client.
    
    Provides high-level interface for Qdrant operations including collection
    management, vector storage, search, and deletion.
    """
    
    def __init__(
        self,
        url: str = DEFAULT_URL,
        api_key: Optional[str] = None,
        timeout: float = DEFAULT_TIMEOUT,
        path: Optional[Union[str, Path]] = None
    ):
        """
        Initialize Qdrant client.
        
        Args:
            url: Qdrant server URL (e.g., "http://localhost:6333")
                If path is provided, url is ignored and local mode is used.
            api_key: Optional API key for authentication
            timeout: Request timeout in seconds
            path: Optional local path for Qdrant local mode (file-based storage)
        """
        if not QDRANT_AVAILABLE:
            raise ImportError(
                "qdrant-client is not installed. "
                "Install it with: pip install qdrant-client"
            )
        
        if path is not None:
            # Local mode (file-based)
            self.client = QdrantClientBase(path=str(path))
            logger.info(f"QdrantClient initialized (local mode: {path})")
        else:
            # Remote mode
            self.client = QdrantClientBase(
                url=url,
                api_key=api_key,
                timeout=timeout
            )
            logger.info(f"QdrantClient initialized (url: {url})")
        
        # Test connection
        try:
            collections = self.client.get_collections()
            logger.info(f"✓ Connected to Qdrant (collections: {len(collections.collections)})")
        except Exception as e:
            logger.warning(f"Could not verify Qdrant connection: {e}")
    
    # ========================================================================
    # COLLECTION MANAGEMENT
    # ========================================================================
    
    def create_collection(
        self,
        collection_name: str,
        vector_size: int,
        distance: Distance = Distance.COSINE,
        recreate: bool = False
    ) -> bool:
        """
        Create a new collection.
        
        Args:
            collection_name: Name of the collection
            vector_size: Dimension of vectors in this collection
            distance: Distance metric (COSINE, EUCLID, DOT)
            recreate: If True, delete existing collection before creating
        
        Returns:
            True if collection was created successfully
        """
        try:
            if recreate and self.collection_exists(collection_name):
                logger.info(f"Deleting existing collection: {collection_name}")
                self.delete_collection(collection_name)
            
            self.client.create_collection(
                collection_name=collection_name,
                vectors_config=VectorParams(
                    size=vector_size,
                    distance=distance
                )
            )
            logger.info(f"✓ Collection created: {collection_name} (vector_size: {vector_size})")
            return True
        except Exception as e:
            logger.error(f"Failed to create collection {collection_name}: {e}")
            raise
    
    def delete_collection(self, collection_name: str) -> bool:
        """
        Delete a collection.
        
        Args:
            collection_name: Name of the collection to delete
        
        Returns:
            True if collection was deleted successfully
        """
        try:
            self.client.delete_collection(collection_name)
            logger.info(f"✓ Collection deleted: {collection_name}")
            return True
        except Exception as e:
            logger.error(f"Failed to delete collection {collection_name}: {e}")
            raise
    
    def collection_exists(self, collection_name: str) -> bool:
        """
        Check if a collection exists.
        
        Args:
            collection_name: Name of the collection to check
        
        Returns:
            True if collection exists
        """
        try:
            collections = self.client.get_collections()
            return any(c.name == collection_name for c in collections.collections)
        except Exception as e:
            logger.error(f"Failed to check collection existence: {e}")
            return False
    
    def list_collections(self) -> List[str]:
        """
        List all collection names.
        
        Returns:
            List of collection names
        """
        try:
            collections = self.client.get_collections()
            return [c.name for c in collections.collections]
        except Exception as e:
            logger.error(f"Failed to list collections: {e}")
            return []
    
    def get_collection_info(self, collection_name: str) -> Optional[Dict[str, Any]]:
        """
        Get information about a collection.
        
        Args:
            collection_name: Name of the collection
        
        Returns:
            Dictionary with collection information, or None if not found
        """
        try:
            collection_info = self.client.get_collection(collection_name)
            return {
                "name": collection_name,
                "points_count": collection_info.points_count,
                "vectors_count": collection_info.vectors_count,
                "status": collection_info.status,
                "config": {
                    "vector_size": collection_info.config.params.vectors.size,
                    "distance": collection_info.config.params.vectors.distance
                }
            }
        except Exception as e:
            logger.error(f"Failed to get collection info: {e}")
            return None
    
    # ========================================================================
    # VECTOR OPERATIONS
    # ========================================================================
    
    def upsert(
        self,
        collection_name: str,
        points: List[PointStruct]
    ) -> bool:
        """
        Insert or update points in a collection.
        
        Args:
            collection_name: Name of the collection
            points: List of PointStruct objects to upsert
        
        Returns:
            True if operation succeeded
        """
        try:
            self.client.upsert(
                collection_name=collection_name,
                points=points
            )
            logger.debug(f"Upserted {len(points)} points to {collection_name}")
            return True
        except Exception as e:
            logger.error(f"Failed to upsert points: {e}")
            raise
    
    def search(
        self,
        collection_name: str,
        query_vector: List[float],
        limit: int = 10,
        score_threshold: Optional[float] = None,
        filter: Optional[Filter] = None,
        with_payload: bool = True,
        with_vectors: bool = False
    ) -> List[Dict[str, Any]]:
        """
        Search for similar vectors in a collection.
        
        Args:
            collection_name: Name of the collection
            query_vector: Query vector to search for
            limit: Maximum number of results to return
            score_threshold: Minimum similarity score threshold
            filter: Optional metadata filter
            with_payload: Whether to include payload (metadata) in results
            with_vectors: Whether to include vectors in results
        
        Returns:
            List of search results, each containing id, score, and optionally payload/vector
        """
        try:
            search_results = self.client.search(
                collection_name=collection_name,
                query_vector=query_vector,
                limit=limit,
                score_threshold=score_threshold,
                query_filter=filter,
                with_payload=with_payload,
                with_vectors=with_vectors
            )
            
            results = []
            for result in search_results:
                result_dict = {
                    "id": result.id,
                    "score": result.score
                }
                if with_payload and result.payload:
                    result_dict["payload"] = result.payload
                if with_vectors and result.vector:
                    result_dict["vector"] = result.vector
                results.append(result_dict)
            
            return results
        except Exception as e:
            logger.error(f"Failed to search collection {collection_name}: {e}")
            raise
    
    def delete_points(
        self,
        collection_name: str,
        point_ids: List[Union[str, int]]
    ) -> bool:
        """
        Delete points from a collection by IDs.
        
        Args:
            collection_name: Name of the collection
            point_ids: List of point IDs to delete
        
        Returns:
            True if operation succeeded
        """
        try:
            self.client.delete(
                collection_name=collection_name,
                points_selector=point_ids
            )
            logger.debug(f"Deleted {len(point_ids)} points from {collection_name}")
            return True
        except Exception as e:
            logger.error(f"Failed to delete points: {e}")
            raise
    
    def delete_points_by_filter(
        self,
        collection_name: str,
        filter: Filter
    ) -> bool:
        """
        Delete points from a collection by metadata filter.
        
        Args:
            collection_name: Name of the collection
            filter: Metadata filter to match points for deletion
        
        Returns:
            True if operation succeeded
        """
        try:
            from qdrant_client.models import PointsSelector, FilterSelector
            
            self.client.delete(
                collection_name=collection_name,
                points_selector=FilterSelector(filter=filter)
            )
            logger.debug(f"Deleted points matching filter from {collection_name}")
            return True
        except Exception as e:
            logger.error(f"Failed to delete points by filter: {e}")
            raise
    
    # ========================================================================
    # FILTER HELPERS
    # ========================================================================
    
    @staticmethod
    def create_filter(
        must: Optional[List[FieldCondition]] = None,
        must_not: Optional[List[FieldCondition]] = None,
        should: Optional[List[FieldCondition]] = None
    ) -> Filter:
        """
        Create a metadata filter for queries.
        
        Args:
            must: Conditions that must be satisfied (AND)
            must_not: Conditions that must not be satisfied (NOT)
            should: Conditions where at least one must be satisfied (OR)
        
        Returns:
            Filter object
        """
        from qdrant_client.models import Filter
        
        conditions = {}
        if must:
            conditions["must"] = must
        if must_not:
            conditions["must_not"] = must_not
        if should:
            conditions["should"] = should
        
        return Filter(**conditions)
    
    @staticmethod
    def create_field_condition_match(key: str, value: Any) -> FieldCondition:
        """
        Create a field condition for exact match.
        
        Args:
            key: Metadata field name
            value: Value to match
        
        Returns:
            FieldCondition for exact match
        """
        return FieldCondition(
            key=key,
            match=MatchValue(value=value)
        )
    
    @staticmethod
    def create_field_condition_text(key: str, text: str) -> FieldCondition:
        """
        Create a field condition for text match (substring search).
        
        Args:
            key: Metadata field name
            text: Text to search for
        
        Returns:
            FieldCondition for text match
        """
        return FieldCondition(
            key=key,
            match=MatchText(text=text)
        )
    
    @staticmethod
    def create_field_condition_range(
        key: str,
        gt: Optional[float] = None,
        gte: Optional[float] = None,
        lt: Optional[float] = None,
        lte: Optional[float] = None
    ) -> FieldCondition:
        """
        Create a field condition for range matching.
        
        Args:
            key: Metadata field name
            gt: Greater than
            gte: Greater than or equal
            lt: Less than
            lte: Less than or equal
        
        Returns:
            FieldCondition for range match
        """
        return FieldCondition(
            key=key,
            range=Range(gt=gt, gte=gte, lt=lt, lte=lte)
        )

