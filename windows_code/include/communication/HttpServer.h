#pragma once
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_    // Prevent inclusion of winsock.h
#include <functional>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>
#include <string>

#pragma comment(lib, "ws2_32.lib")

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string body;
    std::map<std::string, std::string> headers;
    
    void SetHeader(const std::string& key, const std::string& value) {
        headers[key] = value;
    }
    
    void SetBody(const std::string& content) {
        body = content;
    }
};

class HttpServer {
private:
    int port;
    bool running;
    std::function<void(const HttpRequest&, HttpResponse&)> requestHandler;
    SOCKET listenSocket;
    
    void HandleClient(SOCKET clientSocket);
    std::string BuildHttpResponse(const HttpResponse& response);
    HttpRequest ParseHttpRequest(const std::string& rawRequest);
    
public:
    HttpServer(int port = 8777);
    ~HttpServer();
    
    void SetRequestHandler(std::function<void(const HttpRequest&, HttpResponse&)> handler);
    bool Start();
    void Stop();
    void Run(); // Blocking call to handle requests
};