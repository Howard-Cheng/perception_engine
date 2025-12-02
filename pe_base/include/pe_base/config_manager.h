#pragma once

#include <string>
#include <mutex>
#include <memory>
#include "pe_base/json.hpp"
#include "pe_base/pe_exports.h"

namespace pe_base {

/**
 * ConfigManager - Centralized configuration management
 * 
 * Handles loading and accessing configuration from config.json
 * Thread-safe singleton pattern
 * 
 * Usage:
 *   ConfigManager::GetInstance().LoadConfig("config.json");
 *   std::string modelPath = ConfigManager::GetInstance().GetEmbeddingModelPath();
 */
class PE_BASE_API ConfigManager {
public:
    // Singleton access
    static ConfigManager& GetInstance();

    // Load configuration from file
    bool LoadConfig(const std::string& configPath);
    
    // Save current configuration to file
    bool SaveConfig(const std::string& configPath);

    // === Embedding Model Configuration ===
    std::wstring GetEmbeddingModelPath() const;
    std::string GetEmbeddingModelPathUtf8() const;
    std::string GetTokenizerPath() const;
    std::string GetPythonExecutable() const;
    std::string GetChunkDocumentScript() const;
    
    // === Session Manager Configuration ===
    int GetCompressionThreshold() const;
    float GetSimilarityThreshold() const;
    int GetBatchSize() const;
    bool IsSessionManagerEnabled() const;

    // === Database Configuration ===
    std::string GetDatabaseType() const;
    std::string GetDatabaseHost() const;
    int GetDatabasePort() const;
    std::string GetDatabaseIndexName() const;

    // === Model Paths Configuration ===
    std::string GetWhisperModelPath() const;
    std::string GetVADModelPath() const;

    // === Runtime Paths ===
    std::string GetTempDirectory() const;
    std::string GetLogDirectory() const;

    // === Update Configuration ===
    void SetEmbeddingModelPath(const std::wstring& path);
    void SetTokenizerPath(const std::string& path);
    void SetCompressionThreshold(int threshold);
    void SetSimilarityThreshold(float threshold);
    void SetPythonExecutable(const std::string& execute);

    // === Validation ===
    bool ValidateConfiguration() const;
    std::string GetLastError() const;
    bool IsLoaded() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return loaded_; 
    }

private:
    ConfigManager();
    ~ConfigManager() = default;
    
    // Prevent copying
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Helper functions
    std::string GetExecutableDirectory() const;
    std::string ResolvePath(const std::string& path) const;
    std::wstring ConvertToWideString(const std::string& str) const;
    std::string ConvertToUtf8(const std::wstring& wstr) const;
    
    // Internal helper methods (without locking) for use within locked sections
    std::string GetEmbeddingModelPathUtf8_Unlocked() const;
    std::string GetTokenizerPath_Unlocked() const;
    int GetCompressionThreshold_Unlocked() const;
    float GetSimilarityThreshold_Unlocked() const;

    mutable std::mutex mutex_;
    Json config_;
    bool loaded_;
    mutable std::string lastError_;
};

} // namespace pe_base
