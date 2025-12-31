/**
 * @file example_usage.cpp
 * @brief Example usage of LLM Summary C++ module
 */

#include "LLMClient.h"
#include <iostream>
#include <iomanip>

using namespace perception;

// ============================================================================
// Example Functions
// ============================================================================

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(50, '=') << "\n";
}

void example1_BasicGeneration() {
    printSeparator("Example 1: Basic Text Generation");
    
    LLMConfig config;
    config.model_path = getDefaultModelPath();
    
    LLMClient client(config);
    
    std::string prompt = "Please explain what artificial intelligence is in simple terms.";
    std::string response = client.generate(prompt, 0.7f, 200);
    
    std::cout << "Prompt: " << prompt << "\n";
    std::cout << "Response: " << response << "\n";
}

void example2_Summarization() {
    printSeparator("Example 2: Text Summarization");
    
    LLMClient client;
    
    std::string long_text = R"(
    Artificial intelligence (AI) is the simulation of human intelligence processes 
    by machines, especially computer systems. These processes include learning, 
    reasoning, and self-correction. Particular applications of AI include expert 
    systems, natural language processing, speech recognition and machine vision.
    AI programming focuses on three cognitive skills: learning, reasoning and 
    self-correction. Learning processes focus on acquiring data and creating rules 
    for how to turn the data into actionable information.
    )";
    
    std::string summary = client.summarize(long_text, 100);
    
    std::cout << "Original text length: " << long_text.length() << " characters\n";
    std::cout << "Summary: " << summary << "\n";
}

void example3_QuestionAnswering() {
    printSeparator("Example 3: Question Answering");
    
    LLMClient client;
    
    std::string context = R"(
    Machine learning is a subset of artificial intelligence that enables systems 
    to learn and improve from experience without being explicitly programmed. 
    It focuses on developing computer programs that can access data and use it 
    to learn for themselves.
    )";
    
    std::string question = "What is machine learning?";
    std::string answer = client.answerQuestion(question, context);
    
    std::cout << "Question: " << question << "\n";
    std::cout << "Answer: " << answer << "\n";
}

void example4_Chat() {
    printSeparator("Example 4: Multi-turn Conversation");
    
    LLMClient client;
    
    std::vector<ChatMessage> messages = {
        {"user", "What is Python?"},
        {"assistant", "Python is a high-level programming language."},
        {"user", "What are its main advantages?"}
    };
    
    std::string response = client.chat(messages, 0.7f, 200);
    
    std::cout << "Conversation:\n";
    for (const auto& msg : messages) {
        std::cout << msg.role << ": " << msg.content << "\n";
    }
    std::cout << "Assistant: " << response << "\n";
}

void example5_ReadSQLite() {
    printSeparator("Example 5: Read from SQLite Database");
    
    LLMClient client;
    
    std::filesystem::path sqlite_path = "../vectordb/raw_events.db";
    
    if (!std::filesystem::exists(sqlite_path)) {
        std::cout << "Database not found: " << sqlite_path << "\n";
        std::cout << "Please update the path to your actual database file.\n";
        return;
    }
    
    try {
        auto records = client.readFromDatabase(sqlite_path, std::nullopt, 3);
        
        std::cout << "Read " << records.size() << " records from SQLite\n";
        for (size_t i = 0; i < records.size(); ++i) {
            std::cout << "\nRecord " << (i + 1) << ":\n";
            std::cout << "  ID: " << records[i].id << "\n";
            std::cout << "  Type: " << records[i].type << "\n";
            std::cout << "  Title: " << records[i].title << "\n";
            std::cout << "  Timestamp: " << records[i].timestamp << "\n";
            
            if (!records[i].summary.empty()) {
                std::string preview = records[i].summary.substr(0, 100);
                std::cout << "  Content: " << preview << "...\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error reading database: " << e.what() << "\n";
    }
}

void example6_ProcessDatabase() {
    printSeparator("Example 6: Process Database Content with LLM");
    
    LLMClient client;
    
    std::filesystem::path db_path = "../vectordb/raw_events.db";
    
    if (!std::filesystem::exists(db_path)) {
        std::cout << "Database not found: " << db_path << "\n";
        return;
    }
    
    try {
        auto records = client.processDatabaseContent(db_path, "summarize", 2);
        
        std::cout << "Processed " << records.size() << " records\n";
        for (size_t i = 0; i < records.size(); ++i) {
            std::cout << "\nRecord " << (i + 1) << ":\n";
            std::cout << "  ID: " << records[i].id << "\n";
            
            auto it = records[i].metadata.find("llm_summary");
            if (it != records[i].metadata.end()) {
                std::cout << "  LLM Summary: " << it->second << "\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error processing database: " << e.what() << "\n";
    }
}

void example7_GPUSettings() {
    printSeparator("Example 7: GPU Acceleration Settings");
    
    // CPU only
    std::cout << "Creating CPU-only client...\n";
    LLMConfig cpu_config;
    cpu_config.n_gpu_layers = 0;
    LLMClient client_cpu(cpu_config);
    
    // GPU acceleration
    std::cout << "Creating GPU-accelerated client (35 layers)...\n";
    LLMConfig gpu_config;
    gpu_config.n_gpu_layers = 35;
    LLMClient client_gpu(gpu_config);
    
    std::cout << "Clients created successfully!\n";
}

// ============================================================================
// Main Function
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "LLM Summary Module (C++) - Example Usage\n";
    std::cout << std::string(50, '=') << "\n";
    
    // Check model existence
    auto model_path = getDefaultModelPath();
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "\n⚠️  Warning: Model file not found at " << model_path << "\n";
        std::cerr << "Please ensure the model file is downloaded and placed correctly.\n";
        return 1;
    }
    
    std::cout << "Model found: " << model_path << "\n";
    
    try {
        // Run examples
        example1_BasicGeneration();
        example2_Summarization();
        example3_QuestionAnswering();
        example4_Chat();
        
        // Database examples (may fail if databases don't exist)
        example5_ReadSQLite();
        example6_ProcessDatabase();
        
        // Configuration examples
        example7_GPUSettings();
        
        printSeparator("Examples completed!");
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error running examples: " << e.what() << "\n";
        std::cerr << "Please check your installation and model path.\n";
        return 1;
    }
    
    return 0;
}
