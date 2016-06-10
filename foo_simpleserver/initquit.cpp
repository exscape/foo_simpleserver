#include "stdafx.h"
#include <thread>

// server.cpp
void serverFunc();
void announceServerFunc();

class myinitquit : public initquit {
public:
    void on_init() {
#ifndef NDEBUG
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif

        try {
            std::thread serverThread(serverFunc);
            serverThread.detach();
        }
        catch (...) {
            popup_message::g_complain("foo_simpleserver: unable to create main server thread! Component will not work.");
        }

        try {
            std::thread announcerThread(announceServerFunc);
            announcerThread.detach();
        }
        catch (...) {
            popup_message::g_complain("foo_simpleserver: unable to create announcer thread! That part of the component will not work.");
        }
    }
    void on_quit() {
    }
};

static initquit_factory_t<myinitquit> g_myinitquit_factory;