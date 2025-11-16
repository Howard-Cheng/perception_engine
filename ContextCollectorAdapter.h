// ContextCollectorAdapter.h
// 适配器模式 - 允许在旧/新架构间无缝切换

#pragma once

#include "ContextCollector.h"
#include "ContextCollectorRefactored.h"
#include <memory>

/**
 * @brief ContextCollector 适配器
 * 
 * 提供统一接口,可以在运行时选择使用旧的或新的 ContextCollector
 * 用于渐进式迁移,保证系统稳定性
 */
class ContextCollectorAdapter {
public:
    enum class Mode {
        LEGACY,      // 使用旧的 ContextCollector
        REFACTORED   // 使用新的 ContextCollectorRefactored
    };
    
    explicit ContextCollectorAdapter(Mode mode = Mode::REFACTORED) 
        : mode_(mode) 
    {
        if (mode_ == Mode::LEGACY) {
            legacyCollector_ = std::make_unique<ContextCollector>();
        } else {
            refactoredCollector_ = std::make_unique<ContextCollectorRefactored>();
        }
    }
    
    ~ContextCollectorAdapter() = default;
    
    // Unified API (委托给相应的实现)
    
    Json CollectCurrentContext() {
        if (mode_ == Mode::LEGACY) {
            return legacyCollector_->CollectCurrentContext();
        } else {
            return refactoredCollector_->CollectCurrentContext();
        }
    }
    
    void StartPeriodicUpdate() {
        if (mode_ == Mode::LEGACY) {
            legacyCollector_->StartPeriodicUpdate();
        } else {
            refactoredCollector_->StartPeriodicUpdate();
        }
    }
    
    void StopPeriodicUpdate() {
        if (mode_ == Mode::LEGACY) {
            legacyCollector_->StopPeriodicUpdate();
        } else {
            refactoredCollector_->StopPeriodicUpdate();
        }
    }
    
    void UpdateVoiceContext(const std::string& transcription, float latencyMs = 0.0f) {
        if (mode_ == Mode::LEGACY) {
            legacyCollector_->UpdateVoiceContext(transcription, latencyMs);
        } else {
            refactoredCollector_->UpdateVoiceContext(transcription, latencyMs);
        }
    }
    
    void UpdateCameraContext(const std::string& description, float latencyMs = 0.0f) {
        if (mode_ == Mode::LEGACY) {
            legacyCollector_->UpdateCameraContext(description, latencyMs);
        } else {
            refactoredCollector_->UpdateCameraContext(description, latencyMs);
        }
    }
    
    bool InitializeElasticsearch(const std::string& esHost = "http://localhost:9200",
                                 const std::string& indexName = "perception_context") {
        if (mode_ == Mode::LEGACY) {
            return legacyCollector_->InitializeElasticsearch(esHost, indexName);
        } else {
            return refactoredCollector_->InitializeDatabase(esHost, indexName);
        }
    }
    
    void ShutdownElasticsearch() {
        if (mode_ == Mode::LEGACY) {
            legacyCollector_->ShutdownElasticsearch();
        } else {
            refactoredCollector_->ShutdownDatabase();
        }
    }
    
    Json GetESDBData(const std::string& keyword,
                    std::time_t startTime,
                    std::time_t endTime,
                    int maxResults = 100) {
        if (mode_ == Mode::LEGACY) {
            return legacyCollector_->GetESDBData(keyword, startTime, endTime, maxResults);
        } else {
            return refactoredCollector_->GetESDBData(keyword, startTime, endTime, maxResults);
        }
    }
    
    bool IsElasticsearchAvailable() const {
        if (mode_ == Mode::LEGACY) {
            return legacyCollector_->IsElasticsearchAvailable();
        } else {
            return refactoredCollector_->IsElasticsearchAvailable();
        }
    }
    
    Mode getMode() const { return mode_; }
    
    std::string getModeString() const {
        return mode_ == Mode::LEGACY ? "Legacy" : "Refactored";
    }
    
private:
    Mode mode_;
    std::unique_ptr<ContextCollector> legacyCollector_;
    std::unique_ptr<ContextCollectorRefactored> refactoredCollector_;
};
