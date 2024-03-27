#include <Arduino.h>
#include <Wire.h>
#include "WiFiProv.h"

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

/************Printer Identification*************/
#define printerId "1"
#define username "ayoubkx" //to change to a default value
#define printerName "test" ////to change to a default value
/************Variables for Wifi Provisioning*************/
// #define USE_SOFT_AP // Uncomment if you want to enforce using Soft AP method instead of BLE

const char * pop = "abcd1234"; // Proof of possession - otherwise called a PIN - string provided by the device, entered by user in the phone app
const char * service_name = "PROV_123"; // Name of your device (the Espressif apps expects by default device name starting with "Prov_")
const char * service_key = NULL; // Password used for SofAP method (NULL = no password needed)
bool reset_provisioned = true; // When true the library will automatically delete previously provisioned data.
bool wifi_connected=false; // When false, the program does no proceed 

/************Variables for IR sensor*************/

const int sensorPin = 34;

char string_buffer[100];
int previous_distance=0;
int calibrated_distance=0;
long same_distance_count=0;
long error_count=0;

/************Variables for Firebase*************/
// Insert Firebase project API Key
#define API_KEY "AIzaSyAIfgpLG4aHalsMipBkazJE14G4_-K66LI"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://dsense-8baa5-default-rtdb.firebaseio.com/" 

//Define Firebase Data object
FirebaseData fbdo;
  
String uniqueKey = "-NtmTTJj0lChTWqPl5Ak";

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Wifi_Prov_setup(); //WIFI provisioning set up
  while (!wifi_connected){
    Serial.print(".");
    delay(300);
  }
  pinMode(sensorPin, INPUT); //set up for IR sensor readings
  Firebase_setup();


Firebase.RTDB.setString(&fbdo, "printers/" + uniqueKey + "/printerId", printerId);
Firebase.RTDB.setString(&fbdo, "printers/" + uniqueKey + "/username", username);
Firebase.RTDB.setString(&fbdo, "printers/" + uniqueKey + "/printerName", printerName);
  
  
  
  // Firebase.RTDB.setString(&fbdo, "printers/test/printerId", printerId);
  // Firebase.RTDB.setString(&fbdo, "printers/test/username", username);
  // Firebase.RTDB.setString(&fbdo, "printers/test/printerName", printerName);  
}

void loop() {
  // put your main code here, to run repeatedly:
    previous_distance=calibrated_distance;
    IR_sensor_actions();
    
    
    
 if(previous_distance==calibrated_distance||previous_distance-3<=calibrated_distance&&calibrated_distance<=previous_distance+3||calibrated_distance==69){ //take into account incertitude 
      same_distance_count++; //records number of times the distance did not change between readings
      if(calibrated_distance==69){
        error_count++;
          if(error_count<=5){
              Serial.println("No issue for now");
          }
          else{
            Serial.println("Reading error");
          }
  } 
      
  }                       //relevent when it gets to 20, since there are around 20 readings in 40 seconds since there is a reading every 2 seconds
 
    else{
      same_distance_count=0; //when the distance changes, the counter is reset
      error_count=0;
    }

    Serial.println(same_distance_count);
    
    if(same_distance_count>=20UL){                                      
      char status[]="idle";
      Serial.println(status); 
      
      if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){ //where we send notification to database that printer is idle each 5 seconds
              sendDataPrevMillis = millis();
            // Write an Int number on the database path test/int
          if (Firebase.RTDB.setString(&fbdo, "printers/" + uniqueKey + "/status", status)){
            Serial.println("PASSED");
            Serial.println("PATH: " + fbdo.dataPath());
            Serial.println("TYPE: " + fbdo.dataType());
          }
          else {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }
      }
    }
    else{
            char status[]="running";
            Serial.println(status); 

            if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){ //where we send notification to database that printer is running each 5 seconds
              sendDataPrevMillis = millis();
            // Write an Int number on the database path test/int
          if (Firebase.RTDB.setString(&fbdo, "printers/" + uniqueKey + "/status", status)){
            Serial.println("PASSED");
            Serial.println("PATH: " + fbdo.dataPath());
            Serial.println("TYPE: " + fbdo.dataType());
          }
          else {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }
      }
    }
}




void IR_sensor_actions(){
 int raw = analogRead(sensorPin);
    calibrated_distance = (69.0 - ((float)raw * 0.0226));
    sprintf(string_buffer, "Distance of %u cm", calibrated_distance);
    Serial.println(string_buffer);  
    delay(1000);
}



void SysProvEvent(arduino_event_t *sys_event) //function needed for WIfiProv
{
    switch (sys_event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("\nConnected IP address : ");
        Serial.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
        wifi_connected=true; // changing the state of wifi connected to true
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("\nDisconnected. Connecting to the AP again... ");
        wifi_connected=false; // keeping the state of wifi connected to false
        break;
    case ARDUINO_EVENT_PROV_START:
        Serial.println("\nProvisioning started\nGive Credentials of your access point using smartphone app");
        break;
    case ARDUINO_EVENT_PROV_CRED_RECV: {
        Serial.println("\nReceived Wi-Fi credentials");
        Serial.print("\tSSID : ");
        Serial.println((const char *) sys_event->event_info.prov_cred_recv.ssid);
        Serial.print("\tPassword : ");
        Serial.println((char const *) sys_event->event_info.prov_cred_recv.password);
        break;
    }
    case ARDUINO_EVENT_PROV_CRED_FAIL: {
        Serial.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
        if(sys_event->event_info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR)
            Serial.println("\nWi-Fi AP password incorrect");
        else
            Serial.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");
        break;
    }
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
        Serial.println("\nProvisioning Successful");
        break;
    case ARDUINO_EVENT_PROV_END:
        Serial.println("\nProvisioning Ends");
        break;
    default:
        break;
    }
}

void Wifi_Prov_setup(){
   WiFi.onEvent(SysProvEvent);

#if CONFIG_IDF_TARGET_ESP32 && CONFIG_BLUEDROID_ENABLED && not USE_SOFT_AP
    Serial.println("Begin Provisioning using BLE");
    // Sample uuid that user can pass during provisioning using BLE
    uint8_t uuid[16] = {0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                        0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02 };
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name, service_key, uuid, reset_provisioned);
#else
    Serial.println("Begin Provisioning using Soft AP");
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name, service_key);
#endif

  #if CONFIG_BLUEDROID_ENABLED && not USE_SOFT_AP
    log_d("ble qr");
    WiFiProv.printQR(service_name, pop, "ble");
  #else
    log_d("wifi qr");
    WiFiProv.printQR(service_name, pop, "softap");
  #endif
}

void Firebase_setup(){
    /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

}
