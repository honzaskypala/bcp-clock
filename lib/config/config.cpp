#include "config.h"

CConfig::CConfig() {
    // Default configuration values
    _event = "";
    _yellow = 60;
    _red = 0;
    _hostname = "BCP-clock";
    // TODO: Load configuration from persistent storage if available
}

CConfig& getConfigInstance() {
    static CConfig instance;
    return instance;
}

CConfig& Config = getConfigInstance();