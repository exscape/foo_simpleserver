#include "stdafx.h"
#include "actions.h"
#include "json.hpp"
#include "clienthandler.h"
#include <vector>
#include <sstream>

using json = nlohmann::json;
using std::uint8_t;

#define REQUEST_BUFSIZE 65536 // a few times more than enough to allow queueing the foobar hard limit of 64 tracks

void ClientHandler::send_error_response(const json &request, const std::string &error, const std::string &description) {
    send_response(request, {
        { "error", error },
        { "error_description", description }
    });
}

void ClientHandler::send_response(const json &request, const json &response) {
    if (this->hPipe == nullptr || this->hPipe == INVALID_HANDLE_VALUE)
        return;

    auto data = json::to_cbor(response);
    auto size = data.size();
    if (size == 0)
        return;

    DWORD bytes_written = 0;
    WriteFile(hPipe, data.data(), size, &bytes_written, nullptr);
    if (bytes_written != size) {
        std::cerr << "foo_simpleserver: WriteFile failed for request \"";
        try { std::cerr << request["request_type"]; }
        catch (...) { std::cerr << "<unknown>"; }
        std::cerr << "\"." << std::endl;
        std::cerr << "Asked to write " << size << " bytes, but " << bytes_written << " were written. GetLastError() = " << GetLastError() << std::endl;
    }
}

void ClientHandler::go() {
    rawRequest = (uint8_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, REQUEST_BUFSIZE);
    if (!rawRequest)
        return;

    // Read the request
    if (!ReadFile(hPipe, rawRequest, REQUEST_BUFSIZE, &bytes_read, nullptr)) {
        if (GetLastError() == ERROR_MORE_DATA) {
            std::cerr << "foo_simpleserver: request too big, ignoring!" << std::endl;
            std::ostringstream ss;
            ss << "the request was bigger than the server's buffer size of " << REQUEST_BUFSIZE << " bytes.";
            send_error_response({}, "request_too_big", ss.str());
        }
        else {
            std::cerr << "foo_simpleserver: ReadFile failed, ignoring request! GetLastError() = " << GetLastError() << std::endl;
            std::ostringstream ss;
            ss << "unhandled error. Windows error code " << GetLastError() << ".";
            send_error_response({}, "read_error", ss.str());
        }

        return;
    }

    std::vector<uint8_t> request_cbor(rawRequest, rawRequest + bytes_read);
    json request;
    try {
        request = json::from_cbor(request_cbor);
    }
    catch (...) {
        send_error_response({}, "deserialization_error", "from_cbor threw an exception; invalid data received");
        return;
    }

    if (request.find("request_type") == request.end()) {
        send_error_response({}, "invalid_request", "request does not contain key \"request_type\", which is required");
        return;
    }

    //
    // This block handles requests that have response data associated with them.
    //
     if (request["request_type"] == "get_library") {
         send_response(request, getLibraryInfo());
    }
    else if (request["request_type"] == "get_playlists") {
        send_response(request, getAllPlaylists());
    }
    else if (request["request_type"] == "get_playlist_tracks:") {
        if (request.find("playlist_id") == request.end()) {
            send_error_response(request, "missing_parameter", "request type \"get_playlist_tracks:\" without required parameter playlist_id");
            return;
        }

        t_size playlist_id = (t_size)request["playlist_id"];
        send_response(request, getPlaylistTracks(playlist_id));
    }

    //
    // This block handles requests that don't have any response data (except an OK/error status) associated with them:
    // play, enqueue, add_top, replace
    //
    else if (request["request_type"] == "play:" || request["request_type"] == "enqueue:" || request["request_type"] == "add_top:" || request["request_type"] == "replace:") {



        /// TODO
        /// FIXME
        /// TODO
        /// TODO
        /// FIXME
        /// FIXME


        struct url *urls = nullptr;
        uint32_t count = 0;

        if (request["request_type"] == "play:")
            play_tracks(urls, count);
        else if (request["request_type"] == "enqueue:")
            enqueue_tracks(urls, count);
        else if (request["request_type"] == "add_top:")
            add_tracks_to_queue_top(urls, count);
        else if (request["request_type"] == "replace:")
            replace_queue_with_tracks(urls, count);

        send_response(request, {
            { "ok", "request completed successfully" }
        });

    }
    else {
        // Catch-all: unknown request type!
        send_error_response(request, "unknown_request", "request_type value not recognized");
    }
}

ClientHandler::~ClientHandler()
{
    if (hPipe && hPipe != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    if (rawRequest)
        HeapFree(GetProcessHeap(), 0, rawRequest);
}
