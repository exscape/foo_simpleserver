#include "stdafx.h"
#include "actions.h"
#include <vector>

#define REQUEST_BUFSIZE 65536 // a few times more than enough to allow queueing the foobar hard limit of 64 tracks


void clientHandler(HANDLE hPipe) {
    DWORD bytes_read = 0;
    char *rawRequest = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, REQUEST_BUFSIZE);
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
    uint32_t requestDataLength = *(uint32_t *)rawRequest - 32; // subtract the request size itself (4 bytes)  and request type (28 bytes)
    char *requestType = rawRequest + 4;
    char *requestData = rawRequest + 32;

    bool success = false;
    bool replySent = false;

    if (strncmp(requestType, "get_library", 28) == 0) {
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
    else if (strncmp(requestType, "get_playlists", 28) == 0) {
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
    else if (strncmp(requestType, "get_playlist_tracks:", 28) == 0) {
		t_size playlist_id = *((t_size *)requestData);
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
    else if (strncmp(requestType, "play:", 28) == 0 || strncmp(requestType, "enqueue:", 28) == 0 || strncmp(requestType, "add_top:", 28) == 0 || strncmp(requestType, "replace:", 28) == 0) {
        struct url *urls = (struct url *)requestData;
        uint32_t count = requestDataLength / sizeof(struct url);

        if (strncmp(requestType, "play:", 28) == 0)
            play_tracks(urls, count);
        else if (strncmp(requestType, "enqueue:", 28) == 0)
            enqueue_tracks(urls, count);
        else if (strncmp(requestType, "add_top:", 28) == 0)
            add_tracks_to_queue_top(urls, count);
        else if (strncmp(requestType, "replace:", 28) == 0)
            replace_queue_with_tracks(urls, count);

        success = true;
    }

    if (!replySent) {
        if (success) {
            WriteFile(hPipe, "\x01\x00\x00\x00\x06\x00\x00\x00OK", 10, &bytes_written, nullptr);
        }
        else {
            fprintf(stderr, "foo_simpleserver: request failed: %s\n", requestType);
            WriteFile(hPipe, "\x01\x00\x00\x00\x08\x00\x00\00FAIL", 12, &bytes_written, nullptr);
        }
    }

	FlushFileBuffers(hPipe); // Ensures the client has read all the data before we disconnect; solves a race condition triggered/noticed in only some clients
    HeapFree(GetProcessHeap(), 0, rawRequest);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}
