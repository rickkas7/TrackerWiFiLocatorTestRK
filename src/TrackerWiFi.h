#ifndef __TRACKERWIFI_H
#define __TRACKERWIFI_H

#include "Particle.h"

#include "deque"

class TrackerWiFiAccessPoint {
public:
    String toString() const;

    String bssidString() const;

    JSONWriter &jsonWrite(JSONWriter &writer) const;


    String      ssid;
    uint8_t     bssid[6] = {0};
    int         rssi = 0;
    int         channel = 0;

};

class TrackerWiFi  {
public: 
    /**
     */
    TrackerWiFi();
    virtual ~TrackerWiFi();

    void setup();
    bool scan(std::function<void(const TrackerWiFiAccessPoint &ap)> callback);

    int format(const char *fmt, ...);
    int write(const char* data, size_t size);

    static TrackerWiFi *getInstance() { return instance; };

protected:
    void checkForData();
    os_thread_return_t wifiThread();

    uint32_t defaultTimeoutMs = 10000;
    char sendBuf[128];   
    char readBuf[256]; // Contains at most one line of response
    size_t readSize = 0;
    std::deque<String> readLines;
    os_mutex_t readLinexMutex;
    static TrackerWiFi *instance;
};



#endif /* __TRACKERWIFI_H */