"""
LLM Summary Module
==================

Module for local LLM inference using llama.cpp.
Supports reading data from databases and using Phi-4 model for text summarization and Q&A.
"""

from .llm_client import LLMClient, MODEL_PATH

__all__ = ['LLMClient', 'MODEL_PATH']
