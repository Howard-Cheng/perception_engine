#!/usr/bin/env python3
"""
Tokenize Text Script
====================

Tokenize a single text using the specified tokenizer and save to binary format.

Usage:
    python tokenize_text.py <input_file> <output_file> <tokenizer_path>

Example:
    python tokenize_text.py text.txt tokens.bin "D:\\path\\to\\tokenizer"

Output format (binary):
    - int64: num_tokens
    - int64[num_tokens]: input_ids
    - int64[num_tokens]: attention_mask
    - int64[num_tokens]: token_type_ids
"""

import sys
import struct
import os
import numpy as np
from transformers import AutoTokenizer


def tokenize_text(text, tokenizer, max_length=512):
    """
    Tokenize text into token IDs
    
    Args:
        text: Input text
        tokenizer: HuggingFace tokenizer
        max_length: Maximum sequence length (default: 512)
        
    Returns:
        Dictionary with 'input_ids', 'attention_mask', 'token_type_ids'
    """
    # Tokenize
    encoding = tokenizer(
        text,
        max_length=max_length,
        truncation=True,
        padding='max_length',
        return_tensors='np'
    )
    
    input_ids = encoding['input_ids'][0]  # Shape: (max_length,)
    attention_mask = encoding['attention_mask'][0]
    
    # Token type IDs (all zeros for single sequence)
    token_type_ids = np.zeros(max_length, dtype=np.int64)
    
    # Count actual tokens (non-padding)
    num_tokens = int(np.sum(attention_mask))
    
    return {
        'input_ids': input_ids,
        'attention_mask': attention_mask,
        'token_type_ids': token_type_ids,
        'num_tokens': num_tokens
    }


def save_tokens_binary(tokens, output_file):
    """
    Save tokens to binary file for C++ to read
    
    Format:
        - int64: num_tokens (actual non-padding tokens)
        - int64[max_length]: input_ids
        - int64[max_length]: attention_mask
        - int64[max_length]: token_type_ids
    """
    with open(output_file, 'wb') as f:
        # Write number of actual tokens
        f.write(struct.pack('<Q', tokens['num_tokens']))
        
        # Write arrays
        f.write(tokens['input_ids'].tobytes())
        f.write(tokens['attention_mask'].tobytes())
        f.write(tokens['token_type_ids'].tobytes())
    
    return tokens['num_tokens']


def main():
    if len(sys.argv) < 4:
        print("Usage: python tokenize_text.py <input_file> <output_file> <tokenizer_path>")
        print("\nExample:")
        print("  python tokenize_text.py text.txt tokens.bin \"D:\\path\\to\\tokenizer\"")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    tokenizer_path = sys.argv[3]
    
    print(f"Tokenizing text from: {input_file}")
    
    # Normalize tokenizer path
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
    
    # Load tokenizer
    try:
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
    
    # Read text
    print("Reading text...")
    with open(input_file, 'r', encoding='utf-8') as f:
        text = f.read().strip()
    
    if not text:
        print("ERROR: Empty text file")
        sys.exit(1)
    
    print(f"Text length: {len(text)} characters")
    
    # Tokenize
    print("Tokenizing...")
    tokens = tokenize_text(text, tokenizer)
    
    print(f"Generated {tokens['num_tokens']} tokens")
    
    # Save to binary
    print(f"Saving to {output_file}...")
    num_tokens = save_tokens_binary(tokens, output_file)
    
    file_size = 8 + (512 * 3 * 8)  # Header + 3 arrays of 512 int64s
    print(f"Saved {num_tokens} tokens ({file_size} bytes)")
    print("Done!")


if __name__ == '__main__':
    main()
