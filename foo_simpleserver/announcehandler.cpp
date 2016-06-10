#include "stdafx.h"
#include "actions.h"
#include <future>
#include <iostream>
#include <thread>

// announceCV, announceCount and info are all protected by this mutex
std::mutex mutex;
std::condition_variable announceCV;
int announceCount = 0; // Number of track change announcements made, used to wake threads
struct songInfo info;

bool updateSongInfo() {
    // Read the currently playing track, and fill in the info struct
    // NOTE: The thread calling this must already hold the mutex that protects the "info" struct! See server.cpp.

    HANDLE hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
    bool didUpdate = false;

    in_main_thread([&] {
        metadb_handle_ptr now;
        if (static_api_ptr_t<playback_control>()->get_now_playing(now)) {
            get_meta_helper(now, info.artist, "artist", 512);
            get_meta_helper(now, info.title, "title", 512);
            get_meta_helper(now, info.path, "path", 512);
            didUpdate = true;
        }
        else
            memset(&info, 0, sizeof(struct songInfo));

        SetEvent(hFinished);
    });

    WaitForSingleObject(hFinished, INFINITE);

    return didUpdate;
}

DWORD doAnnounce(HANDLE hPipe) {
    if (!updateSongInfo()) {
        // No update happened, so there's no point in announcing.
        return ERROR_SUCCESS;
    }
    DWORD bytes_written = 0;
    BOOL success = WriteFile(hPipe, &info, sizeof(info), &bytes_written, NULL);

    if (!success) {
        DWORD err = GetLastError();
        std::cout << "doAnnounce failed to write! Last error is" << err;
        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA || ERROR_PIPE_NOT_CONNECTED) {
            // The other end is disconnected (ERROR_NO_DATA is the only error I've actually observed in practice, on Windows 10),
            // so we can shut down this server thread.
            return err;
        }

        // Other errors are assumed to be temporary and thus ignored.
    }
    else {
        return ERROR_SUCCESS;
    }
}

void announceHandler(HANDLE hPipe) {
    // Send an initial announcement immediately on connect
    if (doAnnounce(hPipe) != 0) {
        // Broken pipe, abort thread.
        // I'm not sure if these two are necessary or not, but they shouldn't hurt.
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        return;
    }

    while (true) {
        int oldCount = announceCount;

        // Wait for a track change notification to wake us up (see server.cpp)
        std::unique_lock<std::mutex> lock(mutex);
        announceCV.wait(lock, [&] { return oldCount != announceCount; });

        // Send the announcement
        if (doAnnounce(hPipe) != 0) {
            // Broken pipe, abort thread.
            // I'm not sure if these two are necessary or not, but they shouldn't hurt.
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            return;
        }
    }
}