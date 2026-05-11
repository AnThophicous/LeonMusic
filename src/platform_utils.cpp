#include "internal.hpp"

nlohmann::json leonmusic::trackToJson(const Track& track) {
    return {
        {"id", track.id},
        {"title", track.title},
        {"artist", track.artist},
        {"sourceUrl", track.sourceUrl},
        {"durationSeconds", track.durationSeconds},
        {"supports8D", track.supports8D}
    };
}

nlohmann::json leonmusic::playbackStateToJson(const PlaybackState& state) {
    nlohmann::json payload {
        {"playing", state.playing},
        {"paused", state.paused},
        {"immersive8DEnabled", state.immersive8DEnabled},
        {"currentTrack", nullptr},
        {"currentIndex", state.currentIndex},
        {"volume", state.volume},
        {"queue", nlohmann::json::array()}
    };

    if (state.currentTrack.has_value()) {
        payload["currentTrack"] = trackToJson(*state.currentTrack);
    }

    for (const auto& track : state.queue) {
        payload["queue"].push_back(trackToJson(track));
    }

    return payload;
}

nlohmann::json leonmusic::resultToJson(const Result& result) {
    return {
        {"status", static_cast<std::uint32_t>(result.code)},
        {"message", result.message}
    };
}
