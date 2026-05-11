#include "internal.hpp"

#include <chrono>
#include <iostream>
#include <thread>

static void printUsage() {
    std::cout
        << "LeonMusic CLI (lma)\n"
        << "  lma --search-music <query>\n"
        << "  lma --play-first <query>\n"
        << "  lma --enqueue-first <query>\n"
        << "  lma --play-index <query> <index>\n"
        << "  lma --play-music <path|url|youtube-id>\n"
        << "  lma --enqueue-music <path|url|youtube-id>\n"
        << "  lma --pause-music\n"
        << "  lma --resume-music\n"
        << "  lma --set-volume <0.0..2.0>\n"
        << "  lma --next-music\n"
        << "  lma --previous-music\n"
        << "  lma --clear-queue\n"
        << "  lma --stop-music\n"
        << "  lma --playback-state\n"
        << "  lma --wait-until-finished\n"
        << "  lma --no-wait\n"
        << "  lma --serve [stdio-jsonrpc|named-pipe|unix-socket] [endpoint]\n"
        << "  lma --set-discord-client-id <client-id>\n"
        << "\n"
        << "Commands can be chained in a single process:\n"
        << "  lma --play-music song1.mp3 --enqueue-music song2.mp3 --wait-until-finished\n";
}

static leonmusic::Result playFromSearch(leonmusic::LeonMusicApi& api, std::string_view query, std::size_t index, bool enqueue) {
    const leonmusic::SearchResponse response = api.searchMusic(query);
    if (!response.result.ok()) {
        return response.result;
    }
    if (response.tracks.empty()) {
        return {leonmusic::StatusCode::NotFound, "no tracks found for the query"};
    }
    if (index >= response.tracks.size()) {
        return {leonmusic::StatusCode::InvalidArgument, "search result index is out of range"};
    }

    const auto& track = response.tracks[index];
    const std::string input = track.id.empty() ? track.sourceUrl : track.id;
    return enqueue ? api.enqueueMusic(input) : api.playMusic(input);
}

leonmusic::Result leonmusic::runCliCommand(LeonMusicApi& api, int argc, char** argv) {
    if (argc <= 1) {
        printUsage();
        return {StatusCode::Ok, "usage printed"};
    }

    Result lastResult {StatusCode::Ok, "no command executed"};
    bool waitUntilFinished = false;
    bool playbackTouched = false;
    bool noWait = false;
    for (int i = 1; i < argc; ++i) {
        const std::string command = argv[i];
        if (command == "--search-music" && i + 1 < argc) {
            const SearchResponse response = api.searchMusic(argv[++i]);
            std::cout << nlohmann::json{
                {"status", static_cast<std::uint32_t>(response.result.code)},
                {"message", response.result.message},
                {"tracks", [&] {
                    nlohmann::json tracks = nlohmann::json::array();
                    for (const auto& track : response.tracks) {
                        tracks.push_back(trackToJson(track));
                    }
                    return tracks;
                }()}
            }.dump(2) << std::endl;
            lastResult = response.result;
            if (!lastResult.ok()) {
                return lastResult;
            }
            continue;
        }

        if (command == "--play-first" && i + 1 < argc) {
            lastResult = playFromSearch(api, argv[++i], 0, false);
            playbackTouched = playbackTouched || lastResult.ok();
            if (!lastResult.ok()) {
                return lastResult;
            }
            continue;
        }

        if (command == "--enqueue-first" && i + 1 < argc) {
            lastResult = playFromSearch(api, argv[++i], 0, true);
            playbackTouched = playbackTouched || lastResult.ok();
            if (!lastResult.ok()) {
                return lastResult;
            }
            continue;
        }

        if (command == "--play-index" && i + 2 < argc) {
            const std::string query = argv[++i];
            const std::size_t index = static_cast<std::size_t>(std::stoul(argv[++i]));
            lastResult = playFromSearch(api, query, index, false);
            playbackTouched = playbackTouched || lastResult.ok();
            if (!lastResult.ok()) {
                return lastResult;
            }
            continue;
        }

        if (command == "--play-music" && i + 1 < argc) {
            lastResult = api.playMusic(argv[++i]);
            playbackTouched = playbackTouched || lastResult.ok();
        } else if (command == "--enqueue-music" && i + 1 < argc) {
            lastResult = api.enqueueMusic(argv[++i]);
            playbackTouched = playbackTouched || lastResult.ok();
        } else if (command == "--pause-music") {
            lastResult = api.pauseMusic();
        } else if (command == "--resume-music") {
            lastResult = api.resumeMusic();
            playbackTouched = playbackTouched || lastResult.ok();
        } else if (command == "--set-volume" && i + 1 < argc) {
            lastResult = api.setVolume(std::stof(argv[++i]));
        } else if (command == "--next-music") {
            lastResult = api.nextMusic();
            playbackTouched = playbackTouched || lastResult.ok();
        } else if (command == "--previous-music") {
            lastResult = api.previousMusic();
            playbackTouched = playbackTouched || lastResult.ok();
        } else if (command == "--clear-queue") {
            lastResult = api.clearQueue();
        } else if (command == "--stop-music") {
            lastResult = api.stopMusic();
        } else if (command == "--playback-state") {
            std::cout << playbackStateToJson(api.playbackState()).dump(2) << std::endl;
            lastResult = {StatusCode::Ok, "playback state printed"};
        } else if (command == "--wait-until-finished") {
            waitUntilFinished = true;
            lastResult = {StatusCode::Ok, "wait flag enabled"};
        } else if (command == "--no-wait") {
            noWait = true;
            lastResult = {StatusCode::Ok, "no-wait flag enabled"};
        } else if (command == "--serve") {
            StartServerRequest request {};
            if (i + 1 < argc) {
                request.transport = argv[++i];
            }
            if (i + 1 < argc && std::string_view(argv[i + 1]).starts_with("--") == false) {
                request.endpoint = argv[++i];
            }
            lastResult = api.startServer(request);
        } else if (command == "--set-discord-client-id" && i + 1 < argc) {
            lastResult = api.setDiscordClientId(argv[++i]);
        } else {
            printUsage();
            return {StatusCode::InvalidArgument, "unknown command or missing argument"};
        }

        if (!lastResult.ok()) {
            return lastResult;
        }
    }

    if ((waitUntilFinished || playbackTouched) && !noWait) {
        while (true) {
            const PlaybackState state = api.playbackState();
            if (!state.playing && !state.paused) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        lastResult = {StatusCode::Ok, "playback finished"};
    }

    return lastResult;
}
