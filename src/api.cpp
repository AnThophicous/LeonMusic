#include "internal.hpp"

#include <leonmusic/version.hpp>

#include <cstring>
#include <cstdlib>
#include <new>
#include <utility>

using nlohmann::json;
using leonmusic::LeonMusicApi;
using leonmusic::PlaybackState;
using leonmusic::Result;
using leonmusic::SearchResponse;
using leonmusic::StartServerRequest;
using leonmusic::StatusCode;

static leonmusic::LibraryService& service() {
    static leonmusic::LibraryService instance {};
    return instance;
}

LeonMusicApi::LeonMusicApi() = default;

SearchResponse LeonMusicApi::searchMusic(std::string_view query) const {
    return service().searchMusic(query);
}

Result LeonMusicApi::playMusic(std::string_view input) {
    return service().playMusic(input);
}

Result LeonMusicApi::enqueueMusic(std::string_view input) {
    return service().enqueueMusic(input);
}

Result LeonMusicApi::pauseMusic() {
    return service().pauseMusic();
}

Result LeonMusicApi::resumeMusic() {
    return service().resumeMusic();
}

Result LeonMusicApi::setVolume(float volume) {
    return service().setVolume(volume);
}

Result LeonMusicApi::nextMusic() {
    return service().nextMusic();
}

Result LeonMusicApi::previousMusic() {
    return service().previousMusic();
}

Result LeonMusicApi::clearQueue() {
    return service().clearQueue();
}

Result LeonMusicApi::stopMusic() {
    return service().stopMusic();
}

PlaybackState LeonMusicApi::playbackState() const {
    return service().playbackState();
}

Result LeonMusicApi::startServer(const StartServerRequest& request) const {
    if (request.transport == "stdio-jsonrpc") {
        LeonMusicApi api {};
        return leonmusic::runJsonRpcServer(api);
    }

    return leonmusic::startPlatformIpcServer(request);
}

Result LeonMusicApi::setDiscordClientId(std::string_view clientId) {
    return service().setDiscordClientId(clientId);
}

std::string leonmusic::version() {
    return LEONMUSIC_VERSION;
}

struct lm_handle {
    LeonMusicApi api {};
};

static const char* to_owned_cstr(const json& payload) {
    const std::string dumped = payload.dump();
    auto* buffer = new (std::nothrow) char[dumped.size() + 1];
    if (!buffer) {
        return nullptr;
    }

    std::memcpy(buffer, dumped.c_str(), dumped.size() + 1);
    return buffer;
}

extern "C" {

lm_handle* lm_create(void) {
    return new (std::nothrow) lm_handle {};
}

void lm_destroy(lm_handle* handle) {
    delete handle;
}

const char* lm_version(void) {
    static std::string v = leonmusic::version();
    return v.c_str();
}

const char* lm_search_music_json(lm_handle* handle, const char* query) {
    if (!handle || !query) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle and query are required"}));
    }

    const SearchResponse response = handle->api.searchMusic(query);
    json payload = leonmusic::resultToJson(response.result);
    payload["tracks"] = json::array();
    for (const auto& track : response.tracks) {
        payload["tracks"].push_back(leonmusic::trackToJson(track));
    }
    return to_owned_cstr(payload);
}

const char* lm_play_music_json(lm_handle* handle, const char* input) {
    if (!handle || !input) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle and input are required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.playMusic(input)));
}

const char* lm_enqueue_music_json(lm_handle* handle, const char* input) {
    if (!handle || !input) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle and input are required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.enqueueMusic(input)));
}

const char* lm_pause_music_json(lm_handle* handle) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.pauseMusic()));
}

const char* lm_resume_music_json(lm_handle* handle) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.resumeMusic()));
}

const char* lm_set_volume_json(lm_handle* handle, float volume) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.setVolume(volume)));
}

const char* lm_next_music_json(lm_handle* handle) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.nextMusic()));
}

const char* lm_previous_music_json(lm_handle* handle) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.previousMusic()));
}

const char* lm_clear_queue_json(lm_handle* handle) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.clearQueue()));
}

const char* lm_stop_music_json(lm_handle* handle) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.stopMusic()));
}

const char* lm_playback_state_json(lm_handle* handle) {
    if (!handle) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle is required"}));
    }
    return to_owned_cstr(leonmusic::playbackStateToJson(handle->api.playbackState()));
}

const char* lm_start_server_json(lm_handle* handle, const char* request_json) {
    if (!handle || !request_json) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle and request_json are required"}));
    }

    const auto parsed = json::parse(request_json, nullptr, false);
    if (parsed.is_discarded()) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "request_json is not valid JSON"}));
    }

    StartServerRequest request {};
    request.transport = parsed.value("transport", "stdio-jsonrpc");
    request.endpoint = parsed.value("endpoint", "");
    return to_owned_cstr(leonmusic::resultToJson(handle->api.startServer(request)));
}

const char* lm_set_discord_client_id_json(lm_handle* handle, const char* client_id) {
    if (!handle || !client_id) {
        return to_owned_cstr(leonmusic::resultToJson({StatusCode::InvalidArgument, "handle and client_id are required"}));
    }
    return to_owned_cstr(leonmusic::resultToJson(handle->api.setDiscordClientId(client_id)));
}

void lm_free_string(const char* value) {
    delete[] value;
}

}  // extern "C"
