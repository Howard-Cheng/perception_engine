#include "config/ConfigManager.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <windows.h>
#include <algorithm>

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() : loaded_(false) {
    // Set default values - match config.ini key names exactly
    config_.set("model_path", "models/embedding/model_q4.onnx");  
    config_.set("tokenizer_path", "models/embedding/tokenizer");
    config_.set("python_executable", "python");
    config_.set("chunk_document_script", "scripts/chunk_document.py");

    config_.set("compression_threshold", 100);
    config_.set("similarity_threshold", 70.0f);
    config_.set("batch_size", 50);
    config_.set("session_manager_enabled", true);

    config_.set("database_type", "elasticsearch");
    config_.set("database_host", "localhost");
    config_.set("database_port", 9200);
    config_.set("database_index", "perception_events");

    config_.set("whisper_path", "models/whisper/ggml-small.bin");  
    config_.set("vad_path", "models/vad/silero_vad.onnx"); 

    config_.set("temp_directory", "temp");
    config_.set("log_directory", "logs");
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
            LOG_WARN(lastError_);
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
                
                // Construct full key with section prefix
                std::string fullKey = key;
                
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
        
        loaded_ = true;
        LOG_INFO("Configuration loaded successfully from: " + configPath);
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = std::string("Exception loading config: ") + e.what();
        LOG_ERROR(lastError_);
        return false;
    }
}

bool ConfigManager::SaveConfig(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ofstream file(configPath);
        if (!file.is_open()) {
            lastError_ = "Failed to open config file for writing: " + configPath;
            LOG_ERROR(lastError_);
            return false;
        }
        
        file << "# Perception Engine Configuration\n";
        file << "\n[embedding]\n";
        file << "model_path=" << config_.getString("model_path", "") << "\n";
        file << "tokenizer_path=" << config_.getString("tokenizer_path", "") << "\n";
        file << "python_executable=" << config_.getString("python_executable", "") << "\n";
        file << "chunk_document_script=" << config_.getString("chunk_document_script", "") << "\n";
        
        file << "\n[session_manager]\n";
        file << "enabled=" << (config_.getBool("session_manager_enabled", true) ? "true" : "false") << "\n";
        file << "compression_threshold=" << config_.getInt("compression_threshold", 100) << "\n";
        file << "similarity_threshold=" << config_.getDouble("similarity_threshold", 70.0) << "\n";
        file << "batch_size=" << config_.getInt("batch_size", 50) << "\n";
        
        file << "\n[database]\n";
        file << "type=" << config_.getString("database_type", "") << "\n";
        file << "host=" << config_.getString("database_host", "") << "\n";
        file << "port=" << config_.getInt("database_port", 9200) << "\n";
        file << "index=" << config_.getString("database_index", "") << "\n";
        
        file << "\n[models]\n";
        file << "whisper_path=" << config_.getString("whisper_path", "") << "\n";
        file << "vad_path=" << config_.getString("vad_path", "") << "\n";
        
        file << "\n[paths]\n";
        file << "temp_directory=" << config_.getString("temp_directory", "") << "\n";
        file << "log_directory=" << config_.getString("log_directory", "") << "\n";
        
        file.close();
        
        LOG_INFO("Configuration saved to: " + configPath);
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = std::string("Exception saving config: ") + e.what();
        LOG_ERROR(lastError_);
        return false;
    }
}

std::wstring ConfigManager::GetEmbeddingModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("model_path", "models/embedding/model_q4.onnx");
    std::string resolved = ResolvePath(path);
    return ConvertToWideString(resolved);
}

std::string ConfigManager::GetEmbeddingModelPathUtf8() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetEmbeddingModelPathUtf8_Unlocked();
}

std::string ConfigManager::GetEmbeddingModelPathUtf8_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    std::string path = config_.getString("model_path", "models/embedding/model_q4.onnx");
    return ResolvePath(path);
}

std::string ConfigManager::GetTokenizerPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetTokenizerPath_Unlocked();
}

std::string ConfigManager::GetTokenizerPath_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    std::string path = config_.getString("tokenizer_path", "models/embedding/tokenizer");
    return ResolvePath(path);
}

std::string ConfigManager::GetPythonExecutable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getString("python_executable", "python");
}

std::string ConfigManager::GetChunkDocumentScript() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("chunk_document_script", "scripts/chunk_document.py");
    return ResolvePath(path);
}

int ConfigManager::GetCompressionThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetCompressionThreshold_Unlocked();
}

int ConfigManager::GetCompressionThreshold_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    return config_.getInt("compression_threshold", 100);
}

float ConfigManager::GetSimilarityThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetSimilarityThreshold_Unlocked();
}

float ConfigManager::GetSimilarityThreshold_Unlocked() const {
    // Internal helper - assumes mutex is already locked
    return static_cast<float>(config_.getDouble("similarity_threshold", 70.0));
}

int ConfigManager::GetBatchSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getInt("batch_size", 50);
}

bool ConfigManager::IsSessionManagerEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getBool("session_manager_enabled", true);
}

std::string ConfigManager::GetDatabaseType() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getString("database_type", "elasticsearch");
}

std::string ConfigManager::GetDatabaseHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getString("database_host", "localhost");
}

int ConfigManager::GetDatabasePort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getInt("database_port", 9200);
}

std::string ConfigManager::GetDatabaseIndexName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.getString("database_index", "perception_events");
}

std::string ConfigManager::GetWhisperModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("whisper_path", "models/whisper/ggml-small.bin");
    return ResolvePath(path);
}

std::string ConfigManager::GetVADModelPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("vad_path", "models/vad/silero_vad.onnx");
    return ResolvePath(path);
}

std::string ConfigManager::GetTempDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("temp_directory", "temp");
    return ResolvePath(path);
}

std::string ConfigManager::GetLogDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = config_.getString("log_directory", "logs");
    return ResolvePath(path);
}

void ConfigManager::SetEmbeddingModelPath(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string utf8Path = ConvertToUtf8(path);
    config_.set("model_path", utf8Path);
}

void ConfigManager::SetTokenizerPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("tokenizer_path", path);
}

void ConfigManager::SetCompressionThreshold(int threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("compression_threshold", threshold);
}

void ConfigManager::SetSimilarityThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("similarity_threshold", threshold);
}

void ConfigManager::SetPythonExecutable(const std::string& execute) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.set("python_executable", execute);
}

bool ConfigManager::ValidateConfiguration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check critical paths exist - use unlocked internal methods
    std::string modelPath = GetEmbeddingModelPathUtf8_Unlocked();
    if (!std::filesystem::exists(modelPath)) {
        lastError_ = "Embedding model not found: " + modelPath;
        LOG_ERROR(lastError_);
        return false;
    }
    
    std::string tokenizerPath = GetTokenizerPath_Unlocked();
    if (!std::filesystem::exists(tokenizerPath)) {
        lastError_ = "Tokenizer not found: " + tokenizerPath;
        LOG_WARN(lastError_ + " (may be acceptable if not using document comparison)");
    }
    
    // Check thresholds are reasonable - use unlocked internal methods
    int compressionThreshold = GetCompressionThreshold_Unlocked();
    if (compressionThreshold < 1 || compressionThreshold > 10000) {
        lastError_ = "Invalid compression threshold: " + std::to_string(compressionThreshold);
        LOG_ERROR(lastError_);
        return false;
    }
    
    float similarityThreshold = GetSimilarityThreshold_Unlocked();
    if (similarityThreshold < 0.0f || similarityThreshold > 100.0f) {
        lastError_ = "Invalid similarity threshold: " + std::to_string(similarityThreshold);
        LOG_ERROR(lastError_);
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
