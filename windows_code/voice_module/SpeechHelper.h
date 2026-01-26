//
// SpeechHelper.h
// Speech Helper Utility Class - For environment verification and configuration management
//

#pragma once

#include "SpeechRecognitionExport.h"
#include <string>
#include <vector>
#include <memory>

namespace Microsoft {
    namespace CognitiveServices {
        namespace Speech {
            class EmbeddedSpeechConfig;
            class SpeechRecognizer;
            class SpeechConfig;
            class VoiceInfo;
        }
    }
}

namespace SpeechRecognitionModule
{
    class SPEECHRECOGNITION_API SpeechHelper
    {
    public:
        // Speech model license
        static std::string SpeechModelLicense;
        
        // Backup speech model license
        static std::string SpeechModelLicense1;
        
        // License file path
        static std::string SpeechModelLicensePath;
        
        // License path name
        static std::string SpeechModelLicensePathName;
        
        // License file name
        static std::string SpeechModelLicenseName;
        
        // Speech recognition model path
        static std::string SpeechRecognitionModelPath;
        
        // Backup speech recognition model path
        static std::string SpeechRecognitionModelPath1;
        
        // Speech recognition model path name
        static std::string SpeechRecognitionModelPathName;
        
        // Backup speech recognition model path name
        static std::string SpeechRecognitionModelPathName1;
        
        // Speech recognition model locale
        static std::string SpeechRecognitionModelLocale;
        
        // Speech recognition model name
        static std::string SpeechRecognitionModelName;
        
        // Speech synthesis voice path
        static std::string SpeechSynthesisVoicePath;
        
        // Backup speech synthesis voice path
        static std::string SpeechSynthesisVoicePath1;
        
        // Speech synthesis voice path name
        static std::string SpeechSynthesisVoicePathName;
        
        // Backup speech synthesis voice path name
        static std::string SpeechSynthesisVoicePathName1;
        
        // Speech synthesis voice locale
        static std::string SpeechSynthesisVoiceLocale;
        
        // Speech synthesis voice full name
        static std::string SpeechSynthesisVoiceFullName;
        
        // Speech synthesis voice name
        static std::string SpeechSynthesisVoiceName;

        // Verify environment configuration
        static bool EnvironmentVerify();

        // Check if embedded speech extension DLLs exist
        static bool CheckEmbeddedSpeechExtensions();

        // Check if STT model exists
        static bool CheckSTTModelExists(const std::string& locale);
        
        // Check if TTS voice exists
        static bool CheckTTSVoiceExists(const std::string& locale);
        
        // Get all voice names
        static std::vector<std::shared_ptr<Microsoft::CognitiveServices::Speech::VoiceInfo>> GetAllVoiceNames(const std::string& locale);

        // Get speech recognition model full path
        static std::string GetSpeechRecognitionModelFullPath() { return s_speechRecognitionModelFullPath; }

        // Get speech synthesis voice full path  
        static std::string GetSpeechSynthesisVoiceFullPath() { return s_speechSynthesisVoiceFullPath; }

        // Create embedded speech config (includes both STT and TTS)
        static std::shared_ptr<Microsoft::CognitiveServices::Speech::EmbeddedSpeechConfig> CreateSpeechConfig();

        // Create online speech config
        static std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechConfig> CreateOnlineSpeechConfig(
            const std::string& subscriptionKey,
            const std::string& serviceRegion,
            const std::string& language = "zh-CN");

        // Create embedded speech config (STT only)
        static std::shared_ptr<Microsoft::CognitiveServices::Speech::EmbeddedSpeechConfig> CreateEmbeddedSpeechConfig();

        // Find subdirectory containing specified keywords
        static std::string FindDirectoryContaining(const std::string& basePath, 
                                                    const std::vector<std::string>& keywords);

        // Check if directory exists
        static bool DirectoryExists(const std::string& path);

        // Get subdirectory list in directory
        static std::vector<std::string> GetSubDirectories(const std::string& path);

        // Convert string to lowercase
        static std::string ToLower(const std::string& str);
        
        // Get local application data directory
        static std::string GetLocalAppDataPath();
        
        // Get program files directory
        static std::string GetProgramFilesPath();
        
        // Get parent directory of current working directory
        static std::string GetParentDirectory(const std::string& path);

    private:
        static std::string s_speechRecognitionModelFullPath;
        static std::string s_speechSynthesisVoiceFullPath;
    };

} // namespace SpeechRecognitionModule
