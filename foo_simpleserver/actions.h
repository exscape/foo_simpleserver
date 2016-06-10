#include "stdafx.h"

// Starts playback on the queued tracks.
void start_playing_queue();

// Adds a list of tracks to the top of the play queue and immediately starts playing the first of them
void play_tracks(struct url *urls, uint32_t count);

// Adds a list of tracks to the *end* of the play queue
void enqueue_tracks(struct url *urls, uint32_t count);

// Replaces the play queue with the list of tracks
void replace_queue_with_tracks(struct url *urls, uint32_t count);

// Adds a list of tracks to the *top* of the play queue
void add_tracks_to_queue_top(struct url *urls, uint32_t count);
