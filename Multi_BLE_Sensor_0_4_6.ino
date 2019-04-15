/*
    Multi BLE Sensor 0.4.6 - Richard Hedderly 2019

    Based on heart sensor code by Andreas Spiess which was based on a Neil Kolban example.
    
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h> 

//byte flags = 0b00111110;
byte bpm;                                                // Beats Per Minute
byte speedkph;                                           // Speed in KPH
byte speedmph;                                           // Speed in MPH 
byte heart[8] = {0b00001110, 60, 0, 0, 0 , 0, 0, 0};     // Heart array for BLE heart characteristic
byte cscmeasurement[5] = {0b00000010, 0, 0, 0, 0};       // 0x2A5B NOTIFY - Binary to select cadence only, 2 bytes for cumlat crank revs, 2 for last event time 
byte cscfeature[1] = {0b0000000000000010};               // 0x2A5C READ - Crank revolution supported only 
byte sensorlocation[1] = {6};                            // 0x2A5D READ - Sensor on right crank 
//byte sccontrol[1] = {2};                               // 0x2A55 WRITE & INDICATE - SC Control Point - OPTIONAL 
int current_crank_pulse;                                 // Faux random milliseconds int for how long one crank rev takes
unsigned long time_now = 0;                              // Timestamp in millis for cadence section
unsigned long time_second = 0;                           // Timestamp in millis for heart section
unsigned long current_crank_time = 0;                    // Timestamp in millis for current crank time
unsigned long last_crank_time = 0;                       // Timestamp in millis for last crank time
unsigned long diff_crank_time = 0;                       // Timestamp for difference between current and last crank times
double temp_calc_crank_time = 0;                         // Holds first part of conversion from 1/1000 second to 1/1024 second - preserves decimal places
uint16_t calc_crank_time = 0;                            // Holds second part of conversion from 1/100 second to 1/1024 second
uint16_t cuml_crank_revs = 0;                            // Cumlative number of crank revolutions

bool _BLEClientConnected = false;

// Define Heart Properties
#define heartRateService BLEUUID((uint16_t)0x180D)                                                                                // Define BLE Heart Rate Service
BLECharacteristic heartRateMeasurementCharacteristics(BLEUUID((uint16_t)0x2A37), BLECharacteristic::PROPERTY_NOTIFY);             // Heart Rate Characteristics
BLEDescriptor heartRateDescriptor(BLEUUID((uint16_t)0x2901));                                                                     //0x2901 is a custom user description

// Define Speed and Cadence Properties
#define speedService BLEUUID((uint16_t)0x1816)                                                                                    // Define CSC Service
BLECharacteristic cscMeasurementCharacteristics(BLEUUID((uint16_t)0x2A5B), BLECharacteristic::PROPERTY_NOTIFY);                   //CSC Measurement Characteristic
BLECharacteristic cscFeatureCharacteristics(BLEUUID((uint16_t)0x2A5C), BLECharacteristic::PROPERTY_READ);                         //CSC Feature Characteristic
BLECharacteristic sensorLocationCharacteristics(BLEUUID((uint16_t)0x2A5D), BLECharacteristic::PROPERTY_READ);                     //Sensor Location Characteristic
//BLECharacteristic scControlPointCharacteristics(BLEUUID((uint16_t)0x2A55), BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE); // SC Control Point Characteristic
BLEDescriptor cscMeasurementDescriptor(BLEUUID((uint16_t)0x2901));                                                                //0x2901 is a custom user description - OPTIONAL
BLEDescriptor cscFeatureDescriptor(BLEUUID((uint16_t)0x2901));                                                                    //0x2901 is a custom user description
BLEDescriptor sensorLocationDescriptor(BLEUUID((uint16_t)0x2901));                                                                //0x2901 is a custom user description
//BLEDescriptor scControlPointDescriptor(BLEUUID((uint16_t)0x2901));                                                              //0x2901 is a custom user description - OPTIONAL

// Define Battery
// Service UUID 0x180F - Battery Level UUID 0x2A19

// Define Firmware
// UUID 0x2A26


class MyServerCallbacks : public BLEServerCallbacks {                                                                             // BLE Server Connect and Disconnect routines
    void onConnect(BLEServer* pServer) {                                                                                          //                      "
      _BLEClientConnected = true;                                                                                                 //                      "
    };                                                                                                                            //                      "  
                                                                                                                                  //                      "
    void onDisconnect(BLEServer* pServer) {                                                                                       //                      "
      _BLEClientConnected = false;                                                                                                //                      "
    }                                                                                                                             //                      "
};                                                                                                                                //                      "

void InitBLE() {
  
  // Create BLE Device
  BLEDevice::init("Multi Fitness BLE Sensor");                                                                                    

  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();                                                                                 
  pServer->setCallbacks(new MyServerCallbacks());                                                                              
  
  // Create BLE Heart Configuration
  BLEService *pHeart = pServer->createService(heartRateService);                                                                   // Create Heart Service
  pHeart->addCharacteristic(&heartRateMeasurementCharacteristics);                                                                 // Add Heart Rate Characteristics to Heart Service
  heartRateDescriptor.setValue("Exercise Bike Pulse Grips Rate");                                                                  // Description to Heart Service
  heartRateMeasurementCharacteristics.addDescriptor(&heartRateDescriptor);                                                         // Add Description to Heart Service
  heartRateMeasurementCharacteristics.addDescriptor(new BLE2902());                                                                // Add BLE2902 Description to Heart Service
                                
  // Create Speed and Cadence Configuration
  BLEService *pSpeed = pServer->createService(speedService);                                                                       // Create Speed and Cadence CSC Service

  //pSpeed->addCharacteristic(&scControlPointCharacteristics); - OPTIONAL
  pSpeed->addCharacteristic(&sensorLocationCharacteristics);                                                                       // Add Sensor Location characteristic to CSC Service
  pSpeed->addCharacteristic(&cscFeatureCharacteristics);                                                                           // Add crank only feature characteristic to CSC Service
  pSpeed->addCharacteristic(&cscMeasurementCharacteristics);                                                                       // Add cadence only feature characteristic to CSC Service
 
  //scControlPointDescriptor.setValue("Exercise Bike CSC SC Control Point"); - OPTIONAL
  //scControlPointCharacteristics.addDescriptor(&scControlPointDescriptor); - OPTIONAL
  sensorLocationDescriptor.setValue("Exercise Bike CSC Sensor Location");                                                          // CSC Sensor Location description
  sensorLocationCharacteristics.addDescriptor(&sensorLocationDescriptor);                                                          // Add CSC Sensor Location description to CSC Service                                                         
  cscFeatureDescriptor.setValue("Exercise Bike CSC Feature");                                                                      // CSC Feature description
  cscFeatureCharacteristics.addDescriptor(&cscFeatureDescriptor);                                                                  // Add CSC Feature Location description to CSC Service
  cscMeasurementDescriptor.setValue("Exercise Bike CSC Measurement");                                                              // CSC Measurement description
  cscMeasurementCharacteristics.addDescriptor(&cscMeasurementDescriptor);                                                          // Add CSC Measurement description to CSC Service
   
  // Add UUIDs for Services to BLE Service Advertising                                                                             
  pServer->getAdvertising()->addServiceUUID(heartRateService);                                                                     // BLE Advertise Heart Rate Service
  pServer->getAdvertising()->addServiceUUID(speedService);                                                                         // BLE Advertise Speed and Cadence Service

  // Start p Instances
  pSpeed->start();
  pHeart->start();
 
  // Start Advertising
  pServer->getAdvertising()->start();  
}

void setup() {
  Serial.begin(115200);                                                                                                            // Set serial baud rate
  InitBLE();                                                                                                                       // Start InitBLE
  bpm = random(95, 125);                                                                                                           // Set random heart rate
  last_crank_time = millis();                                                                                                      // Set millis for last crank time
  current_crank_time = millis();                                                                                                   // Set millis for current crank time
  time_now = millis();            
  current_crank_pulse = random(1100, 1300);                                                                                        // Random complete revolution in faux milliseconds

  cscFeatureCharacteristics.setValue(cscfeature, 1);                                                                               // Set cscfeature with single
  sensorLocationCharacteristics.setValue(sensorlocation, 1);                                                                       // Set csc sensor location with single
  //scControlPointCharacteristics.setValue(sccontrol, 1); - OPTIONAL
}

void loop() {
 
  // _______________________
  // SPEED + CADENCE SECTION
  // ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
  // Read Cadence RPM Pins
  // Stand-in Cadence Reed Switch
  // Calculate Speed from cadence 
  // Calculate Last Crank 
 
  if (millis() >= time_now + current_crank_pulse)               // If current millis is greater or equal to previous time millis + faux random millis
     {
     cuml_crank_revs++;                                         // Add another crank revolution to rev counter
     cscmeasurement[1] = highByte (cuml_crank_revs);            // Convert higher (left) part of current cumlative amount of revs uint16_t to a byte 
     cscmeasurement[2] = lowByte (cuml_crank_revs);             // Convert lower (right) part of current cumlative amount of revs uint16_t to a byte
     
     // Calculate crank timings     
    
     current_crank_pulse = random(1100, 1300);                  // Create new faux random millis as pretend complete crank revolution 
     current_crank_time = millis();                             // Set current crank time to now in millis
     diff_crank_time = current_crank_time - last_crank_time;    // difference in millis is now in millis - the last crank in millis
     temp_calc_crank_time = (diff_crank_time / 1000.00);        // Convert the difference in millis into double to preserve decimal places  
     calc_crank_time = temp_calc_crank_time * 1024;             // Convert to 1/1024 of a second
     
     // Convert calc_crank_time into cscmeasurement byte array                     

     cscmeasurement[3] = highByte (calc_crank_time);            // Convert higher (left) part of time since last crank (calc_crank_time) uint16_t to a byte
     cscmeasurement[4] = lowByte (calc_crank_time);             // Convert lower (right) part of time since last crank (calc_crank_time) uint16_t to a byte

     // Display current values
          
     //Serial.print ("Curr Crank Time = ");
     //Serial.print (current_crank_time);
     //Serial.print (" | Last Crank Time = ");
     //Serial.print (last_crank_time);    
     Serial.print ("Diff in millis = ");
     Serial.print (diff_crank_time);
     Serial.print (" | Calc to 1/1024 = ");
     Serial.print (calc_crank_time);
     Serial.print (" | CSC Mea H = ");
     Serial.print (cscmeasurement[3],HEX);
     Serial.print (" | CSC Mea L = ");
     Serial.print (cscmeasurement[4],HEX);
     Serial.print (" |Cuml Crank Revs = ");
     Serial.print (cuml_crank_revs);   
     Serial.print (" | Cuml Crank Revs H = ");
     Serial.print (cscmeasurement[1],HEX);
     Serial.print (" | Cuml Crank Revs L = ");
     Serial.print (cscmeasurement[2],HEX);
               
     time_now = millis();
     last_crank_time = current_crank_time;                       // Reset last crank time with current 
     cscMeasurementCharacteristics.setValue(cscmeasurement, 5);  // Pull latest csc measurement array values
     cscMeasurementCharacteristics.notify();                     // BLE notify latest csc measurement array values
     }

  // _____________
  // NOTIFY ON THE SECOND SECTION
  // ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
  
  if (millis() >= time_second + 1000)                            // Things that notify or update every second(ish)
     {
     Serial.println(" ");
     Serial.print(" TIME SECOND PULSE ");
     Serial.print(time_second);
     
     // HEART SECTION
     bpm = random(95, 125);                                      // Pretend heartbeat
     Serial.print(" | BPM = ");
     Serial.println(bpm);
     heart[1] = (byte)bpm;                                       // Update heart array with latest bpm and energy
     int energyUsed = 3000;                                      //                       "
     heart[3] = energyUsed / 256;                                //                       "
     heart[2] = energyUsed - (heart[2] * 256);                   //                       "

     // Update Heart over BLE
     
     heartRateMeasurementCharacteristics.setValue(heart, 8);     // Pull latest heart array values
     heartRateMeasurementCharacteristics.notify();               // BLE notify latest heart array values
     time_second = millis();                                     // Reset on the second time stamp
     }
 
 delay(100);                                                     // Wait 10 milliseconds for BLE operations to complete, avoiding the ESP32 crashing
}
