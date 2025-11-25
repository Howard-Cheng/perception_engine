#!/usr/bin/env python3
"""
Chunk a document into overlapping segments for embedding comparison

Usage:
    python chunk_document.py <input_file> <output_file> [chunk_size] [overlap] [tokenizer_path]

Example:
    python chunk_document.py doc_A.txt chunks_A.bin 450 50 "D:\\path\\to\\tokenizer"
"""

import sys
import struct
import json
import os
import numpy as np
from transformers import AutoTokenizer

def extract_content_fields(obj, path=""):
    """
    Extract ONLY screen_content and window_title from Elasticsearch documents.

    This is specifically for perception_context documents where we only want
    to compare the actual screen content and window title, ignoring all other
    fields including metadata, events, and system info.

    Fields extracted:
    - screen_content (the main content to compare)
    - window_title (for context)

    Everything else is ignored.
    """
    content_texts = []

    if isinstance(obj, dict):
        for key, value in obj.items():
            # ONLY extract these two specific fields
            if key == 'screen_content' or key == 'window_title':
                if isinstance(value, str) and value.strip():
                    content_texts.append(value.strip())
            # Recurse into _source wrapper (Elasticsearch structure)
            elif key == '_source':
                content_texts.extend(extract_content_fields(value, path))
            # Ignore everything else (no recursion)

    elif isinstance(obj, list):
        # Only recurse if it's a list at top level
        for item in obj:
            content_texts.extend(extract_content_fields(item, path))

    # No string extraction from unknown fields - we're being very strict

    return content_texts

def extract_content_from_input(text):
    """
    Extract text content from input.
    If input is JSON, extract only content fields (not metadata).
    If input is plain text, return as-is.
    """
    text = text.strip()

    # Try to parse as JSON
    try:
        json_obj = json.loads(text)
        content_parts = extract_content_fields(json_obj)

        # Remove duplicates while preserving order
        seen = set()
        unique_content = []
        for part in content_parts:
            if part not in seen:
                seen.add(part)
                unique_content.append(part)

        content = ' '.join(unique_content)

        if content.strip():
            return content
        else:
            # Empty JSON, return original
            return text
    except (json.JSONDecodeError, ValueError):
        # Not JSON, return as-is
        return text

def chunk_document(text, tokenizer, chunk_size=450, overlap=50):
    """
    Chunk a document with sliding window

    Args:
        text: Input text (can be very long)
        tokenizer: HuggingFace tokenizer
        chunk_size: Size of each chunk in tokens
        overlap: Overlap between chunks in tokens

    Returns:
        List of tokenized chunks
    """
    # Tokenize full document first
    full_encoding = tokenizer(
        text,
        max_length=None,  # No truncation
        truncation=False,
        return_tensors='np'
    )

    input_ids = full_encoding['input_ids'][0]
    attention_mask = full_encoding['attention_mask'][0]

    total_tokens = len(input_ids)
    stride = chunk_size - overlap

    chunks = []

    # Sliding window chunking
    for start in range(0, total_tokens, stride):
        end = min(start + chunk_size, total_tokens)

        # Extract chunk
        chunk_ids = input_ids[start:end]
        chunk_mask = attention_mask[start:end]

        # Pad to 512 if needed
        pad_length = 512 - len(chunk_ids)
        if pad_length > 0:
            chunk_ids = np.concatenate([chunk_ids, np.zeros(pad_length, dtype=np.int64)])
            chunk_mask = np.concatenate([chunk_mask, np.zeros(pad_length, dtype=np.int64)])
        else:
            # Truncate if somehow larger
            chunk_ids = chunk_ids[:512]
            chunk_mask = chunk_mask[:512]

        chunk_type_ids = np.zeros(512, dtype=np.int64)

        chunks.append({
            'input_ids': chunk_ids,
            'attention_mask': chunk_mask,
            'token_type_ids': chunk_type_ids,
            'length': min(end - start, 512)
        })

        # If we've covered the entire document, break
        if end >= total_tokens:
            break

    return chunks

def save_chunks_binary(chunks, output_file):
    """
    Save chunks to binary file for C++ to read

    Format:
        - int64: num_chunks
        - int64: seq_length (always 512)
        - For each chunk:
            - int64[512]: input_ids
            - int64[512]: attention_mask
            - int64[512]: token_type_ids
    """
    num_chunks = len(chunks)
    seq_length = 512

    with open(output_file, 'wb') as f:
        # Write header
        f.write(struct.pack('<QQ', num_chunks, seq_length))

        # Write all input_ids
        for chunk in chunks:
            f.write(chunk['input_ids'].tobytes())

        # Write all attention_mask
        for chunk in chunks:
            f.write(chunk['attention_mask'].tobytes())

        # Write all token_type_ids
        for chunk in chunks:
            f.write(chunk['token_type_ids'].tobytes())

    return num_chunks

def main():
    if len(sys.argv) < 3:
        print("Usage: python chunk_document.py <input_file> <output_file> [chunk_size] [overlap] [tokenizer_path]")
        print("\nExample:")
        print("  python chunk_document.py doc_A.txt chunks_A.bin 450 50 \"D:\\path\\to\\tokenizer\"")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    chunk_size = int(sys.argv[3]) if len(sys.argv) > 3 else 450
    overlap = int(sys.argv[4]) if len(sys.argv) > 4 else 50
    
    # Get tokenizer path from command line or use default
    if len(sys.argv) > 5:
        tokenizer_path = sys.argv[5]
    else:
        # Fallback to old behavior for backward compatibility
        script_dir = os.path.dirname(os.path.abspath(__file__))
        tokenizer_path = os.path.join(script_dir, 'tokenizer')

    print(f"Chunking document: {input_file}")
    print(f"Chunk size: {chunk_size}, Overlap: {overlap}")

    # Normalize and convert tokenizer path to absolute path
    tokenizer_path = os.path.abspath(os.path.normpath(tokenizer_path))
    print(f"Loading tokenizer from: {tokenizer_path}")
    
    # Check if tokenizer directory exists
    if not os.path.exists(tokenizer_path):
        print(f"ERROR: Tokenizer path does not exist: {tokenizer_path}")
        sys.exit(1)
    
    if not os.path.isdir(tokenizer_path):
        print(f"ERROR: Tokenizer path is not a directory: {tokenizer_path}")
        sys.exit(1)
    
    # Check for required tokenizer files
    required_files = ['tokenizer_config.json']
    missing_files = [f for f in required_files if not os.path.exists(os.path.join(tokenizer_path, f))]
    if missing_files:
        print(f"ERROR: Missing required tokenizer files: {missing_files}")
        print(f"Directory contents: {os.listdir(tokenizer_path)}")
        sys.exit(1)
    
    # Load tokenizer - use absolute path with forward slashes for compatibility
    try:
        # Replace backslashes with forward slashes for better cross-platform compatibility
        tokenizer_path_normalized = tokenizer_path.replace('\\', '/')
        tokenizer = AutoTokenizer.from_pretrained(
            tokenizer_path_normalized, 
            trust_remote_code=True, 
            local_files_only=True
        )
        print(f"Tokenizer loaded successfully!")
    except Exception as e:
        print(f"ERROR: Failed to load tokenizer: {e}")
        print(f"Tried path: {tokenizer_path_normalized}")
        sys.exit(1)

    # Read document
    print("Reading document...")
    with open(input_file, 'r', encoding='utf-8') as f:
        raw_text = f.read()

    # Extract content (handles JSON automatically)
    print("Extracting content...")
    text = extract_content_from_input(raw_text)

    if text != raw_text:
        print(f"  [JSON] Detected JSON input, extracted text content only")
        print(f"  Original: {len(raw_text)} chars")
        print(f"  Extracted: {len(text)} chars")

    # NOTE: embeddinggemma does NOT use query/passage prefixes
    # (E5 models used 'query:' and 'passage:' prefixes, but embeddinggemma doesn't)

    print(f"Document length: {len(text)} characters")

    # Chunk document
    print("Chunking...")
    chunks = chunk_document(text, tokenizer, chunk_size, overlap)

    print(f"Created {len(chunks)} chunks")

    # Save to binary
    print(f"Saving to {output_file}...")
    num_chunks = save_chunks_binary(chunks, output_file)

    file_size = 16 + (num_chunks * 512 * 3 * 8)  # Header + data
    print(f"Saved {num_chunks} chunks ({file_size} bytes)")
    print("Done!")

if __name__ == '__main__':
    main()
