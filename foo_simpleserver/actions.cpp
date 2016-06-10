#include "stdafx.h"
#include "actions.h"

// Starts playback on the queued tracks.
void start_playing_queue() {
    in_main_thread([] {
        auto p_control = static_api_ptr_t<playback_control>();

        if (p_control->is_playing() || p_control->is_paused())
            p_control->next();
        else
            p_control->play_or_unpause();
    });
}

// Must NOT be called from the main thread.
void play_tracks(struct url *urls, uint32_t count) {
    add_tracks_to_queue_top(urls, count);
    start_playing_queue();
}

// Must NOT be called from the main thread.
// Adds a list of tracks to the *end* of the play queue.
// Blocks until the task has finished, since the urls pointer may become invalid during execution otherwise.
void enqueue_tracks(struct url *urls, uint32_t count) {
    HANDLE hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);

    metadb_handle_list tracks = get_handles_from_urls(urls, count);

    in_main_thread([=] {
        for (size_t i = 0; i < tracks.get_count(); i++) {
            static_api_ptr_t<playlist_manager>()->queue_add_item(tracks.get_item(i));
        }

        SetEvent(hFinished);
    });

    WaitForSingleObject(hFinished, INFINITE);
}

// Must NOT be called from the main thread.
// Replaces the play queue with the list of tracks
// Blocks until the task has finished, since the urls pointer may become invalid during execution otherwise.
void replace_queue_with_tracks(struct url *urls, uint32_t count) {
    HANDLE hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);

    in_main_thread([=] {
        static_api_ptr_t<playlist_manager>()->queue_flush();
        SetEvent(hFinished);
    });

    WaitForSingleObject(hFinished, INFINITE);

    enqueue_tracks(urls, count);
}

// Must NOT be called from the main thread.
// Adds a list of tracks to the *top* of the play queue
// Blocks until the task has finished, since the urls pointer may become invalid during execution otherwise.
void add_tracks_to_queue_top(struct url *urls, uint32_t count) {
    HANDLE hFinished = CreateEvent(NULL, TRUE, FALSE, NULL);

    // First, fetch the current queue contents; this must happen in the main thread.
    pfc::list_t<t_playback_queue_item> queueContents;
    in_main_thread([&] {
        static_api_ptr_t<playlist_manager>()->queue_get_contents(queueContents);
        SetEvent(hFinished);
    });
    WaitForSingleObject(hFinished, INFINITE);

    // Next, add our tracks to the top of the queue; this must NOT happen in the main thread.
    if (queueContents.get_count() == 0) {
        enqueue_tracks(urls, count);
        return;
    }
    else
        replace_queue_with_tracks(urls, count);

    // Finally, add back the tracks that were in the queue to begin with; this must happen in the main thread.
    ResetEvent(hFinished);
    in_main_thread([&] {
        for (t_size i = 0; i < queueContents.get_count(); i++) {
            static_api_ptr_t<playlist_manager>()->queue_add_item(queueContents.get_item(i).m_handle);
        }

        SetEvent(hFinished);
    });
    WaitForSingleObject(hFinished, INFINITE);
}