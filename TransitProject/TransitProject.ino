/*
  TALLINN TRANSIT TRACKER
  Using ESP32-HUB75-MatrixPanel-I2S-DMA library
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// WiFi Configuration
// ============================================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API Configuration
// ============================================
const char* apiUrl = "https://api.dev.peatus.ee/routing/v1/routers/estonia/index/graphql";
const char* graphqlQuery = "{\"query\":\"{ stop1: stop(id: \\\"estonia:STOP_ID_1\\\") { name stoptimesWithoutPatterns(numberOfDepartures: 5) { realtimeArrival headsign trip { route { shortName } } } } stop2: stop(id: \\\"estonia:STOP_ID_2\\\") { name stoptimesWithoutPatterns(numberOfDepartures: 5) { realtimeArrival headsign trip { route { shortName } } } } }\"}";
int lastHttpError = 0;

// Time Configuration
// ============================================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 2 * 3600;
const int daylightOffset_sec = 3600;

// LED Matrix Configuration
// ============================================
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define PANELS_NUMBER 2

// Pin definitions
#define R1_PIN 42
#define G1_PIN 40
#define B1_PIN 41
#define R2_PIN 38
#define G2_PIN 37
#define B2_PIN 39
#define A_PIN 45
#define B_PIN 36
#define C_PIN 48
#define D_PIN 35
#define E_PIN 21
#define LAT_PIN 47
#define OE_PIN 14
#define CLK_PIN 2

HUB75_I2S_CFG::i2s_pins _pins = {
  R1_PIN, G1_PIN, B1_PIN,
  R2_PIN, G2_PIN, B2_PIN,
  A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
  LAT_PIN, OE_PIN, CLK_PIN
};

MatrixPanel_I2S_DMA *matrix = nullptr;

// ============================================
// Data Structures
struct Departure {
  int arrivalSeconds;
  String routeNumber;
  String destination;
  int minutesUntil;
};

Departure allDepartures[10];

// Setup
// ============================================
void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println("\n=== TALLINN TRANSIT TRACKER ===\n");

  // Configure matrix
  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,
    PANEL_HEIGHT,
    PANELS_NUMBER,
    _pins
  );
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;  // Lower speed = better signal quality!
  mxconfig.clkphase = false;

  // Initialize matrix
  Serial.println("[1/4] Initializing LED matrix...");
  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(60);  // Lower or raise brightness 
  matrix->clearScreen();
  Serial.println("✓ Matrix initialized");

  matrix->setTextColor(matrix->color565(0, 255, 0));
  matrix->setCursor(2, 2);
  matrix->print("MATRIX OK");
  delay(1500);

  // Connect WiFi
  Serial.println("\n[2/4] Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);

  matrix->clearScreen();
  matrix->setCursor(2, 2);
  matrix->setTextColor(matrix->color565(255, 200, 0));
  matrix->print("WIFI...");

  WiFi.begin(ssid, password);

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 40) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected");
    Serial.print("Network: ");
    Serial.println(ssid);

    matrix->clearScreen();
    matrix->setCursor(2, 2);
    matrix->setTextColor(matrix->color565(0, 255, 0));
    matrix->print("WIFI OK");
    matrix->setCursor(2, 12);
    matrix->setTextColor(matrix->color565(255, 255, 255));
    matrix->print(ssid);
    delay(2000);
  } else {
    Serial.println("\n✗ WiFi connection failed");
    matrix->clearScreen();
    matrix->setCursor(2, 2);
    matrix->setTextColor(matrix->color565(255, 0, 0));
    matrix->print("WIFI FAIL");
    delay(3000);
  }

  // Sync time
  Serial.println("\n[3/4] Syncing time with NTP server...");
  Serial.print("NTP Server: ");
  Serial.println(ntpServer);

  matrix->clearScreen();
  matrix->setCursor(2, 2);
  matrix->setTextColor(matrix->color565(255, 200, 0));
  matrix->print("TIME SYNC...");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  int timeAttempts = 0;
  while (time(nullptr) < 100000 && timeAttempts < 20) {
    delay(500);
    Serial.print(".");
    timeAttempts++;
  }

  if (time(nullptr) > 100000) {
    Serial.println("\n✓ Time synced successfully");
    time_t now = time(nullptr);
    Serial.print("Current time: ");
    Serial.println(ctime(&now));

    matrix->clearScreen();
    matrix->setCursor(2, 2);
    matrix->setTextColor(matrix->color565(0, 255, 0));
    matrix->print("TIME OK");
    delay(1500);
  } else {
    Serial.println("\n✗ Time sync failed");
    matrix->clearScreen();
    matrix->setCursor(2, 2);
    matrix->setTextColor(matrix->color565(255, 0, 0));
    matrix->print("TIME FAIL");
    delay(2000);
  }

  // Network stabilization
  Serial.println("\n[4/4] Stabilizing network stack...");
  matrix->clearScreen();
  matrix->setCursor(2, 2);
  matrix->setTextColor(matrix->color565(255, 200, 0));
  matrix->print("STABILIZING");
  delay(2000);
  Serial.println("✓ Network ready");

  // Ready!
  Serial.println("\n=== SYSTEM READY ===");
  Serial.println("Starting transit monitoring...\n");

  matrix->clearScreen();
  matrix->setCursor(2, 2);
  matrix->setTextColor(matrix->color565(0, 255, 0));
  matrix->print("READY!");
  matrix->setCursor(2, 14);
  matrix->setTextColor(matrix->color565(255, 255, 255));
  matrix->print("Starting...");
  delay(2000);
  matrix->clearScreen();
}

// Main Loop
// ============================================
void loop() {
  Serial.println("=== Fetching Departures ===");

  String jsonResponse = queryTransitAPI();

  if (jsonResponse.length() > 0) {
    int departureCount = parseAndMergeDepartures(jsonResponse);

    if (departureCount > 0) {
      sortDepartures(departureCount);
      displayOnLED(departureCount);
      printToSerial(departureCount);
      Serial.println("Waiting 60 seconds...\n");
      delay(60000);
    } else {
      Serial.println("✗ No departures");
      showError("NO DATA");
      Serial.println("Waiting 60 seconds...\n");
      delay(60000);
    }
  } else {
    Serial.println("✗ API failed");

    // Create custom error message with the HTTP error code
    String errorMsg = "API ERR:" + String(lastHttpError);
    showError(errorMsg);

    // If error is -1 or -2 (connection/WiFi issue), try to reconnect
    if (lastHttpError == -1 || lastHttpError == -2) {
      Serial.println("Connection issue detected. Attempting WiFi reconnect...");
      delay(1000);

      if (reconnectWiFi()) {
        Serial.println("Retrying API call immediately...");
        delay(1000);
      } else {
        Serial.println("Waiting 5 seconds before retry...\n");
        delay(5000);
      }
    } else {
      Serial.println("Waiting 5 seconds...\n");
      delay(5000);
    }
  }
}

// API Query
// ============================================
String queryTransitAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ WiFi not connected");
    lastHttpError = -2;  // Custom code for WiFi disconnected
    return "";
  }

  HTTPClient http;
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);

  int httpCode = http.POST(graphqlQuery);
  String response = "";

  if (httpCode == 200) {
    response = http.getString();
    Serial.println("✓ API success");
    lastHttpError = 0;  // Success
  } else {
    Serial.printf("✗ HTTP error: %d\n", httpCode);
    lastHttpError = httpCode;  // Store the error code
  }

  http.end();
  delay(100);
  return response;
}

// Reconnect WiFi
// ============================================
bool reconnectWiFi() {
  Serial.println("Attempting to reconnect WiFi...");

  // Show reconnecting message on display
  matrix->clearScreen();
  matrix->setCursor(2, 12);
  matrix->setTextColor(matrix->color565(255, 200, 0));
  matrix->print("RECONNECTING");

  WiFi.disconnect();
  delay(1000);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi reconnected");
    Serial.print("Network: ");
    Serial.println(ssid);

    matrix->clearScreen();
    matrix->setCursor(2, 12);
    matrix->setTextColor(matrix->color565(0, 255, 0));
    matrix->print("WIFI OK");
    delay(2000);

    return true;
  } else {
    Serial.println("\n✗ WiFi reconnection failed");

    matrix->clearScreen();
    matrix->setCursor(2, 12);
    matrix->setTextColor(matrix->color565(255, 0, 0));
    matrix->print("WIFI FAIL");
    delay(2000);

    return false;
  }
}


// Parse JSON
// ============================================
int parseAndMergeDepartures(String json) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("✗ JSON error: ");
    Serial.println(error.c_str());
    return 0;
  }

  int index = 0;
  int currentSeconds = getCurrentSecondsOfDay();

  JsonArray stop1Times = doc["data"]["stop1"]["stoptimesWithoutPatterns"];
  for (JsonVariant departure : stop1Times) {
    if (index >= 10) break;

    allDepartures[index].arrivalSeconds = departure["realtimeArrival"];
    allDepartures[index].routeNumber = departure["trip"]["route"]["shortName"].as<String>();
    allDepartures[index].destination = departure["headsign"].as<String>();

    int secondsUntil = allDepartures[index].arrivalSeconds - currentSeconds;
    if (secondsUntil < 0) secondsUntil += 86400;
    allDepartures[index].minutesUntil = secondsUntil / 60;

    index++;
  }

  JsonArray stop2Times = doc["data"]["stop2"]["stoptimesWithoutPatterns"];
  for (JsonVariant departure : stop2Times) {
    if (index >= 10) break;

    allDepartures[index].arrivalSeconds = departure["realtimeArrival"];
    allDepartures[index].routeNumber = departure["trip"]["route"]["shortName"].as<String>();
    allDepartures[index].destination = departure["headsign"].as<String>();

    int secondsUntil = allDepartures[index].arrivalSeconds - currentSeconds;
    if (secondsUntil < 0) secondsUntil += 86400;
    allDepartures[index].minutesUntil = secondsUntil / 60;

    index++;
  }

  return index;
}


// Sort
// ============================================
void sortDepartures(int count) {
  for (int i = 0; i < count - 1; i++) {
    for (int j = 0; j < count - i - 1; j++) {
      if (allDepartures[j].minutesUntil > allDepartures[j + 1].minutesUntil) {
        Departure temp = allDepartures[j];
        allDepartures[j] = allDepartures[j + 1];
        allDepartures[j + 1] = temp;
      }
    }
  }
}

// Clean Estonian Characters
// ============================================
String cleanEstonianText(String text) { // the library can't parse weird letters onto the led matrix board
  text.replace("õ", "o");
  text.replace("Õ", "O");
  text.replace("ä", "a");
  text.replace("Ä", "A");
  text.replace("ö", "o");
  text.replace("Ö", "O");
  text.replace("ü", "u");
  text.replace("Ü", "U");

  // Remove words to fit long destination names
  text.replace(" parkla", "");
  text.replace(" Parkla", "");

  return text;
}

// Display on LED
// ============================================
void displayOnLED(int count) {
  matrix->clearScreen();

  int displayedRows = 0;

  for (int i = 0; i < count && displayedRows < 3; i++) {
    // Skip departures more than 99 minutes away
    if (allDepartures[i].minutesUntil > 99) {
      continue;
    }

    int y = displayedRows * 10 + 2;  // Shift down by 2 pixels

    // Route number - color-coded by route
    matrix->setCursor(2, y);  // Shift right by 2 pixels
    String routeNum = allDepartures[i].routeNumber;

    // Set color based on route number
    if (routeNum == "54") {
      matrix->setTextColor(matrix->color565(0, 100, 255));  // Blue for route 54
    } else if (routeNum == "61") {
      matrix->setTextColor(matrix->color565(0, 255, 0));    // Green for route 61
    } else {
      matrix->setTextColor(matrix->color565(255, 255, 255)); // White for other routes
    }
    matrix->print(routeNum);

    // Destination (orange)
    matrix->setCursor(20, y);
    matrix->setTextColor(matrix->color565(255, 177, 0));
    String dest = cleanEstonianText(allDepartures[i].destination);
    if (dest.length() > 14) {
      dest = dest.substring(0, 14);
    }
    matrix->print(dest);

    // Minutes (orange), NOW (red)
    matrix->setCursor(108, y);
    matrix->setTextColor(matrix->color565(255, 177, 0));
    int mins = allDepartures[i].minutesUntil;
    if (mins < 1) {
      matrix->setTextColor(matrix->color565(187, 0, 0));
      matrix->print("NOW");
    } else {
      matrix->print(mins);
      matrix->print("m");
    }

    displayedRows++;
  }
}

// Print to Serial
// ============================================
void printToSerial(int count) {
  Serial.println("\n--- NEXT 3 DEPARTURES ---");

  int displayCount = min(count, 3);
  for (int i = 0; i < displayCount; i++) {
    Serial.printf("%d. [%s] %s - %d min\n",
                  i + 1,
                  allDepartures[i].routeNumber.c_str(),
                  allDepartures[i].destination.c_str(),
                  allDepartures[i].minutesUntil);
  }

  Serial.println("-------------------------\n");
}

// Error Message
// ============================================
void showError(String msg) {
  matrix->clearScreen();
  matrix->setTextColor(matrix->color565(255, 0, 0));
  matrix->setCursor(2, 12);
  matrix->print(msg);
}

// Get Current Time
// ============================================
int getCurrentSecondsOfDay() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  return timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec;
}
