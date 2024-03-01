#include <Wire.h>

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
//unsigned long previous_time=0;
//const unsigned long time_interval=40000; //interval of 40 seconds before seing if idle allert is ent or not

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  setup_IR_sensor();
    


}

void loop() {
  // put your main code here, to run repeatedly:
    unsigned long current_time=millis();
    previous_distance=distance_in_cm;
    IR_sensor_actions();
    if(previous_distance==distance_in_cm){
      same_distance_count++; //records number of times the distance did not change between readings
    }                       //relevent when it gets to 20, since there are around 20 readings in 40 seconds since there is a reading every 2 seconds
    else{
      same_distance_count=0; //when the distance changes, the counter is reset
    }
    
    if(same_distance_count>=20UL){
      Serial.println("Printer is idle"); //where we send notification to database that printer is idle
      
      // Serial.print("Printing time: ");
      // Serial.print(current_time/1000UL);
      // Serial.println(" secounds");

    }
    else{
      Serial.println("Printer is still running");
    }

}


void setup_IR_sensor(){
    
  Wire.begin(); //initialyze I2C bus

  delay(2000);
  

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
}

void IR_sensor_actions(){
  Wire.beginTransmission(GP2Y0E03_SENSOR_ADDRESS);
    Wire.write(byte(DISTANCE_REGISTER_ADDRESS));
    Wire.endTransmission();

    Wire.requestFrom(GP2Y0E03_SENSOR_ADDRESS, 2);

  if (2 <= Wire.available())
  {
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
      }
    
    
  }
  
  else
  {
    Serial.println("Reading error from distance register");
    //stop();
  }

  // distance_count++;
  // Serial.println(distance_count);
  delay(2000); //delay before next reading
}

void stop(){
  stopFlag=1;
  while(stopFlag);
}
