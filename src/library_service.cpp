#include "internal.hpp"

#include <chrono>

using leonmusic::LibraryService;
using leonmusic::PlaybackState;
using leonmusic::Result;
using leonmusic::SearchResponse;
using leonmusic::StatusCode;
using leonmusic::Track;

LibraryService::LibraryService() : searchProvider_(makeDefaultSearchProvider()) {
    worker_ = std::thread(&LibraryService::playbackWorker, this);
}

LibraryService::~LibraryService() {
    {
        std::scoped_lock lock(mutex_);
        shutdown_ = true;
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    const Result stopResult = audioPlayer_.stop();
    (void)stopResult;
}

SearchResponse LibraryService::searchMusic(std::string_view query) const {
    return searchProvider_->search(query);
}

Result LibraryService::playMusic(std::string_view input) {
    std::scoped_lock lock(mutex_);
    const auto resolvedTrack = searchProvider_->resolve(input);
    if (!resolvedTrack.has_value()) {
        return {StatusCode::NotFound, "could not resolve the requested track or video id"};
    }

    state_.queue.clear();
    state_.queue.push_back(*resolvedTrack);
    state_.currentIndex = 0;
    return loadCurrentTrackLocked();
}

Result LibraryService::enqueueMusic(std::string_view input) {
    std::scoped_lock lock(mutex_);
    const auto resolvedTrack = searchProvider_->resolve(input);
    if (!resolvedTrack.has_value()) {
        return {StatusCode::NotFound, "could not resolve the requested track or video id"};
    }

    state_.queue.push_back(*resolvedTrack);
    prefetchTrack(*resolvedTrack);
    if (!state_.currentTrack.has_value()) {
        state_.currentIndex = 0;
        return loadCurrentTrackLocked();
    }

    return {StatusCode::Ok, "track added to queue"};
}

Result LibraryService::pauseMusic() {
    std::scoped_lock lock(mutex_);
    const Result pauseResult = audioPlayer_.pause();
    if (pauseResult.ok()) {
        state_.paused = true;
        state_.playing = true;
    }
    return pauseResult;
}

Result LibraryService::resumeMusic() {
    std::scoped_lock lock(mutex_);
    const Result resumeResult = audioPlayer_.resume();
    if (resumeResult.ok()) {
        state_.paused = false;
        state_.playing = true;
    }
    return resumeResult;
}

Result LibraryService::setVolume(float volume) {
    if (volume < 0.0F || volume > 2.0F) {
        return {StatusCode::InvalidArgument, "volume must be between 0.0 and 2.0"};
    }

    std::scoped_lock lock(mutex_);
    state_.volume = volume;
    const Result volumeResult = audioPlayer_.setVolume(volume);
    if (!volumeResult.ok() && state_.currentTrack.has_value()) {
        return volumeResult;
    }
    return {StatusCode::Ok, "volume state updated"};
}

Result LibraryService::nextMusic() {
    std::scoped_lock lock(mutex_);
    return advanceLocked(true);
}

Result LibraryService::previousMusic() {
    std::scoped_lock lock(mutex_);
    return advanceLocked(false);
}

Result LibraryService::clearQueue() {
    std::scoped_lock lock(mutex_);
    const Result stopResult = audioPlayer_.stop();
    (void)stopResult;
    state_.queue.clear();
    state_.currentTrack.reset();
    state_.currentIndex = 0;
    state_.playing = false;
    state_.paused = false;
    state_.immersive8DEnabled = false;
    return discordRpc_.publishPlayback(state_);
}

Result LibraryService::stopMusic() {
    std::scoped_lock lock(mutex_);
    const Result stopResult = audioPlayer_.stop();
    (void)stopResult;
    state_.playing = false;
    state_.paused = false;
    state_.immersive8DEnabled = false;
    return discordRpc_.publishPlayback(state_);
}

PlaybackState LibraryService::playbackState() const {
    std::scoped_lock lock(mutex_);
    return state_;
}

Result LibraryService::setDiscordClientId(std::string_view clientId) {
    return discordRpc_.setClientId(clientId);
}

void LibraryService::playbackWorker() {
    double phaseSeconds = 0.0;
    while (true) {
        {
            std::scoped_lock lock(mutex_);
            if (shutdown_) {
                break;
            }

            if (state_.playing && !state_.paused && audioPlayer_.isTrackComplete()) {
                const Result advanceResult = advanceLocked(true);
                (void)advanceResult;
            } else if (state_.playing && !state_.paused && state_.immersive8DEnabled) {
                audioPlayer_.updateSpatialMotion(phaseSeconds);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        phaseSeconds += 0.2;
    }
}

Result LibraryService::loadCurrentTrackLocked() {
    if (state_.queue.empty() || state_.currentIndex >= state_.queue.size()) {
        state_.playing = false;
        state_.paused = false;
        state_.currentTrack.reset();
        state_.immersive8DEnabled = false;
        return {StatusCode::NotFound, "queue is empty"};
    }

    return startPlaybackLocked(state_.queue[state_.currentIndex]);
}

Result LibraryService::startPlaybackLocked(const Track& track) {
    PreparedTrack prepared {};
    const Result prepareResult = prepareTrackForPlayback(track, prepared);
    if (!prepareResult.ok()) {
        state_.playing = false;
        return prepareResult;
    }

    state_.currentTrack = prepared.track;
    state_.queue[state_.currentIndex] = prepared.track;
    state_.paused = false;
    state_.immersive8DEnabled = prepared.track.supports8D;

    const Result audioResult = audioPlayer_.play(prepared.playablePath, prepared.track.supports8D);
    if (!audioResult.ok()) {
        state_.playing = false;
        return audioResult;
    }

    state_.playing = true;
    const Result volumeResult = audioPlayer_.setVolume(state_.volume);
    (void)volumeResult;
    const Result rpcResult = discordRpc_.publishPlayback(state_);
    if (!rpcResult.ok()) {
        return rpcResult;
    }

    return {StatusCode::Ok, "playback state updated"};
}

Result LibraryService::advanceLocked(bool forward) {
    if (state_.queue.empty()) {
        return {StatusCode::NotFound, "queue is empty"};
    }

    if (forward) {
        if (state_.currentIndex + 1 >= state_.queue.size()) {
            const Result stopResult = audioPlayer_.stop();
            (void)stopResult;
        state_.playing = false;
        state_.paused = false;
            state_.currentTrack.reset();
            state_.immersive8DEnabled = false;
            return discordRpc_.publishPlayback(state_);
        }
        ++state_.currentIndex;
    } else {
        if (state_.currentIndex == 0) {
            return startPlaybackLocked(state_.queue.front());
        }
        --state_.currentIndex;
    }

    return loadCurrentTrackLocked();
}
