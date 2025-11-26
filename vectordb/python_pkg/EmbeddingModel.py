#!/usr/bin/env python3
"""
Embedding Model Wrapper
=======================

Wrapper for sentence-transformers model to generate embeddings for text.

Features:
- Lazy loading of model (loaded on first use)
- Batch encoding support
- Consistent interface for embedding generation

Usage:
    from vectordb import EmbeddingModel
    
    model = EmbeddingModel()
    embedding = model.encode("Hello world")
    embeddings = model.encode_batch(["text1", "text2"])
"""

import logging
from typing import List, Union
import numpy as np

# Try to import sentence-transformers
try:
    from sentence_transformers import SentenceTransformer
    EMBEDDING_AVAILABLE = True
except ImportError:
    EMBEDDING_AVAILABLE = False
    SentenceTransformer = None

# ============================================================================
# CONFIGURATION
# ============================================================================

# Default embedding model
DEFAULT_MODEL_NAME = "all-MiniLM-L6-v2"

# Logging configuration
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


# ============================================================================
# EMBEDDING MODEL
# ============================================================================

class EmbeddingModel:
    """
    Wrapper for sentence-transformers embedding model.
    
    Provides lazy loading and consistent interface for generating embeddings.
    """
    
    def __init__(self, model_name: str = DEFAULT_MODEL_NAME):
        """
        Initialize embedding model.
        
        Args:
            model_name: Name of the sentence-transformers model to use.
                       Default: "all-MiniLM-L6-v2"
        """
        if not EMBEDDING_AVAILABLE:
            raise ImportError(
                "sentence-transformers is not installed. "
                "Install it with: pip install sentence-transformers"
            )
        
        self.model_name = model_name
        self._model = None
        self._dimension = None
        logger.info(f"EmbeddingModel initialized (model: {model_name})")
        logger.info("Model will be loaded on first use")
    
    @property
    def model(self) -> SentenceTransformer:
        """Lazy load the embedding model."""
        if self._model is None:
            logger.info(f"Loading embedding model: {self.model_name}")
            self._model = SentenceTransformer(self.model_name)
            # Get dimension from first encoding
            test_embedding = self._model.encode("test", convert_to_numpy=True)
            self._dimension = len(test_embedding)
            logger.info(f"✓ Embedding model loaded (dimension: {self._dimension})")
        return self._model
    
    @property
    def dimension(self) -> int:
        """Get the dimension of embeddings produced by this model."""
        if self._dimension is None:
            # Trigger model loading
            _ = self.model
        return self._dimension
    
    def encode(self, text: str, normalize: bool = True) -> List[float]:
        """
        Encode a single text into an embedding vector.
        
        Args:
            text: Input text to encode
            normalize: Whether to normalize the embedding vector (L2 normalization)
        
        Returns:
            List of floats representing the embedding vector
        """
        embedding = self.model.encode(text, convert_to_numpy=True, normalize_embeddings=normalize)
        return embedding.tolist()
    
    def encode_batch(self, texts: List[str], normalize: bool = True) -> List[List[float]]:
        """
        Encode a batch of texts into embedding vectors.
        
        Args:
            texts: List of input texts to encode
            normalize: Whether to normalize the embedding vectors (L2 normalization)
        
        Returns:
            List of embedding vectors, each as a list of floats
        """
        if not texts:
            return []
        
        embeddings = self.model.encode(
            texts,
            convert_to_numpy=True,
            normalize_embeddings=normalize,
            show_progress_bar=False
        )
        return embeddings.tolist()
    
    def encode_batch_numpy(self, texts: List[str], normalize: bool = True) -> np.ndarray:
        """
        Encode a batch of texts into embedding vectors as numpy array.
        
        Args:
            texts: List of input texts to encode
            normalize: Whether to normalize the embedding vectors (L2 normalization)
        
        Returns:
            Numpy array of shape (batch_size, dimension)
        """
        if not texts:
            return np.array([])
        
        embeddings = self.model.encode(
            texts,
            convert_to_numpy=True,
            normalize_embeddings=normalize,
            show_progress_bar=False
        )
        return embeddings

