// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <windows.h>
#include "../SDK/foobar2000/SDK/foobar2000.h"
#include "json.hpp"

// Declared in helpers.cpp
char* utf8cpy(char* dst, const char* src, size_t sizeDest);
bool get_meta_helper(service_ptr_t<metadb_handle> item, char *libraryData, const char *metaitem, size_t BUFSIZE);
nlohmann::json getLibraryInfo();
nlohmann::json getAllPlaylists();
nlohmann::json getPlaylistTracks(t_size playlist_id);
metadb_handle_list get_handles_from_urls(struct url *urls_in, size_t count);

template <typename F>
void in_main_thread(F f)
{
    struct in_main : main_thread_callback
    {
        void callback_run() override
        {
            f();
        }

        in_main(F f) : f(f) {}
        F f;
    };

    // Resolves a deadlock issue when trying to wait for an event, when this function is called FROM the main thread.
    // If we use add_callback, we'll wait forever -- the wait function waits for the callback to execute,
    // but the callback cannot execute until the wait function returns.
    if (core_api::is_main_thread())
        f();
    else
        static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<in_main>(f));
}

// Used internally
struct url {
    char urlString[260];
};

// Used by the announceHandler
struct songInfo {
	char artist[512];
	char title[512];
	char path[512];
};