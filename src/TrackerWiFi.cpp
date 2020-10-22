#include "TrackerWiFi.h"

#include "port.h"
#include "sdspi_host.h"

static Logger _log("twfi");

static spi_context_t context;

TrackerWiFi *TrackerWiFi::instance;


TrackerWiFi::TrackerWiFi() {
    instance = this;
}

TrackerWiFi::~TrackerWiFi() {

}

void TrackerWiFi::setup() {
    PORT_SPI.setDataMode(SPI_MODE0);
    PORT_SPI.begin();

    pinMode(PORT_CS_PIN, OUTPUT);
    digitalWrite(PORT_CS_PIN, HIGH);
    pinMode(PORT_BOOT_PIN, OUTPUT);
    digitalWrite(PORT_BOOT_PIN, HIGH);
    pinMode(PORT_EN_PIN, OUTPUT);
    digitalWrite(PORT_EN_PIN, LOW);
    delay(500);
    digitalWrite(PORT_EN_PIN, HIGH);

    esp_err_t err = at_sdspi_init();
    if (err != ESP_OK) {
        _log.error("at_sdspi_init failed %ld", err);
    }
    
    memset(&context, 0x0, sizeof(spi_context_t));

    os_mutex_create(&readLinexMutex);

    // Start the reading thread
    new Thread("wifi", [this]() { wifiThread(); }, 2, 768);
}


bool TrackerWiFi::scan(std::function<void(const TrackerWiFiAccessPoint &ap)> callback) {

    format("AT+CWLAP\r\n");

    while(true) {
        String line;
        bool empty;
        
        // Process a response line
        os_mutex_lock(readLinexMutex);
        empty = readLines.empty();
        if (!empty) {
            line = readLines.front();
            readLines.pop_front();
        }
        os_mutex_unlock(readLinexMutex);

        if (empty) {
            os_thread_yield();
            continue;
        }

        if (line.equals("OK")) {
            return true;
        }
        else
        if (line.startsWith("ERROR")) {
            return false;
        }

        // These constants are also in the sscanf string, so if you change them, change both
        const size_t MAX_SSID_SIZE = 32;
        const size_t MAC_ADDRESS_STRING_SIZE = 17;

        char ssid[MAX_SSID_SIZE + 2];
        char bssidStr[MAC_ADDRESS_STRING_SIZE + 1];
        int security = 0;
        int channel = 0;
        int rssi = 0;
        const int r = sscanf(line, "+CWLAP:(%d,\"%33[^,],%d,\"%17[^\"]\",%d)", &security, ssid, &rssi, bssidStr, &channel);
        if (r != 5) {
            continue;
        }

        // Fixup SSID by removing the trailing double quote
        if (strlen(ssid) > 0) {
            ssid[strlen(ssid) - 1] = 0;
        }

        // Parse bssid
        int bssid[6];
        if (sscanf(bssidStr, "%02x:%02x:%02x:%02x:%02x:%02x", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]) != 6) {
            continue;
        }  

        TrackerWiFiAccessPoint ap;
        ap.ssid = ssid;

        for(size_t ii = 0; ii < 6; ii++) {
            ap.bssid[ii] = (uint8_t)bssid[ii];
        }
        ap.rssi = rssi;
        ap.channel = channel;
        
        // _log.info("ssid=%s bssid=%02x:%02x:%02x:%02x:%02x:%02x rssi=%d channel=%d", 
        //    ssid, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], rssi, channel);            

        callback(ap);
    }

    return true;
}


int TrackerWiFi::format(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(sendBuf, sizeof(sendBuf), fmt, ap);
    va_end(ap);

    esp_err_t err = at_sdspi_send_packet(&context, (uint8_t *) sendBuf, strlen(sendBuf), defaultTimeoutMs);
    if (err != ESP_OK) {
        _log.info("send error %ld", err);
    }

    return 0;
}

int TrackerWiFi::write(const char* data, size_t size) { 
    while(size > 0) {
        // Divide into sendBuf sized chunks (currently 128 bytes)
        size_t count = size;
        if (count > sizeof(sendBuf)) {
            count = sizeof(sendBuf);
        }
        memcpy(sendBuf, data, count);
        data += count;
        size -= count;

        // May want to use a shorter timeout than UINT32_MAX!
        esp_err_t err = at_sdspi_send_packet(&context, (uint8_t *) sendBuf, count, defaultTimeoutMs);
        if (err != ESP_OK) {
            _log.info("send error %ld", err);
        }
    }
    return 0; 
}

void TrackerWiFi::checkForData() {
    while (digitalRead(WIFI_INT) == LOW) {

        // _log.info("[WIFI] INT pin is low, reading WIFI data.");

        readSize = sizeof(readBuf) - 1;

        // Each packet has one line in the response. This assumption is baked into
        // the parser for simplicity to save code over a more general-purpose
        // AT parser that has to buffer lines.

        esp_err_t err = at_sdspi_get_packet(&context, (uint8_t *)readBuf, readSize, &readSize);

        if (err == ESP_ERR_NOT_FOUND) {
            _log.info("interrupt but no data can be read");
            break;
        } else if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED) {
            _log.info("rx packet error %08lX", err);
        }

        readBuf[readSize] = 0;

        // Remove line terminators
        char *src = readBuf;
        char *dst = readBuf;

        while(*src) {
            if (*src != '\r' && *src != '\n') {
                *dst++ = *src;
            }
            src++;
        }
        *dst = 0;

        // Add to buffer
        // _log.print(readBuf);
        // _log.print("\n");

        os_mutex_lock(readLinexMutex);
        readLines.push_back(String(readBuf));
        os_mutex_unlock(readLinexMutex);
    }

}

os_thread_return_t TrackerWiFi::wifiThread() {
    while(true) {
        checkForData();
        os_thread_yield();
    }
}


String TrackerWiFiAccessPoint::toString() const {

    return String::format("ssid=%s bssid=%s rssi=%d channel=%d", ssid.c_str(), bssidString().c_str(), rssi, channel); 
}

String TrackerWiFiAccessPoint::bssidString() const {
    return String::format("%02x:%02x:%02x:%02x:%02x:%02x", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]); 
}


JSONWriter &TrackerWiFiAccessPoint::jsonWrite(JSONWriter &writer) const {

    writer.beginObject();

    writer.name("m").value(bssidString());
    writer.name("s").value(rssi);
    writer.name("c").value(channel);

    writer.endObject();

   return writer; 
}

