# CLAUDE.md - Perception Engine

This document provides comprehensive context for working with the Perception Engine codebase.

## Project Overview

**Nova Perception Engine** is a cross-platform, multi-modal context capture and analytics system that:
- Captures real-time context from user activity (screen, audio, camera, system metrics)
- Processes events through a layered database architecture (raw to compressed to semantic)
- Enables full-text and semantic search over user activity history
- Provides APIs for AI assistant integration (Claude via MCP)

---

## High-Level Architecture

The system consists of three main components:
1. **windows_code/** - Perception Engine (C++ Windows) - Real-time context capture
2. **database_cpp/** - Database Layer (C++) - Multi-layered storage and processing  
3. **mcp_server/** - MCP Server (C# .NET 8) - Claude integration

## Directory Structure

perception_engine/
- windows_code/ - Perception Engine (C++ - Windows)
  - include/ - Headers (audio/, context/, core/, platform/, providers/, communication/, utils/)
  - src/ - Implementation
  - database_client/ - DB integration wrapper
  - third-party/ - whisper.cpp, OpenCV, ONNX
  - CMakeLists.txt
- database_cpp/ - Database Layer (C++)
  - include/ - Headers (collector/, common/, layer0/, layer1/)
  - src/ - Implementation
  - elasticsearch_client_dll/ - ES HTTP client library
  - CMakeLists.txt
- mcp_server/csharp/ - MCP Server (.NET 8)
- sdk/ - Client SDKs
- setup.ps1 - PowerShell setup script
- requirements.txt - Python dependencies

---

# WINDOWS_CODE ARCHITECTURE

## Overview

The windows_code/ folder contains the Perception Engine - a real-time multi-modal context capture system for Windows. It captures screen content, audio (speech-to-text), camera imagery, and system metrics, exposing them via an HTTP API on port 8777.

## Key Components

### 1. Context Providers (Composite Pattern)

Interface: IContextProvider (include/providers/IContextProvider.h)

Implementations:
- SystemContextProvider - CPU, memory, battery, network
- VoiceContextProvider - Speech transcription via whisper.cpp
- CameraContextProvider - Camera descriptions via FastVLM
- AppActivityContextProvider - Active app, window title, URL

Manager: CompositeContextManager - Coordinates all providers

### 2. Audio Capture Engine

File: include/audio/AudioCaptureEngine.h

Features:
- WASAPI microphone capture (user speech)
- WASAPI system audio loopback (device playback)
- Silero VAD for voice activity detection
- whisper.cpp for speech-to-text (CUDA-accelerated)
- Meeting mode for continuous transcription

Configuration:
- Sample Rate: 16kHz (Whisper requirement)
- VAD Chunk: 32ms (512 samples for Silero)
- Whisper Chunk: 3 seconds
- Max Buffer: 30 seconds @ 16kHz

### 3. Window Event Monitor

File: include/platform/WindowEventMonitor.h

Events: WINDOW_ACTIVATED, WINDOW_CREATED, TAB_ACTIVATED, APPLICATION_STARTED, etc.

### 4. HTTP Server

File: include/communication/HttpServer.h
Endpoint: GET http://localhost:8777/context

## Dependencies (windows_code)

- whisper.cpp (Latest) - Speech-to-text
- Silero VAD (1.0) - Voice detection
- ONNX Runtime (1.16+) - ML inference
- OpenCV (4.10.0) - Computer vision
- nlohmann/json (3.x) - JSON parsing

---

# DATABASE_CPP ARCHITECTURE

## Overview

The database_cpp/ folder contains the Database Layer - multi-layered storage and processing:
- Layer 0: Raw event storage with full-text search (Elasticsearch or SQLite)
- Layer 1: Session detection, engagement scoring, LLM compression (DuckDB)
- Layer 2: Vector embeddings for semantic search (Qdrant - planned)

## Key Components

### 1. DataCollector

File: include/collector/DataCollector.h
- Polls http://localhost:8777/context every 5 seconds
- Supports SQLite and Elasticsearch backends

### 2. SessionDetector

File: include/layer1/SessionDetector.h
Break Conditions: Idle > 5min, App change, Domain change, Tab change

### 3. EngagementCalculator

File: include/layer1/EngagementCalculator.h
Score Formula:
- hasCopied: +0.4 (strongest signal)
- hasSelected: +0.2
- interactions > 5: +0.2
- dwellTime > 30s: +0.2
- copiedCount > 3: +0.1

### 4. ContentClassifier

File: include/layer1/ContentClassifier.h
ContentTypes: EMAIL, CHAT, CODE, DOCUMENT, MEETING, VIDEO, SOCIAL, RESEARCH_PAPER
Domains: WORK, ENTERTAINMENT, LIFE, INTERACTION

## Data Models

- RawEvent (Layer 0): include/layer0/DataIngestion.h
- CompressedSession (Layer 1): include/layer1/DuckDBManager.h
- Types: include/common/Types.h

## Dependencies (database_cpp)

- SQLite3 (3.x) - Local storage
- nlohmann/json (3.x) - JSON serialization
- libcurl (7.x) - HTTP client
- DuckDB (0.9+) - Compressed session storage

---

## Tech Stack Summary

- Context Capture: C++17, whisper.cpp, Silero VAD (Windows-only)
- Vision: FastVLM-0.5B (PyTorch), CUDA 12.1 recommended
- Raw Storage: Elasticsearch (port 9200) - Primary
- Alt Storage: SQLite + FTS5 - Local development
- Session Storage: DuckDB - Compressed sessions
- Vector DB: Qdrant (port 6333/6334) - Semantic search
- MCP Server: C# .NET 8 - Claude integration
- Build: CMake 3.15+, .NET 8 SDK

## Build Instructions

### Windows Perception Engine
cd windows_code
mkdir build && cd build
cmake ..
cmake --build . --config Release
Output: build/bin/Release/PerceptionEngine.exe

### Database Layer
cd database_cpp
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
Output: build/bin/perception_data_collector

### MCP Server
cd mcp_server/csharp
dotnet publish -c Release -o publish/
Output: publish/PerceptionMcpBridge.exe

## Running the System

1. Start Infrastructure (Docker):
   docker run -d --name elasticsearch -p 9200:9200 -e "discovery.type=single-node" elasticsearch:8.11.0
   docker run -d --name qdrant -p 6333:6333 -p 6334:6334 qdrant/qdrant

2. Start Perception Engine:
   cd windows_code/build/bin/Release && ./PerceptionEngine.exe

3. Start Data Collector:
   cd database_cpp/build/bin && ./perception_data_collector --storage elasticsearch

4. Start MCP Server:
   cd mcp_server/csharp/publish && ./PerceptionMcpBridge.exe

## Configuration

Environment Variables:
- ELASTICSEARCH_URL=http://localhost:9200
- QDRANT_URL=http://localhost:6333
- OPENAI_API_KEY=sk-... (for LLM compression)
- ANTHROPIC_API_KEY=sk-ant-... (alternative)

Data Retention:
- Layer 0: 24 hours (configurable)
- Layer 1: 30 days (sessions)
- Layer 2: Indefinite (vectors)

## Common Issues

- whisper.cpp build fails: Ensure CUDA toolkit 12.1 installed, run git submodule update --init
- Elasticsearch connection refused: Check docker ps, verify curl localhost:9200
- FastVLM CUDA out of memory: Reduce image resolution or set CUDA_VISIBLE_DEVICES=""

## Project Status

| Feature | Status |
|---------|--------|
| Screen Capture | Done |
| Audio (whisper.cpp) | Done |
| Camera (FastVLM) | Done |
| Elasticsearch Storage | Done |
| Session Detection | Done |
| Engagement Scoring | Done |
| Content Classification | Done |
| MCP Server | Done |
| LLM Compression | In Progress |
| Embedding Generation | Planned |
| Qdrant Integration | Planned |
