#include "internal.hpp"

#include <iostream>

leonmusic::Result leonmusic::runJsonRpcServer(LeonMusicApi& api) {
    std::string line {};
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        const auto request = nlohmann::json::parse(line, nullptr, false);
        if (request.is_discarded()) {
            std::cout << nlohmann::json{
                {"jsonrpc", "2.0"},
                {"error", {{"code", -32700}, {"message", "parse error"}}},
                {"id", nullptr}
            }.dump() << std::endl;
            continue;
        }

        const auto method = request.value("method", "");
        const auto id = request.contains("id") ? request["id"] : nlohmann::json(nullptr);
        nlohmann::json response {
            {"jsonrpc", "2.0"},
            {"id", id}
        };

        if (method == "searchMusic") {
            const auto params = request.value("params", nlohmann::json::object());
            const auto search = api.searchMusic(params.value("query", ""));
            response["result"] = {
                {"status", static_cast<std::uint32_t>(search.result.code)},
                {"message", search.result.message},
                {"tracks", nlohmann::json::array()}
            };
            for (const auto& track : search.tracks) {
                response["result"]["tracks"].push_back(trackToJson(track));
            }
        } else if (method == "playMusic") {
            const auto params = request.value("params", nlohmann::json::object());
            response["result"] = resultToJson(api.playMusic(params.value("input", "")));
        } else if (method == "enqueueMusic") {
            const auto params = request.value("params", nlohmann::json::object());
            response["result"] = resultToJson(api.enqueueMusic(params.value("input", "")));
        } else if (method == "pauseMusic") {
            response["result"] = resultToJson(api.pauseMusic());
        } else if (method == "resumeMusic") {
            response["result"] = resultToJson(api.resumeMusic());
        } else if (method == "setVolume") {
            const auto params = request.value("params", nlohmann::json::object());
            response["result"] = resultToJson(api.setVolume(params.value("volume", 1.0F)));
        } else if (method == "nextMusic") {
            response["result"] = resultToJson(api.nextMusic());
        } else if (method == "previousMusic") {
            response["result"] = resultToJson(api.previousMusic());
        } else if (method == "clearQueue") {
            response["result"] = resultToJson(api.clearQueue());
        } else if (method == "stopMusic") {
            response["result"] = resultToJson(api.stopMusic());
        } else if (method == "playbackState") {
            response["result"] = playbackStateToJson(api.playbackState());
        } else if (method == "version") {
            response["result"] = {{"version", version()}};
        } else {
            response["error"] = {{"code", -32601}, {"message", "method not found"}};
        }

        std::cout << response.dump() << std::endl;
    }

    return {StatusCode::Ok, "stdio JSON-RPC server stopped"};
}
