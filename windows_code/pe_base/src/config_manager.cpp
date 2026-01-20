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
    // Set default values as JSON object
    config_ = nlohmann::json::object();
    
    // Embedding settings
    config_["embedding"]["model_path"] = "models/embedding/model_q4.onnx";
    config_["embedding"]["tokenizer_path"] = "models/embedding/tokenizer";
    config_["embedding"]["python_executable"] = "python";
    config_["embedding"]["chunk_document_script"] = "scripts/chunk_document.py";
    config_["embedding"]["tokenize_text_script"] = "scripts/tokenize_text.py";

    // Session manager settings
    config_["session_manager"]["compression_threshold"] = 100;
    config_["session_manager"]["similarity_threshold"] = 70.0f;
    config_["session_manager"]["batch_size"] = 100;
    config_["session_manager"]["enabled"] = true;

    // Database settings (Elasticsearch - legacy)
    config_["database"]["type"] = "elasticsearch";
    config_["database"]["host"] = "localhost";
    config_["database"]["port"] = 9200;
    config_["database"]["index"] = "perception_events";

    // Model paths
    config_["models"]["whisper_path"] = "models/whisper/ggml-small.bin";
    config_["models"]["vad_path"] = "models/vad/silero_vad.onnx";
    config_["models"]["llm_model_path"] = "models/phi4-aitc/Phi4_FP16-3.8B-Q41-g32d-1027-v1.3.1.gguf";
    
    // LinguaCore settings
    config_["linguacore"]["check_interval_seconds"] = 60;
    config_["linguacore"]["batch_size"] = 10;
    config_["linguacore"]["verbose"] = true;
    
    // PostgreSQL settings
    config_["postgresql"]["host"] = "localhost";
    config_["postgresql"]["port"] = 5432;
    config_["postgresql"]["dbname"] = "perception_engine";
    config_["postgresql"]["user"] = "postgres";
    config_["postgresql"]["password"] = "";
    config_["postgresql"]["table"] = "perception_context";
    
    // LLM settings
    config_["llm"]["max_tokens"] = 200;
    config_["llm"]["temperature"] = 0.7f;
    
    // Qdrant settings
    config_["qdrant"]["host"] = "localhost";
    config_["qdrant"]["port"] = 6333;
    config_["qdrant"]["collection"] = "perception_summaries";

    // Paths
    config_["paths"]["temp_directory"] = "temp";
    config_["paths"]["log_directory"] = "logs";
    
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
                    // Store blacklist entries in a separate array
                    if (!config_.contains("blacklist")) {
                        config_["blacklist"] = nlohmann::json::array();
                    }
                    config_["blacklist"].push_back(value);
                    continue;
                }
                
                // Ensure section exists
                if (!currentSection.empty()) {
                    if (!config_.contains(currentSection)) {
                        config_[currentSection] = nlohmann::json::object();
                    }
                    
                    // Set value based on type
                    if (value == "true" || value == "TRUE") {
                        config_[currentSection][key] = true;
                    } else if (value == "false" || value == "FALSE") {
                        config_[currentSection][key] = false;
                    } else if (value.find('.') != std::string::npos) {
                        try {
                            config_[currentSection][key] = std::stof(value);
                        } catch (...) {
                            config_[currentSection][key] = value;
                        }
                    } else {
                        try {
                            config_[currentSection][key] = std::stoi(value);
                        } catch (...) {
                            config_[currentSection][key] = value;
                        }
                    }
                } else {
                    // Top-level key without section
                    if (value == "true" || value == "TRUE") {
                        config_[key] = true;
                    } else if (value == "false" || value == "FALSE") {
                        config_[key] = false;
                    } else {
                        config_[key] = value;
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
    if (config_.contains("blacklist") && config_["blacklist"].is_array()) {
        for (const auto& appName : config_["blacklist"]) {
            if (appName.is_string()) {
                blacklist_.insert(ToLowerCase(appName.get<std::string>()));
            }
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
        
        // Helper lambda to get value with fallback
        auto getString = [this](const std::string& section, const std::string& key, const std::string& defaultVal) -> std::string {
            if (config_.contains(section) && config_[section].contains(key)) {
                return config_[section][key].get<std::string>();
            }
            return defaultVal;
        };
        
        auto getInt = [this](const std::string& section, const std::string& key, int defaultVal) -> int {
            if (config_.contains(section) && config_[section].contains(key)) {
                return config_[section][key].get<int>();
            }
            return defaultVal;
        };
        
        auto getDouble = [this](const std::string& section, const std::string& key, double defaultVal) -> double {
            if (config_.contains(section) && config_[section].contains(key)) {
                return config_[section][key].get<double>();
            }
            return defaultVal;
        };
        
        auto getBool = [this](const std::string& section, const std::string& key, bool defaultVal) -> bool {
            if (config_.contains(section) && config_[section].contains(key)) {
                return config_[section][key].get<bool>();
            }
            return defaultVal;
        };
        
        file << "\n[embedding]\n";
        file << "model_path=" << getString("embedding", "model_path", "") << "\n";
        file << "tokenizer_path=" << getString("embedding", "tokenizer_path", "") << "\n";
        file << "python_executable=" << getString("embedding", "python_executable", "") << "\n";
        file << "chunk_document_script=" << getString("embedding", "chunk_document_script", "") << "\n";
        file << "tokenize_text_script=" << getString("embedding", "tokenize_text_script", "") << "\n";
        
        file << "\n[session_manager]\n";
        file << "enabled=" << (getBool("session_manager", "enabled", true) ? "true" : "false") << "\n";
        file << "compression_threshold=" << getInt("session_manager", "compression_threshold", 100) << "\n";
        file << "similarity_threshold=" << getDouble("session_manager", "similarity_threshold", 70.0) << "\n";
        file << "batch_size=" << getInt("session_manager", "batch_size", 50) << "\n";
        
        file << "\n[database]\n";
        file << "type=" << getString("database", "type", "") << "\n";
        file << "host=" << getString("database", "host", "") << "\n";
        file << "port=" << getInt("database", "port", 9200) << "\n";
        file << "index=" << getString("database", "index", "") << "\n";
        
        file << "\n[models]\n";
        file << "whisper_path=" << getString("models", "whisper_path", "") << "\n";
        file << "vad_path=" << getString("models", "vad_path", "") << "\n";
        file << "llm_model_path=" << getString("models", "llm_model_path", "") << "\n";
        
        file << "\n[linguacore]\n";
        file << "check_interval_seconds=" << getInt("linguacore", "check_interval_seconds", 60) << "\n";
        file << "batch_size=" << getInt("linguacore", "batch_size", 10) << "\n";
        file << "verbose=" << (getBool("linguacore", "verbose", true) ? "true" : "false") << "\n";
        
        file << "\n[postgresql]\n";
        file << "host=" << getString("postgresql", "host", "") << "\n";
        file << "port=" << getInt("postgresql", "port", 5432) << "\n";
        file << "dbname=" << getString("postgresql", "dbname", "") << "\n";
        file << "user=" << getString("postgresql", "user", "") << "\n";
        file << "password=" << getString("postgresql", "password", "") << "\n";
        file << "table=" << getString("postgresql", "table", "") << "\n";
        
        file << "\n[llm]\n";
        file << "max_tokens=" << getInt("llm", "max_tokens", 200) << "\n";
        file << "temperature=" << getDouble("llm", "temperature", 0.7) << "\n";
        
        file << "\n[qdrant]\n";
        file << "host=" << getString("qdrant", "host", "") << "\n";
        file << "port=" << getInt("qdrant", "port", 6333) << "\n";
        file << "collection=" << getString("qdrant", "collection", "") << "\n";
        
        file << "\n[paths]\n";
        file << "temp_directory=" << getString("paths", "temp_directory", "") << "\n";
        file << "log_directory=" << getString("paths", "log_directory", "") << "\n";
        
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
    std::string path = config_.value("embedding", nlohmann::json::object()).value("model_path", "models/embedding/model_q4.onnx");
    std::string resolved = ResolvePath(path);
    return ConvertToWideString(resolved);
}

std::string ConfigManager::GetEmbeddingModelPathUtf8() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetEmbeddingModelPathUtf8_Unlocked();
}

std::string ConfigManager::GetEmbeddingModelPathUtf8_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    std::string path = config_.value("embedding", nlohmann::json::object()).value("model_path", "models/embedding/model_q4.onnx");
    return ResolvePath(path);
}

std::string ConfigManager::GetTokenizerPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetTokenizerPath_Unlocked();
}

std::string ConfigManager::GetTokenizerPath_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    std::string path = config_.value("embedding", nlohmann::json::object()).value("tokenizer_path", "models/embedding/tokenizer");
    return ResolvePath(path);
}

std::string ConfigManager::GetPythonExecutable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("embedding", nlohmann::json::object()).value("python_executable", "python");
}

std::string ConfigManager::GetChunkDocumentScript() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.value("embedding", nlohmann::json::object()).value("chunk_document_script", "scripts/chunk_document.py");
    return ResolvePath(path);
}

std::string ConfigManager::GetTokenizeTextScript() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.value("embedding", nlohmann::json::object()).value("tokenize_text_script", "scripts/tokenize_text.py");
    return ResolvePath(path);
}

int ConfigManager::GetCompressionThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetCompressionThreshold_Unlocked();
}

int ConfigManager::GetCompressionThreshold_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    return config_.value("session_manager", nlohmann::json::object()).value("compression_threshold", 100);
}

float ConfigManager::GetSimilarityThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetSimilarityThreshold_Unlocked();
}

float ConfigManager::GetSimilarityThreshold_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    return config_.value("session_manager", nlohmann::json::object()).value("similarity_threshold", 70.0f);
}

int ConfigManager::GetBatchSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("session_manager", nlohmann::json::object()).value("batch_size", 50);
}

bool ConfigManager::IsSessionManagerEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("session_manager", nlohmann::json::object()).value("enabled", true);
}

std::string ConfigManager::GetDatabaseType() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("database", nlohmann::json::object()).value("type", "elasticsearch");
}

std::string ConfigManager::GetDatabaseHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("database", nlohmann::json::object()).value("host", "localhost");
}

int ConfigManager::GetDatabasePort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("database", nlohmann::json::object()).value("port", 9200);
}

std::string ConfigManager::GetDatabaseIndexName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("database", nlohmann::json::object()).value("index", "perception_events");
}

std::string ConfigManager::GetWhisperModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.value("models", nlohmann::json::object()).value("whisper_path", "models/whisper/ggml-small.bin");
    return ResolvePath(path);
}

std::string ConfigManager::GetVADModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.value("models", nlohmann::json::object()).value("vad_path", "models/vad/silero_vad.onnx");
    return ResolvePath(path);
}

std::string ConfigManager::GetLLMModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.value("models", nlohmann::json::object()).value("llm_model_path", "models/phi4-aitc/phi-3-mini-4k-instruct-q4.gguf");
    return ResolvePath(path);
}

// === LinguaCore Configuration ===
int ConfigManager::GetCheckIntervalSeconds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("linguacore", nlohmann::json::object()).value("check_interval_seconds", 60);
}

int ConfigManager::GetLinguaCoreBatchSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("linguacore", nlohmann::json::object()).value("batch_size", 10);
}

bool ConfigManager::IsLinguaCoreVerbose() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("linguacore", nlohmann::json::object()).value("verbose", true);
}

// === PostgreSQL Configuration ===
std::string ConfigManager::GetPostgreSQLHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("postgresql", nlohmann::json::object()).value("host", "localhost");
}

int ConfigManager::GetPostgreSQLPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("postgresql", nlohmann::json::object()).value("port", 5432);
}

std::string ConfigManager::GetPostgreSQLDatabase() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("postgresql", nlohmann::json::object()).value("dbname", "perception_engine");
}

std::string ConfigManager::GetPostgreSQLUser() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("postgresql", nlohmann::json::object()).value("user", "postgres");
}

std::string ConfigManager::GetPostgreSQLPassword() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("postgresql", nlohmann::json::object()).value("password", "");
}

int ConfigManager::GetPostgreSQLMaxundeletelength() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("postgresql", nlohmann::json::object()).value("maxundeletelength", 100);
}

std::string ConfigManager::GetPostgreSQLTable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("postgresql", nlohmann::json::object()).value("table", "perception_context");
}

// === LLM Configuration ===
int ConfigManager::GetLLMMaxTokens() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("llm", nlohmann::json::object()).value("max_tokens", 200);
}

float ConfigManager::GetLLMTemperature() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("llm", nlohmann::json::object()).value("temperature", 0.7f);
}

// === Qdrant Configuration ===
std::string ConfigManager::GetQdrantHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("qdrant", nlohmann::json::object()).value("host", "localhost");
}

int ConfigManager::GetQdrantPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("qdrant", nlohmann::json::object()).value("port", 6333);
}

std::string ConfigManager::GetQdrantCollection() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.value("qdrant", nlohmann::json::object()).value("collection", "perception_summaries");
}

std::string ConfigManager::GetTempDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.value("paths", nlohmann::json::object()).value("temp_directory", "temp");
    return ResolvePath(path);
}

std::string ConfigManager::GetLogDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.value("paths", nlohmann::json::object()).value("log_directory", "logs");
    return ResolvePath(path);
}

void ConfigManager::SetEmbeddingModelPath(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string utf8Path = ConvertToUtf8(path);
    if (!config_.contains("embedding")) {
        config_["embedding"] = nlohmann::json::object();
    }
    config_["embedding"]["model_path"] = utf8Path;
}

void ConfigManager::SetTokenizerPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.contains("embedding")) {
        config_["embedding"] = nlohmann::json::object();
    }
    config_["embedding"]["tokenizer_path"] = path;
}

void ConfigManager::SetCompressionThreshold(int threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.contains("session_manager")) {
        config_["session_manager"] = nlohmann::json::object();
    }
    config_["session_manager"]["compression_threshold"] = threshold;
}

void ConfigManager::SetSimilarityThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.contains("session_manager")) {
        config_["session_manager"] = nlohmann::json::object();
    }
    config_["session_manager"]["similarity_threshold"] = threshold;
}

void ConfigManager::SetPythonExecutable(const std::string& execute) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.contains("embedding")) {
        config_["embedding"] = nlohmann::json::object();
    }
    config_["embedding"]["python_executable"] = execute;
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
    std::string llmPath = config_.value("models", nlohmann::json::object()).value("llm_model_path", "");
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
