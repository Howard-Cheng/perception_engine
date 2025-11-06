#include <iostream>
#include <thread>
#include <chrono>
#include "ConnectMeetingAssistant.h"

// Callback function that will be called when meeting status changes
UserDecision OnMeetingStatusChanged(MeetingStatus status, const char* appName, unsigned long processId) {
    std::cout << "\n¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[\n";
    std::cout << "¨U  MEETING STATUS CHANGED NOTIFICATION                       ¨U\n";
    std::cout << "¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a\n";
    
    if (status == MEETING_STATUS_STARTED) {
        std::cout << "Status: Meeting STARTED\n";
        std::cout << "Application: " << appName << "\n";
        std::cout << "Process ID: " << processId << "\n";
        std::cout << "\n";
        std::cout << "Do you want to start meeting recording? (y/n): ";
        
        char choice;
        std::cin >> choice;
        
        if (choice == 'y' || choice == 'Y') {
            std::cout << "? User chose to START recording\n";
            return USER_DECISION_ACCEPT;
        } else {
            std::cout << "? User chose to DECLINE recording\n";
            return USER_DECISION_DECLINE;
        }
        
    } else if (status == MEETING_STATUS_ENDED) {
        std::cout << "Status: Meeting ENDED\n";
        std::cout << "Application: " << appName << "\n";
        std::cout << "Process ID: " << processId << "\n";
        std::cout << "\n";
        std::cout << "Do you want to start meeting summarization? (y/n): ";
        
        char choice;
        std::cin >> choice;
        
        if (choice == 'y' || choice == 'Y') {
            std::cout << "? User chose to START summarization\n";
            return USER_DECISION_ACCEPT;
        } else {
            std::cout << "? User chose to DECLINE summarization\n";
            return USER_DECISION_DECLINE;
        }
    }
    
    return USER_DECISION_DECLINE;
}

int main() {
    std::cout << "================================================================================\n";
    std::cout << "ConnectMeetingAssistant Test Application\n";
    std::cout << "================================================================================\n";
    std::cout << "\n";
    std::cout << "This application demonstrates connecting to MeetingAssistant via named pipe.\n";
    std::cout << "\n";
    std::cout << "Make sure MeetingAssistant.exe is running before using this application.\n";
    std::cout << "\n";
    std::cout << "Press Ctrl+C to exit\n";
    std::cout << "================================================================================\n";
    std::cout << "\n";

    // Connect to MeetingAssistant
    std::cout << "[TestApp] Connecting to MeetingAssistant...\n";
    int result = ConnectMeetingAssistant(OnMeetingStatusChanged);
    
    if (result != 0) {
        std::cerr << "[TestApp] Failed to connect to MeetingAssistant. Error code: " << result << "\n";
        std::cerr << "[TestApp] Make sure MeetingAssistant.exe is running.\n";
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "[TestApp] Successfully connected to MeetingAssistant!\n";
    std::cout << "[TestApp] Waiting for meeting status notifications...\n";
    std::cout << "\n";

    // Keep the application running
    std::cout << "Press Enter to disconnect and exit...\n";
    std::cin.ignore();
    std::cin.get();

    // Disconnect
    std::cout << "\n[TestApp] Disconnecting from MeetingAssistant...\n";
    DisconnectMeetingAssistant();
    
    std::cout << "[TestApp] Disconnected. Goodbye!\n";
    return 0;
}
