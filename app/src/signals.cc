
#include "app/signals.h"

#include <csignal>
#include <iostream>

namespace signalhandler {

namespace {
    volatile std::sig_atomic_t global_run = 1;

    extern "C" void HandleSignal(const int signal_catch) {
        if (signal_catch == SIGINT || signal_catch == SIGTERM) {
            global_run = false;
        }
    };
}

bool RegisterSignals() {
    struct sigaction signal_action{};
    signal_action.sa_handler = HandleSignal;
    signal_action.sa_flags = 0; // allow EINTR

    if (sigaction(SIGINT, &signal_action, nullptr) == -1) {
        return false;
    }

    if (sigaction(SIGTERM, &signal_action, nullptr) == -1) {
        return false;
    }

    return true;
};

bool KeepRunning() {
    return global_run != 0;
}

} //namespace signalhandler