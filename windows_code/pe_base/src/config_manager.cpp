#include "pe_base/config_manager.h"
#include "pe_base/logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <windows.h>
#include <algorithm>
#include <cctype>

namespace pe_base {

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() : loaded_(false) {
    // Set default values with section prefix to match LoadConfig behavior
    
    // Embedding settings
    config_.set("embedding.model_path", "models/embedding/model_q4.onnx");  
    config_.set("embedding.tokenizer_path", "models/embedding/tokenizer");
    config_.set("embedding.python_executable", "python");
    config_.set("embedding.chunk_document_script", "scripts/chunk_document.py");
    config_.set("embedding.tokenize_text_script", "scripts/tokenize_text.py");

    // Session manager settings
    config_.set("session_manager.compression_threshold", 100);
    config_.set("session_manager.similarity_threshold", 70.0f);
    config_.set("session_manager.batch_size", 100);
    config_.set("session_manager.enabled", true);

    // Database settings (Elasticsearch - legacy)
    config_.set("database.type", "elasticsearch");
    config_.set("database.host", "localhost");
    config_.set("database.port", 9200);
    config_.set("database.index", "perception_events");

    // Model paths
    config_.set("models.whisper_path", "models/whisper/ggml-small.bin");  
    config_.set("models.vad_path", "models/vad/silero_vad.onnx"); 
    config_.set("models.llm_model_path", "models/phi4-aitc/Phi4_FP16-3.8B-Q41-g32d-1027-v1.3.1.gguf");
    
    // LinguaCore settings
    config_.set("linguacore.check_interval_seconds", 60);
    config_.set("linguacore.batch_size", 10);
    config_.set("linguacore.verbose", true);
    
    // PostgreSQL settings
    config_.set("postgresql.host", "localhost");
    config_.set("postgresql.port", 5432);
    config_.set("postgresql.dbname", "perception_engine");
    config_.set("postgresql.user", "postgres");
    config_.set("postgresql.password", "");
    config_.set("postgresql.table", "perception_context");
    
    // LLM settings
    config_.set("llm.max_tokens", 200);
    config_.set("llm.temperature", 0.7f);
    
    // Qdrant settings
    config_.set("qdrant.host", "localhost");
    config_.set("qdrant.port", 6333);
    config_.set("qdrant.collection", "perception_summaries");

    // Paths
    config_.set("paths.temp_directory", "temp");
    config_.set("paths.log_directory", "logs");
    
    // Initialize default blacklist (common system apps)
    blacklist_.insert("weixin");
    blacklist_.insert("dwm");
    blacklist_.insert("winlogon");
    blacklist_.insert("csrss");
    blacklist_.insert("taskmgr");
}

// Helper function to trim whitespace
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool ConfigManager::LoadConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            lastError_ = "Failed to open config file: " + configPath;
            PE_WARN(lastError_);
            return false;
        }
        
        std::string line;
        std::string currentSection = "";
        
        while (std::getline(file, line)) {
            line = trim(line);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }
            
            // Check for section header [section_name]
            if (line[0] == '[') {
                size_t end = line.find(']');
                if (end != std::string::npos) {
                    currentSection = line.substr(1, end - 1);
                }
                continue;
            }
            
            // Parse key=value
            size_t equals = line.find('=');
            if (equals != std::string::npos) {
                std::string key = trim(line.substr(0, equals));
                std::string value = trim(line.substr(equals + 1));
                
                // Handle blacklist section specially
                if (currentSection == "blacklist") {
                    // Store blacklist entries with "blacklist_" prefix for later retrieval
                    config_.set("blacklist_" + key, value);
                    continue;
                }
                
                // Construct full key with section prefix to avoid conflicts
                // Format: section.key (e.g., "postgresql.host", "qdrant.host")
                std::string fullKey;
                if (!currentSection.empty()) {
                    fullKey = currentSection + "." + key;
                } else {
                    fullKey = key;
                }
                
                // Set value based on type
                if (value == "true" || value == "TRUE") {
                    config_.set(fullKey, true);
                } else if (value == "false" || value == "FALSE") {
                    config_.set(fullKey, false);
                } else if (value.find('.') != std::string::npos) {
                    try {
                        config_.set(fullKey, std::stof(value));
                    } catch (...) {
                        config_.set(fullKey, value);
                    }
                } else {
                    try {
                        config_.set(fullKey, std::stoi(value));
                    } catch (...) {
                        config_.set(fullKey, value);
                    }
                }
            }
        }
        
        // Load blacklist after config is parsed
        LoadBlacklist_Unlocked();
        
        loaded_ = true;
        PE_INFO("Configuration loaded successfully from: " + configPath);
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = std::string("Exception loading config: ") + e.what();
        PE_ERROR(lastError_);
        return false;
    }
}

// Load blacklist from config (internal, assumes mutex is locked)
void ConfigManager::LoadBlacklist_Unlocked() {
    // Clear existing blacklist (but keep defaults)
    std::set<std::string> defaults = {"weixin", "dwm", "winlogon", "csrss", "taskmgr"};
    blacklist_ = defaults;
    
    // Load blacklist entries from config
    // Keys are stored as "blacklist_app_1", "blacklist_app_2", etc.
    for (int i = 1; i <= 100; ++i) {  // Check up to 100 entries
        std::string key = "blacklist_app_" + std::to_string(i);
        std::string appName = config_.getString(key, "");
        if (!appName.empty()) {
            blacklist_.insert(ToLowerCase(appName));
        }
    }
    
    PE_INFO("Loaded " + std::to_string(blacklist_.size()) + " blacklisted applications");
}

bool ConfigManager::SaveConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ofstream file(configPath);
        if (!file.is_open()) {
            lastError_ = "Failed to open config file for writing: " + configPath;
            PE_ERROR(lastError_);
            return false;
        }
        
        file << "# Perception Engine Configuration\n";
        file << "\n[embedding]\n";
        file << "model_path=" << config_.getString("embedding.model_path", "") << "\n";
        file << "tokenizer_path=" << config_.getString("embedding.tokenizer_path", "") << "\n";
        file << "python_executable=" << config_.getString("embedding.python_executable", "") << "\n";
        file << "chunk_document_script=" << config_.getString("embedding.chunk_document_script", "") << "\n";
        file << "tokenize_text_script=" << config_.getString("embedding.tokenize_text_script", "") << "\n";
        
        file << "\n[session_manager]\n";
        file << "enabled=" << (config_.getBool("session_manager.enabled", true) ? "true" : "false") << "\n";
        file << "compression_threshold=" << config_.getInt("session_manager.compression_threshold", 100) << "\n";
        file << "similarity_threshold=" << config_.getDouble("session_manager.similarity_threshold", 70.0) << "\n";
        file << "batch_size=" << config_.getInt("session_manager.batch_size", 50) << "\n";
        
        file << "\n[database]\n";
        file << "type=" << config_.getString("database.type", "") << "\n";
        file << "host=" << config_.getString("database.host", "") << "\n";
        file << "port=" << config_.getInt("database.port", 9200) << "\n";
        file << "index=" << config_.getString("database.index", "") << "\n";
        
        file << "\n[models]\n";
        file << "whisper_path=" << config_.getString("models.whisper_path", "") << "\n";
        file << "vad_path=" << config_.getString("models.vad_path", "") << "\n";
        file << "llm_model_path=" << config_.getString("models.llm_model_path", "") << "\n";
        
        file << "\n[linguacore]\n";
        file << "check_interval_seconds=" << config_.getInt("linguacore.check_interval_seconds", 60) << "\n";
        file << "batch_size=" << config_.getInt("linguacore.batch_size", 10) << "\n";
        file << "verbose=" << (config_.getBool("linguacore.verbose", true) ? "true" : "false") << "\n";
        
        file << "\n[postgresql]\n";
        file << "host=" << config_.getString("postgresql.host", "") << "\n";
        file << "port=" << config_.getInt("postgresql.port", 5432) << "\n";
        file << "dbname=" << config_.getString("postgresql.dbname", "") << "\n";
        file << "user=" << config_.getString("postgresql.user", "") << "\n";
        file << "password=" << config_.getString("postgresql.password", "") << "\n";
        file << "table=" << config_.getString("postgresql.table", "") << "\n";
        
        file << "\n[llm]\n";
        file << "max_tokens=" << config_.getInt("llm.max_tokens", 200) << "\n";
        file << "temperature=" << config_.getDouble("llm.temperature", 0.7) << "\n";
        
        file << "\n[qdrant]\n";
        file << "host=" << config_.getString("qdrant.host", "") << "\n";
        file << "port=" << config_.getInt("qdrant.port", 6333) << "\n";
        file << "collection=" << config_.getString("qdrant.collection", "") << "\n";
        
        file << "\n[paths]\n";
        file << "temp_directory=" << config_.getString("paths.temp_directory", "") << "\n";
        file << "log_directory=" << config_.getString("paths.log_directory", "") << "\n";
        
        // Save blacklist
        file << "\n[blacklist]\n";
        file << "# Application names to ignore (case-insensitive)\n";
        int index = 1;
        for (const auto& appName : blacklist_) {
            file << "app_" << index++ << "=" << appName << "\n";
        }
        
        file.close();
        
        PE_INFO("Configuration saved to: " + configPath);
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = std::string("Exception saving config: ") + e.what();
        PE_ERROR(lastError_);
        return false;
    }
}

std::wstring ConfigManager::GetEmbeddingModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("embedding.model_path", "models/embedding/model_q4.onnx");
    std::string resolved = ResolvePath(path);
    return ConvertToWideString(resolved);
}

std::string ConfigManager::GetEmbeddingModelPathUtf8() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetEmbeddingModelPathUtf8_Unlocked();
}

std::string ConfigManager::GetEmbeddingModelPathUtf8_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    // Try with section prefix first, fall back to without prefix for backward compatibility
    std::string path = config_.getString("embedding.model_path", "");
    if (path.empty()) {
        path = config_.getString("model_path", "models/embedding/model_q4.onnx");
    }
    return ResolvePath(path);
}

std::string ConfigManager::GetTokenizerPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetTokenizerPath_Unlocked();
}

std::string ConfigManager::GetTokenizerPath_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    std::string path = config_.getString("embedding.tokenizer_path", "");
    if (path.empty()) {
        path = config_.getString("tokenizer_path", "models/embedding/tokenizer");
    }
    return ResolvePath(path);
}

std::string ConfigManager::GetPythonExecutable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string exe = config_.getString("embedding.python_executable", "");
    if (exe.empty()) {
        exe = config_.getString("python_executable", "python");
    }
    return exe;
}

std::string ConfigManager::GetChunkDocumentScript() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("embedding.chunk_document_script", "");
    if (path.empty()) {
        path = config_.getString("chunk_document_script", "scripts/chunk_document.py");
    }
    return ResolvePath(path);
}

std::string ConfigManager::GetTokenizeTextScript() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("embedding.tokenize_text_script", "");
    if (path.empty()) {
        path = config_.getString("tokenize_text_script", "scripts/tokenize_text.py");
    }
    return ResolvePath(path);
}

int ConfigManager::GetCompressionThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetCompressionThreshold_Unlocked();
}

int ConfigManager::GetCompressionThreshold_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    return config_.getInt("session_manager.compression_threshold", 100);
}

float ConfigManager::GetSimilarityThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetSimilarityThreshold_Unlocked();
}

float ConfigManager::GetSimilarityThreshold_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    return static_cast<float>(config_.getDouble("session_manager.similarity_threshold", 70.0));
}

int ConfigManager::GetBatchSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getInt("session_manager.batch_size", 50);
}

bool ConfigManager::IsSessionManagerEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getBool("session_manager.enabled", true);
}

std::string ConfigManager::GetDatabaseType() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getString("database.type", "elasticsearch");
}

std::string ConfigManager::GetDatabaseHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getString("database.host", "localhost");
}

int ConfigManager::GetDatabasePort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getInt("database.port", 9200);
}

std::string ConfigManager::GetDatabaseIndexName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getString("database.index", "perception_events");
}

std::string ConfigManager::GetWhisperModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("models.whisper_path", "");
    if (path.empty()) {
        path = config_.getString("whisper_path", "models/whisper/ggml-small.bin");
    }
    return ResolvePath(path);
}

std::string ConfigManager::GetVADModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("models.vad_path", "");
    if (path.empty()) {
        path = config_.getString("vad_path", "models/vad/silero_vad.onnx");
    }
    return ResolvePath(path);
}

std::string ConfigManager::GetLLMModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("models.llm_model_path", "");
    if (path.empty()) {
        path = config_.getString("llm_model_path", "models/phi4-aitc/phi-3-mini-4k-instruct-q4.gguf");
    }
    return ResolvePath(path);
}

// === LinguaCore Configuration ===
int ConfigManager::GetCheckIntervalSeconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int value = config_.getInt("linguacore.check_interval_seconds", -1);
    if (value == -1) {
        value = config_.getInt("check_interval_seconds", 60);
    }
    return value;
}

int ConfigManager::GetLinguaCoreBatchSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int value = config_.getInt("linguacore.batch_size", -1);
    if (value == -1) {
        value = config_.getInt("linguacore_batch_size", 10);
    }
    return value;
}

bool ConfigManager::IsLinguaCoreVerbose() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Check if section.key exists first
    std::string strValue = config_.getString("linguacore.verbose", "");
    if (!strValue.empty()) {
        return config_.getBool("linguacore.verbose", true);
    }
    return config_.getBool("linguacore_verbose", true);
}

// === PostgreSQL Configuration ===
std::string ConfigManager::GetPostgreSQLHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string host = config_.getString("postgresql.host", "");
    if (host.empty()) {
        host = config_.getString("pg_host", "localhost");
    }
    return host;
}

int ConfigManager::GetPostgreSQLPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int port = config_.getInt("postgresql.port", -1);
    if (port == -1) {
        port = config_.getInt("pg_port", 5432);
    }
    return port;
}

std::string ConfigManager::GetPostgreSQLDatabase() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string dbname = config_.getString("postgresql.dbname", "");
    if (dbname.empty()) {
        dbname = config_.getString("pg_dbname", "perception_engine");
    }
    return dbname;
}

std::string ConfigManager::GetPostgreSQLUser() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string user = config_.getString("postgresql.user", "");
    if (user.empty()) {
        user = config_.getString("pg_user", "postgres");
    }
    return user;
}

std::string ConfigManager::GetPostgreSQLPassword() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string pwd = config_.getString("postgresql.password", "");
    if (pwd.empty()) {
        pwd = config_.getString("pg_password", "");
    }
    return pwd;
}

std::string ConfigManager::GetPostgreSQLTable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string table = config_.getString("postgresql.table", "");
    if (table.empty()) {
        table = config_.getString("pg_table", "perception_context");
    }
    return table;
}

// === LLM Configuration ===
int ConfigManager::GetLLMMaxTokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int tokens = config_.getInt("llm.max_tokens", -1);
    if (tokens == -1) {
        tokens = config_.getInt("llm_max_tokens", 200);
    }
    return tokens;
}

float ConfigManager::GetLLMTemperature() const {
    std::lock_guard<std::mutex> lock(mutex_);
    float temp = static_cast<float>(config_.getDouble("llm.temperature", -1.0));
    if (temp < 0.0f) {
        temp = static_cast<float>(config_.getDouble("llm_temperature", 0.7));
    }
    return temp;
}

// === Qdrant Configuration ===
std::string ConfigManager::GetQdrantHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string host = config_.getString("qdrant.host", "");
    if (host.empty()) {
        host = config_.getString("qdrant_host", "localhost");
    }
    return host;
}

int ConfigManager::GetQdrantPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int port = config_.getInt("qdrant.port", -1);
    if (port == -1) {
        port = config_.getInt("qdrant_port", 6333);
    }
    return port;
}

std::string ConfigManager::GetQdrantCollection() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string collection = config_.getString("qdrant.collection", "");
    if (collection.empty()) {
        collection = config_.getString("qdrant_collection", "perception_summaries");
    }
    return collection;
}

std::string ConfigManager::GetTempDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("paths.temp_directory", "temp");
    return ResolvePath(path);
}

std::string ConfigManager::GetLogDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("paths.log_directory", "logs");
    return ResolvePath(path);
}

void ConfigManager::SetEmbeddingModelPath(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string utf8Path = ConvertToUtf8(path);
    config_.set("embedding.model_path", utf8Path);
}

void ConfigManager::SetTokenizerPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("embedding.tokenizer_path", path);
}

void ConfigManager::SetCompressionThreshold(int threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("session_manager.compression_threshold", threshold);
}

void ConfigManager::SetSimilarityThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("session_manager.similarity_threshold", threshold);
}

void ConfigManager::SetPythonExecutable(const std::string& execute) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("embedding.python_executable", execute);
}

bool ConfigManager::ValidateConfiguration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check critical paths exist - use unlocked internal methods
    std::string modelPath = GetEmbeddingModelPathUtf8_Unlocked();
    if (!std::filesystem::exists(modelPath)) {
        lastError_ = "Embedding model not found: " + modelPath;
        PE_ERROR(lastError_);
        return false;
    }
    
    std::string tokenizerPath = GetTokenizerPath_Unlocked();
    if (!std::filesystem::exists(tokenizerPath)) {
        lastError_ = "Tokenizer not found: " + tokenizerPath;
        PE_WARN(lastError_ + " (may be acceptable if not using document comparison)");
    }
    
    // Check LLM model path (optional, but warn if configured and not found)
    std::string llmPath = config_.getString("models.llm_model_path", "");
    if (llmPath.empty()) {
        llmPath = config_.getString("llm_model_path", "");
    }
    if (!llmPath.empty()) {
        std::string resolvedLlmPath = ResolvePath(llmPath);
        if (!std::filesystem::exists(resolvedLlmPath)) {
            lastError_ = "LLM model not found: " + resolvedLlmPath;
            PE_WARN(lastError_ + " (may be acceptable if not using LLM features)");
        }
    }
    
    // Check thresholds are reasonable - use unlocked internal methods
    int compressionThreshold = GetCompressionThreshold_Unlocked();
    if (compressionThreshold < 1 || compressionThreshold > 10000) {
        lastError_ = "Invalid compression threshold: " + std::to_string(compressionThreshold);
        PE_ERROR(lastError_);
        return false;
    }
    
    float similarityThreshold = GetSimilarityThreshold_Unlocked();
    if (similarityThreshold < 0.0f || similarityThreshold > 100.0f) {
        lastError_ = "Invalid similarity threshold: " + std::to_string(similarityThreshold);
        PE_ERROR(lastError_);
        return false;
    }
    
    return true;
}

std::string ConfigManager::GetLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

std::string ConfigManager::GetExecutableDirectory() const {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring wpath(buffer);
    std::filesystem::path exePath(wpath);
    return exePath.parent_path().string();
}

std::string ConfigManager::ResolvePath(const std::string& path) const {
    std::filesystem::path p(path);
    
    // If already absolute, return as-is
    if (p.is_absolute()) {
        return p.string();
    }
    
    // Otherwise, resolve relative to executable directory
    std::filesystem::path exeDir(GetExecutableDirectory());
    std::filesystem::path resolved = exeDir / p;
    
    return resolved.string();
}

std::wstring ConfigManager::ConvertToWideString(const std::string& str) const {
    if (str.empty()) {
        return std::wstring();
    }
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                                         static_cast<int>(str.length()), 
                                         NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), 
                       static_cast<int>(str.length()), 
                       &wstr[0], size_needed);
    return wstr;
}

std::string ConfigManager::ConvertToUtf8(const std::wstring& wstr) const {
    if (wstr.empty()) {
        return std::string();
    }
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), 
                                         static_cast<int>(wstr.length()), 
                                         NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), 
                       static_cast<int>(wstr.length()), 
                       &str[0], size_needed, NULL, NULL);
    return str;
}

// Blacklist functionality implementation
std::set<std::string> ConfigManager::GetBlacklist() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return blacklist_;
}

bool ConfigManager::IsBlacklisted(const std::string& appName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string lowerAppName = ToLowerCase(appName);
    return blacklist_.find(lowerAppName) != blacklist_.end();
}

void ConfigManager::AddToBlacklist(const std::string& appName) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string lowerAppName = ToLowerCase(appName);
    blacklist_.insert(lowerAppName);
    PE_INFO("Added to blacklist: " + appName);
}

void ConfigManager::RemoveFromBlacklist(const std::string& appName) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string lowerAppName = ToLowerCase(appName);
    blacklist_.erase(lowerAppName);
    PE_INFO("Removed from blacklist: " + appName);
}

void ConfigManager::ClearBlacklist() {
    std::lock_guard<std::mutex> lock(mutex_);
    blacklist_.clear();
    PE_INFO("Blacklist cleared");
}

std::string ConfigManager::ToLowerCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace pe_base
