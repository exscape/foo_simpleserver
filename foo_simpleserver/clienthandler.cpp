#include "stdafx.h"
#include "actions.h"
#include "json.hpp"
#include <vector>

using json = nlohmann::json;
using std::uint8_t;

#define REQUEST_BUFSIZE 65536 // a few times more than enough to allow queueing the foobar hard limit of 64 tracks

void clientHandler(HANDLE hPipe) {
    DWORD bytes_read = 0;
    uint8_t *rawRequest = (uint8_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, REQUEST_BUFSIZE);
    if (!rawRequest) {
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        return;
    }

    // Read the request
    if (!ReadFile(hPipe, rawRequest, REQUEST_BUFSIZE, &bytes_read, nullptr)) {
        if (GetLastError() == ERROR_MORE_DATA)
            fprintf(stderr, "foo_simpleserver: request too big, ignoring!\n");
        else
            fprintf(stderr, "foo_simpleserver: ReadFile failed, ignoring request! GetLastError() == %d\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, rawRequest);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        return;
    }
    assert(bytes_read > 0);

    DWORD bytes_written = 0;
    bool success = false;
    bool replySent = false;

    std::vector<uint8_t> request(rawRequest, rawRequest + bytes_read);
    try {
        json j = json::from_cbor(request);

        if (j.find("request_type") == j.end()) {
            // Request does not contain the "request_type" key, which is required.
            // success and replySent are both set to false above.
            goto exit_func;
        }

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
                // TODO: write an error response
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
        // play, enqueue, add_top, replace
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
    catch (...) {
        goto exit_func;
    }

    exit_func:

    if (!replySent) {
        if (success) {
            WriteFile(hPipe, "\x01\x00\x00\x00\x06\x00\x00\x00OK", 10, &bytes_written, nullptr);
        }
        else {
            fprintf(stderr, "foo_simpleserver: request failed\n");
            WriteFile(hPipe, "\x01\x00\x00\x00\x08\x00\x00\00FAIL", 12, &bytes_written, nullptr);
        }
    }

	FlushFileBuffers(hPipe); // Ensures the client has read all the data before we disconnect; solves a race condition triggered/noticed in only some clients
    HeapFree(GetProcessHeap(), 0, rawRequest);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}
