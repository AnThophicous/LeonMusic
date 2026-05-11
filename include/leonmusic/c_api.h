#pragma once

#include <stdint.h>

#ifdef _WIN32
#define LEONMUSIC_EXPORT __declspec(dllexport)
#else
#define LEONMUSIC_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lm_handle lm_handle;

LEONMUSIC_EXPORT lm_handle* lm_create(void);
LEONMUSIC_EXPORT void lm_destroy(lm_handle* handle);

LEONMUSIC_EXPORT const char* lm_version(void);
LEONMUSIC_EXPORT const char* lm_search_music_json(lm_handle* handle, const char* query);
LEONMUSIC_EXPORT const char* lm_play_music_json(lm_handle* handle, const char* input);
LEONMUSIC_EXPORT const char* lm_enqueue_music_json(lm_handle* handle, const char* input);
LEONMUSIC_EXPORT const char* lm_pause_music_json(lm_handle* handle);
LEONMUSIC_EXPORT const char* lm_resume_music_json(lm_handle* handle);
LEONMUSIC_EXPORT const char* lm_set_volume_json(lm_handle* handle, float volume);
LEONMUSIC_EXPORT const char* lm_next_music_json(lm_handle* handle);
LEONMUSIC_EXPORT const char* lm_previous_music_json(lm_handle* handle);
LEONMUSIC_EXPORT const char* lm_clear_queue_json(lm_handle* handle);
LEONMUSIC_EXPORT const char* lm_stop_music_json(lm_handle* handle);
LEONMUSIC_EXPORT const char* lm_playback_state_json(lm_handle* handle);
LEONMUSIC_EXPORT const char* lm_start_server_json(lm_handle* handle, const char* request_json);
LEONMUSIC_EXPORT const char* lm_set_discord_client_id_json(lm_handle* handle, const char* client_id);
LEONMUSIC_EXPORT void lm_free_string(const char* value);

#ifdef __cplusplus
}
#endif
