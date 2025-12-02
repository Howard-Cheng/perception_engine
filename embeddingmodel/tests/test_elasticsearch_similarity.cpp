/**
 * Test Elasticsearch Integration with E5 Similarity
 * 
 * This test demonstrates the complete workflow:
 * 1. Initialize Elasticsearch and create test index
 * 2. Insert 10 test events with realistic screen content
 * 3. Retrieve events sorted by timestamp
 * 4. Compare consecutive events using E5 embedding model
 * 5. Update similar_screen_content and compressed fields
 * 
 * Compile:
 *   cmake -B build -G "Visual Studio 17 2022" -A x64
 *   cmake --build build --config Release
 * 
 * Run:
 *   cd build\bin\Release
 *   test_elasticsearch_similarity.exe
 */

#include "E5EmbeddingDLL.h"
#include "pe_base/config_manager.h"
#include "pe_base/logger.h"
#include "ElasticsearchClient.h"
#include "DatabaseTypes.h"
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>
#include <memory>
#include <filesystem>

using namespace database;

// Helper: Generate test screen content (realistic scenarios)
struct TestContent {
    std::string content;
    std::string description;
};

std::vector<TestContent> GenerateTestContents() {
    return {
        // Group 1: Similar ML/AI content (events 0-2)
        {
           R"(
            Machine learning is a subset of artificial intelligence that focuses on the development
            of algorithms and statistical models that enable computer systems to improve their
            performance on a specific task through experience. Deep learning, a subfield of machine
            learning, uses neural networks with multiple layers to progressively extract higher-level
            features from raw input. These deep neural networks have revolutionized computer vision,
            natural language processing, and speech recognition. The success of deep learning can be
            attributed to the availability of large datasets, powerful GPUs for parallel computation,
            and innovations in network architectures such as convolutional neural networks (CNNs) and
            recurrent neural networks (RNNs). Modern applications of deep learning include image
            classification, object detection, machine translation, and autonomous driving systems.
            )",
            "ML/AI Overview 1"
        },
        {
            R"(
            Deep learning represents a powerful approach within the field of artificial intelligence,
            utilizing artificial neural networks with multiple hidden layers to learn complex patterns
            from data. This technology has achieved remarkable success in various domains including
            computer vision, where convolutional neural networks excel at image recognition tasks,
            and natural language processing, where transformer architectures have enabled breakthrough
            performance in language understanding. The recent advances in deep learning are driven by
            the combination of massive datasets, GPU-accelerated computing, and novel network designs.
            Applications range from facial recognition and medical image analysis to language translation
            and self-driving cars, demonstrating the versatility and power of these techniques.
            )",
            "ML/AI Overview 2"
        },
        {
            "Artificial intelligence and machine learning have transformed how we process information. "
            "Neural networks, especially deep learning models, can automatically discover patterns in "
            "data without explicit programming. Applications span from autonomous vehicles to medical "
            "diagnosis, demonstrating the broad impact of these technologies.",
            "ML/AI Overview 3"
        },
        
        // Group 2: Different topic - Web Development (event 3)
        {
            "Web development involves creating websites and web applications using HTML, CSS, and JavaScript. "
            "Modern frameworks like React, Vue, and Angular have simplified building interactive user interfaces. "
            "Backend development uses technologies like Node.js, Python Django, or Java Spring to handle "
            "server-side logic and database interactions.",
            "Web Development"
        },
        
        // Group 3: Similar Data Science content (events 4-5)
        {
            "Data science combines statistics, programming, and domain knowledge to extract insights from data. "
            "Python libraries like pandas, NumPy, and scikit-learn are essential tools for data manipulation "
            "and analysis. Data scientists use visualization tools to communicate findings and build predictive "
            "models using machine learning algorithms.",
            "Data Science 1"
        },
        {
            "The field of data science involves collecting, cleaning, and analyzing large datasets to uncover "
            "patterns and trends. Statistical methods and machine learning techniques help make data-driven "
            "decisions. Tools like Jupyter notebooks and pandas facilitate exploratory data analysis and "
            "model development.",
            "Data Science 2"
        },
        
        // Group 4: Different topic - Classical Music (event 6)
        {
            "Classical music has a rich history spanning centuries, with composers like Mozart, Beethoven, "
            "and Bach creating timeless masterpieces. The symphony orchestra, with its diverse instrumentation, "
            "produces complex harmonies and melodies. Classical music theory covers harmony, counterpoint, "
            "and orchestration techniques.",
            "Classical Music"
        },
        
        // Group 5: Similar Cloud Computing content (events 7-8)
        {
            "Cloud computing delivers computing services over the internet, including storage, processing, "
            "and networking. Major providers like AWS, Azure, and Google Cloud offer scalable infrastructure "
            "as a service (IaaS), platform as a service (PaaS), and software as a service (SaaS). Cloud "
            "architecture enables businesses to scale resources on demand.",
            "Cloud Computing 1"
        },
        {
            "The cloud computing paradigm allows organizations to access computing resources via the internet "
            "without maintaining physical infrastructure. Services range from virtual machines and storage to "
            "managed databases and serverless functions. Leading cloud platforms provide global data centers "
            "for high availability and disaster recovery.",
            "Cloud Computing 2"
        },
        
        // Group 6: Different topic - Sports (event 9)
        {
            "Professional sports encompass a wide range of athletic competitions, from team sports like "
            "football and basketball to individual events like tennis and swimming. Athletes train intensively "
            "to improve physical fitness, technique, and mental preparation. Modern sports science uses "
            "technology to optimize performance and prevent injuries.",
            "Professional Sports"
        }
    };
}

// Helper: Create RawEvent with test data
RawEvent CreateTestEvent(int index, const TestContent& content, std::time_t baseTime) {
    RawEvent event;
    
    // Generate unique event ID
    event.eventId = "test_event_" + std::string(std::to_string(1000 + index));
    
    // Set timestamps (5 minutes apart)
    event.timestamp = baseTime + (index * 300);  // 300 seconds = 5 minutes
    event.createdAt = event.timestamp;
    
    // Set basic fields
    event.deviceId = "test_device_001";
    event.appName = "TestApp";
    event.windowTitle = content.description;
    event.screenContent = content.content;
    
    // Initialize as uncompressed
    event.compressed = false;
    event.interactionCount = 1;
    event.dwellTimeSeconds = 60;
    
    return event;
}

std::string GetExePath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exePathStr(exePath);
    size_t lastSlash = exePathStr.find_last_of("\\/");
    std::string exe_dir = exePathStr.substr(0, lastSlash);
    return exe_dir;
}

// Main test function
int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "  Elasticsearch + E5 Similarity Integration Test" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << std::endl;
    
    // Initialize Logger
    std::filesystem::path log_path = "";
    if (auto* p_appdata = getenv("APPDATA")) {
        log_path =
            std::filesystem::path(p_appdata) / "Lenovo" / "PerceptionEngine" / "logs";
    }
    pe_base::LogWriter::SetLogFilePrefix(
        (log_path / "test_es_similarity").generic_string());
    PE_INFO("Test started");

    std::string config_path = GetExePath() + "\\config.ini";
    // Load configuration
    std::cout << "[1/6] Loading configuration..." << std::endl;
    if (!pe_base::ConfigManager::GetInstance().LoadConfig(config_path)) {
        std::cerr << "    X Failed to load config.json" << std::endl;
        PE_ERROR("Failed to load configuration");
        return 1;
    }
    std::cout << "    V Configuration loaded" << std::endl;
    PE_INFO("Configuration loaded successfully");
    
    // Initialize E5 model
    std::cout << "\n[2/6] Initializing E5 embedding model..." << std::endl;
    std::wstring modelPath = pe_base::ConfigManager::GetInstance().GetEmbeddingModelPath();
    std::wcout << L"    Model path: " << modelPath << std::endl;
    
    int result = E5_Initialize(modelPath.c_str());
    if (result != 0) {
        std::cerr << "    X Initialization failed: " << E5_GetLastError() << std::endl;
        PE_ERROR(std::string("E5 initialization failed: ") + E5_GetLastError());
        return 1;
    }
    std::cout << "    V Model loaded (dim=" << E5_GetEmbeddingDimension() << ")" << std::endl;
    PE_INFO("E5 model initialized successfully");
    
    // Initialize Elasticsearch client
    std::cout << "\n[3/6] Initializing Elasticsearch client..." << std::endl;
    std::string esUrl = "http://localhost:9200";  // Use default URL
    std::cout << "    ES URL: " << esUrl << std::endl;
    
    auto esClient = std::make_shared<ElasticsearchClient>(esUrl);
    
    // Test connection
    if (!esClient->testConnection()) {
        std::cerr << "    X Failed to connect to Elasticsearch" << std::endl;
        std::cerr << "    Make sure Elasticsearch is running at: " << esUrl << std::endl;
        PE_ERROR("Failed to connect to Elasticsearch");
        E5_Cleanup();
        return 1;
    }
    std::cout << "    V Connected to Elasticsearch" << std::endl;
    PE_INFO("Connected to Elasticsearch");
    
    // Create test index
    std::string testIndex = "test_similarity_" + std::to_string(std::time(nullptr));
    std::cout << "    Creating test index: " << testIndex << std::endl;
    
    if (!esClient->initializeCollection(testIndex)) {
        std::cerr << "    X Failed to create test index" << std::endl;
        PE_ERROR("Failed to create test index");
        E5_Cleanup();
        return 1;
    }
    std::cout << "    V Test index created" << std::endl;
    PE_INFO(std::string("Test index created: ") + testIndex);
    
    // Insert test data
    std::cout << "\n[4/6] Inserting test data (10 events)..." << std::endl;
    std::vector<TestContent> testContents = GenerateTestContents();
    std::time_t baseTime = std::time(nullptr) - 3600;  // Start 1 hour ago
    
    std::vector<RawEvent> testEvents;
    for (size_t i = 0; i < testContents.size(); i++) {
        RawEvent event = CreateTestEvent(i, testContents[i], baseTime);
        testEvents.push_back(event);
        
        std::string eventId = esClient->indexDocument(testIndex, event);
        if (eventId.empty()) {
            std::cerr << "    X Failed to insert event " << i << std::endl;
            PE_ERROR(std::string("Failed to insert event ") + std::to_string(i));
        } else {
            std::cout << "    V Event " << i << " inserted: " << testContents[i].description << std::endl;
            PE_INFO(std::string("Event inserted: ") + eventId);
        }
        
        // Small delay to ensure ordering
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Refresh index to make data searchable
    std::cout << "    Refreshing index..." << std::endl;
    esClient->refreshCollection(testIndex);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "    V All events inserted" << std::endl;
    
    // Retrieve and process events
    std::cout << "\n[5/6] Processing events with E5 similarity..." << std::endl;
    
    // Query all uncompressed events sorted by timestamp
    std::string query = R"({
        "query": {
            "term": {
                "compressed": false
            }
        },
        "sort": [
            {
                "timestamp": {
                    "order": "asc"
                }
            }
        ],
        "size": 100
    })";
    
    SearchResult searchResult = esClient->search(testIndex, query, 0, 100);
    std::cout << "    Retrieved " << searchResult.events.size() << " uncompressed events" << std::endl;
    PE_INFO(std::string("Retrieved ") + std::to_string(searchResult.events.size()) + " events");
    
    if (searchResult.events.empty()) {
        std::cerr << "    X No events found" << std::endl;
        PE_ERROR("No events found");
        esClient->deleteCollection(testIndex);
        E5_Cleanup();
        return 1;
    }
    
    // Compare consecutive events and update
    int similarPairs = 0;
    int dissimilarPairs = 0;
    float similarityThreshold = 60.0f;
    
    for (size_t i = 0; i < searchResult.events.size() - 1; i++) {
        const auto& event1 = searchResult.events[i];
        const auto& event2 = searchResult.events[i + 1];
        
        std::cout << "\n    --- Comparing Event " << i << " vs Event " << (i + 1) << " ---" << std::endl;
        std::cout << "    Event " << i << ": " << event1.windowTitle.value_or("N/A") << std::endl;
        std::cout << "    Event " << (i + 1) << ": " << event2.windowTitle.value_or("N/A") << std::endl;
        
        // Get screen content
        std::string content1 = event1.screenContent.value_or("");
        std::string content2 = event2.screenContent.value_or("");
        
        if (content1.empty() || content2.empty()) {
            std::cout << "    X Skipping: empty content" << std::endl;
            continue;
        }
        
        // Compare using E5
        float similarity;
        result = E5_CompareDocumentsSimple(content1.c_str(), content2.c_str(), &similarity);
        
        if (result != 0) {
            std::cerr << "    X Comparison failed: " << E5_GetLastError() << std::endl;
            PE_ERROR(std::string("Comparison failed: ") + E5_GetLastError());
            continue;
        }
        
        std::cout << "    Similarity: " << similarity << "%" << std::endl;
        
        if (similarity > similarityThreshold) {
            std::cout << "    V Similar (threshold: " << similarityThreshold << "%)" << std::endl;
            similarPairs++;
            
            // Get similar chunks
            E5_SimilarChunkPair chunks[3];
            int numChunks = 0;
            
            if (E5_GetSimilarChunks(chunks, 3, &numChunks) == 0 && numChunks > 0) {
                // Build similarity summary for event1 (previous)
                std::ostringstream summary1;
                summary1 << "Similarity: " << similarity << "%\n";
                summary1 << "Top " << numChunks << " matching sections:\n\n";
                
                for (int j = 0; j < numChunks; j++) {
                    summary1 << (j + 1) << ": " << std::string(chunks[j].text_A) << ".\n\n";
                }
                
                // Build similarity summary for event2 (current)
                std::ostringstream summary2;
                summary2 << "Similarity: " << similarity << "%\n";
                summary2 << "Top " << numChunks << " matching sections:\n\n";
                
                for (int j = 0; j < numChunks; j++) {
                    summary2 << (j + 1) << ": " << std::string(chunks[j].text_B) << ".\n\n";
                }
                
                // Update event1 with similarity info
                std::vector<std::string> event1Ids = {event1.eventId};
                bool success1 = esClient->markEventsAsCompressedWithSimilarity(
                    testIndex,
                    event1Ids,
                    "session_similar",
                    summary1.str()
                );
                
                // Update event2 with similarity info
                std::vector<std::string> event2Ids = {event2.eventId};
                bool success2 = esClient->markEventsAsCompressedWithSimilarity(
                    testIndex,
                    event2Ids,
                    "session_similar",
                    summary2.str()
                );
                
                if (success1 && success2) {
                    std::cout << "    V Updated both events with similarity info" << std::endl;
                    PE_INFO(std::string("Updated events ") + event1.eventId + " and " + event2.eventId);
                    
                    // Display first chunk pair
                    std::cout << "    Top match (score: " << chunks[0].similarity_score << "):" << std::endl;
                    std::cout << "      A: " << std::string(chunks[0].text_A).substr(0, 80) << "..." << std::endl;
                    std::cout << "      B: " << std::string(chunks[0].text_B).substr(0, 80) << "..." << std::endl;
                } else {
                    std::cerr << "    X Failed to update events" << std::endl;
                }
            }
            
            // Clear comparison results for next iteration
            E5_ClearComparisonResults();
            
        } else {
            std::cout << "    X Not similar" << std::endl;
            dissimilarPairs++;
            
            // Mark as compressed without similarity info
            std::vector<std::string> eventIds = {event1.eventId};
            esClient->markEventsAsCompressed(testIndex, eventIds, "session_dissimilar");
        }
    }
    
    // Mark last event
    if (!searchResult.events.empty()) {
        const auto& lastEvent = searchResult.events.back();
        std::vector<std::string> lastIds = {lastEvent.eventId};
        esClient->markEventsAsCompressed(testIndex, lastIds, "session_last");
    }
    
    // Refresh to make updates visible
    esClient->refreshCollection(testIndex);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Summary
    std::cout << "\n[6/6] Test Summary" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << "  Total events: " << searchResult.events.size() << std::endl;
    std::cout << "  Similar pairs: " << similarPairs << std::endl;
    std::cout << "  Dissimilar pairs: " << dissimilarPairs << std::endl;
    std::cout << "  Similarity threshold: " << similarityThreshold << "%" << std::endl;
    
    // Verify compressed count
    int compressedCount = 0;
    std::string verifyQuery = R"({
        "query": {
            "term": {
                "compressed": true
            }
        },
        "size": 0
    })";
    
    SearchResult verifyResult = esClient->search(testIndex, verifyQuery, 0, 0);
    compressedCount = verifyResult.totalHits;
    
    std::cout << "  Compressed events: " << compressedCount << std::endl;
    
    // Query one event to show similarity content
    std::cout << "\n  Sample Event with Similarity:" << std::endl;
    std::string sampleQuery = R"({
        "query": {
            "bool": {
                "must": [
                    {"term": {"compressed": true}},
                    {"exists": {"field": "similar_screen_content"}}
                ]
            }
        },
        "size": 1
    })";
    
    SearchResult sampleResult = esClient->search(testIndex, sampleQuery, 0, 1);
    if (!sampleResult.events.empty()) {
        const auto& sampleEvent = sampleResult.events[0];
        std::cout << "    Event ID: " << sampleEvent.eventId << std::endl;
        std::cout << "    Window: " << sampleEvent.windowTitle.value_or("N/A") << std::endl;
        std::cout << "    Compressed: " << (sampleEvent.compressed ? "Yes" : "No") << std::endl;
        std::cout << "    Session ID: " << sampleEvent.sessionId.value_or("N/A") << std::endl;
        
        if (sampleEvent.similarScreenContent.has_value()) {
            std::string simContent = sampleEvent.similarScreenContent.value();
            std::cout << "    Similarity Content (first 200 chars):" << std::endl;
            std::cout << "      " << simContent.substr(0, 200) << "..." << std::endl;
        }
    }
    
    std::cout << "\n  Test index: " << testIndex << std::endl;
    std::cout << "  (Index will be kept for inspection)" << std::endl;
    
    std::cout << "\n===============================================" << std::endl;
    std::cout << "  Test Complete!" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    // Cleanup
    PE_INFO("Test completed successfully");
    E5_Cleanup();
    
    std::cout << "\nNote: To delete the test index, run:" << std::endl;
    std::cout << "  curl -X DELETE http://localhost:9200/" << testIndex << std::endl;
    std::cout << std::endl;
    
    return 0;
}
