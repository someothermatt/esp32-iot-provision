/* 
 *  This program is designed to allow the user to setup an ESP32 device using a SoftAP on
 *  first boot, or if the saved details have been erased.
 *  
 *  If valid settings are not found, it will launch a SoftAP with a WebServer hosting a form which
 *  allows the user to input SSID, Password, Devicename. The LED lights up when we are in Setup Mode.
 *  
 *  The form is posted to the ESP32 where the ESP32 will attempt to connect to the WiFi
 *  network and test it works. If it does, setupcomplete will be marked as true.
 *  
 *  If setupcomplete is true, the device simply boots and connects to the WiFi network. This could
 *  be expanded to do whatever else is needed such as waking from deepsleep upon a GPIO event to
 *  update a remote server and then sleeping inbetween.
 *  
 *  A user buttonpin is used to forcibly enter setup.
 *  
 */

#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
//Include mdns for hostname resolution
#include <ESPmDNS.h>


const int buttonPin = 47; // Ensure button is pulled high, press pulls LOW
const int ledPin = 21; //led lights up when in setup mode

char ap_ssid[50];  // AP SSID with dynamic part
const char *ap_password = "";

Preferences preferences;
WebServer server(80);

struct {
  String ssid;
  String password;
  String devicename;
  bool setupcomplete;
} settings;




void handleRoot() {
  char temp[1000];

  snprintf(temp, sizeof(temp),
  "<html>\
  <head><title>ESP32 Setup</title>\
  <style>\
  body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; margin: 5; padding: 0; }\
  label { display: block; margin-bottom: 8px; }\
  input { width: 70%; padding: 8px; border: 1px solid #ccc; border-radius: 4px; margin-bottom: 16px; }\
  </style>\
  </head>\
  <body>\
  <h1>Setup page for %s</h1>\
  <form method='POST' action='/submit'>\
  <label for='ssid'>WiFi SSID:</label><input name='ssid' type='text' value='%s'><br>\
  <label for='password'>WiFi Password:</label><input name='password' type='password' value='%s'><br>\
  <label for='devicename'>Device Name:</label><input name='devicename' type='text' value='%s'><br>\
  <input type='submit' value='Submit'>\
  </form>\
  </body>\
  </html>",
  settings.devicename.c_str(),settings.ssid.c_str(), settings.password.c_str(), settings.devicename.c_str());
  server.send(200, "text/html", temp);
}


void handleFormSubmit() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("devicename")) {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    String newDeviceName = server.arg("devicename");

    preferences.begin("my-app", false);
    preferences.putString("ssid", newSSID);
    preferences.putString("password", newPassword);
    preferences.putString("devicename", newDeviceName);
    
    settings.ssid = newSSID;
    settings.password = newPassword;
    settings.devicename = newDeviceName;

    if (WiFi_STA_UP()) {
      server.send(200, "text/html", "<html><body><h1>Success!</h1><p>Setup complete, device is now connected, restarting....</p></body></html>");
      preferences.putBool("setupcomplete", true);
      preferences.end();
      delay(2000);
      ESP.restart();
    } else {
      server.send(200, "text/html", "<html><body><h1>Error!</h1><p>Failed to connect. Please <a href='/'>try again.</a></p></body></html>");
    }
    preferences.end();
  } else {
    server.send(400, "text/html", "<html><body><h1>Bad Request</h1></body></html>");
  }
}


/******************************************************************************
Description.: bring the WiFi up
Return Value: true if WiFi is up, false if it timed out
******************************************************************************/
bool WiFi_STA_UP() {
  WiFi.persistent(false);
  WiFi.setHostname(settings.devicename.c_str());
  if (!settings.setupcomplete) {
    //In setup mode, we need to retain the AP and enter STA mode
    WiFi.mode(WIFI_AP_STA);
  } else {
    //Otherwise, we're just connecting to the saved network
    WiFi.mode(WIFI_STA);
  }
  WiFi.begin(settings.ssid, settings.password);
  
  for (unsigned long i=millis(); millis()-i < 10000;) {
    delay(10);
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
  }
  return false;
}


void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin,LOW);

  // Open Preferences with my-app namespace. Open storage in
  // RW-mode (second parameter false).
  preferences.begin("my-app", false);

  // Read SSID, password, and setupcomplete from Preferences
  // Defaults of null and false are set so we can test if setup is incomplete later
  settings.ssid = preferences.getString("ssid", "");
  settings.password = preferences.getString("password", "");
  settings.devicename = preferences.getString("devicename","");
  settings.setupcomplete = preferences.getBool("setupcomplete", false);

  // Close the Preferences
  preferences.end();

  Serial.println("SSID: " + settings.ssid);
  Serial.println("Password: " + settings.password);
  Serial.println("Setup Complete: " + String(settings.setupcomplete));
  Serial.println("Device Name: " + settings.devicename);

  if (digitalRead(buttonPin) == LOW) {
    settings.setupcomplete = false;
    Serial.println("Setup button pressed, overriding setupcomplete flag");
  }
  if (!settings.setupcomplete || settings.ssid == "" || settings.devicename == "") {
    // Setup Mode init
    Serial.println("Entering setup");
    digitalWrite(ledPin,HIGH);
    
    IPAddress local_IP(192, 168, 10, 1);
    IPAddress gateway(192, 168, 10, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(ap_ssid, sizeof(ap_ssid), "Setup_%02X%02X%02X", mac[3], mac[4], mac[5]);

    // Configure the AP IP settings
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ap_ssid,ap_password);

    server.on("/", handleRoot);
    server.on("/submit", handleFormSubmit);
    server.begin();
    Serial.println("Setup started, connect to http://192.168.10.1");
    
  } else {
    // From here, setup is complete and ssid/password/devicename all have values
    if (WiFi_STA_UP()) {
      Serial.println("Connected to " + settings.ssid + " successfully");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      //Register ourselves on mdns
      if (MDNS.begin(settings.devicename.c_str())) {
        Serial.println("MDNS responder started at " + settings.devicename + ".local");
        MDNS.addService("http", "tcp", 80);
      }
    } else {
      Serial.println("Failed to connect to " + settings.ssid);
      Serial.println("Restarting in 2 seconds");
      delay(2000);
      ESP.restart();
    }    
  }
}

void loop() {
  
  // In Normal mode, this loop will not get run

  if (!settings.setupcomplete) {
    server.handleClient();
    delay(2);
  }
}
