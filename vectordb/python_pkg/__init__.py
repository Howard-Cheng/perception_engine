#!/usr/bin/env python3
"""
Vector Database Module
======================

High-level interface for vector storage and retrieval using Qdrant and embeddings.

This module provides:
- EmbeddingModel: Wrapper for sentence-transformers models
- QdrantClient: Low-level Qdrant client wrapper
- VectorStore: High-level interface combining embeddings and Qdrant

Usage:
    from vectordb import VectorStore
    
    # Initialize vector store
    store = VectorStore(collection_name="documents")
    
    # Store text with metadata
    store.store("doc1", "This is a document", metadata={"type": "article", "author": "John"})
    
    # Search for similar texts
    results = store.search("query text", limit=5)
    
    # Search with metadata filter
    results = store.search_with_filter(
        "query text",
        filter_conditions={"type": "article"},
        limit=5
    )
    
    # Delete by ID
    store.delete(["doc1"])
    
    # Delete by filter
    store.delete_by_filter({"type": "article"})
"""

from .EmbeddingModel import EmbeddingModel, EMBEDDING_AVAILABLE
from .QdrantClient import QdrantClient, QDRANT_AVAILABLE
from .VectorStore import VectorStore

__all__ = [
    "EmbeddingModel",
    "QdrantClient",
    "VectorStore",
    "EMBEDDING_AVAILABLE",
    "QDRANT_AVAILABLE",
]

__version__ = "1.0.0"

