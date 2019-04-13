#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

unsigned long start = millis();

#define RETRIES 5

OneWire oneWire1(D1);
DallasTemperature DS18B20_1(&oneWire1);

WiFiClient espClient;

void slp(int ms)
{
    ESP.deepSleep(ms * 1000);
}

void setup()
{
    Serial.begin(115200);

    // WiFi.disconnect(true);

    DS18B20_1.begin();

    Serial.println(F("Go!"));
}

bool sendMQTT(const char *const topic, const float value)
{
    PubSubClient client(espClient);

    client.setServer("192.168.64.1", 1883);

    unsigned long int startM = millis();

    for (int i = 0; i < RETRIES; i++) {
        while (!client.connected()) {
            client.connect("watertemp");
            Serial.print(',');
            delay(100);

            if (millis() - startM > 5000)
                return false;
        }

        char buffer[32] = { 0 };

        dtostrf(value, 8, 8, buffer);
        if (client.publish(topic, buffer))
            break;

        client.loop();
    }

    client.disconnect();

    return true;
}

void loop()
{
    float temp1 = 127.0, temp2 = 127.0, temp3 = 127.0;;

    for (int i = 0; i < RETRIES; i++) {
        DS18B20_1.requestTemperatures();
        temp1 = DS18B20_1.getTempCByIndex(0);
        if (temp1 < 80 && temp1 > -120)
            break;
        delay(100);
    }

    for (int i = 0; i < RETRIES; i++) {
        temp2 = DS18B20_1.getTempCByIndex(1);
        if (temp2 < 80 && temp2 > -120)
            break;
        delay(100);
        DS18B20_1.requestTemperatures();
    }

    for (int i = 0; i < RETRIES; i++) {
        temp3 = DS18B20_1.getTempCByIndex(2);
        if (temp3 < 80 && temp3 > -120)
            break;
        delay(100);
        DS18B20_1.requestTemperatures();
    }

    Serial.print(temp1);
    Serial.print(' ');
    Serial.print(temp2);
    Serial.print(' ');
    Serial.print(temp3);
    Serial.println(F(""));

    float volt = analogRead(A0) / 0.902 / 1024.0 * 4.2;
    Serial.println(volt);

    if (temp1 < 80 && temp2 < 80 && temp3 < 80 && temp1 > -120 && temp2 > -120 && temp3 > -120) {       // 85 degrees == error, -127 == disconnected
        const unsigned long wifiStart = millis();

        WiFi.persistent(false);
        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_STA);

        WiFi.begin("www.vanheusden.com", "");

        for (;;) {
            int rc = WiFi.status();

            if (rc == WL_CONNECTED)
                break;

            if (rc != WL_DISCONNECTED) {
                WiFi.begin("www.vanheusden.com", "");
                delay(250);
            } else {
                delay(100);
            }

            Serial.print('[');
            Serial.print(rc);
            Serial.print(']');

            if (millis() - start > 60000l) {
                Serial.print("FAIL");
                goto fail;
            }
        }

        unsigned long wifiConnectTook = millis() - wifiStart;

        sendMQTT("watertmp2-1", temp1); // ignore rc for now
        sendMQTT("watertmp2-2", temp2); // ignore rc for now
        sendMQTT("watertmp2-3", temp3); // ignore rc for now

        sendMQTT("watertmp2-batV", volt);

        sendMQTT("watertmp2-WRT", wifiConnectTook / 1000.0);    // wifi ready time
    }
/*  else {
    char buffer[256];
    snprintf(buffer, sizeof buffer, "failed acquire sensors %d", millis());
    sendString("watertmp2-meta", buffer);
  } */

    Serial.print(F("Took: "));
    Serial.println(millis() - start);
    Serial.flush();

  fail:
    slp(300 * 1000);
}
