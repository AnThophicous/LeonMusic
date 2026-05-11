#pragma once

#include <leonmusic/api.hpp>

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace leonmusic {

class SearchProvider {
public:
    virtual ~SearchProvider() = default;
    virtual SearchResponse search(std::string_view query) const = 0;
    virtual std::optional<Track> resolve(std::string_view input) const = 0;
};

std::unique_ptr<SearchProvider> makeDefaultSearchProvider();

struct ProcessResult {
    int exitCode {-1};
    std::string output {};
    std::string error {};
};

struct PreparedTrack {
    Track track {};
    std::filesystem::path playablePath {};
    bool temporaryFile {false};
};

[[nodiscard]] ProcessResult runProcess(const std::vector<std::string>& args);
[[nodiscard]] std::filesystem::path cacheDirectory();
[[nodiscard]] bool isYouTubeLikeInput(std::string_view input);
[[nodiscard]] bool isLikely8D(std::string_view text);
[[nodiscard]] Result enrichTrackMetadata(const std::filesystem::path& filePath, Track& track);
[[nodiscard]] Result prepareTrackForPlayback(const Track& input, PreparedTrack& prepared);
void prefetchTrack(const Track& input);

class DiscordRpcClient {
public:
    Result setClientId(std::string_view clientId);
    Result publishPlayback(const PlaybackState& state) const;

private:
    std::string clientId_ {};
    [[nodiscard]] Result sendPresence(const DiscordPresence& presence) const;
};

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    [[nodiscard]] Result play(const std::filesystem::path& filePath, bool immersive8D);
    [[nodiscard]] Result stop();
    [[nodiscard]] Result pause();
    [[nodiscard]] Result resume();
    [[nodiscard]] Result setVolume(float volume);
    void updateSpatialMotion(double seconds);
    [[nodiscard]] bool isInitialized() const;
    [[nodiscard]] bool isTrackComplete() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class LibraryService {
public:
    LibraryService();
    ~LibraryService();

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
    [[nodiscard]] Result setDiscordClientId(std::string_view clientId);

private:
    void playbackWorker();
    [[nodiscard]] Result loadCurrentTrackLocked();
    [[nodiscard]] Result startPlaybackLocked(const Track& track);
    [[nodiscard]] Result advanceLocked(bool forward);

    mutable std::mutex mutex_;
    std::unique_ptr<SearchProvider> searchProvider_;
    AudioPlayer audioPlayer_ {};
    PlaybackState state_ {};
    DiscordRpcClient discordRpc_;
    bool shutdown_ {false};
    std::thread worker_;
};

Result runCliCommand(LeonMusicApi& api, int argc, char** argv);
Result runJsonRpcServer(LeonMusicApi& api);
Result startPlatformIpcServer(const StartServerRequest& request);

[[nodiscard]] nlohmann::json trackToJson(const Track& track);
[[nodiscard]] nlohmann::json playbackStateToJson(const PlaybackState& state);
[[nodiscard]] nlohmann::json resultToJson(const Result& result);

}  // namespace leonmusic
