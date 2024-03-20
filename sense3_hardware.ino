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

/************Variables for Wifi Provisioning*************/
// #define USE_SOFT_AP // Uncomment if you want to enforce using Soft AP method instead of BLE

const char * pop = "abcd1234"; // Proof of possession - otherwise called a PIN - string provided by the device, entered by user in the phone app
const char * service_name = "PROV_123"; // Name of your device (the Espressif apps expects by default device name starting with "Prov_")
const char * service_key = NULL; // Password used for SofAP method (NULL = no password needed)
bool reset_provisioned = true; // When true the library will automatically delete previously provisioned data.
bool wifi_connected=false; // When false, the program does no proceed 

/************Variables for IR sensor*************/
#define GP2Y0E03_SENSOR_ADDRESS 0x40 // found with a wire scan
                           // registers adresses  and formula to get distance in cm found on https://www.digikey.co.th/htmldatasheets/production/1568259/0/0/1/gp2y0e03-specification.html
#define DISTANCE_REGISTER_ADDRESS 0x5E // starting register address containing raw data of distance that take 2 bytes of space 
#define SHIFT_BIT_REGISTER_ADDRESS 0x35 // register address containing coefficient n found in formula

byte shift=0;
byte raw_distance_array[2]={0};
byte distance_in_cm=0;
char string_buffer[100];
volatile int stopFlag;
byte previous_distance=0;
long same_distance_count=0;
long distance_error_count=0; //to be sure that there are no Reading error from distance register before sending idle signal
//unsigned long previous_time=0;
//const unsigned long time_interval=40000; //interval of 40 seconds before seing if idle allert is ent or not

/************Variables for Firebase*************/
// Insert Firebase project API Key
#define API_KEY "AIzaSyBOBl-OiKnSTJHiV2cohByZnJ7xvj5QU7o"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://dsensetest-default-rtdb.firebaseio.com/" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup() {
  // put your setup code here, to run once:
  Wire.begin(); //initialyze I2C bus
  delay(1000);
  Serial.begin(115200);
  Wifi_Prov_setup(); //WIFI provisioning set up
  while (!wifi_connected){
    Serial.print(".");
    delay(300);
  }
  setup_IR_sensor();
  Firebase_setup();
    


}

void loop() {
  // put your main code here, to run repeatedly:
    previous_distance=distance_in_cm;
    IR_sensor_actions();
    
    
    
    if(previous_distance==distance_in_cm){
      same_distance_count++; //records number of times the distance did not change between readings
    }                       //relevent when it gets to 58, since there are around 58 readings in 40 seconds since there is a reading every 700 milliseconds
    else{
      same_distance_count=0; //when the distance changes, the counter is reset
    }
    
    if(same_distance_count>=58UL&&distance_error_count==0){
      char status[]="Printer is idle";
      Serial.println(status); 
      
      if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){ //where we send notification to database that printer is idle each 5 seconds
              sendDataPrevMillis = millis();
            // Write an Int number on the database path test/int
          if (Firebase.RTDB.setString(&fbdo, "test/String", status)){
            Serial.println("PASSED");
            Serial.println("PATH: " + fbdo.dataPath());
            Serial.println("TYPE: " + fbdo.dataType());
          }
          else {
            Serial.println("FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }
      }
      // Serial.print("Printing time: ");
      // Serial.print(current_time/1000UL);
      // Serial.println(" secounds");

    }
    else{
          
            char status[]="Printer is still running";
            Serial.println(status); 

            if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){ //where we send notification to database that printer is running each 5 seconds
              sendDataPrevMillis = millis();
            // Write an Int number on the database path test/int
          if (Firebase.RTDB.setString(&fbdo, "test/String", status)){
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


void setup_IR_sensor(){
  
  Wire.beginTransmission(GP2Y0E03_SENSOR_ADDRESS); //starting communication with sensor
  Wire.write(byte(SHIFT_BIT_REGISTER_ADDRESS)); //writing data from bit shift register
  Wire.endTransmission();

  Wire.requestFrom(GP2Y0E03_SENSOR_ADDRESS, 1); //ESP32 requests the 1 byte from bit shift register
  
  if (1 <= Wire.available())
  {
    shift = Wire.read(); //store data from bit shift register
  }
  else{
    Serial.println("Reading error from shift bit register");
    //stop(); 
  }
  delay(1000);
}

void IR_sensor_actions(){
    int return_value;
    Wire.beginTransmission(GP2Y0E03_SENSOR_ADDRESS);
    Wire.write(byte(DISTANCE_REGISTER_ADDRESS));
    return_value=Wire.endTransmission();
    delay(200);

    Serial.println(return_value);//test to see value of end transmission

  if (return_value==0)
  {
    Wire.requestFrom(GP2Y0E03_SENSOR_ADDRESS, 2);
    raw_distance_array[0] = Wire.read(); //upper 8 bits of data
    raw_distance_array[1] = Wire.read(); //lower 8 bits of data
    byte new_lower_distance=0;

    for(int i=7;i>3;i--){
      new_lower_distance=bitClear(raw_distance_array[1],i); //clear the 4 most significant bit since according to the formula, only the 4 least significant bit are kept
    }

    
    distance_in_cm=((raw_distance_array[0]<<4)|new_lower_distance)/16/(int)pow(2,shift); //formula from https://www.digikey.co.th/htmldatasheets/production/1568259/0/0/1/gp2y0e03-specification.html

      if(distance_in_cm>50||distance_in_cm<4){
      Serial.println("Out of bounds");
      }
      
      else{
      sprintf(string_buffer, "Distance of %u cm", distance_in_cm);
      Serial.println(string_buffer);
      distance_error_count=0;
      }
    
    
  }
  
  else
  {
    Serial.println("Reading error from distance register");
    distance_error_count++;
    //stop();
  }


  delay(500); //delay before next reading
}

void stop(){
  stopFlag=1;
  while(stopFlag);
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
