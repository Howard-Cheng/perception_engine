#pragma once

#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>
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
    std::string GetTokenizeTextScript() const;  // NEW: For single text tokenization
    
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
    std::string GetLLMModelPath() const;
    
    // === LinguaCore Configuration ===
    int GetCheckIntervalSeconds() const;
    int GetLinguaCoreBatchSize() const;
    bool IsLinguaCoreVerbose() const;
    
    // === PostgreSQL Configuration ===
    std::string GetPostgreSQLHost() const;
    int GetPostgreSQLPort() const;
    std::string GetPostgreSQLDatabase() const;
    std::string GetPostgreSQLUser() const;
    std::string GetPostgreSQLPassword() const;
    std::string GetPostgreSQLTable() const;
    int GetPostgreSQLMaxundeletelength() const;
	float GetPostgreSQLOutofdatehour() const;
    
    // === LLM Configuration ===
    int GetLLMMaxTokens() const;
    float GetLLMTemperature() const;
    
    // === Qdrant Configuration ===
    std::string GetQdrantHost() const;
    int GetQdrantPort() const;
    std::string GetQdrantCollection() const;

	// === Onlinelocation Configuration ===
	std::string GetOnlineLocationBaseUrl() const;
	std::string GetOnlineLocationFormat() const;
	int GetOnlineLocationAddressDetails() const;
	int GetOnlineLocationExtraTags() const;
	int GetOnlineLocationZoom() const;
	std::string GetOnlineLocationEmail() const;
	std::string GetOnlineLocationAcceptLanguage() const;

	

    // === Runtime Paths ===
    std::string GetTempDirectory() const;
    std::string GetLogDirectory() const;

    // === Blacklist Configuration ===
    /**
     * Get application blacklist
     * @return Set of blacklisted application names (lowercase)
     */
    std::set<std::string> GetBlacklist() const;
    
    /**
     * Check if an application is blacklisted
     * @param appName Application name to check (case-insensitive)
     * @return true if blacklisted, false otherwise
     */
    bool IsBlacklisted(const std::string& appName) const;
    
    /**
     * Add application to blacklist
     * @param appName Application name to add
     */
    void AddToBlacklist(const std::string& appName);
    
    /**
     * Remove application from blacklist
     * @param appName Application name to remove
     */
    void RemoveFromBlacklist(const std::string& appName);
    
    /**
     * Clear all blacklisted applications
     */
    void ClearBlacklist();

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
    std::string ToLowerCase(const std::string& str) const;
    
    // Internal helper methods (without locking) for use within locked sections
    std::string GetEmbeddingModelPathUtf8_Unlocked() const;
    std::string GetTokenizerPath_Unlocked() const;
    int GetCompressionThreshold_Unlocked() const;
    float GetSimilarityThreshold_Unlocked() const;
    
    // Blacklist helper (without locking)
    void LoadBlacklist_Unlocked();

    mutable std::mutex mutex_;
    nlohmann::json config_;  // Changed from custom Json to nlohmann::json
    bool loaded_;
    mutable std::string lastError_;
    
    // Blacklist storage (lowercase for case-insensitive comparison)
    std::set<std::string> blacklist_;
};

} // namespace pe_base
