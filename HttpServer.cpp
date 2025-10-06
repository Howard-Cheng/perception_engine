#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "HttpServer.h"
#include <thread>
#include <iostream>
#include <sstream>

HttpServer::HttpServer(int port) : port(port), running(false), listenSocket(INVALID_SOCKET) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cout << "[ERROR] WSAStartup failed with error: " << result << std::endl;
    } else {
        std::cout << "[DEBUG] Winsock initialized successfully" << std::endl;
    }
}

HttpServer::~HttpServer() {
    Stop();
    WSACleanup();
}

void HttpServer::SetRequestHandler(std::function<void(const HttpRequest&, HttpResponse&)> handler) {
    requestHandler = handler;
}

bool HttpServer::Start() {
    if (running) {
        std::cout << "[WARNING] Server is already running" << std::endl;
        return true;
    }
    
    std::cout << "[DEBUG] Creating socket..." << std::endl;
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        int error = WSAGetLastError();
        std::cout << "[ERROR] Failed to create socket. WSA Error: " << error << std::endl;
        return false;
    }
    std::cout << "[DEBUG] Socket created successfully" << std::endl;
    
    // Allow socket reuse
    int opt = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cout << "[WARNING] Failed to set SO_REUSEADDR. WSA Error: " << WSAGetLastError() << std::endl;
    }
    
    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Bind only to localhost
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    
    std::cout << "[DEBUG] Attempting to bind to 127.0.0.1:" << port << std::endl;
    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        std::cout << "[ERROR] Failed to bind socket. WSA Error: " << error << std::endl;
        
        // Common error explanations
        switch (error) {
            case WSAEADDRINUSE:
                std::cout << "[ERROR] Address already in use. Port " << port << " is busy." << std::endl;
                std::cout << "[HINT] Check what's using port " << port << ": netstat -ano | findstr :" << port << std::endl;
                break;
            case WSAEACCES:
                std::cout << "[ERROR] Permission denied. Try running as administrator." << std::endl;
                break;
            case WSAEADDRNOTAVAIL:
                std::cout << "[ERROR] Address not available." << std::endl;
                break;
            default:
                std::cout << "[ERROR] Unknown bind error: " << error << std::endl;
                break;
        }
        
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        return false;
    }
    std::cout << "[DEBUG] Socket bound successfully to 127.0.0.1:" << port << std::endl;
    
    std::cout << "[DEBUG] Starting to listen..." << std::endl;
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        std::cout << "[ERROR] Failed to listen on socket. WSA Error: " << error << std::endl;
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        return false;
    }
    
    running = true;
    std::cout << "[SUCCESS] HTTP server is now listening on 127.0.0.1:" << port << std::endl;
    return true;
}

void HttpServer::Stop() {
    if (!running) return;
    
    std::cout << "[DEBUG] Stopping HTTP server..." << std::endl;
    running = false;
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        std::cout << "[DEBUG] Listen socket closed" << std::endl;
    }
}

void HttpServer::Run() {
    if (!running || !requestHandler) {
        std::cout << "[ERROR] Cannot run server - not properly initialized" << std::endl;
        return;
    }
    
    std::cout << "[DEBUG] Entering server main loop..." << std::endl;
    std::cout << "[INFO] Server ready to accept connections on http://localhost:" << port << std::endl;
    
    while (running) {
        std::cout << "[DEBUG] Waiting for incoming connections..." << std::endl;
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        
        if (clientSocket != INVALID_SOCKET) {
            std::cout << "[DEBUG] New client connection accepted" << std::endl;
            
            // Handle each client in a separate thread for better responsiveness
            std::thread clientThread([this, clientSocket]() {
                HandleClient(clientSocket);
            });
            clientThread.detach();
        } else {
            if (running) {  // Only log error if we're still supposed to be running
                int error = WSAGetLastError();
                std::cout << "[WARNING] Accept failed. WSA Error: " << error << std::endl;
                
                // If it's a serious error, break the loop
                if (error != WSAEINTR && error != WSAEWOULDBLOCK) {
                    std::cout << "[ERROR] Fatal accept error, stopping server" << std::endl;
                    break;
                }
            }
        }
    }
    
    std::cout << "[DEBUG] Server main loop ended" << std::endl;
}

void HttpServer::HandleClient(SOCKET clientSocket) {
    std::cout << "[DEBUG] Handling client request..." << std::endl;
    
    char buffer[4096] = {0};
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        std::cout << "[DEBUG] Received " << bytesReceived << " bytes from client" << std::endl;
        
        std::string rawRequest(buffer, bytesReceived);
        std::cout << "[DEBUG] Raw request: " << rawRequest.substr(0, 100) << "..." << std::endl;
        
        HttpRequest request = ParseHttpRequest(rawRequest);
        HttpResponse response;
        
        // Set default headers
        response.SetHeader("Content-Type", "application/json");
        response.SetHeader("Access-Control-Allow-Origin", "*");
        response.SetHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        response.SetHeader("Access-Control-Allow-Headers", "Content-Type");
        
        if (requestHandler) {
            requestHandler(request, response);
        } else {
            std::cout << "[ERROR] No request handler set!" << std::endl;
        }
        
        std::string httpResponse = BuildHttpResponse(response);
        std::cout << "[DEBUG] Sending response (status " << response.status << ", " 
                  << httpResponse.length() << " bytes)" << std::endl;
        
        int bytesSent = send(clientSocket, httpResponse.c_str(), static_cast<int>(httpResponse.length()), 0);
        if (bytesSent == SOCKET_ERROR) {
            std::cout << "[ERROR] Failed to send response. WSA Error: " << WSAGetLastError() << std::endl;
        } else {
            std::cout << "[DEBUG] Response sent successfully (" << bytesSent << " bytes)" << std::endl;
        }
    } else {
        std::cout << "[WARNING] No data received from client or connection closed" << std::endl;
    }
    
    closesocket(clientSocket);
    std::cout << "[DEBUG] Client connection closed" << std::endl;
}

HttpRequest HttpServer::ParseHttpRequest(const std::string& rawRequest) {
    HttpRequest request;
    
    std::istringstream stream(rawRequest);
    std::string line;
    
    if (std::getline(stream, line)) {
        std::istringstream lineStream(line);
        lineStream >> request.method >> request.path;
        
        // Remove \r if present
        if (!request.path.empty() && request.path.back() == '\r') {
            request.path.pop_back();
        }
        
        std::cout << "[DEBUG] Parsed request: " << request.method << " " << request.path << std::endl;
    }
    
    // Parse headers and body if needed
    std::string headers;
    while (std::getline(stream, line) && line != "\r") {
        headers += line + "\n";
    }
    
    // Read body
    std::string body;
    while (std::getline(stream, line)) {
        body += line + "\n";
    }
    request.body = body;
    
    return request;
}

std::string HttpServer::BuildHttpResponse(const HttpResponse& response) {
    std::ostringstream oss;
    
    oss << "HTTP/1.1 " << response.status << " ";
    
    switch (response.status) {
        case 200: oss << "OK"; break;
        case 404: oss << "Not Found"; break;
        case 500: oss << "Internal Server Error"; break;
        default: oss << "Unknown"; break;
    }
    
    oss << "\r\n";
    
    // Add headers
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    oss << "Content-Length: " << response.body.length() << "\r\n";
    oss << "\r\n";
    oss << response.body;
    
    return oss.str();
}