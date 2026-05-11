#include "internal.hpp"

#include <algorithm>
#include <cctype>

namespace leonmusic {

namespace {

bool hasBlockedTerms(std::string_view value) {
    std::string lower {};
    lower.reserve(value.size());
    for (const char ch : value) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    static constexpr std::string_view blockedTerms[] = {
        "porn",
        "pornhub",
        "xvideos",
        "xnxx",
        "hentai"
    };

    return std::any_of(std::begin(blockedTerms), std::end(blockedTerms), [&](std::string_view term) {
        return lower.find(term) != std::string::npos;
    });
}

}  // namespace

Result DiscordRpcClient::setClientId(std::string_view clientId) {
    if (clientId.empty()) {
        return {StatusCode::InvalidArgument, "discord client id is required"};
    }

    clientId_ = std::string(clientId);
    return {StatusCode::Ok, "discord rpc configured"};
}

Result DiscordRpcClient::publishPlayback(const PlaybackState& state) const {
    if (clientId_.empty()) {
        return {StatusCode::Ok, "discord rpc disabled"};
    }

    if (!state.playing || !state.currentTrack.has_value()) {
        return sendPresence({"Idle", "No active playback", "leonmusic", "LeonMusic"});
    }

    const auto& track = *state.currentTrack;

    // Security policy: presence is derived only from resolved playback state, never caller text.
    // If metadata looks risky or low-quality, the RPC falls back to neutral labels.
    const bool blocked = hasBlockedTerms(track.title) || hasBlockedTerms(track.artist);
    const std::string title = blocked ? "Private track" : track.title;
    const std::string artist = blocked ? "Metadata hidden" : track.artist;

    return sendPresence({
        title,
        artist,
        "leonmusic",
        "LeonMusic"
    });
}

Result DiscordRpcClient::sendPresence(const DiscordPresence& presence) const {
    (void)presence;
    return {
        StatusCode::Ok,
        "discord rpc placeholder accepted only internal player state"
    };
}

}  // namespace leonmusic

