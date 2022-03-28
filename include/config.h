#define WIFI_SSID ""
#define WIFI_PASS ""

#define URL "loki.edjusted.com" // "192.168.0.1" or "lokiserver.yourdomain.local" No http or https, No port or path
#define PORT 443
#define PATH "/loki/api/v1/push"
#define NTP "172.20.31.1"

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 600         /* Time ESP32 will go to sleep (in seconds) */
#define WDT_TIMEOUT 300
#define TRAP_GPIO GPIO_NUM_12

// I don't know an easier way to do this to be able to string substitute
// the ID in the labels and then do comparison for enabling the temp sensor...
#define ID "1"
#define IDINT 1