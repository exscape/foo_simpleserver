#include "stdafx.h"
#include "actions.h"
#include "json.hpp"
#include "clienthandler.h"
#include <vector>
#include <sstream>

using json = nlohmann::json;
using std::uint8_t;

#define REQUEST_BUFSIZE 65536 // a few times more than enough to allow queueing the foobar hard limit of 64 tracks

void ClientHandler::send_error_reply(json j)
{
    if (this->hPipe == nullptr || this->hPipe == INVALID_HANDLE_VALUE)
        return;

    auto data = json::to_cbor(j);
    auto size = data.size();
    if (size == 0)
        return;

    DWORD bytes_written = 0;
    WriteFile(hPipe, data.data(), size, &bytes_written, nullptr);
    if (bytes_written != size) {
        fprintf(stderr, "foo_simpleserver: WriteFile failed when sending error response; %d bytes out of %d written. GetLastError() == %d\n", bytes_written, size, GetLastError());
    }
}

void ClientHandler::go() {
    rawRequest = (uint8_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, REQUEST_BUFSIZE);
    if (!rawRequest) {
        return;
    }

    // Read the request
    if (!ReadFile(hPipe, rawRequest, REQUEST_BUFSIZE, &bytes_read, nullptr)) {
        if (GetLastError() == ERROR_MORE_DATA) {
            fprintf(stderr, "foo_simpleserver: request too big, ignoring!\n");
            std::ostringstream ss;
            ss << "the request was bigger than the server's buffer size of " << REQUEST_BUFSIZE << " bytes.";
            send_error_reply({
                { "error", "request_too_big" },
                { "error_description", ss.str() }
            });
            return;
        }
        else {
            fprintf(stderr, "foo_simpleserver: ReadFile failed, ignoring request! GetLastError() == %d\n", GetLastError());
            std::ostringstream ss;
            ss << "unhandled error. Windows error code " << GetLastError() << ".";
            send_error_reply({
                { "error", "read_error" },
                { "error_description", ss.str() }
            });
            return;

        }
        return;
    }
    assert(bytes_read > 0);

    DWORD bytes_written = 0;
    bool success = false;
    bool replySent = false;

    std::vector<uint8_t> request(rawRequest, rawRequest + bytes_read);
    json j;
    try {
        j = json::from_cbor(request);
    }
    catch (...) {
        send_error_reply({
            { "error", "deserialization_error" },
            { "error_description", "from_cbor threw an exception; invalid data received" }
        });
        return;
    }

    if (j.find("request_type") == j.end()) {
        // Request does not contain the "request_type" key, which is required.
        // success and replySent are both set to false above.
        send_error_reply({
            { "error", "invalid_request" },
            { "error_description", "request does not contain \"request_type\" key, which is required" }
        });
        return;
    }

    //
    // This block handles requests that have response data associated with them.
     if (j["request_type"] == "get_library") {
        auto data = getLibraryInfo();
        auto size = data.size();
        if (size > 0) {
            WriteFile(hPipe, data.data(), size, &bytes_written, nullptr);
            replySent = true;
            if (bytes_written != size)
                fprintf(stderr, "foo_simpleserver: WriteFile failed! Asked to write %d bytes but %d bytes were written. GetLastError() == %d\n", size, bytes_written, GetLastError());
            else
                success = true;
        }
        else {
            fprintf(stderr, "foo_simpleserver: getLibraryInfo failed, ignoring request\n");
        }
    }
    else if (j["request_type"] == "get_playlists") {
        auto data = getAllPlaylists();
        auto size = data.size();
        if (size > 0) {
            WriteFile(hPipe, data.data(), size, &bytes_written, nullptr);
            //			fprintf(stderr, "%d bytes written\n", bytes_written);
            replySent = true;
            if (bytes_written != size)
                fprintf(stderr, "foo_simpleserver: WriteFile failed! Asked to write %d bytes but %d bytes were written. GetLastError() == %d\n", size, bytes_written, GetLastError());
            else
                success = true;
        }
        else {
            fprintf(stderr, "foo_simpleserver: getAllPlaylists failed, ignoring request\n");
        }
    }
    else if (j["request_type"] == "get_playlist_tracks:") {
        if (j.find("playlist_id") == j.end()) {
            send_error_reply({
                { "error", "missing_parameter" },
                { "error_description", "request type \"get_playlist_tracks:\" without required parameter playlist_id" }
            });
            return;
        }
        t_size playlist_id = (t_size)j["playlist_id"];
        auto data = getPlaylistTracks(playlist_id);
        auto size = data.size();
        if (size > 0) {
            WriteFile(hPipe, data.data(), size, &bytes_written, nullptr);
            //			fprintf(stderr, "%d bytes written\n", bytes_written);
            replySent = true;
            if (bytes_written != size)
                fprintf(stderr, "foo_simpleserver: WriteFile failed! Asked to write %d bytes but %d bytes were written. GetLastError() == %d\n", size, bytes_written, GetLastError());
            else
                success = true;
        }
        else {
            fprintf(stderr, "foo_simpleserver: getPlaylistTracks failed, ignoring request\n");
        }
    }


    //
    // This block handles requests that don't have any response data (expect an OK/error status) associated with them:
    // play, enqueue, add_top, replace
    //
    else if (j["request_type"] == "play:" || j["request_type"] == "enqueue:" || j["request_type"] == "add_top:" || j["request_type"] == "replace:") {



        /// TODO
        /// FIXME
        /// TODO
        /// TODO
        /// FIXME
        /// FIXME


        struct url *urls = nullptr;
        uint32_t count = 0;

        if (j["request_type"] == "play:")
            play_tracks(urls, count);
        else if (j["request_type"] == "enqueue:")
            enqueue_tracks(urls, count);
        else if (j["request_type"] == "add_top:")
            add_tracks_to_queue_top(urls, count);
        else if (j["request_type"] == "replace:")
            replace_queue_with_tracks(urls, count);

        success = true;
    }
}

ClientHandler::~ClientHandler()
{
    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    if (rawRequest) {
        HeapFree(GetProcessHeap(), 0, rawRequest);
        rawRequest = nullptr;
    }
}
