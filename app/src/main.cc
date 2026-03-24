
#include "app/logger.h"
#include "app/signals.h"

#include "quill/LogMacros.h"

int main() {
    if (!signalhandler::RegisterSignals()) {
        return EXIT_FAILURE;
    }

    const auto logger = GetLogger();

    for (int i = 2; i < 4; ++i){
        LOG_INFO(logger, "{method} to {endpoint} took {elapsed} ms", "POST", "http://", 10 * i);
    }

    uint i = 0;
    while(signalhandler::KeepRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (i != 0 && i % 6000 == 0) {
            LOG_ERROR(logger, "New log with index {index}", i);
        } else if (i != 0 && i % 3000 == 0) {
            LOG_WARNING(logger, "New log with index {index}", i);
        } else if (i != 0 && i % 300 == 0) {
            LOG_INFO(logger, "New log with index {index}", i);
        }

        ++i;
    }

    LOG_INFO(logger, "Operation {name} completed with code {code}, Data synced successfully", "Update", 123);
    LOG_DEBUG(logger, "This is debug", "");
    LOG_INFO(logger, "This is info", "");
    LOG_WARNING(logger, "This is warning", "");
    LOG_ERROR(logger, "This is error", "");
    return 0;

}