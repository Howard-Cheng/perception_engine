#!/usr/bin/env python3
"""
Vector Store
============

High-level interface for vector storage and retrieval using Qdrant and embeddings.

This module combines EmbeddingModel and QdrantClient to provide a convenient
interface for storing and querying text data as vectors.

Features:
- Automatic embedding generation from text
- Vector storage with metadata
- Semantic search with metadata filtering
- Batch operations
- Collection management

Usage:
    from vectordb import VectorStore
    
    store = VectorStore(collection_name="documents")
    store.store("doc1", "This is a document", metadata={"type": "article"})
    results = store.search("query text", limit=5)
    results = store.search_with_filter("query", filter_conditions={"type": "article"})
"""

from __future__ import annotations

import logging
import uuid
from typing import List, Dict, Optional, Any, Union
from pathlib import Path

from .EmbeddingModel import EmbeddingModel
from .QdrantClient import QdrantClient

# ============================================================================
# CONFIGURATION
# ============================================================================

# Logging configuration
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


# ============================================================================
# VECTOR STORE
# ============================================================================

class VectorStore:
    """
    High-level interface for vector storage and retrieval.
    
    Combines embedding model and Qdrant client to provide convenient
    text-to-vector storage and search operations.
    """
    
    def __init__(
        self,
        collection_name: str,
        embedding_model_name: str = "all-MiniLM-L6-v2",
        qdrant_url: str = "http://localhost:6333",
        qdrant_api_key: Optional[str] = None,
        qdrant_path: Optional[Union[str, Path]] = None,
        recreate_collection: bool = False
    ):
        """
        Initialize vector store.
        
        Args:
            collection_name: Name of the Qdrant collection to use
            embedding_model_name: Name of the sentence-transformers model
            qdrant_url: Qdrant server URL (ignored if qdrant_path is provided)
            qdrant_api_key: Optional API key for Qdrant
            qdrant_path: Optional local path for Qdrant local mode
            recreate_collection: If True, recreate collection if it exists
        """
        # Initialize embedding model
        self.embedding_model = EmbeddingModel(model_name=embedding_model_name)
        self.vector_dimension = self.embedding_model.dimension
        
        # Initialize Qdrant client
        self.qdrant_client = QdrantClient(
            url=qdrant_url,
            api_key=qdrant_api_key,
            path=qdrant_path
        )
        
        self.collection_name = collection_name
        
        # Create collection if it doesn't exist
        if not self.qdrant_client.collection_exists(collection_name):
            logger.info(f"Collection {collection_name} does not exist, creating...")
            self.qdrant_client.create_collection(
                collection_name=collection_name,
                vector_size=self.vector_dimension,
                recreate=recreate_collection
            )
        elif recreate_collection:
            logger.info(f"Recreating collection {collection_name}...")
            self.qdrant_client.create_collection(
                collection_name=collection_name,
                vector_size=self.vector_dimension,
                recreate=True
            )
        else:
            logger.info(f"Using existing collection: {collection_name}")
        
        logger.info(f"✓ VectorStore initialized (collection: {collection_name}, dimension: {self.vector_dimension})")
    
    # ========================================================================
    # ID CONVERSION HELPERS
    # ========================================================================
    
    @staticmethod
    def _convert_to_uuid(point_id: Union[str, int, uuid.UUID]) -> uuid.UUID:
        """
        Convert point ID to UUID format.
        
        Qdrant local mode requires UUID format. This method converts
        string or integer IDs to UUID using UUID5 (deterministic).
        
        Args:
            point_id: Point ID (string, int, or UUID)
        
        Returns:
            UUID object
        """
        if isinstance(point_id, uuid.UUID):
            return point_id
        
        # Use UUID5 with a fixed namespace to ensure deterministic conversion
        # Same string/int will always produce the same UUID
        namespace = uuid.UUID('6ba7b810-9dad-11d1-80b4-00c04fd430c8')  # Standard namespace
        
        if isinstance(point_id, int):
            id_str = str(point_id)
        else:
            id_str = str(point_id)
        
        return uuid.uuid5(namespace, id_str)
    
    # ========================================================================
    # STORAGE OPERATIONS
    # ========================================================================
    
    def store(
        self,
        point_id: Union[str, int],
        text: str,
        metadata: Optional[Dict[str, Any]] = None
    ) -> bool:
        """
        Store a single text with metadata as a vector.
        
        Args:
            point_id: Unique identifier for this point
            text: Text content to store
            metadata: Optional metadata dictionary
        
        Returns:
            True if stored successfully
        """
        # Generate embedding
        embedding = self.embedding_model.encode(text)
        
        # Convert ID to UUID string (required for Qdrant local mode)
        uuid_id = self._convert_to_uuid(point_id)
        uuid_str = str(uuid_id)
        
        # Prepare point
        from qdrant_client.models import PointStruct
        
        point = PointStruct(
            id=uuid_str,
            vector=embedding,
            payload=metadata or {}
        )
        
        # Store in Qdrant
        return self.qdrant_client.upsert(
            collection_name=self.collection_name,
            points=[point]
        )
    
    def store_batch(
        self,
        items: List[Dict[str, Any]]
    ) -> bool:
        """
        Store multiple texts with metadata as vectors in batch.
        
        Args:
            items: List of dictionaries, each containing:
                  - "id": point ID (required)
                  - "text": text content (required)
                  - "metadata": optional metadata dictionary
        
        Returns:
            True if all items stored successfully
        """
        if not items:
            return True
        
        # Extract texts for batch encoding
        texts = [item["text"] for item in items]
        
        # Generate embeddings in batch
        embeddings = self.embedding_model.encode_batch(texts)
        
        # Prepare points
        from qdrant_client.models import PointStruct
        
        points = []
        for i, item in enumerate(items):
            # Convert ID to UUID string (required for Qdrant local mode)
            uuid_id = self._convert_to_uuid(item["id"])
            uuid_str = str(uuid_id)
            
            point = PointStruct(
                id=uuid_str,
                vector=embeddings[i],
                payload=item.get("metadata", {})
            )
            points.append(point)
        
        # Store in Qdrant
        return self.qdrant_client.upsert(
            collection_name=self.collection_name,
            points=points
        )
    
    # ========================================================================
    # SEARCH OPERATIONS
    # ========================================================================
    
    def search(
        self,
        query_text: str,
        limit: int = 10,
        score_threshold: Optional[float] = None,
        with_metadata: bool = True
    ) -> List[Dict[str, Any]]:
        """
        Search for similar texts using semantic similarity.
        
        Args:
            query_text: Query text to search for
            limit: Maximum number of results to return
            score_threshold: Minimum similarity score threshold
            with_metadata: Whether to include metadata in results
        
        Returns:
            List of search results, each containing id, score, text (if stored), and metadata
        """
        # Generate query embedding
        query_embedding = self.embedding_model.encode(query_text)
        
        # Search in Qdrant
        results = self.qdrant_client.search(
            collection_name=self.collection_name,
            query_vector=query_embedding,
            limit=limit,
            score_threshold=score_threshold,
            with_payload=with_metadata,
            with_vectors=False
        )
        
        return results
    
    def search_with_filter(
        self,
        query_text: str,
        filter_conditions: Dict[str, Any],
        limit: int = 10,
        score_threshold: Optional[float] = None,
        with_metadata: bool = True,
        filter_type: str = "must"
    ) -> List[Dict[str, Any]]:
        """
        Search for similar texts with metadata filtering.
        
        Args:
            query_text: Query text to search for
            filter_conditions: Dictionary of metadata field conditions.
                              Supports:
                              - Exact match: {"field": "value"}
                              - Text match: {"field": {"text": "substring"}}
                              - Range: {"field": {"gt": 10, "lt": 20}}
            limit: Maximum number of results to return
            score_threshold: Minimum similarity score threshold
            with_metadata: Whether to include metadata in results
            filter_type: Type of filter combination ("must", "must_not", "should")
        
        Returns:
            List of search results matching the filter conditions
        """
        # Generate query embedding
        query_embedding = self.embedding_model.encode(query_text)
        
        # Build filter conditions
        field_conditions = []
        for key, value in filter_conditions.items():
            if isinstance(value, dict):
                # Handle special filter types
                if "text" in value:
                    # Text match
                    condition = QdrantClient.create_field_condition_text(key, value["text"])
                elif any(k in value for k in ["gt", "gte", "lt", "lte"]):
                    # Range match
                    condition = QdrantClient.create_field_condition_range(
                        key,
                        gt=value.get("gt"),
                        gte=value.get("gte"),
                        lt=value.get("lt"),
                        lte=value.get("lte")
                    )
                else:
                    # Default to exact match
                    condition = QdrantClient.create_field_condition_match(key, value)
            else:
                # Exact match
                condition = QdrantClient.create_field_condition_match(key, value)
            
            field_conditions.append(condition)
        
        # Create filter
        if filter_type == "must":
            filter_obj = QdrantClient.create_filter(must=field_conditions)
        elif filter_type == "must_not":
            filter_obj = QdrantClient.create_filter(must_not=field_conditions)
        elif filter_type == "should":
            filter_obj = QdrantClient.create_filter(should=field_conditions)
        else:
            raise ValueError(f"Invalid filter_type: {filter_type}. Must be 'must', 'must_not', or 'should'")
        
        # Search in Qdrant with filter
        results = self.qdrant_client.search(
            collection_name=self.collection_name,
            query_vector=query_embedding,
            limit=limit,
            score_threshold=score_threshold,
            filter=filter_obj,
            with_payload=with_metadata,
            with_vectors=False
        )
        
        return results
    
    def search_by_filter_only(
        self,
        filter_conditions: Dict[str, Any],
        limit: int = 100,
        with_metadata: bool = True,
        filter_type: str = "must"
    ) -> List[Dict[str, Any]]:
        """
        Search points by metadata filter only (no semantic similarity).
        
        Args:
            filter_conditions: Dictionary of metadata field conditions
            limit: Maximum number of results to return
            with_metadata: Whether to include metadata in results
            filter_type: Type of filter combination ("must", "must_not", "should")
        
        Returns:
            List of points matching the filter conditions
        """
        # Build filter conditions
        field_conditions = []
        for key, value in filter_conditions.items():
            if isinstance(value, dict):
                if "text" in value:
                    condition = QdrantClient.create_field_condition_text(key, value["text"])
                elif any(k in value for k in ["gt", "gte", "lt", "lte"]):
                    condition = QdrantClient.create_field_condition_range(
                        key,
                        gt=value.get("gt"),
                        gte=value.get("gte"),
                        lt=value.get("lt"),
                        lte=value.get("lte")
                    )
                else:
                    condition = QdrantClient.create_field_condition_match(key, value)
            else:
                condition = QdrantClient.create_field_condition_match(key, value)
            
            field_conditions.append(condition)
        
        # Create filter
        if filter_type == "must":
            filter_obj = QdrantClient.create_filter(must=field_conditions)
        elif filter_type == "must_not":
            filter_obj = QdrantClient.create_filter(must_not=field_conditions)
        elif filter_type == "should":
            filter_obj = QdrantClient.create_filter(should=field_conditions)
        else:
            raise ValueError(f"Invalid filter_type: {filter_type}")
        
        # Use scroll to get all points matching filter
        try:
            scroll_results = self.qdrant_client.client.scroll(
                collection_name=self.collection_name,
                scroll_filter=filter_obj,
                limit=limit,
                with_payload=with_metadata,
                with_vectors=False
            )
            
            results = []
            for point in scroll_results[0]:  # scroll returns (points, next_page_offset)
                result_dict = {
                    "id": point.id,
                    "score": None  # No similarity score for filter-only search
                }
                if with_metadata and point.payload:
                    result_dict["payload"] = point.payload
                results.append(result_dict)
            
            return results
        except Exception as e:
            logger.error(f"Failed to search by filter: {e}")
            raise
    
    # ========================================================================
    # DELETION OPERATIONS
    # ========================================================================
    
    def delete(self, point_ids: List[Union[str, int]]) -> bool:
        """
        Delete points by their IDs.
        
        Args:
            point_ids: List of point IDs to delete
        
        Returns:
            True if deletion succeeded
        """
        # Convert IDs to UUID string format
        uuid_ids = [str(self._convert_to_uuid(pid)) for pid in point_ids]
        
        return self.qdrant_client.delete_points(
            collection_name=self.collection_name,
            point_ids=uuid_ids
        )
    
    def delete_by_filter(
        self,
        filter_conditions: Dict[str, Any],
        filter_type: str = "must"
    ) -> bool:
        """
        Delete points by metadata filter.
        
        Args:
            filter_conditions: Dictionary of metadata field conditions
            filter_type: Type of filter combination ("must", "must_not", "should")
        
        Returns:
            True if deletion succeeded
        """
        # Build filter conditions (same logic as search_with_filter)
        field_conditions = []
        for key, value in filter_conditions.items():
            if isinstance(value, dict):
                if "text" in value:
                    condition = QdrantClient.create_field_condition_text(key, value["text"])
                elif any(k in value for k in ["gt", "gte", "lt", "lte"]):
                    condition = QdrantClient.create_field_condition_range(
                        key,
                        gt=value.get("gt"),
                        gte=value.get("gte"),
                        lt=value.get("lt"),
                        lte=value.get("lte")
                    )
                else:
                    condition = QdrantClient.create_field_condition_match(key, value)
            else:
                condition = QdrantClient.create_field_condition_match(key, value)
            
            field_conditions.append(condition)
        
        # Create filter
        if filter_type == "must":
            filter_obj = QdrantClient.create_filter(must=field_conditions)
        elif filter_type == "must_not":
            filter_obj = QdrantClient.create_filter(must_not=field_conditions)
        elif filter_type == "should":
            filter_obj = QdrantClient.create_filter(should=field_conditions)
        else:
            raise ValueError(f"Invalid filter_type: {filter_type}")
        
        return self.qdrant_client.delete_points_by_filter(
            collection_name=self.collection_name,
            filter=filter_obj
        )
    
    # ========================================================================
    # COLLECTION MANAGEMENT
    # ========================================================================
    
    def get_collection_info(self) -> Optional[Dict[str, Any]]:
        """Get information about the current collection."""
        return self.qdrant_client.get_collection_info(self.collection_name)
    
    def recreate_collection(self) -> bool:
        """Recreate the collection (deletes all data)."""
        return self.qdrant_client.create_collection(
            collection_name=self.collection_name,
            vector_size=self.vector_dimension,
            recreate=True
        )

