#include <Arduino.h>
#include "config.h"
#include "certificates.h"
#include <PromLokiTransport.h>
#include <GrafanaLoki.h>
#include <esp_task_wdt.h>

// Adding these as deps so platformio knows to include them for the SHT31 code to compile (even if we dont' import it)
#include <Wire.h>
#include <SPI.h>

// There is one block below also checking to include temp sensor
#if IDINT == 3
#include "Adafruit_SHT31.h"
Adafruit_SHT31 sht31 = Adafruit_SHT31();
#endif

// Create a transport and client object for sending our data.
PromLokiTransport transport;
LokiClient client(transport);

// Every hour send a heartbeat: "msg=heartbeat batt=0.000000 rssi=-62 temp=21.58 humidity=00.00"
#define HBM_LENGTH 100
LokiStream heartbeat(1, HBM_LENGTH, "{job=\"mousetrap\",type=\"heartbeat\",id=\"" ID "\"}");

// Whenever a mouse is caught: "msg=mouse"
#define MM_LENGTH 10
LokiStream mouse(1, MM_LENGTH, "{job=\"mousetrap\",type=\"mouse\",id=\"" ID "\"}");

LokiStreams streams(2);

void setup()
{
  Serial.begin(9600);

  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  Serial.println("Running Setup");

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  // Keep just one message because we only configured one entry per stream.
  char heartbeatMsg[HBM_LENGTH] = {'\0'};
  bool timerHeartbeat = false;
  char mouseMsg[MM_LENGTH] = {'\0'};

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    // In a real event the switch will stay low for at least 1 second
    {
      bool s = true;
      for (uint8_t j = 0; j < 100; j++)
      {
        if (digitalRead(TRAP_GPIO))
        {
          // If the value is back to 1 that means the switch is off
          s = false;
          break;
        }
        else
        {
          Serial.println("Checking GPIO");
          delay(10);
        }
      }
      if (s)
      {
        Serial.println("mouse trigger");
        snprintf(mouseMsg, MM_LENGTH, "msg=mouse");
      }
    }
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    // Build the message below so we can get the RSSI because wifi isn't seutp yet.
    // This isn't great but it's late and I don't feel like refactoring enough stuff to make this seem less brittle.
    timerHeartbeat = true;
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    snprintf(heartbeatMsg, HBM_LENGTH, "msg=poweron");
    break;
  }

  if (timerHeartbeat || heartbeatMsg[0] == '\0' || mouseMsg[0] == '\0')
  {

    transport.setWifiSsid(WIFI_SSID);
    transport.setWifiPass(WIFI_PASS);
    transport.setUseTls(true);
    transport.setCerts(lokiCert, strlen(lokiCert));
    transport.setNtpServer(NTP);
    transport.setDebug(Serial); // Remove this line to disable debug logging of the transport layer.
    if (!transport.begin())
    {
      Serial.println(transport.errmsg);
      while (true)
      {
      };
    }

    // Configure the client
    client.setUrl(URL);
    client.setPath(PATH);
    client.setPort(PORT);

    client.setDebug(Serial); // Remove this line to disable debug logging of the client.
    if (!client.begin())
    {
      Serial.println(client.errmsg);
      while (true)
      {
      };
    }

    // Add our stream objects to the streams object
    streams.addStream(heartbeat);
    streams.addStream(mouse);
    streams.setDebug(Serial); // Remove this line to disable debug logging of the write request serialization and compression.

    uint64_t time;
    time = client.getTimeNanos();

    if (mouseMsg[0] != '\0')
    {
      if (!mouse.addEntry(time, mouseMsg, strlen(mouseMsg)))
      {
        Serial.println(mouse.errmsg);
      }
    }

    if (heartbeatMsg[0] == '\0' && timerHeartbeat)
    {
      float bv = 0.0;
      {
        uint32_t batt = 0;
        // Average a few values
        for (uint8_t i = 0; i < 10; i++)
        {
          batt += analogRead(A0);
          delay(5);
        }

        bv = (batt / 10) * (3.3 / 4095.0) * 2;
      }
#if IDINT == 3
      float t = 0.0;
      float h = 0.0;
      if (!sht31.begin(0x44))
      {
        Serial.println("Couldn't find SHT31");
      }
      else
      {
        t = sht31.readTemperature();
        h = sht31.readHumidity();
      }
      snprintf(heartbeatMsg, HBM_LENGTH, "msg=heartbeat batt=%f rssi=%d temp=%.2f humidity=%.2f", bv, WiFi.RSSI(), t, h);
#else
      snprintf(heartbeatMsg, HBM_LENGTH, "msg=heartbeat batt=%f rssi=%d", bv, WiFi.RSSI());
#endif
    }
    if (heartbeatMsg[0] != '\0')
    {
      if (!heartbeat.addEntry(time, heartbeatMsg, strlen(heartbeatMsg)))
      {
        Serial.println(heartbeat.errmsg);
      }
    }

    uint64_t start = millis();
    for (uint8_t i = 0; i <= 5; i++)
    {
      LokiClient::SendResult res = client.send(streams);
      if (res != LokiClient::SendResult::SUCCESS)
      {
        // Failed to send
        Serial.println(client.errmsg);
        delay(1000);
      }
      else
      {
        heartbeat.resetEntries();
        mouse.resetEntries();

        uint32_t diff = millis() - start;
        Serial.print("Send succesful in ");
        Serial.print(diff);
        Serial.println("ms");
        break;
      }
    }
  }

  // Setup GPIO pin on mouse event (this should be redundant on any but the first run)
  pinMode(TRAP_GPIO, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup(TRAP_GPIO, 0);

  // Reset the wakeup timer
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  // Disable watchdog (not sure if this is strictly necessary) and go to sleep
  esp_task_wdt_delete(NULL);
  esp_task_wdt_deinit();

  Serial.println("Going to sleep now");
  Serial.flush();

  esp_deep_sleep_start();
}

void loop()
{
  // nothing to do here, we loop based on deep sleep cycles
}