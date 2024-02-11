#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

// Configuration
// const char *apiUrl = "https://api.opendata.metlink.org.nz/v1/stop-predictions?stop_id=5010";
const char *apiUrl = "https://api.opendata.metlink.org.nz/v1/stop-predictions?stop_id=3234";
const char *apiKey = "";

TFT_eSPI tft = TFT_eSPI(); // Initialize TFT display

class ArrivalDetail
{
public:
  String serviceId;
  String destinationName;
  String aimedTime;
  String expectedTime;
  String delay;
};

// Assume a maximum of 20 arrival details for simplicity
#define MAX_ARRIVALS 4
ArrivalDetail arrivalDetails[MAX_ARRIVALS];
int arrivalCount = 0; // Keep track of the actual number of arrival details

String extractTime(const String &timestamp)
{
  // Find the position of 'T' which separates the date and time
  int tIndex = timestamp.indexOf('T');
  // Extract the time part by taking the substring starting from 'T' + 1 to 'T' + 6 (to get hh:mm)
  String time = timestamp.substring(tIndex + 1, tIndex + 6);

  // Return the extracted time string
  return time;
}

bool connectToWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true; // Already connected
  }

  /*
    Serial.print("Connecting to WiFi..");
    WiFi.begin(ssid, password);

    for (int i = 0; i < 10; i++)
    { // Try for 10 seconds
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("\nConnected to WiFi");
        return true;
      }
      delay(1000);
      Serial.print(".");
  */
  //  }

  Serial.println("\nFailed to connect to WiFi");
  return false;
}

String httpGETRequest(const char *url)
{
  HTTPClient http;
  http.begin(url); // Specify the URL
  http.addHeader("accept", "application/json");
  http.addHeader("x-api-key", apiKey);
  int httpCode = http.GET();
  String payload = "";

  if (httpCode > 0)
  {
    payload = http.getString();
  }
  else
  {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpCode);
  }

  http.end();
  return payload;
}

int rowHeight = 20;

void displayArrivalTime(const ArrivalDetail &detail, int index)
{

  // Starting position for the first line of text.
  int y = 0 + (index * rowHeight * 4); // Adjust the multiplication factor to ensure each block is separate.

  // Draw the destination name on the first row.
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(detail.destinationName, 5, y, 2);

  // Move down another row for the due time.
  y += rowHeight;
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Due:", 5, y, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(detail.aimedTime, 70, y, 2);

  // Move down one row for the expected time.
  y += rowHeight;
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Expected:", 5, y, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(detail.expectedTime + " [" + detail.delay + "]", 70, y, 2);
}

void displayArrivalTimes()
{
  for (int i = 0; i < arrivalCount; i++)
  {
    displayArrivalTime(arrivalDetails[i], i); // Display each arrival detail
  }
}

int processApiResponse(const String &payload)
{
  arrivalCount = 0; // Reset count for each API response

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (!error)
  {
    JsonArray departures = doc["departures"].as<JsonArray>();
    for (JsonObject departure : departures)
    {
      // Check if the service_id equals "1" before proceeding
      String serviceId = departure["service_id"].as<String>();
      String destinationName = departure["destination"]["name"].as<String>();

      // if (serviceId != "1" || destinationName != "Churton Park")
      if (serviceId != "1" || destinationName != "Island Bay")
      {
        continue;
      }

      if (arrivalCount >= MAX_ARRIVALS)
        break; // Ensure we don't exceed array bounds

      ArrivalDetail &arrivalDetail = arrivalDetails[arrivalCount++];
      arrivalDetail.serviceId = departure["service_id"].as<String>();
      arrivalDetail.destinationName = departure["destination"]["name"].as<String>();
      arrivalDetail.delay = departure["delay"].as<String>();

      JsonObject arrival = departure["arrival"];
      arrivalDetail.expectedTime = extractTime(arrival["expected"].as<String>());
      arrivalDetail.aimedTime = extractTime(arrival["aimed"].as<String>());
    }
  }
  else
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
  }

  return arrivalCount; // Return the number of ArrivalDetail objects filled
}

void setup()
{
  Serial.begin(115200);

  tft.init();
  // tft.setRotation(3); // Set the display in landscape
  tft.setRotation(2); // Set the display in landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  // indicate on the TFT display that the device is starting
  tft.drawString("Starting...", 5, 30, 2);

  // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
   wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap

  if (!res)
  {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else
  {
    // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }
}

void loop()
{

  // Check if connected to WiFi, if not, attempt to reconnect
  if (connectToWiFi())
  {
    // Perform the request and process the response
    String response = httpGETRequest(apiUrl);
    // Clear the previous display content
    tft.fillScreen(TFT_BLACK);
    int count = processApiResponse(response);
    if (count > 0)
    {
      displayArrivalTimes(); // Display all arrival times
    }
  }
  else
  {
    // Display an error message or handle the lack of connection appropriately
    Serial.println("Unable to connect to WiFi. Check your WiFi settings.");
    tft.drawString("WiFi Disconnected", 5, 30, 2);
  }

  // Wait for 2 minutes before the next update
  delay(120000);
}
