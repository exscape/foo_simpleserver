#include "stdafx.h"
#include "json.hpp"
#include <future>

using json = nlohmann::json;

// Copy a UTF-8 string to a size-limited buffer, ensuring both null termination and that no
// "half" UTF-8 characters are copied (in case of multi-byte characters).
// http://stackoverflow.com/a/27832746/1668576
char* utf8cpy(char* dst, const char* src, size_t sizeDest) {
    if (sizeDest) {
        size_t sizeSrc = strlen(src); // number of bytes not including null
        while (sizeSrc >= sizeDest){
            const char *lastByte = src + sizeSrc; // Initially, pointing to the null terminator.
            while (lastByte-- > src)
                if ((*lastByte & 0xC0) != 0x80) // Found the initial byte of the (potentially) multi-byte character (or found null).
                    break;

            sizeSrc = lastByte - src;
        }

        memcpy(dst, src, sizeSrc);
        dst[sizeSrc] = '\0';
    }

    return dst;
}

// Fetches a piece of meta information from an item, and copies it to a size-constrained buffer.
bool get_meta_helper(service_ptr_t<metadb_handle> item, char *libraryData, const char *metaitem, size_t BUFSIZE) {
    file_info_impl info;
    item->get_info(info);

    const char *p = nullptr;
    if (!strcmp(metaitem, "path"))
        p = item->get_location().get_path();
    else
        p = info.meta_get(metaitem, 0);

    memset(libraryData, 0, BUFSIZE);
    if (p != nullptr) {
        utf8cpy(libraryData, p, BUFSIZE);
        return true;
    }
    else
        return false;
}

std::string get_meta_string(service_ptr_t<metadb_handle> item, const char *metaitem) {
    file_info_impl info;
    item->get_info(info);

    const char *p = nullptr;
    if (!strcmp(metaitem, "path"))
        p = item->get_location().get_path();
    else
        p = info.meta_get(metaitem, 0);

    return std::string(p == nullptr ? "" : p);
}

// Fetches information about the entire foobar library (every track's artist, title, path and so on)
// Data is returned in CBOR format.
json getLibraryInfo() {
    // We can't access the library from a thread, so we need to use a main_thread_callback via in_main_thread.
    HANDLE hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
    metadb_handle_list library;
    in_main_thread([&] {
        static_api_ptr_t<library_manager>()->get_all_items(library);
        SetEvent(hFinished);
    });
    WaitForSingleObject(hFinished, INFINITE);

    json response;
    for (size_t trackid = 0; trackid < library.get_count(); trackid++) {
        auto artist = get_meta_string(library.get_item(trackid), "artist");
        if (artist.size() < 1)
            artist = get_meta_string(library.get_item(trackid), "album artist");

        auto discnumber = atoi(get_meta_string(library.get_item(trackid), "discnumber").c_str());
        auto tracknumber = atoi(get_meta_string(library.get_item(trackid), "tracknumber").c_str());

        // TODO: support dates such as 2017-04-28 (extract year)
        auto tmp = get_meta_string(library.get_item(trackid), "date");
        int year = 0;
        if (tmp.length() == 4 && (tmp[0] == '1' || tmp[0] == '2'))
            year = atoi(tmp.c_str());

        response.push_back({
            { "artist", artist },
            { "album", get_meta_string(library.get_item(trackid), "album") },
            { "title", get_meta_string(library.get_item(trackid), "title") },
            { "path", get_meta_string(library.get_item(trackid), "path") },
            { "genre", get_meta_string(library.get_item(trackid), "genre") },
            { "discnumber", discnumber },
            { "tracknumber", tracknumber },
            { "year", year }
        });
    }

    return response;
}

json getPlaylistTracks(t_size playlist_id) {
    // We can't access playlists from a thread, so we need to use a main_thread_callback via in_main_thread.
    HANDLE hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
    metadb_handle_list tracks;
    in_main_thread([&] {
		static_api_ptr_t<playlist_manager>()->playlist_get_all_items(playlist_id, tracks);
        SetEvent(hFinished);
    });
    WaitForSingleObject(hFinished, INFINITE);

    json response;
    for (size_t trackid = 0; trackid < tracks.get_count(); trackid++) {
        auto artist = get_meta_string(tracks.get_item(trackid), "artist");
        if (artist.size() < 1)
            artist = get_meta_string(tracks.get_item(trackid), "album artist");

        auto discnumber = atoi(get_meta_string(tracks.get_item(trackid), "discnumber").c_str());
        auto tracknumber = atoi(get_meta_string(tracks.get_item(trackid), "tracknumber").c_str());

        // TODO: support dates such as 2017-04-28 (extract year)
        auto tmp = get_meta_string(tracks.get_item(trackid), "date");
        int year = 0;
        if (tmp.length() == 4 && (tmp[0] == '1' || tmp[0] == '2'))
            year = atoi(tmp.c_str());

        response.push_back({
            { "artist", artist },
            { "album", get_meta_string(tracks.get_item(trackid), "album") },
            { "title", get_meta_string(tracks.get_item(trackid), "title") },
            { "path", get_meta_string(tracks.get_item(trackid), "path") },
            { "genre", get_meta_string(tracks.get_item(trackid), "genre") },
            { "discnumber", discnumber },
            { "tracknumber", tracknumber },
            { "year", year }
        });
    }

    return response;
}

// Fetches information about the entire foobar library (every track's artist, title, path and so on)
json getAllPlaylists() {
	// We can't access playlists from a thread, so we need to use a main_thread_callback via in_main_thread.
	HANDLE hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
	t_size numPlaylists = 0;
	in_main_thread([&] {
		numPlaylists = static_api_ptr_t<playlist_manager>()->get_playlist_count();
		SetEvent(hFinished);
	});
	WaitForSingleObject(hFinished, INFINITE);


    if (numPlaylists > 1000) {
        // Surely something is wrong here, e.g. pfc_infinite was returned
        return {
            { "error", "foobar_error" },
            { "error_description", "unable to read playlists from foobar" }
        };
    }

    json response;

	hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
	in_main_thread([&] {
		for (t_size playlistid = 0; playlistid < numPlaylists; playlistid++) {
			pfc::string8 outstr;
			static_api_ptr_t<playlist_manager>()->playlist_get_name(playlistid, outstr);

            response.push_back({
                { "name", std::string(outstr.c_str()) },
                { "num_tracks", static_api_ptr_t<playlist_manager>()->playlist_get_item_count(playlistid) },
                { "id", playlistid }
            });
		}
		SetEvent(hFinished);
	});
	WaitForSingleObject(hFinished, INFINITE);

    return response;
}

// Must NOT be called from the main thread!
// This function is, unfortunately, rather complex. It's not *too* difficult, though -- this description actually looks worse
// than the code itself, once you grasp it.
// I came up with a better solution, but couldn't get it compile due to internal compiler errors and compiler stack overflows. :(
//
// The entire function needs to run in the main thread, so the outer part is just a main_thread_callback,
// followed by scheduling the callback to run (add_callback) and extracting the return value from the associated future.
//
// Inside the main_thread_callback subclass, we *ALSO* use a promise, as we need to use a callback *inside* the main thread, too!
// There, we set up a process_locations_notify callback, which sets the "internal" promise to the return value.
// The execution works like this:
// 1) Set up a main_task instance
// 2) Add it to the execution queue
// 3) Wait until it has finished executing (handlesTask->get_future().get().get()), and extract the value from the inner future
//
// While the future is waiting, the main task will:
// 1) Add the URLs to a pfc::list_t of char pointers,
// 2) Set up a callback (process_callback)
// 3) Ask foobar to perform the actual work, and then call the callback when finished
// 4) Set the "outer" promise to contain the inner one, just after calling process_locations_async
// 5) Run on_completion(), which sets the value of the inner future
//    That finally allows the get() call at the function's end to return, and the value we get is returned to the function caller.
metadb_handle_list get_handles_from_urls(struct url *urls_in, size_t count) {
    class main_task : public main_thread_callback {
    private:
        std::promise<std::future<metadb_handle_list>> promise;
        struct url *urls_in;
        size_t count;

    public:
        main_task(struct url *urls_in, size_t count) : urls_in(urls_in), count(count) {}
        virtual void callback_run() override {
            metadb_handle_list library;
            pfc::list_t<const char *> urls;

            for (size_t i = 0; i < count; i++) {
                urls.add_item(urls_in[i].urlString);
            }

            struct process_callback : public process_locations_notify {
            private:
                std::promise<metadb_handle_list> cb_promise;
            public:
                void on_completion(const pfc::list_base_const_t< metadb_handle_ptr > &p_items) override {
                    cb_promise.set_value(p_items);
                }
                void on_aborted() override {
                    cb_promise.set_value(pfc::list_t<metadb_handle_ptr>());
                }
                std::future<metadb_handle_list> get_future() {
                    return cb_promise.get_future();
                }
            };

            service_ptr_t<struct process_callback> cb_instance = new service_impl_t<process_callback>();
            static_api_ptr_t<playlist_incoming_item_filter_v2>()->process_locations_async(urls, NULL, NULL, NULL, core_api::get_main_window(), cb_instance);

            promise.set_value(cb_instance->get_future());
        }

        std::future<std::future<metadb_handle_list>> get_future() {
            return promise.get_future();
        }
    };

    service_ptr_t<main_task> handlesTask(new service_impl_t<main_task>(urls_in, count));
    static_api_ptr_t<main_thread_callback_manager>()->add_callback(handlesTask);
    metadb_handle_list handles = handlesTask->get_future().get().get();

    return handles;
}