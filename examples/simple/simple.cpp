#include "TrackerWiFi.h"
#include "google-maps-device-locator.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

SerialLogHandler logHandler;

TrackerWiFi trackerWiFi;

GoogleMapsDeviceLocator locator;


void setup() {
    // Optional: Enable to make it easier to see debug USB serial messages at startup
    // waitFor(Serial.isConnected, 15000);
    // delay(1000);

    // Set up the Tracker Wi-Fi module (required)
    trackerWiFi.setup();
    
    // Set the Google Maps Device Locator parameters. This sets it to periodic
    // mode but you can also set it to manual mode. This is also where you would
    // register a callback
    locator.withLocateOnce();
    
    // .withCheckFlags(GoogleMapsDeviceLocator::CHECK_CELLULAR | GoogleMapsDeviceLocator::CHECK_WIFI);
    // .withLocateOnce()
    // .withLocatePeriodic(120);

    Particle.connect();
}

void loop() {
    // Be sure to call locator.loop() from loop!
    locator.loop();
}