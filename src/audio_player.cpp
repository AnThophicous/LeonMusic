#include "internal.hpp"

#include <cmath>
#include <filesystem>
#include <string>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

using leonmusic::AudioPlayer;
using leonmusic::Result;
using leonmusic::StatusCode;
using leonmusic::Track;

struct AudioPlayer::Impl {
    ma_engine engine {};
    ma_sound sound {};
    bool engineReady {false};
    bool soundReady {false};
    bool immersive8D {false};
    bool paused {false};
    ma_uint64 pausedFrame {0};
};

static Result makeAudioError(std::string_view message) {
    return {StatusCode::InternalError, std::string(message)};
}

AudioPlayer::AudioPlayer() : impl_(std::make_unique<Impl>()) {
    if (ma_engine_init(nullptr, &impl_->engine) == MA_SUCCESS) {
        impl_->engineReady = true;
        ma_engine_listener_set_position(&impl_->engine, 0, 0.0F, 0.0F, 0.0F);
        ma_engine_listener_set_direction(&impl_->engine, 0, 0.0F, 0.0F, -1.0F);
        ma_engine_listener_set_world_up(&impl_->engine, 0, 0.0F, 1.0F, 0.0F);
    }
}

AudioPlayer::~AudioPlayer() {
    const Result stopResult = stop();
    (void)stopResult;
    if (impl_ && impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
    }
}

Result AudioPlayer::play(const std::filesystem::path& filePath, bool immersive8D) {
    if (!impl_->engineReady) {
        return makeAudioError("miniaudio engine could not be initialized");
    }

    if (!std::filesystem::exists(filePath)) {
        return {StatusCode::NotFound, "prepared audio file does not exist"};
    }

    const Result stopResult = stop();
    (void)stopResult;

    const std::string path = filePath.string();
    if (ma_sound_init_from_file(&impl_->engine, path.c_str(), 0, nullptr, nullptr, &impl_->sound) != MA_SUCCESS) {
        return makeAudioError("failed to decode the selected audio file");
    }

    impl_->soundReady = true;
    impl_->immersive8D = immersive8D;
    impl_->paused = false;

    ma_sound_set_spatialization_enabled(&impl_->sound, immersive8D ? MA_TRUE : MA_FALSE);
    if (immersive8D) {
        ma_sound_set_positioning(&impl_->sound, ma_positioning_absolute);
        ma_sound_set_attenuation_model(&impl_->sound, ma_attenuation_model_none);
        ma_sound_set_rolloff(&impl_->sound, 1.0F);
        ma_sound_set_min_distance(&impl_->sound, 0.1F);
        ma_sound_set_max_distance(&impl_->sound, 100.0F);
        updateSpatialMotion(0.0);
    }

    if (ma_sound_start(&impl_->sound) != MA_SUCCESS) {
        ma_sound_uninit(&impl_->sound);
        impl_->soundReady = false;
        impl_->immersive8D = false;
        return makeAudioError("failed to start audio playback");
    }

    return {StatusCode::Ok, "playback started"};
}

Result AudioPlayer::stop() {
    if (!impl_) {
        return {StatusCode::Ok, "audio player released"};
    }

    if (impl_->soundReady) {
        ma_sound_stop(&impl_->sound);
        ma_sound_uninit(&impl_->sound);
        impl_->soundReady = false;
    }
    impl_->immersive8D = false;
    impl_->paused = false;
    impl_->pausedFrame = 0;

    return {StatusCode::Ok, "playback stopped"};
}

Result AudioPlayer::pause() {
    if (!impl_ || !impl_->soundReady) {
        return {StatusCode::NotFound, "no active playback to pause"};
    }

    ma_sound_get_cursor_in_pcm_frames(&impl_->sound, &impl_->pausedFrame);
    ma_sound_stop(&impl_->sound);
    impl_->paused = true;
    return {StatusCode::Ok, "playback paused"};
}

Result AudioPlayer::resume() {
    if (!impl_ || !impl_->soundReady) {
        return {StatusCode::NotFound, "no active playback to resume"};
    }

    ma_sound_seek_to_pcm_frame(&impl_->sound, impl_->pausedFrame);
    ma_sound_start(&impl_->sound);
    impl_->paused = false;
    return {StatusCode::Ok, "playback resumed"};
}

Result AudioPlayer::setVolume(float volume) {
    if (!impl_ || !impl_->soundReady) {
        return {StatusCode::NotFound, "no active playback to change volume"};
    }

    ma_sound_set_volume(&impl_->sound, volume);
    return {StatusCode::Ok, "volume updated"};
}

void AudioPlayer::updateSpatialMotion(double seconds) {
    if (!impl_ || !impl_->soundReady || !impl_->immersive8D) {
        return;
    }

    const float angle = static_cast<float>(seconds * 1.4);
    const float x = std::sin(angle) * 1.5F;
    const float z = std::cos(angle) * 1.5F;
    ma_sound_set_position(&impl_->sound, x, 0.0F, z);
}

bool AudioPlayer::isInitialized() const {
    return impl_ && impl_->engineReady;
}

bool AudioPlayer::isTrackComplete() const {
    if (!impl_ || !impl_->soundReady) {
        return false;
    }

    return ma_sound_at_end(&impl_->sound) == MA_TRUE;
}
