//
// main.cpp
// Real-time Speech Recognition Module Test Program
//

#include "SpeechRecognition.h"
#include "SpeechHelper.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace SpeechRecognitionModule;

// Test 1: Using embedded model (recommended, similar to C# version)
// NOTE: Embedded speech requires special Microsoft.CognitiveServices.Speech.extension.embedded.*.dll files
// These are NOT included in the standard NuGet package.
// To obtain them, visit: https://aka.ms/embedded-speech
void TestEmbeddedRecognition()
{
    std::cout << "\n=== Test 1: Embedded Speech Recognition ===" << std::endl;
    std::cout << "\n[WARNING] Embedded speech requires special extension DLLs from Microsoft." << std::endl;
    std::cout << "These are NOT included in the standard NuGet package." << std::endl;
    std::cout << "To obtain embedded speech access, visit: https://aka.ms/embedded-speech" << std::endl;
    std::cout << "\nIf you don't have the embedded SDK, please use Test 2 (Online Recognition) instead.\n" << std::endl;

    // Set embedded model path - use Lenovo Qira preloaded model
    // Primary path
    SpeechHelper::SpeechRecognitionModelPath = "C:\\Program Files\\Lenovo\\Lenovo Qira\\Preloaded";
    SpeechHelper::SpeechRecognitionModelPathName = "ms_stt_speechmodel";
    SpeechHelper::SpeechRecognitionModelLocale = "en-US";
    
    // Backup path (optional)
    SpeechHelper::SpeechRecognitionModelPath1 = "C:\\Program Files\\Lenovo\\Lenovo Qira\\Preloaded";
    SpeechHelper::SpeechRecognitionModelPathName1 = "ms_stt_speechmodel";
    
    // TTS path
    SpeechHelper::SpeechSynthesisVoicePath = "C:\\Program Files\\Lenovo\\Lenovo Qira\\Preloaded";
    SpeechHelper::SpeechSynthesisVoicePathName = "ms_tts_ttsmodel";
    SpeechHelper::SpeechSynthesisVoiceLocale = "En-US";
    SpeechHelper::SpeechSynthesisVoiceName = "Aria";

    // Verify environment
    if (!SpeechHelper::EnvironmentVerify())
    {
        std::cerr << "Environment verification failed" << std::endl;
        return;
    }

    std::cout << "Model path: " << SpeechHelper::GetSpeechRecognitionModelFullPath() << std::endl;
    std::cout << "Model name: " << SpeechHelper::SpeechRecognitionModelName << std::endl;

    SpeechRecognition recognizer;

    // Set callback functions
    recognizer.SetRecognizeStartCallback([]()
    {
        std::cout << "[Callback] Recognition started!" << std::endl;
    });

    recognizer.SetRecognizeStopCallback([]()
    {
        std::cout << "[Callback] Recognition stopped!" << std::endl;
    });

    recognizer.SetUpdateTextCallback([](const std::string& text)
    {
        //std::cout << "[Callback] Current text: " << text << std::endl;
    });

    recognizer.SetRecognizedCallback([](const std::string& text)
    {
        std::cout << "[Callback] Final recognized: " << text << std::endl;
    });

    recognizer.SetTimeOutCallback([]()
    {
        std::cout << "[Callback] Timeout - no speech detected" << std::endl;
    });

    // Initialize using SpeechHelper config (recommended)
    if (!recognizer.Initialize())
    {
        std::cerr << "Failed to initialize speech recognizer" << std::endl;
        return;
    }

    // Start recognition
    std::cout << "\nSay something... (Press Enter to stop)" << std::endl;
    if (recognizer.BeginRecognizer())
    {
        std::cin.get();
        recognizer.StopRecognizer();
    }

    std::cout << "Embedded recognition test finished." << std::endl;
}

// Test 2: Using online mode (Azure Cloud Service)
void TestOnlineRecognition()
{
    std::cout << "\n=== Test 2: Online Speech Recognition ===" << std::endl;
    
    // Replace with your Azure Speech Service credentials
    const std::string subscriptionKey = "YourSubscriptionKey";
    const std::string serviceRegion = "eastasia";  // e.g., eastasia, westus, etc.
    const std::string language = "zh-CN";          // Chinese Simplified

    SpeechRecognition recognizer;

    // Set callback functions
    recognizer.SetRecognizeStartCallback([]()
    {
        std::cout << "[Callback] Recognition started!" << std::endl;
    });

    recognizer.SetRecognizeStopCallback([]()
    {
        std::cout << "[Callback] Recognition stopped!" << std::endl;
    });

    recognizer.SetUpdateTextCallback([](const std::string& text)
    {
        std::cout << "[Callback] Current text: " << text << std::endl;
    });

    recognizer.SetRecognizedCallback([](const std::string& text)
    {
        std::cout << "[Callback] Final recognized: " << text << std::endl;
    });

    recognizer.SetTimeOutCallback([]()
    {
        std::cout << "[Callback] Timeout - no speech detected" << std::endl;
    });

    // Initialize (online mode)
    if (!recognizer.InitializeOnline(subscriptionKey, serviceRegion, language))
    {
        std::cerr << "Failed to initialize speech recognizer" << std::endl;
        return;
    }

    // Start recognition
    std::cout << "\nSay something... (Press Enter to stop)" << std::endl;
    if (recognizer.BeginRecognizer())
    {
        // Wait for user input to stop
        std::cin.get();
        
        // Stop recognition
        recognizer.StopRecognizer();
    }

    std::cout << "Online recognition test finished." << std::endl;
}

// Test 3: Using embedded model with specified path
void TestEmbeddedModelPath()
{
    std::cout << "\n=== Test 3: Embedded Model Path ===" << std::endl;

    SpeechRecognition recognizer;

    // Set callback functions
    recognizer.SetRecognizeStartCallback([]()
    {
        std::cout << "[Callback] Recognition started!" << std::endl;
    });

    recognizer.SetUpdateTextCallback([](const std::string& text)
    {
        std::cout << "[Callback] Current text: " << text << std::endl;
    });

    recognizer.SetRecognizedCallback([](const std::string& text)
    {
        std::cout << "[Callback] Final recognized: " << text << std::endl;
    });

    // Directly specify model path and name for initialization
    std::string modelPath = "C:\\Path\\To\\Your\\Model";
    std::string modelName = "Microsoft Speech Recognizer en-US FP Model V9";
    std::string license = SpeechHelper::SpeechModelLicense;

    if (!recognizer.InitializeWithEmbeddedModel(modelPath, modelName, license))
    {
        std::cerr << "Failed to initialize with embedded model" << std::endl;
        return;
    }

    // Start recognition
    std::cout << "\nSay something... (Press Enter to stop)" << std::endl;
    if (recognizer.BeginRecognizer())
    {
        std::cin.get();
        recognizer.StopRecognizer();
    }

    std::cout << "Embedded model path test finished." << std::endl;
}

// Test 4: Simple interactive test
void TestInteractive()
{
    std::cout << "\n=== Test 4: Interactive Speech Recognition ===" << std::endl;
    std::cout << "Please enter your Azure Speech Service credentials:" << std::endl;
    
    std::string subscriptionKey;
    std::string serviceRegion;
    std::string language;

    std::cout << "Subscription Key: ";
    std::getline(std::cin, subscriptionKey);

    std::cout << "Service Region (e.g., eastasia, westus): ";
    std::getline(std::cin, serviceRegion);

    std::cout << "Language (e.g., zh-CN, en-US) [default: zh-CN]: ";
    std::getline(std::cin, language);
    if (language.empty())
    {
        language = "zh-CN";
    }

    SpeechRecognition recognizer;

    // Set callbacks
    recognizer.SetRecognizedCallback([](const std::string& text)
    {
        std::cout << "\n>>> Recognized: " << text << std::endl;
    });

    recognizer.SetUpdateTextCallback([](const std::string& text)
    {
        //std::cout << "\r>>> Recognizing: " << text << std::flush;
    });

    // Initialize
    if (!recognizer.InitializeOnline(subscriptionKey, serviceRegion, language))
    {
        std::cerr << "Failed to initialize. Please check your credentials." << std::endl;
        return;
    }

    std::cout << "\n=== Recognition Ready ===" << std::endl;
    std::cout << "Commands: 'start' - begin recognition, 'stop' - stop recognition, 'quit' - exit" << std::endl;

    std::string command;
    while (true)
    {
        std::cout << "\nCommand: ";
        std::getline(std::cin, command);

        if (command == "start")
        {
            if (recognizer.BeginRecognizer())
            {
                std::cout << "Recognition started. Speak now..." << std::endl;
            }
            else
            {
                std::cout << "Failed to start recognition" << std::endl;
            }
        }
        else if (command == "stop")
        {
            recognizer.StopRecognizer();
            std::cout << "Recognition stopped." << std::endl;
        }
        else if (command == "quit" || command == "exit")
        {
            recognizer.StopRecognizer();
            break;
        }
        else
        {
            std::cout << "Unknown command. Use 'start', 'stop', or 'quit'." << std::endl;
        }
    }

    std::cout << "Goodbye!" << std::endl;
}

// Test 5: Check model exists
void TestCheckModelExists()
{
    std::cout << "\n=== Test 5: Check Model Exists ===" << std::endl;
    
    // Set paths
    SpeechHelper::SpeechRecognitionModelPath = "C:\\Path\\To\\Your\\Models";
    SpeechHelper::SpeechSynthesisVoicePath = "C:\\Path\\To\\Your\\Models";
    
    // Check STT models
    std::vector<std::string> locales = {"en-us", "zh-cn", "ja-jp", "de-de"};
    
    std::cout << "\nChecking STT models:" << std::endl;
    for (const auto& locale : locales)
    {
        bool exists = SpeechHelper::CheckSTTModelExists(locale);
        std::cout << "  " << locale << ": " << (exists ? "Found" : "Not found") << std::endl;
    }
    
    std::cout << "\nChecking TTS voices:" << std::endl;
    for (const auto& locale : locales)
    {
        bool exists = SpeechHelper::CheckTTSVoiceExists(locale);
        std::cout << "  " << locale << ": " << (exists ? "Found" : "Not found") << std::endl;
    }
}

// Test 6: DLL version info
void TestDllInfo()
{
    std::cout << "\n=== Test 6: DLL Info ===" << std::endl;
    std::cout << "SpeechRecognitionModule DLL loaded successfully!" << std::endl;
    std::cout << "Testing basic functionality..." << std::endl;
    
    // Create a recognizer instance to verify DLL is working
    SpeechRecognition recognizer;
    
    std::cout << "SpeechRecognition object created successfully." << std::endl;
    std::cout << "Current state: " << static_cast<int>(recognizer.GetState()) << " (0=WaitInitialize)" << std::endl;
    std::cout << "Is running: " << (recognizer.IsRunning() ? "Yes" : "No") << std::endl;
    
    std::cout << "\nDLL test passed!" << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "  SpeechRecognitionModule DLL Test     " << std::endl;
    std::cout << "  Based on Microsoft Azure Speech SDK  " << std::endl;
    std::cout << "========================================" << std::endl;

    std::cout << "\nSelect test to run:" << std::endl;
    std::cout << "1. Embedded Recognition (uses SpeechHelper)" << std::endl;
    std::cout << "2. Online Recognition (Azure Cloud)" << std::endl;
    std::cout << "3. Embedded Model Path (Direct path)" << std::endl;
    std::cout << "4. Interactive Mode" << std::endl;
    std::cout << "5. Check Model Exists" << std::endl;
    std::cout << "6. DLL Info (Quick test)" << std::endl;
    std::cout << "\nChoice (1-6): ";

    int choice;
    std::cin >> choice;
    std::cin.ignore(); // Clear newline

    switch (choice)
    {
    case 1:
        TestEmbeddedRecognition();
        break;
    case 2:
        TestOnlineRecognition();
        break;
    case 3:
        TestEmbeddedModelPath();
        break;
    case 4:
        TestInteractive();
        break;
    case 5:
        TestCheckModelExists();
        break;
    case 6:
        TestDllInfo();
        break;
    default:
        std::cout << "Invalid choice" << std::endl;
        break;
    }

    return 0;
}
