"""
Download required models for Windows perception engine.

This script downloads:
1. PaddleOCR models (detection + recognition)
2. FastVLM-0.5B ONNX model
3. Whisper tiny.en model

Total download size: ~500MB
"""

import os
import urllib.request
from pathlib import Path
import zipfile
import tarfile

def download_file(url, destination, description):
    """Download a file with progress indication."""
    print(f"\n📥 Downloading {description}...")
    print(f"   URL: {url}")
    print(f"   Destination: {destination}")

    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)

    def show_progress(block_num, block_size, total_size):
        downloaded = block_num * block_size
        if total_size > 0:
            percent = min(downloaded * 100 / total_size, 100)
            print(f"\r   Progress: {percent:.1f}% ({downloaded / 1024 / 1024:.1f} MB / {total_size / 1024 / 1024:.1f} MB)", end='')

    try:
        urllib.request.urlretrieve(url, destination, show_progress)
        print(f"\n   ✅ Downloaded successfully!")
        return True
    except Exception as e:
        print(f"\n   ❌ Error: {e}")
        return False

def create_directory_structure():
    """Create models directory structure."""
    dirs = [
        "models/whisper",
        "models/vision",
        "models/ocr"
    ]

    for dir_path in dirs:
        Path(dir_path).mkdir(parents=True, exist_ok=True)
        print(f"✅ Created directory: {dir_path}")

def download_paddleocr():
    """Download PaddleOCR ONNX models."""
    print("\n" + "="*60)
    print("📦 DOWNLOADING PADDLEOCR MODELS")
    print("="*60)

    base_url = "https://huggingface.co/marsena/paddleocr-onnx-models/resolve/main"

    models = [
        {
            "url": f"{base_url}/PP-OCRv5_server_det_infer.onnx",
            "dest": "models/ocr/PP-OCRv5_server_det_infer.onnx",
            "desc": "PaddleOCR Detection Model (88 MB)"
        },
        {
            "url": f"{base_url}/PP-OCRv5_server_rec_infer.onnx",
            "dest": "models/ocr/PP-OCRv5_server_rec_infer.onnx",
            "desc": "PaddleOCR Recognition Model (84 MB)"
        }
    ]

    for model in models:
        download_file(model["url"], model["dest"], model["desc"])

def download_fastvlm():
    """Download FastVLM ONNX model."""
    print("\n" + "="*60)
    print("📦 DOWNLOADING FASTVLM MODEL")
    print("="*60)

    print("\n⚠️  FastVLM ONNX models need to be downloaded manually from:")
    print("   https://huggingface.co/onnx-community/FastVLM-0.5B-ONNX")
    print("\n   Download the following files:")
    print("   - model_fp16.onnx (or model.onnx)")
    print("   - Any required config files")
    print("\n   Place them in: models/vision/")
    print("\n   Note: You may need to quantize to INT8 yourself using ONNX tools")
    print("         or use FP16 for now (slower but easier)")

def download_whisper():
    """Download Whisper model."""
    print("\n" + "="*60)
    print("📦 DOWNLOADING WHISPER MODEL")
    print("="*60)

    url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en-q8_0.bin"
    dest = "models/whisper/ggml-tiny.en-q8_0.bin"

    download_file(url, dest, "Whisper tiny.en Quantized Model (40 MB)")

def verify_downloads():
    """Verify all required files are present."""
    print("\n" + "="*60)
    print("🔍 VERIFYING DOWNLOADS")
    print("="*60)

    required_files = [
        "models/ocr/PP-OCRv5_server_det_infer.onnx",
        "models/ocr/PP-OCRv5_server_rec_infer.onnx",
        "models/whisper/ggml-tiny.en-q8_0.bin",
    ]

    all_present = True
    for file_path in required_files:
        exists = Path(file_path).exists()
        status = "✅" if exists else "❌"
        size = ""
        if exists:
            size_mb = Path(file_path).stat().st_size / 1024 / 1024
            size = f"({size_mb:.1f} MB)"
        print(f"{status} {file_path} {size}")
        all_present = all_present and exists

    print("\n⚠️  Manual download required:")
    print("   - FastVLM ONNX model (see instructions above)")

    if all_present:
        print("\n✅ All automatic downloads completed!")
    else:
        print("\n❌ Some files are missing. Please check errors above.")

    return all_present

def main():
    print("="*60)
    print("🚀 WINDOWS PERCEPTION ENGINE - MODEL DOWNLOADER")
    print("="*60)

    print("\nThis will download approximately 500MB of models.")
    print("Press Enter to continue or Ctrl+C to cancel...")
    input()

    # Create directory structure
    print("\n📁 Creating directory structure...")
    create_directory_structure()

    # Download models
    download_paddleocr()
    download_whisper()
    download_fastvlm()

    # Verify
    verify_downloads()

    print("\n" + "="*60)
    print("✅ DOWNLOAD PROCESS COMPLETED")
    print("="*60)
    print("\nNext steps:")
    print("1. Manually download FastVLM from HuggingFace")
    print("2. Run test scripts to verify models work")
    print("3. Start perception clients")

if __name__ == "__main__":
    main()
