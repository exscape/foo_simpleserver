#include "stdafx.h"
#include "actions.h"

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

    if (strncmp(requestType, "get_library", 32) == 0) {
        void *libraryInfo = nullptr;
        DWORD libraryInfoLength = 0;
        if (getLibraryInfo(&libraryInfo, &libraryInfoLength)) {
            WriteFile(hPipe, libraryInfo, libraryInfoLength, &bytes_written, nullptr);
            HeapFree(GetProcessHeap(), 0, libraryInfo);
            replySent = true;
            if (bytes_written != libraryInfoLength)
                fprintf(stderr, "foo_simpleserver: WriteFile failed! Asked to write %d bytes but %d bytes were written. GetLastError() == %d\n", libraryInfoLength, bytes_written, GetLastError());
            else
                success = true;
        }
        else {
            fprintf(stderr, "foo_simpleserver: getLibraryInfo failed, ignoring request\n");
        }
    }
    else if (strncmp(requestType, "play:", 32) == 0 || strncmp(requestType, "enqueue:", 32) == 0 || strncmp(requestType, "add_top:", 32) == 0 || strncmp(requestType, "replace:", 32) == 0) {
        struct url *urls = (struct url *)requestData;
        uint32_t count = requestDataLength / sizeof(struct url);

        if (strncmp(requestType, "play:", 32) == 0)
            play_tracks(urls, count);
        else if (strncmp(requestType, "enqueue:", 32) == 0)
            enqueue_tracks(urls, count);
        else if (strncmp(requestType, "add_top:", 32) == 0)
            add_tracks_to_queue_top(urls, count);
        else if (strncmp(requestType, "replace:", 32) == 0)
            replace_queue_with_tracks(urls, count);

        success = true;
    }

    if (!replySent) {
        if (success) {
            WriteFile(hPipe, "\x06\x00\x00\x00OK", 6, &bytes_written, nullptr);
        }
        else {
            fprintf(stderr, "foo_simpleserver: request failed: %s\n", requestType);
            WriteFile(hPipe, "\x08\x00\x00\00FAIL", 8, &bytes_written, nullptr);
        }
    }

    HeapFree(GetProcessHeap(), 0, rawRequest);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}
