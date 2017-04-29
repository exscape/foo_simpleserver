#pragma once
#include <Windows.h>
#include "json.hpp"
using std::uint8_t;
using json = nlohmann::json;

class ClientHandler {
protected:
    HANDLE hPipe = nullptr;
    DWORD bytes_read = 0;
    uint8_t *rawRequest = nullptr;

public:
    ClientHandler(HANDLE pipe) : hPipe(pipe) {}
    void go();
    void send_error_response(json request, json response);
    void send_response(json request, std::vector<std::uint8_t> data);
    ~ClientHandler();
};

