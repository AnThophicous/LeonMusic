#include "internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_set>

#ifdef LEONMUSIC_HAS_TAGLIB
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#endif

using leonmusic::PreparedTrack;
using leonmusic::ProcessResult;
using leonmusic::Result;
using leonmusic::StatusCode;
using leonmusic::Track;

namespace {

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string toLower(std::string_view value) {
    std::string lower(value);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower;
}

std::optional<std::string> readEnv(const char* name) {
#ifdef _WIN32
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) == 0 && buffer != nullptr && length > 1) {
        std::string value(buffer);
        free(buffer);
        return value;
    }
    free(buffer);
#else
    if (const char* value = std::getenv(name); value && *value) {
        return std::string(value);
    }
#endif
    return std::nullopt;
}

int readEnvInt(const char* name, int fallback) {
    if (const auto value = readEnv(name); value.has_value()) {
        try {
            return std::stoi(*value);
        } catch (...) {
        }
    }
    return fallback;
}

std::string youTubeUrlForTrack(const Track& track) {
    if (track.sourceUrl.starts_with("http://") || track.sourceUrl.starts_with("https://")) {
        return track.sourceUrl;
    }

    if (!track.id.empty()) {
        return "https://www.youtube.com/watch?v=" + track.id;
    }

    return track.sourceUrl;
}

std::filesystem::path stableCacheStem(const Track& track) {
    if (!track.id.empty()) {
        return leonmusic::cacheDirectory() / track.id;
    }

    const auto hash = std::hash<std::string> {}(track.sourceUrl.empty() ? track.title : track.sourceUrl);
    return leonmusic::cacheDirectory() / ("track-" + std::to_string(hash));
}

std::optional<std::filesystem::path> existingDownloadedFile(const std::filesystem::path& stem) {
    const auto directory = stem.parent_path();
    if (!std::filesystem::exists(directory)) {
        return std::nullopt;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (entry.path().stem() == stem.filename()) {
            return entry.path();
        }
    }

    return std::nullopt;
}

Result downloadYouTubeAudio(const Track& track, std::filesystem::path& outputFile) {
    const std::filesystem::path stem = stableCacheStem(track);
    std::filesystem::create_directories(stem.parent_path());

    if (const auto existing = existingDownloadedFile(stem); existing.has_value()) {
        outputFile = *existing;
        return {StatusCode::Ok, "reused cached audio file"};
    }

    const auto cookiesFile = readEnv("LEONMUSIC_YTDLP_COOKIES");
    const auto cookiesFromBrowser = readEnv("LEONMUSIC_YTDLP_COOKIES_FROM_BROWSER");
    const int concurrentFragments = std::clamp(readEnvInt("LEONMUSIC_YTDLP_CONCURRENT_FRAGMENTS", 8), 1, 16);
    const int audioQuality = std::clamp(readEnvInt("LEONMUSIC_YTDLP_AUDIO_QUALITY", 4), 0, 9);

    const std::vector<std::vector<std::string>> extractorArgSets = {
        {"youtube:player_client=web_safari,android_vr,tv_simply"},
        {"youtube:player_client=tv,web_safari"},
        {"youtube:player_client=android,web_safari"},
        {"youtube:player_client=mweb,web_safari"}
    };

    std::string combinedErrors {};
    for (const auto& extractorArgs : extractorArgSets) {
        std::vector<std::string> args {
            "yt-dlp",
            "--ignore-config",
            "--no-progress",
            "--no-warnings",
            "--no-playlist",
            "--concurrent-fragments",
            std::to_string(concurrentFragments),
            "--format",
            "m4a/bestaudio/best",
            "--extractor-args"
        };

        args.push_back(extractorArgs.front());

        if (cookiesFile.has_value()) {
            args.push_back("--cookies");
            args.push_back(*cookiesFile);
        } else if (cookiesFromBrowser.has_value()) {
            args.push_back("--cookies-from-browser");
            args.push_back(*cookiesFromBrowser);
        }

        args.insert(args.end(), {
            "--extract-audio",
            "--audio-format",
            "mp3",
            "--audio-quality",
            std::to_string(audioQuality),
            "--postprocessor-args",
            "ffmpeg:-threads 0",
            "--output",
            (stem.string() + ".%(ext)s"),
            youTubeUrlForTrack(track)
        });

        const ProcessResult process = leonmusic::runProcess(args);
        if (process.exitCode == 0) {
            const auto downloaded = existingDownloadedFile(stem);
            if (downloaded.has_value()) {
                outputFile = *downloaded;
                return {StatusCode::Ok, "downloaded audio to cache"};
            }
        }

        if (!combinedErrors.empty()) {
            combinedErrors += "\n---\n";
        }
        combinedErrors += "extractor args [" + extractorArgs.front() + "] failed:\n" + trim(process.output);
    }

    if (cookiesFile.has_value() || cookiesFromBrowser.has_value()) {
        combinedErrors += "\nHint: cookies were supplied but the current video/client combination still failed.";
    } else {
        combinedErrors += "\nHint: set LEONMUSIC_YTDLP_COOKIES_FROM_BROWSER=chrome or LEONMUSIC_YTDLP_COOKIES=<cookies.txt> to improve restricted downloads.";
    }

    return {
        StatusCode::InternalError,
        "yt-dlp download failed after multiple client fallbacks:\n" + combinedErrors
    };
}

}  // namespace

void leonmusic::prefetchTrack(const Track& input) {
    static std::mutex prefetchMutex {};
    static std::unordered_set<std::string> inFlight {};

    const std::string key = input.id.empty() ? input.sourceUrl : input.id;
    if (key.empty()) {
        return;
    }

    {
        std::scoped_lock lock(prefetchMutex);
        if (inFlight.contains(key)) {
            return;
        }
        inFlight.insert(key);
    }

    std::thread([track = input, key]() {
        PreparedTrack prepared {};
        const Result result = prepareTrackForPlayback(track, prepared);
        (void)result;

        std::scoped_lock lock(prefetchMutex);
        inFlight.erase(key);
    }).detach();
}

std::filesystem::path leonmusic::cacheDirectory() {
    return std::filesystem::temp_directory_path() / "LeonMusic" / "cache";
}

bool leonmusic::isYouTubeLikeInput(std::string_view input) {
    static const std::regex idRegex(R"(^[A-Za-z0-9_-]{11}$)");
    static const std::regex urlRegex(R"((youtube\.com|youtu\.be))");
    const std::string text(input);
    return std::regex_match(text, idRegex) || std::regex_search(text, urlRegex);
}

bool leonmusic::isLikely8D(std::string_view text) {
    const std::string lower = toLower(text);
    return lower.find("8d") != std::string::npos ||
        lower.find("binaural") != std::string::npos ||
        lower.find("spatial audio") != std::string::npos;
}

Result leonmusic::enrichTrackMetadata(const std::filesystem::path& filePath, Track& track) {
#ifdef LEONMUSIC_HAS_TAGLIB
    const std::string path = filePath.string();
    TagLib::FileRef file(path.c_str(), true);
    if (file.isNull()) {
        return {StatusCode::Unsupported, "TagLib could not read metadata for this file"};
    }

    if (auto* tag = file.tag()) {
        const auto title = tag->title().to8Bit(true);
        const auto artist = tag->artist().to8Bit(true);
        if (!title.empty()) {
            track.title = title;
        }
        if (!artist.empty()) {
            track.artist = artist;
        }

        const auto comment = tag->comment().to8Bit(true);
        if (!comment.empty()) {
            track.supports8D = track.supports8D || isLikely8D(comment);
        }
    }

    if (auto* props = file.audioProperties()) {
        track.durationSeconds = static_cast<std::uint32_t>(props->lengthInSeconds());
    }

    track.supports8D = track.supports8D || isLikely8D(track.title) || isLikely8D(track.artist) || isLikely8D(filePath.filename().string());
    return {StatusCode::Ok, "metadata loaded from TagLib"};
#else
    const ProcessResult process = runProcess({
        "ffprobe",
        "-v",
        "error",
        "-print_format",
        "json",
        "-show_format",
        "-show_streams",
        filePath.string()
    });

    if (process.exitCode != 0) {
        return {StatusCode::Unsupported, "neither TagLib nor ffprobe metadata extraction succeeded"};
    }

    const auto payload = nlohmann::json::parse(process.output, nullptr, false);
    if (payload.is_discarded()) {
        return {StatusCode::Unsupported, "ffprobe returned invalid metadata JSON"};
    }

    const auto format = payload.value("format", nlohmann::json::object());
    const auto tags = format.value("tags", nlohmann::json::object());
    if (const std::string title = tags.value("title", ""); !title.empty()) {
        track.title = title;
    }
    if (const std::string artist = tags.value("artist", tags.value("ARTIST", "")); !artist.empty()) {
        track.artist = artist;
    }
    if (const std::string comment = tags.value("comment", tags.value("COMMENT", "")); !comment.empty()) {
        track.supports8D = track.supports8D || isLikely8D(comment);
    }

    if (const std::string duration = format.value("duration", ""); !duration.empty()) {
        track.durationSeconds = static_cast<std::uint32_t>(std::max(0.0, std::stod(duration)));
    }

    track.supports8D = track.supports8D || isLikely8D(track.title) || isLikely8D(track.artist) || isLikely8D(filePath.filename().string());
    return {StatusCode::Ok, "metadata loaded with ffprobe fallback"};
#endif
}

Result leonmusic::prepareTrackForPlayback(const Track& input, PreparedTrack& prepared) {
    prepared.track = input;

    const std::filesystem::path localPath(input.sourceUrl);
    if (std::filesystem::exists(localPath) && std::filesystem::is_regular_file(localPath)) {
        prepared.playablePath = localPath;
        prepared.track.supports8D = prepared.track.supports8D || isLikely8D(localPath.filename().string());
        const Result metadataResult = enrichTrackMetadata(localPath, prepared.track);
        if (!metadataResult.ok() && prepared.track.title.empty()) {
            prepared.track.title = localPath.stem().string();
            prepared.track.artist = prepared.track.artist.empty() ? "Local file" : prepared.track.artist;
        }
        return {StatusCode::Ok, "prepared local track"};
    }

    if (!isYouTubeLikeInput(input.sourceUrl) && !isYouTubeLikeInput(input.id)) {
        return {StatusCode::Unsupported, "only local audio files and YouTube-backed tracks are supported"};
    }

    Result downloadResult = downloadYouTubeAudio(input, prepared.playablePath);
    if (!downloadResult.ok()) {
        return downloadResult;
    }

    prepared.temporaryFile = true;
    prepared.track.supports8D = prepared.track.supports8D || isLikely8D(prepared.track.title);
    const Result metadataResult = enrichTrackMetadata(prepared.playablePath, prepared.track);
    if (metadataResult.ok()) {
        return {StatusCode::Ok, "downloaded and enriched track metadata"};
    }

    return {StatusCode::Ok, "downloaded track without additional metadata"};
}
