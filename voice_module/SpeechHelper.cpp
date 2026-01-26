//
// SpeechHelper.cpp
// Speech Helper Utility Class Implementation
//

#include "SpeechHelper.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <speechapi_cxx.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

using namespace Microsoft::CognitiveServices::Speech;

namespace SpeechRecognitionModule
{

// Static member initialization
std::string SpeechHelper::SpeechModelLicense = "This model and the software may not be used or distributed in any manner except as authorized under a valid written agreement, using the reference number 5192510683. The model and software are licensed and not sold, and the agreement provides limited rights to use the model and the software and Microsoft reserves all other rights. You may not work around any technical limitations in the model or the software, reverse engineer, decompile, or disassemble the model or the software; remove, minimize, block or modify any notices of Microsoft or its suppliers in the model or the software; or, share, publish, rent, or lease the model or software, or provide the model or software as a standalone solution for others to use.";
std::string SpeechHelper::SpeechModelLicense1 = "This model and the software may not be used or distributed in any manner except as authorized under a valid written agreement, using the reference number 5192510683. The model and software are licensed and not sold, and the agreement provides limited rights to use the model and the software and Microsoft reserves all other rights. You may not work around any technical limitations in the model or the software, reverse engineer, decompile, or disassemble the model or the software; remove, minimize, block or modify any notices of Microsoft or its suppliers in the model or the software; or, share, publish, rent, or lease the model or software, or provide the model or software as a standalone solution for others to use.";

std::string SpeechHelper::SpeechModelLicensePath = "";
std::string SpeechHelper::SpeechModelLicensePathName = "ms_speech_license";
std::string SpeechHelper::SpeechModelLicenseName = "speech_license.txt";

std::string SpeechHelper::SpeechRecognitionModelPath = "";
std::string SpeechHelper::SpeechRecognitionModelPath1 = "";
std::string SpeechHelper::SpeechRecognitionModelPathName = "stt";
std::string SpeechHelper::SpeechRecognitionModelPathName1 = "stt";
std::string SpeechHelper::SpeechRecognitionModelLocale = "en-us";
std::string SpeechHelper::SpeechRecognitionModelName = "Microsoft Speech Recognizer en-US FP Model V9";

std::string SpeechHelper::SpeechSynthesisVoicePath = "";
std::string SpeechHelper::SpeechSynthesisVoicePath1 = "";
std::string SpeechHelper::SpeechSynthesisVoicePathName = "tts";
std::string SpeechHelper::SpeechSynthesisVoicePathName1 = "tts";
std::string SpeechHelper::SpeechSynthesisVoiceLocale = "en-us";
std::string SpeechHelper::SpeechSynthesisVoiceFullName = "Microsoft Server Speech Text to Speech Voice (en-US, AriaNeural)";
std::string SpeechHelper::SpeechSynthesisVoiceName = "Aria";

std::string SpeechHelper::s_speechRecognitionModelFullPath = "";
std::string SpeechHelper::s_speechSynthesisVoiceFullPath = "";

std::string SpeechHelper::GetLocalAppDataPath()
{
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
    {
        return std::string(path);
    }
#endif
    return "";
}

std::string SpeechHelper::GetProgramFilesPath()
{
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, 0, path)))
    {
        return std::string(path);
    }
#endif
    return "";
}

std::string SpeechHelper::GetParentDirectory(const std::string& path)
{
    try
    {
        fs::path p(path);
        if (p.has_parent_path())
        {
            return p.parent_path().string();
        }
    }
    catch (const std::exception&)
    {
    }
    return "";
}

bool SpeechHelper::EnvironmentVerify()
{
    // Set Qira model path as default/backup path
    std::string programFiles = GetProgramFilesPath();
    std::string qiraPreloadedPath = programFiles + "\\Lenovo\\Lenovo Qira\\Preloaded";
    
#ifdef _DEBUG
    std::cout << "EnvironmentVerify in Debug mode" << std::endl;
#endif

    // If primary path is not set or doesn't exist, use Qira path
    if (SpeechRecognitionModelPath.empty() || !DirectoryExists(SpeechRecognitionModelPath))
    {
        if (DirectoryExists(qiraPreloadedPath))
        {
            SpeechRecognitionModelPath = qiraPreloadedPath;
            //std::wcout << L"Using Qira preloaded path for STT: " << qiraPreloadedPath << std::endl;
        }
    }
    
    // Set backup path
    if (SpeechRecognitionModelPath1.empty() || !DirectoryExists(SpeechRecognitionModelPath1))
    {
        if (DirectoryExists(qiraPreloadedPath))
        {
            SpeechRecognitionModelPath1 = qiraPreloadedPath;
        }
    }
    
    if (SpeechSynthesisVoicePath.empty() || !DirectoryExists(SpeechSynthesisVoicePath))
    {
        if (DirectoryExists(qiraPreloadedPath))
        {
            SpeechSynthesisVoicePath = qiraPreloadedPath;
            std::cout << "Using Qira preloaded path for TTS: " << qiraPreloadedPath << std::endl;
        }
    }
    
    if (SpeechSynthesisVoicePath1.empty() || !DirectoryExists(SpeechSynthesisVoicePath1))
    {
        if (DirectoryExists(qiraPreloadedPath))
        {
            SpeechSynthesisVoicePath1 = qiraPreloadedPath;
        }
    }

    std::cout << "SpeechRecognitionModelPath is " << SpeechRecognitionModelPath << std::endl;
    std::cout << "SpeechRecognitionModelPath1 is " << SpeechRecognitionModelPath1 << std::endl;
    std::cout << "SpeechSynthesisVoicePath is " << SpeechSynthesisVoicePath << std::endl;
    std::cout << "SpeechSynthesisVoicePath1 is " << SpeechSynthesisVoicePath1 << std::endl;
    
    s_speechRecognitionModelFullPath.clear();
    s_speechSynthesisVoiceFullPath.clear();

    // Get current working directory
    std::string cwd = fs::current_path().string();
    std::cout << "Current working directory: " << cwd << std::endl;

    // Check for embedded speech extension DLLs
    if (!CheckEmbeddedSpeechExtensions())
    {
        std::cerr << "## ERROR: Embedded speech extension DLLs not found!" << std::endl;
        std::cerr << "## The following DLLs are required for embedded speech:" << std::endl;
        std::cerr << "##   - Microsoft.CognitiveServices.Speech.extension.embedded.sr.dll (for STT)" << std::endl;
        std::cerr << "##   - Microsoft.CognitiveServices.Speech.extension.embedded.tts.dll (for TTS)" << std::endl;
        std::cerr << "## " << std::endl;
        std::cerr << "## These are NOT included in the standard NuGet package." << std::endl;
        std::cerr << "## You need to obtain the Embedded Speech SDK from Microsoft." << std::endl;
        std::cerr << "## See: https://learn.microsoft.com/azure/ai-services/speech-service/embedded-speech" << std::endl;
        return false;
    }

    std::string locale = ToLower(SpeechRecognitionModelLocale);

    // ===== Verify speech recognition model path (primary path) =====
    if (!SpeechRecognitionModelPath.empty() && 
        !SpeechRecognitionModelPathName.empty() && 
        !locale.empty() && 
        DirectoryExists(SpeechRecognitionModelPath))
    {
        std::vector<std::string> keywords = { SpeechRecognitionModelPathName, locale };
        std::string directory = FindDirectoryContaining(SpeechRecognitionModelPath, keywords);

        if (!directory.empty())
        {
            std::cout << "Found STT model directory: " << directory << std::endl;
            
            try
            {
                auto config = EmbeddedSpeechConfig::FromPath(directory);
                auto models = config->GetSpeechRecognitionModels();

                if (models.empty())
                {
                    std::cout << "## WARNING: Cannot locate an embedded speech recognition model in " << directory << std::endl;
                }
                else
                {
                    std::cout << "Available STT models:" << std::endl;
                    for (const auto& model : models)
                    {
                        std::cout << "  - " << model->Name << std::endl;
                    }
                    
                    // Find specified model
                    std::shared_ptr<SpeechRecognitionModel> resultModel = nullptr;
                    for (const auto& model : models)
                    {
                        if (model->Name == SpeechRecognitionModelName ||
                            (!model->Locales.empty() && model->Locales[0] == SpeechRecognitionModelName))
                        {
                            resultModel = model;
                            break;
                        }
                    }

                    if (!resultModel && !models.empty())
                    {
                        resultModel = models[0];
                    }

                    if (resultModel)
                    {
                        SpeechRecognitionModelName = resultModel->Name;
                        std::cout << "Selected STT model: " << SpeechRecognitionModelName << std::endl;
                        s_speechRecognitionModelFullPath = directory;
                    }
                }
            }
            catch (const std::exception& ex)
            {
                std::cerr << "Error loading speech recognition model: " << ex.what() << std::endl;
            }
        }
    }

    // ===== If model not found in primary path, try backup path =====
    if (s_speechRecognitionModelFullPath.empty())
    {
        SpeechModelLicense = SpeechModelLicense1;
        
        if (!SpeechRecognitionModelPath1.empty() && 
            !SpeechRecognitionModelPathName1.empty() && 
            !locale.empty() && 
            DirectoryExists(SpeechRecognitionModelPath1))
        {
            std::vector<std::string> keywords = { SpeechRecognitionModelPathName1, locale };
            std::string directory = FindDirectoryContaining(SpeechRecognitionModelPath1, keywords);

            if (!directory.empty())
            {
                std::cout << "Found STT model directory (backup): " << directory << std::endl;
                
                try
                {
                    auto config = EmbeddedSpeechConfig::FromPath(directory);
                    auto models = config->GetSpeechRecognitionModels();

                    if (!models.empty())
                    {
                        std::cout << "Available STT models (backup path):" << std::endl;
                        for (const auto& model : models)
                        {
                            std::cout << "  - " << model->Name << std::endl;
                        }
                        
                        std::shared_ptr<SpeechRecognitionModel> resultModel = nullptr;
                        for (const auto& model : models)
                        {
                            if (model->Name == SpeechRecognitionModelName ||
                                (!model->Locales.empty() && model->Locales[0] == SpeechRecognitionModelName))
                            {
                                resultModel = model;
                                break;
                            }
                        }

                        if (!resultModel && !models.empty())
                        {
                            resultModel = models[0];
                        }

                        if (resultModel)
                        {
                            SpeechRecognitionModelName = resultModel->Name;
                            std::cout << "Selected STT model: " << SpeechRecognitionModelName << std::endl;
                            s_speechRecognitionModelFullPath = directory;
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    std::cerr << "Error loading speech recognition model from backup path: " << ex.what() << std::endl;
                }
            }
        }
    }

    // Check if STT succeeded
    if (s_speechRecognitionModelFullPath.empty())
    {
        std::cerr << "## ERROR: No STT model found. Speech recognition will not work." << std::endl;
        return false;
    }

    std::string voiceLocale = ToLower(SpeechSynthesisVoiceLocale);

    // ===== Verify speech synthesis voice path (TTS is optional) =====
    bool ttsAvailable = false;
    
    if (!SpeechSynthesisVoicePath.empty() && 
        !SpeechSynthesisVoicePathName.empty() && 
        !voiceLocale.empty() && 
        DirectoryExists(SpeechSynthesisVoicePath))
    {
        std::vector<std::string> keywords = { SpeechSynthesisVoicePathName, voiceLocale };
        std::string directory = FindDirectoryContaining(SpeechSynthesisVoicePath, keywords);

        if (!directory.empty())
        {
            std::cout << "Found TTS voice directory: " << directory << std::endl;
            
            // Only set path, do not create SpeechSynthesizer (avoid SPXERR_EXTENSION_LIBRARY_NOT_FOUND error)
            s_speechSynthesisVoiceFullPath = directory;
            ttsAvailable = true;
            
            std::cout << "TTS voice path set (validation skipped to avoid extension library errors)" << std::endl;
        }
    }

    // ===== If not found in primary path, try backup path =====
    if (!ttsAvailable)
    {
        if (!SpeechSynthesisVoicePath1.empty() && 
            !SpeechSynthesisVoicePathName1.empty() && 
            !voiceLocale.empty() && 
            DirectoryExists(SpeechSynthesisVoicePath1))
        {
            std::vector<std::string> keywords = { SpeechSynthesisVoicePathName1, voiceLocale };
            std::string directory = FindDirectoryContaining(SpeechSynthesisVoicePath1, keywords);

            if (!directory.empty())
            {
                std::cout << "Found TTS voice directory (backup): " << directory << std::endl;
                s_speechSynthesisVoiceFullPath = directory;
                ttsAvailable = true;
            }
        }
    }

    // Output final status
    std::cout << std::endl;
    std::cout << "=== Environment Verification Summary ===" << std::endl;
    std::cout << "STT Model Path: " << s_speechRecognitionModelFullPath << std::endl;
    std::cout << "STT Model Name: " << SpeechRecognitionModelName << std::endl;
    std::cout << "TTS Voice Path: " << (s_speechSynthesisVoiceFullPath.empty() ? "(not available)" : s_speechSynthesisVoiceFullPath) << std::endl;
    std::cout << "TTS Available: " << (ttsAvailable ? "Yes" : "No (STT only mode)") << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;

    // Return true as long as STT is available
    return !s_speechRecognitionModelFullPath.empty();
}

bool SpeechHelper::CheckSTTModelExists(const std::string& locale)
{
    try
    {
        std::string lowerLocale = ToLower(locale);
        
        // Check primary path
        if (!SpeechRecognitionModelPath.empty() && 
            !SpeechRecognitionModelPathName.empty() && 
            !lowerLocale.empty() && 
            DirectoryExists(SpeechRecognitionModelPath))
        {
            for (const auto& entry : fs::directory_iterator(SpeechRecognitionModelPath))
            {
                if (entry.is_directory())
                {
                    std::string dirName = ToLower(entry.path().filename().string());
                    if (dirName.find(ToLower(SpeechRecognitionModelPathName)) != std::string::npos)
                    {
                        size_t dotPos = dirName.find('.');
                        if (dotPos != std::string::npos && dotPos + 1 < dirName.length())
                        {
                            std::string dirLocale = dirName.substr(dotPos + 1);
                            if (dirLocale.find(lowerLocale) != std::string::npos)
                            {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        // Check backup path
        if (!SpeechRecognitionModelPath1.empty() && 
            !SpeechRecognitionModelPathName1.empty() && 
            !lowerLocale.empty() && 
            DirectoryExists(SpeechRecognitionModelPath1))
        {
            for (const auto& entry : fs::directory_iterator(SpeechRecognitionModelPath1))
            {
                if (entry.is_directory())
                {
                    std::string dirName = ToLower(entry.path().filename().string());
                    if (dirName.find(ToLower(SpeechRecognitionModelPathName1)) != std::string::npos)
                    {
                        size_t dotPos = dirName.find('.');
                        if (dotPos != std::string::npos && dotPos + 1 < dirName.length())
                        {
                            std::string dirLocale = dirName.substr(dotPos + 1);
                            if (dirLocale.find(lowerLocale) != std::string::npos)
                            {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool SpeechHelper::CheckTTSVoiceExists(const std::string& locale)
{
    try
    {
        std::string lowerLocale = ToLower(locale);
        
        // Check primary path
        if (!SpeechSynthesisVoicePath.empty() && 
            !SpeechSynthesisVoicePathName.empty() && 
            !lowerLocale.empty() && 
            DirectoryExists(SpeechSynthesisVoicePath))
        {
            for (const auto& entry : fs::directory_iterator(SpeechSynthesisVoicePath))
            {
                if (entry.is_directory())
                {
                    std::string dirName = ToLower(entry.path().filename().string());
                    if (dirName.find(ToLower(SpeechSynthesisVoicePathName)) != std::string::npos)
                    {
                        size_t dotPos = dirName.find('.');
                        if (dotPos != std::string::npos && dotPos + 1 < dirName.length())
                        {
                            std::string dirLocale = dirName.substr(dotPos + 1);
                            if (dirLocale.find(lowerLocale) != std::string::npos)
                            {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        // Check backup path
        if (!SpeechSynthesisVoicePath1.empty() && 
            !SpeechSynthesisVoicePathName1.empty() && 
            !lowerLocale.empty() && 
            DirectoryExists(SpeechSynthesisVoicePath1))
        {
            for (const auto& entry : fs::directory_iterator(SpeechSynthesisVoicePath1))
            {
                if (entry.is_directory())
                {
                    std::string dirName = ToLower(entry.path().filename().string());
                    if (dirName.find(ToLower(SpeechSynthesisVoicePathName1)) != std::string::npos)
                    {
                        size_t dotPos = dirName.find('.');
                        if (dotPos != std::string::npos && dotPos + 1 < dirName.length())
                        {
                            std::string dirLocale = dirName.substr(dotPos + 1);
                            if (dirLocale.find(lowerLocale) != std::string::npos)
                            {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::vector<std::shared_ptr<VoiceInfo>> SpeechHelper::GetAllVoiceNames(const std::string& locale)
{
    std::vector<std::shared_ptr<VoiceInfo>> result;
    
    if (!CheckTTSVoiceExists(locale))
    {
        return result;
    }
    
    try
    {
        auto config = EmbeddedSpeechConfig::FromPath(s_speechSynthesisVoiceFullPath);
        auto synthesizer = SpeechSynthesizer::FromConfig(config, nullptr);
        auto voicesResult = synthesizer->GetVoicesAsync("").get();
        
        if (voicesResult->Reason != ResultReason::VoicesListRetrieved)
        {
            return result;
        }
        
        if (voicesResult->Voices.empty())
        {
            return result;
        }
        
        return voicesResult->Voices;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "GetAllVoiceNames Exception: " << ex.what() << std::endl;
        return result;
    }
}

std::shared_ptr<EmbeddedSpeechConfig> SpeechHelper::CreateSpeechConfig()
{
    std::vector<std::string> paths;
    
    if (!s_speechRecognitionModelFullPath.empty())
    {
        paths.push_back(s_speechRecognitionModelFullPath);
    }
    
    // TTS path is optional
    if (!s_speechSynthesisVoiceFullPath.empty())
    {
        paths.push_back(s_speechSynthesisVoiceFullPath);
    }
    
    if (paths.empty())
    {
        std::cerr << "## ERROR: No model path(s) specified." << std::endl;
        return nullptr;
    }
    
    try
    {
        auto config = EmbeddedSpeechConfig::FromPaths(paths);
        
        if (!SpeechRecognitionModelName.empty())
        {
            config->SetSpeechRecognitionModel(SpeechRecognitionModelName, SpeechModelLicense);
        }
        
        // Only set speech synthesis when TTS path is available
        if (!s_speechSynthesisVoiceFullPath.empty() && !SpeechSynthesisVoiceFullName.empty())
        {
            try
            {
                config->SetSpeechSynthesisVoice(SpeechSynthesisVoiceFullName, SpeechModelLicense);
                
                if (SpeechSynthesisVoiceFullName.find("Neural") != std::string::npos)
                {
                    config->SetSpeechSynthesisOutputFormat(SpeechSynthesisOutputFormat::Riff24Khz16BitMonoPcm);
                }
            }
            catch (const std::exception& ex)
            {
                std::cerr << "Warning: Failed to set TTS voice: " << ex.what() << std::endl;
                std::cerr << "TTS will be disabled, STT will continue to work." << std::endl;
            }
        }
        
        return config;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "CreateSpeechConfig Exception: " << ex.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<SpeechConfig> SpeechHelper::CreateOnlineSpeechConfig(
    const std::string& subscriptionKey,
    const std::string& serviceRegion,
    const std::string& language)
{
    try
    {
        std::string endpoint = "https://" + serviceRegion + ".api.cognitive.microsoft.com";
        auto config = SpeechConfig::FromEndpoint(endpoint, subscriptionKey);
        config->SetSpeechRecognitionLanguage(language);
        return config;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "CreateOnlineSpeechConfig Exception: " << ex.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<EmbeddedSpeechConfig> SpeechHelper::CreateEmbeddedSpeechConfig()
{
    if (s_speechRecognitionModelFullPath.empty())
    {
        std::cerr << "Speech recognition model path not set. Call EnvironmentVerify() first." << std::endl;
        return nullptr;
    }

    try
    {
        auto config = EmbeddedSpeechConfig::FromPath(s_speechRecognitionModelFullPath);
        config->SetSpeechRecognitionModel(SpeechRecognitionModelName, SpeechModelLicense);
        return config;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "CreateEmbeddedSpeechConfig Exception: " << ex.what() << std::endl;
        return nullptr;
    }
}

std::string SpeechHelper::FindDirectoryContaining(const std::string& basePath, 
                                                   const std::vector<std::string>& keywords)
{
    try
    {
        for (const auto& entry : fs::directory_iterator(basePath))
        {
            if (entry.is_directory())
            {
                std::string dirName = ToLower(entry.path().filename().string());
                bool allFound = true;
                
                for (const auto& keyword : keywords)
                {
                    if (dirName.find(ToLower(keyword)) == std::string::npos)
                    {
                        allFound = false;
                        break;
                    }
                }
                
                if (allFound)
                {
                    return entry.path().string();
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "FindDirectoryContaining Exception: " << ex.what() << std::endl;
    }
    
    return "";
}

bool SpeechHelper::DirectoryExists(const std::string& path)
{
    try
    {
        return fs::exists(path) && fs::is_directory(path);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::vector<std::string> SpeechHelper::GetSubDirectories(const std::string& path)
{
    std::vector<std::string> directories;
    
    try
    {
        for (const auto& entry : fs::directory_iterator(path))
        {
            if (entry.is_directory())
            {
                directories.push_back(entry.path().string());
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "GetSubDirectories Exception: " << ex.what() << std::endl;
    }
    
    return directories;
}

std::string SpeechHelper::ToLower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool SpeechHelper::CheckEmbeddedSpeechExtensions()
{
    // Get the directory where the executable is located
    std::string exeDir;
    
#ifdef _WIN32
    char path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, MAX_PATH) > 0)
    {
        exeDir = fs::path(path).parent_path().string();
    }
#endif

    if (exeDir.empty())
    {
        exeDir = fs::current_path().string();
    }
    
    std::cout << "Checking for embedded speech extensions in: " << exeDir << std::endl;
    
    // Required DLLs for embedded speech
    std::vector<std::string> requiredDlls = {
        "Microsoft.CognitiveServices.Speech.extension.embedded.sr.dll",  // Embedded STT
        "Microsoft.CognitiveServices.Speech.extension.embedded.tts.dll"  // Embedded TTS
    };
    
    bool allFound = true;
    
    for (const auto& dllName : requiredDlls)
    {
        fs::path dllPath = fs::path(exeDir) / dllName;
        
        if (fs::exists(dllPath))
        {
            std::cout << "  [OK] Found: " << dllName << std::endl;
        }
        else
        {
            std::cerr << "  [MISSING] Not found: " << dllName << std::endl;
            allFound = false;
        }
    }
    
    // Also check for the core DLL (always required)
    fs::path coreDllPath = fs::path(exeDir) / "Microsoft.CognitiveServices.Speech.core.dll";
    if (!fs::exists(coreDllPath))
    {
        std::cerr << "  [MISSING] Not found: Microsoft.CognitiveServices.Speech.core.dll" << std::endl;
        allFound = false;
    }
    
    return allFound;
}

} // namespace SpeechRecognitionModule
