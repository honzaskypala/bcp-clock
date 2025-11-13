#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

class CConfig {
public:
    String getEventId() const { return _event; }
    long getYellowThreshold() const { return _yellow; }
    long getRedThreshold() const { return _red; }
    String getHostname() const { return _hostname; }

private:
    CConfig();
    CConfig(const CConfig&) = delete;
    CConfig& operator=(const CConfig&) = delete;

    friend CConfig& getConfigInstance();

    String _event;
    long _yellow, _red;
    String _hostname;
};

extern CConfig& Config;

#endif