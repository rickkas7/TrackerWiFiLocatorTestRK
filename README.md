# Tracker Wi-Fi Locator

Temporary support for the Google Maps Device Locator integration on the Tracker SoM and Tracker One for Wi-Fi geolocation.

In late 2020 or early 2021 a version of Device OS and Tracker Edge with support for location fusion will be available. This will utilize all three methods of location detection:

- GNSS (GPS)
- Wi-Fi Geolocation
- Cellular Tower Location

Until this is available this library can be used to do a Wi-Fi scan for geolocation separately. This requires the use of the [Google Maps Device Locator](https://docs.particle.io/tutorials/integrations/google-maps/) integration. It's really best to wait until the official support is in place as this code is designed only for temporary use. It's not well tested. It does illustate how to communicate with the ESP32 Wi-Fi module on the Tracker SoM, however.

The official support will be much smaller because it can take advantage of features within Device OS already (AT command parser, ESP32 AT support) as well as not require a separate Google Geolocation integration. It's best to just wait if you can.

This can only be used on the Tracker SoM! Device OS 2.0.0-rc.3 or later is recommended.

From the Command Palette, **Particle: Install Library** then install **TrackerWiFiLocatorTestRK**.

The latest build and instructions can be found [in Github](https://github.com/rickkas7/TrackerWiFiLocatorTestRK).

## Firmware Library API

### Sample Code

```cpp
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
```


### Creating an object

You normally create an locator object as a global variable in your program for both the locator 
and tracker objects:

```
TrackerWiFi trackerWiFi;
GoogleMapsDeviceLocator locator;
```

### Set up Tracker WiFi object

From setup():

```
trackerWiFi.setup();
```

### Operating modes

There are three modes of operation:

If you want to only publish the location once when the device starts up, use withLocateOnce from your setup function.

```
locator.withLocateOnce();
```

To publish every *n* seconds while connected to the cloud, use withLocatePeriodic. The value is in seconds.

```
locator.withLocatePeriodic(120);
```

To manually connect, specify neither option and call publishLocation when you want to publish the location

```
locator.publishLocation();
```

### The loop

With periodic and locate once modes, you must call 

```
locator.loop();
```

from your loop. It doesn't hurt to always call it, even in manual location mode. It gives the library time to process the data.

### Customizing the event name

The default event name is **deviceLocator**. You can change that in setup using:

```
locator.withEventName("myEventName");
```

This also must be updated in the integration, since the eventName is what triggers the webhook. 

### Subscription

You can also have the library tell your firmware code what location was found. Use the withSubscribe option with a callback function.

This goes in setup() for example:

```
locator.withSubscribe(locationCallback).withLocatePeriodic(120);
```

The callback function looks like this:

```
void locationCallback(float lat, float lon, float accuracy)
```

One possibility is that you could display this information on a small OLED display, for example.

### Debugging

```
SerialLogHandler logHandler;
```

## Adding to Tracker Edge Firmware

```cpp
#include "Particle.h"

#include "tracker_config.h"
#include "tracker.h"

#include "TrackerWiFi.h"
#include "google-maps-device-locator.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

PRODUCT_ID(TRACKER_PRODUCT_ID);
PRODUCT_VERSION(TRACKER_PRODUCT_VERSION);

SerialLogHandler logHandler(115200, LOG_LEVEL_TRACE, {
    { "app.gps.nmea", LOG_LEVEL_INFO },
    { "app.gps.ubx",  LOG_LEVEL_INFO },
    { "ncp.at", LOG_LEVEL_INFO },
    { "net.ppp.client", LOG_LEVEL_INFO },
});

TrackerWiFi trackerWiFi;

GoogleMapsDeviceLocator locator;


void setup()
{
    Tracker::instance().init();

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

void loop()
{
     Tracker::instance().loop();

    // Be sure to call locator.loop() from loop!
    locator.loop();
}

```


## Version History

#### 0.0.1 (2020-10-22)

- Initial version
