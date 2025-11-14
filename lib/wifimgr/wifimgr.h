#ifndef WIFIMGR_H
#define WIFIMGR_H

#include <Arduino.h>

class CWifiMgr {
public:
    void connect();

private:
    CWifiMgr();
    CWifiMgr(const CWifiMgr&) = delete;
    CWifiMgr& operator=(const CWifiMgr&) = delete;

    friend CWifiMgr& getWifiMgrInstance();
};

extern CWifiMgr& WifiMgr;

#endif