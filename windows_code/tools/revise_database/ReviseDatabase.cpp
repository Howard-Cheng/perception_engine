/**
 * @file ReviseDatabase.cpp
 * @brief Utility to reset summarized flag in Elasticsearch
 * 
 * This tool resets the 'summarized' field to false for all documents
 * in the Elasticsearch perception_context index.
 */

#include "DatabaseClientFactory.h"
#include "IDatabaseClient.h"
#include "DatabaseTypes.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

using json = nlohmann::json;

/**
 * Configuration for the database revision tool
 */
struct ReviseConfig {
    std::string es_host = "localhost";
    int es_port = 9200;
    std::string es_index = "perception_context";
    bool dry_run = false;
    int batch_size = 100;
};

/**
 * Parse command line arguments
 */
ReviseConfig parseArguments(int argc, char* argv[]) {
    ReviseConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--host" && i + 1 < argc) {
            config.es_host = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc) {
            config.es_port = std::stoi(argv[++i]);
        }
        else if (arg == "--index" && i + 1 < argc) {
            config.es_index = argv[++i];
        }
        else if (arg == "--dry-run") {
            config.dry_run = true;
        }
        else if (arg == "--batch-size" && i + 1 < argc) {
            config.batch_size = std::stoi(argv[++i]);
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: ReviseDatabase [options]\n\n"
                      << "Options:\n"
                      << "  --host HOST        Elasticsearch host (default: localhost)\n"
                      << "  --port PORT        Elasticsearch port (default: 9200)\n"
                      << "  --index INDEX      Index name (default: perception_context)\n"
                      << "  --batch-size N     Batch size for updates (default: 100)\n"
                      << "  --dry-run          Show what would be updated without updating\n"
                      << "  --help, -h         Show this help message\n\n"
                      << "Examples:\n"
                      << "  ReviseDatabase\n"
                      << "  ReviseDatabase --dry-run\n"
                      << "  ReviseDatabase --host 192.168.1.100 --port 9200\n"
                      << "  ReviseDatabase --index my_custom_index\n";
            exit(0);
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Use --help for usage information\n";
            exit(1);
        }
    }
    
    return config;
}

/**
 * Count documents by summarized status
 */
struct DocumentCounts {
    size_t total = 0;
    size_t summarized_true = 0;
    size_t summarized_false = 0;
    size_t missing = 0;
};

DocumentCounts countDocuments(
    database::IDatabaseClient* client,
    const std::string& index) {
    
    DocumentCounts counts;
    
    try {
        // Count total documents
        json total_query = {
            {"query", {{"match_all", json::object()}}}
        };
        auto total_result = client->search(index, total_query.dump(), 0, 1);
        counts.total = total_result.totalHits;
        
        // Count summarized=true
        json true_query = {
            {"query", {{"term", {{"summarized", true}}}}}
        };
        auto true_result = client->search(index, true_query.dump(), 0, 1);
        counts.summarized_true = true_result.totalHits;
        
        // Count summarized=false
        json false_query = {
            {"query", {{"term", {{"summarized", false}}}}}
        };
        auto false_result = client->search(index, false_query.dump(), 0, 1);
        counts.summarized_false = false_result.totalHits;
        
        // Calculate missing
        counts.missing = counts.total - counts.summarized_true - counts.summarized_false;
        
    } catch (const std::exception& e) {
        std::cerr << "Error counting documents: " << e.what() << std::endl;
    }
    
    return counts;
}

/**
 * Reset all documents to summarized=false
 */
size_t resetAllToFalse(
    database::IDatabaseClient* client,
    const std::string& index,
    bool dry_run) {
    
    size_t updated = 0;
    
    try {
        // Get all documents that need updating (summarized=true or missing field)
        json query = {
            {"size", 1000},  // Process in batches
            {"query", {
                {"bool", {
                    {"should", json::array({
                        {{"term", {{"summarized", true}}}},
                        {{"bool", {{"must_not", {{"exists", {{"field", "summarized"}}}}}}}}
                    })}
                }}
            }}
        };
        
        bool has_more = true;
        size_t from = 0;
        
        while (has_more) {
            auto result = client->search(index, query.dump(), from, 1000);
            
            if (result.events.empty()) {
                break;
            }
            
            std::cout << "Processing batch of " << result.events.size() << " documents..." << std::endl;
            
            for (const auto& event : result.events) {
                if (!dry_run) {
                    // Build update document
                    json update_doc = {
                        {"doc", {
                            {"summarized", false}
                        }}
                    };
                    
                    // Update the document
                    bool success = client->updateDocument(
                        index,
                        event.eventId,
                        update_doc.dump()
                    );
                    
                    if (success) {
                        updated++;
                    } else {
                        std::cerr << "Failed to update document: " << event.eventId << std::endl;
                    }
                } else {
                    updated++;  // Count for dry run
                }
            }
            
            // Check if there are more documents
            if (result.events.size() < 1000) {
                has_more = false;
            } else {
                from += 1000;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error during update: " << e.what() << std::endl;
    }
    
    return updated;
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    std::cout << "========================================\n"
              << " ReviseDatabase - Reset Summarized Flag\n"
              << "========================================\n\n";
    
    // Parse arguments
    ReviseConfig config = parseArguments(argc, argv);
    
    // Display configuration
    std::cout << "Configuration:\n"
              << "  Host: " << config.es_host << "\n"
              << "  Port: " << config.es_port << "\n"
              << "  Index: " << config.es_index << "\n"
              << "  Batch size: " << config.batch_size << "\n"
              << "  Dry run: " << (config.dry_run ? "Yes" : "No") << "\n\n";
    
    try {
        // Create Elasticsearch client
        std::string es_url = "http://" + config.es_host + ":" + std::to_string(config.es_port);
        std::cout << "Connecting to Elasticsearch at " << es_url << "...\n";
        
        auto client = database::DatabaseClientFactory::createElasticsearch(es_url);
        std::cout << "? Connected to Elasticsearch\n\n";
        
        // Count current documents
        std::cout << "Current status:\n";
        auto counts = countDocuments(client.get(), config.es_index);
        std::cout << "  Total documents: " << counts.total << "\n"
                  << "  Summarized=true: " << counts.summarized_true << "\n"
                  << "  Summarized=false: " << counts.summarized_false << "\n"
                  << "  Missing field: " << counts.missing << "\n\n";
        
        if (counts.total == 0) {
            std::cout << "? No documents found in index\n";
            return 0;
        }
        
        // Calculate documents to update
        size_t to_update = counts.summarized_true + counts.missing;
        
        if (to_update == 0) {
            std::cout << "? All documents already have summarized=false\n";
            return 0;
        }
        
        std::cout << "Documents to update: " << to_update << "\n\n";
        
        if (config.dry_run) {
            std::cout << "[DRY RUN] Would update " << to_update << " documents\n";
            return 0;
        }
        
        // Ask for confirmation
        std::cout << "This will update all documents in the index.\n"
                  << "Continue? (yes/no): ";
        std::string response;
        std::getline(std::cin, response);
        
        if (response != "yes" && response != "y") {
            std::cout << "? Operation cancelled\n";
            return 0;
        }
        
        // Perform the update
        std::cout << "\nUpdating documents...\n";
        size_t updated = resetAllToFalse(client.get(), config.es_index, config.dry_run);
        std::cout << "? Successfully updated " << updated << " documents\n\n";
        
        // Verify result
        std::cout << "New status:\n";
        auto new_counts = countDocuments(client.get(), config.es_index);
        std::cout << "  Total documents: " << new_counts.total << "\n"
                  << "  Summarized=true: " << new_counts.summarized_true << "\n"
                  << "  Summarized=false: " << new_counts.summarized_false << "\n"
                  << "  Missing field: " << new_counts.missing << "\n\n";
        
        std::cout << "? Operation completed\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "? Error: " << e.what() << std::endl;
        return 1;
    }
}
