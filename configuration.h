// Options
// Number of ms between capturing two images. 10s = 6 images per minute
#define CAPTURE_DELAY 10000
// User defined folder name for logs and capture
#define CAPTURE_FOLDER "/PICS/"
#define LOGS_FOLDER "/LOGS/"

// Credentials
// WiFi network credentials
const char* ssid = "*****";
const char* password = "*****";

// Network config
IPAddress ip(192, 168, 1, 250);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(208, 67, 222, 222);    // OpenDNS
IPAddress secondaryDNS(8, 8, 8, 8);         // Google
