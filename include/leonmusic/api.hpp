#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace leonmusic {

enum class StatusCode : std::uint32_t {
    Ok = 0,
    InvalidArgument = 1,
    NotFound = 2,
    Unsupported = 3,
    InternalError = 4,
    SecurityViolation = 5
};

struct Result {
    StatusCode code {StatusCode::Ok};
    std::string message {};

    [[nodiscard]] bool ok() const noexcept { return code == StatusCode::Ok; }
};

struct Track {
    std::string id {};
    std::string title {};
    std::string artist {};
    std::string sourceUrl {};
    std::uint32_t durationSeconds {0};
    bool supports8D {false};
};

struct SearchResponse {
    Result result {};
    std::vector<Track> tracks {};
};

struct PlaybackState {
    bool playing {false};
    bool paused {false};
    std::optional<Track> currentTrack {};
    bool immersive8DEnabled {false};
    std::vector<Track> queue {};
    std::uint32_t currentIndex {0};
    float volume {1.0F};
};

struct StartServerRequest {
    std::string transport {"stdio-jsonrpc"};
    std::string endpoint {};
};

struct DiscordPresence {
    std::string details {};
    std::string state {};
    std::string largeImageKey {"leonmusic"};
    std::string largeImageText {"LeonMusic"};
};

class LeonMusicApi {
public:
    LeonMusicApi();

    [[nodiscard]] SearchResponse searchMusic(std::string_view query) const;
    [[nodiscard]] Result playMusic(std::string_view input);
    [[nodiscard]] Result enqueueMusic(std::string_view input);
    [[nodiscard]] Result pauseMusic();
    [[nodiscard]] Result resumeMusic();
    [[nodiscard]] Result setVolume(float volume);
    [[nodiscard]] Result nextMusic();
    [[nodiscard]] Result previousMusic();
    [[nodiscard]] Result clearQueue();
    [[nodiscard]] Result stopMusic();
    [[nodiscard]] PlaybackState playbackState() const;
    [[nodiscard]] Result startServer(const StartServerRequest& request) const;
    [[nodiscard]] Result setDiscordClientId(std::string_view clientId);
};

[[nodiscard]] std::string version();

}  // namespace leonmusic
