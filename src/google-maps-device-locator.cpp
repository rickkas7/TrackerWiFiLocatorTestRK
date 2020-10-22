#include "Particle.h"
#include "google-maps-device-locator.h"

static Logger _log("locator");

static char requestBuf[622];

GoogleMapsDeviceLocator::GoogleMapsDeviceLocator() : locatorMode(LOCATOR_MODE_MANUAL), periodMs(10000), eventName("deviceLocator"),
	stateTime(0), state(CONNECT_WAIT_STATE), callback(NULL), waitAfterConnect(8000) {

}

GoogleMapsDeviceLocator::~GoogleMapsDeviceLocator() {

}

GoogleMapsDeviceLocator &GoogleMapsDeviceLocator::withLocateOnce() {
	locatorMode = LOCATOR_MODE_ONCE;
	return *this;
}

GoogleMapsDeviceLocator &GoogleMapsDeviceLocator::withLocatePeriodic(unsigned long secondsPeriodic) {
	locatorMode = LOCATOR_MODE_PERIODIC;
	if (secondsPeriodic < 5) {
		secondsPeriodic = 5;
	}
	periodMs = secondsPeriodic * 1000;
	return *this;
}

GoogleMapsDeviceLocator &GoogleMapsDeviceLocator::withEventName(const char *name) {
	this->eventName = name;
	return *this;
}

GoogleMapsDeviceLocator &GoogleMapsDeviceLocator::withSubscribe(GoogleMapsDeviceLocatorSubscriptionCallback callback) {
	this->callback = callback;

	snprintf(requestBuf, sizeof(requestBuf), "hook-response/%s/%s", eventName.c_str(), System.deviceID().c_str());

	Particle.subscribe(requestBuf, &GoogleMapsDeviceLocator::subscriptionHandler, this, MY_DEVICES);

	return *this;
}


void GoogleMapsDeviceLocator::loop() {
	switch(state) {
	case CONNECT_WAIT_STATE:
		if (Particle.connected()) {
			state = CONNECTED_WAIT_STATE;
			stateTime = millis();
		}
		break;

	case CONNECTED_WAIT_STATE:
		if (millis() - stateTime >= waitAfterConnect) {
			// Wait several seconds after connecting before doing the location
			if (locatorMode == LOCATOR_MODE_ONCE) {
				publishLocation();

				state = IDLE_STATE;
			}
			else
			if (locatorMode == LOCATOR_MODE_MANUAL) {
				state = IDLE_STATE;
			}
			else {
				state = CONNECTED_STATE;
				stateTime = millis() - periodMs;
			}
		}
		break;

	case CONNECTED_STATE:
		if (Particle.connected()) {
			if (millis() - stateTime >= periodMs) {
				stateTime = millis();
				publishLocation();
			}
		}
		else {
			// We have disconnected, rec
			state = CONNECT_WAIT_STATE;
		}
		break;


	case IDLE_STATE:
		// Just hang out here forever (entered only on LOCATOR_MODE_ONCE)
		break;
	}

}

bool GoogleMapsDeviceLocator::scan() {
	bool hasResults = false;
	JSONBufferWriter writer(requestBuf, sizeof(requestBuf) - 1);

	writer.beginObject();

	if (checkFlags & CHECK_WIFI) {
		bool bResult = wifiScan(writer);
		if (bResult) {
			hasResults = true;
		}
	}

#if Wiring_Cellular
	if (checkFlags & CHECK_CELLULAR) {
		bool bResult = cellularScan(writer);		
		if (bResult) {
			hasResults = true;
		}
	}
#endif
	writer.endObject();

	writer.buffer()[std::min(writer.bufferSize(), writer.dataSize())] = 0;

	return hasResults;
}


void GoogleMapsDeviceLocator::publishLocation() {

	_log.info("publishLocation");

	bool hasResults = scan();

	if (hasResults) {

		if (Particle.connected()) {
			Particle.publish(eventName, requestBuf, PRIVATE);
		}
	}
}

void GoogleMapsDeviceLocator::subscriptionHandler(const char *event, const char *data) {
	// event: hook-response/deviceLocator/<deviceid>/0

	if (callback) {
		// float lat, float lon, float accuracy
		char *mutableCopy = strdup(data);
		char *part, *end;
		float lat, lon, accuracy;

		part = strtok_r(mutableCopy, ",", &end);
		if (part) {
			lat = atof(part);
			part = strtok_r(NULL, ",", &end);
			if (part) {
				lon = atof(part);
				part = strtok_r(NULL, ",", &end);
				if (part) {
					accuracy = atof(part);

					(*callback)(lat, lon, accuracy);
				}
			}
		}

		free(mutableCopy);
	}
}




bool GoogleMapsDeviceLocator::wifiScan(JSONWriter &writer) {

	int numAdded = 0;

	writer.name("w").beginObject();

	writer.name("a").beginArray();

	TrackerWiFi::getInstance()->scan([&writer,&numAdded](const TrackerWiFiAccessPoint &ap) {
		// _log.info("got ap %s", ap.toString().c_str());

		// Each entry is around 42 bytes so limit to 13 entries to leave space in
		// the publish for other things
		if (++numAdded < 13) {
			ap.jsonWrite(writer);
		}
	});
	
	writer.endArray();
	writer.endObject();

	return numAdded > 0;
}


#if Wiring_Cellular

bool GoogleMapsDeviceLocator::cellularScan(JSONWriter &writer) {

	CellularGlobalIdentity cgi = {0};
	cgi.size = sizeof(CellularGlobalIdentity);
	cgi.version = CGI_VERSION_LATEST;

	cellular_result_t res = cellular_global_identity(&cgi, NULL);
	if (res == SYSTEM_ERROR_NONE) {
		writer.name("c").beginObject();

		writer.name("o").value(""); // Operator, which we don't have from CGI

		writer.name("a").beginArray();

		// Tower object
		writer.beginObject();
		writer.name("i").value((unsigned)cgi.cell_id);
		writer.name("l").value(cgi.location_area_code);
		writer.name("c").value(cgi.mobile_country_code);
		writer.name("n").value(cgi.mobile_network_code);
		writer.endObject();

		writer.endArray();
		writer.endObject();

		return true;
	}
	else {
		_log.info("cellular_global_identity failed %d", res);
		return false;
	}
}

#endif /* Wiring_Cellular */





