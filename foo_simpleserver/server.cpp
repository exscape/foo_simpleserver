#include <iostream>
#include <thread>
#include "stdafx.h"
#include "clienthandler.h"

void clientHandler(HANDLE);
void announceHandler(HANDLE);

// Declared in announcehandler.cpp
extern std::condition_variable announceCV;
extern std::mutex mutex;
extern int announceCount;

class play_callback_handler : public play_callback_static
{
    void on_playback_new_track(metadb_handle_ptr p_track) {
        // Wake all waiting threads
        {
            std::unique_lock<std::mutex> lock(mutex);
            announceCount++;
        }
        announceCV.notify_all();
    }

    void on_playback_edited(metadb_handle_ptr p_track) {}
    void on_playback_starting(play_control::t_track_command p_command, bool p_paused) {}
    void on_playback_dynamic_info_track(const file_info & p_info) {}
    void on_playback_seek(double p_time) {}
    void on_playback_pause(bool p_state) {}
    void on_playback_stop(play_control::t_stop_reason p_reason) {}
    void on_playback_dynamic_info(const file_info & p_info) {}
    void on_playback_time(double p_time) {}
    void on_volume_change(float p_new_val) {}

    unsigned int get_flags() {
        return flag_on_playback_new_track;
    }
};

void serverFunc() {
    // Creates a named pipe, accepts a connection (ConnectNamePipe) and creates a thread to handle that client.
    // ConnectNamedPipe is blocking, but since this all happens in a background thread, that is fine.

    while (true) {
        HANDLE hPipe = CreateNamedPipe(
            TEXT("\\\\.\\pipe\\foo_simpleserver"), // pipe name
            PIPE_ACCESS_DUPLEX,       // read/write access
            PIPE_TYPE_MESSAGE |       // message type pipe
            PIPE_READMODE_MESSAGE |   // message-read mode
            PIPE_WAIT,                // blocking mode
            PIPE_UNLIMITED_INSTANCES, // max. instances
            8192,                     // output buffer size
            8192,                     // input buffer size
            7500,                     // client time-out (in ms)
            NULL);                    // default security attribute 

        if (hPipe == INVALID_HANDLE_VALUE) {
            popup_message::g_complain("foo_simpleserver: unable to create named pipe! Component will not work.");
            return;
        }

        // If the pipe connects *prior* to this call finishing(?), the function will fail, but the pipe will still be connected and usable.
        bool connected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected) {
            CloseHandle(hPipe);
            continue;
        }

        try {
//            std::thread handlerThread(clientHandler, (void *)hPipe);
            std::thread handlerThread([=]() {
                ClientHandler handler(hPipe);
                handler.go();
            });
            handlerThread.detach();
        }
        catch (...) {
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            return;
        }
    }
}

// This method is similar to the main server in creation, but different in usage.
// The main server is used to receive commands, and act on them.
// This only has one task: send out announcements when the current track changes. It does not accept commands or input of any kind.
void announceServerFunc() {
    // Creates a named pipe, accepts a connection (ConnectNamePipe) and creates a thread to handle that client.
    // ConnectNamedPipe is blocking, but since this all happens in a background thread, that is fine.

    while (true) {
        HANDLE hPipe = CreateNamedPipe(
            TEXT("\\\\.\\pipe\\foo_simpleserver_announce"), // pipe name
            PIPE_ACCESS_DUPLEX,       // read/write access
            PIPE_TYPE_MESSAGE |       // message type pipe
            PIPE_READMODE_MESSAGE |   // message-read mode
            PIPE_WAIT,                // blocking mode
            PIPE_UNLIMITED_INSTANCES, // max. instances
            8192,                     // output buffer size
            8192,                     // input buffer size
            7500,                     // client time-out (in ms)
            NULL);                    // default security attribute 

        if (hPipe == INVALID_HANDLE_VALUE) {
            popup_message::g_complain("foo_simpleserver: unable to create named pipe! Announce part of the component will not work.");
            return;
        }

        // If the pipe connects *prior* to this call finishing(?), the function will fail, but the pipe will still be connected and usable.
        bool connected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected) {
            CloseHandle(hPipe);
            continue;
        }

        try {
            std::thread handlerThread(announceHandler, hPipe);
            handlerThread.detach();
        }
        catch (...) {
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            return;
        }
    }
}

static play_callback_static_factory_t<play_callback_handler> g_playback;
