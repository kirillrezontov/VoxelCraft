#pragma once
#include "client/options.hpp"
#include "client/game_session.hpp"

namespace vc::client {
struct SessionEnvironment;
GameSession remoteSession(SessionEnvironment& host,Options options);
}
