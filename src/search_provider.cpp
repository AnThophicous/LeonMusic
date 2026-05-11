#include "internal.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>

using leonmusic::ProcessResult;
using leonmusic::Result;
using leonmusic::SearchProvider;
using leonmusic::SearchResponse;
using leonmusic::StatusCode;
using leonmusic::Track;
using nlohmann::json;

namespace {

std::optional<std::string> extractYoutubeId(std::string_view input) {
    static const std::regex idRegex(R"(^[A-Za-z0-9_-]{11}$)");
    static const std::regex urlRegex(R"((?:v=|youtu\.be/)([A-Za-z0-9_-]{11}))");

    const std::string text(input);
    std::smatch match {};
    if (std::regex_match(text, match, idRegex)) {
        return text;
    }

    if (std::regex_search(text, match, urlRegex) && match.size() > 1) {
        return match[1].str();
    }

    return std::nullopt;
}

std::optional<Track> resolveLocalFile(std::string_view input) {
    const std::filesystem::path path(input);
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }

    const auto extension = path.extension().string();
    static const std::string supported[] = {".mp3", ".wav", ".flac", ".ogg", ".m4a"};
    std::string normalizedExtension = extension;
    std::transform(normalizedExtension.begin(), normalizedExtension.end(), normalizedExtension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    const bool accepted = std::any_of(std::begin(supported), std::end(supported), [&](const std::string& item) {
        return normalizedExtension == item;
    });

    if (!accepted) {
        return std::nullopt;
    }

    Track track {
        path.stem().string(),
        path.stem().string(),
        "Local file",
        path.string(),
        0,
        leonmusic::isLikely8D(path.filename().string())
    };
    const Result metadataResult = leonmusic::enrichTrackMetadata(path, track);
    (void)metadataResult;
    return track;
}

Track trackFromJson(const json& item) {
    const std::string id = item.value("id", "");
    const std::string title = item.value("title", "YouTube video");
    const std::string artist = item.value("artist", item.value("channel", item.value("uploader", "Unknown artist")));
    const std::string webpageUrl = item.value("webpage_url", id.empty() ? "" : "https://www.youtube.com/watch?v=" + id);
    const auto durationValue = item.value("duration", 0);

    return Track {
        id,
        title,
        artist,
        webpageUrl,
        durationValue > 0 ? static_cast<std::uint32_t>(durationValue) : 0U,
        leonmusic::isLikely8D(title) || leonmusic::isLikely8D(artist)
    };
}

std::optional<json> fetchSingleJson(const std::vector<std::string>& args, Result& result) {
    const ProcessResult process = leonmusic::runProcess(args);
    if (process.exitCode != 0) {
        result = {
            StatusCode::InternalError,
            "yt-dlp request failed: " + process.output
        };
        return std::nullopt;
    }

    const json payload = json::parse(process.output, nullptr, false);
    if (payload.is_discarded()) {
        result = {StatusCode::InternalError, "yt-dlp returned invalid JSON"};
        return std::nullopt;
    }

    result = {StatusCode::Ok, "yt-dlp request succeeded"};
    return payload;
}

class DefaultSearchProvider final : public SearchProvider {
public:
    SearchResponse search(std::string_view query) const override {
        if (query.empty()) {
            return {{StatusCode::InvalidArgument, "query is required"}, {}};
        }

        if (const auto localTrack = resolveLocalFile(query); localTrack.has_value()) {
            return {{StatusCode::Ok, "resolved local audio file"}, {*localTrack}};
        }

        if (leonmusic::isYouTubeLikeInput(query)) {
            if (const auto track = resolve(query); track.has_value()) {
                return {{StatusCode::Ok, "resolved YouTube track"}, {*track}};
            }
        }

        Result processResult {};
        const auto payload = fetchSingleJson({
            "yt-dlp",
            "--ignore-config",
            "--quiet",
            "--no-warnings",
            "--dump-single-json",
            "--flat-playlist",
            ("ytsearch6:" + std::string(query))
        }, processResult);

        if (!payload.has_value()) {
            return {processResult, {}};
        }

        SearchResponse response {};
        response.result = {StatusCode::Ok, "YouTube search completed"};

        const auto entries = payload->value("entries", json::array());
        for (const auto& entry : entries) {
            response.tracks.push_back(trackFromJson(entry));
        }

        if (response.tracks.empty()) {
            response.result = {StatusCode::NotFound, "no tracks found for the query"};
        }

        return response;
    }

    std::optional<Track> resolve(std::string_view input) const override {
        if (const auto localTrack = resolveLocalFile(input); localTrack.has_value()) {
            return localTrack;
        }

        if (!leonmusic::isYouTubeLikeInput(input)) {
            const SearchResponse searchResponse = search(input);
            if (searchResponse.result.ok() && !searchResponse.tracks.empty()) {
                return searchResponse.tracks.front();
            }
            return std::nullopt;
        }

        const std::string source = [&]() {
            if (const auto id = extractYoutubeId(input); id.has_value()) {
                return "https://www.youtube.com/watch?v=" + *id;
            }
            return std::string(input);
        }();

        Result processResult {};
        const auto payload = fetchSingleJson({
            "yt-dlp",
            "--ignore-config",
            "--quiet",
            "--no-warnings",
            "--dump-single-json",
            "--no-playlist",
            source
        }, processResult);

        if (!payload.has_value()) {
            return std::nullopt;
        }

        return trackFromJson(*payload);
    }
};

}  // namespace

std::unique_ptr<leonmusic::SearchProvider> leonmusic::makeDefaultSearchProvider() {
    return std::make_unique<DefaultSearchProvider>();
}
