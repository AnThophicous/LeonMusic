#include "internal.hpp"

namespace leonmusic {

Result startPlatformIpcServer(const StartServerRequest& request) {
    if (request.transport.empty()) {
        return {StatusCode::InvalidArgument, "transport is required"};
    }

    return {StatusCode::Unsupported, "named pipe and unix socket servers are scaffolded but not implemented yet"};
}

}  // namespace leonmusic

