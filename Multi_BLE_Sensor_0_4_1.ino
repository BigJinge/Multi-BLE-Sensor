/*
    Multi BLE Sensor 0.4.1 - Richard Hedderly 2019

    Based on heart sensor code by Andreas Spiess which was based on a Neil Kolban example.
    
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h> 

byte flags = 0b00111110;
byte bpm;                                                // Beats Per Minute
byte crankrev1;
byte crankrev2;                                          // Cadence RPM
//uint16_t lastcrank;                                    // Last crank time
byte lastcrank1;
byte lastcrank2;
byte lastcrank3;
byte speedkph;                                           // Speed in KPH
byte speedmph;                                           // Speed in MPH 
byte heart[8] = {0b00001110, 60, 0, 0, 0 , 0, 0, 0};
byte cscmeasurement[5] = {0b00000010, 0, 0, 0, 0};       // Binary to select cadence only, 2 bytes for cumlat crank revs, 2 for last event time  
byte cscfeature[1] = {0b0000000000000010};               // Crank revolution supported only
byte sensorlocation[1] = {6};                            // Sensor on right crank
//byte sccontrol[1] = {2};                                 // SC Control - OPTIONAL            

bool _BLEClientConnected = false;

// Define Heart Properties
#define heartRateService BLEUUID((uint16_t)0x180D)       // Define BLE Heart Rate Service
BLECharacteristic heartRateMeasurementCharacteristics(BLEUUID((uint16_t)0x2A37), BLECharacteristic::PROPERTY_NOTIFY); // Heart Rate Characteristics
BLEDescriptor heartRateDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description

// Define Speed and Cadence Properties
#define speedService BLEUUID((uint16_t)0x1816)
BLECharacteristic cscMeasurementCharacteristics(BLEUUID((uint16_t)0x2A5B), BLECharacteristic::PROPERTY_NOTIFY); //CSC Measurement Characteristic
BLECharacteristic cscFeatureCharacteristics(BLEUUID((uint16_t)0x2A5C), BLECharacteristic::PROPERTY_READ); //CSC Feature Characteristic
BLECharacteristic sensorLocationCharacteristics(BLEUUID((uint16_t)0x2A5D), BLECharacteristic::PROPERTY_READ); //Sensor Location Characteristic
//BLECharacteristic scControlPointCharacteristics(BLEUUID((uint16_t)0x2A55), BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE); // SC Control Point Characteristic
BLEDescriptor cscMeasurementDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description - OPTIONAL
BLEDescriptor cscFeatureDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description
BLEDescriptor sensorLocationDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description
//BLEDescriptor scControlPointDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description - OPTIONAL

// 0x2A5B - Notify -  Client Charactaristic Config is 1 - DONE                  : CSC Measurement
// 0x2A5C - Read - 0x200 - DONE                                                 : CSC Feature 
// 0x2A5D - Read - None or 0x06 when active - DONE                              : Sensor Location - 6 is right crank
// 0x2A55 - Write Indicate -  Client Characteristic Config is 2 - OPTIONAL      : SC Control Point

// Define Battery
// Service UUID 0x180F - Battery Level UUID 0x2A19

// Define Firmware
// UUID 0x2A26


class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      _BLEClientConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      _BLEClientConnected = false;
    }
};

void InitBLE() {
  // Create BLE Device
  BLEDevice::init("Multi Fitness BLE Sensor");

  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create BLE Heart Configuration
  BLEService *pHeart = pServer->createService(heartRateService);  // Create Heart Service
  pHeart->addCharacteristic(&heartRateMeasurementCharacteristics);
  heartRateDescriptor.setValue("Exercise Bike Pulse Grips Rate");
  heartRateMeasurementCharacteristics.addDescriptor(&heartRateDescriptor);
  heartRateMeasurementCharacteristics.addDescriptor(new BLE2902());
    
  // Create Speed and Cadence Configuration
  BLEService *pSpeed = pServer->createService(speedService); // Create Speed and Cadence Service

  //pSpeed->addCharacteristic(&scControlPointCharacteristics); - OPTIONAL
  pSpeed->addCharacteristic(&sensorLocationCharacteristics);
  pSpeed->addCharacteristic(&cscFeatureCharacteristics);
  pSpeed->addCharacteristic(&cscMeasurementCharacteristics);
 
  //scControlPointDescriptor.setValue("Exercise Bike CSC SC Control Point"); - OPTIONAL
  //scControlPointCharacteristics.addDescriptor(&scControlPointDescriptor); - OPTIONAL
  sensorLocationDescriptor.setValue("Exercise Bike CSC Sensor Location");
  sensorLocationCharacteristics.addDescriptor(&sensorLocationDescriptor);
  cscFeatureDescriptor.setValue("Exercise Bike CSC Feature");
  cscFeatureCharacteristics.addDescriptor(&cscFeatureDescriptor);
  cscMeasurementDescriptor.setValue("Exercise Bike CSC Measurement");
  cscMeasurementCharacteristics.addDescriptor(&cscMeasurementDescriptor);
   
  // Add UUIDs for Services to BLE Service Advertising
  pServer->getAdvertising()->addServiceUUID(heartRateService);
  pServer->getAdvertising()->addServiceUUID(speedService);

  // Start p Instances
  pSpeed->start();
  pHeart->start();
 
  // Start Advertising
  pServer->getAdvertising()->start();

}

void setup() {
  Serial.begin(115200);
  InitBLE();
  bpm = 1;
  crankrev1 = 70;  // Hex 0x46 - Reflect Wahoo Cadence starting value
  crankrev2 = 2;   // Hex 0x02 -            ""          "" 
  cscFeatureCharacteristics.setValue(cscfeature, 1);
  sensorLocationCharacteristics.setValue(sensorlocation, 1);
  //scControlPointCharacteristics.setValue(sccontrol, 1); - OPTIONAL
}

void loop() {
 
  // _____________
  // HEART SECTION
  // ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
  // Read Heart Pins
  // *****
   
  // Stand-in Heart Sensor Grips
  heart[1] = (byte)bpm;
  int energyUsed = 3000;                        
  heart[3] = energyUsed / 256;                  
  heart[2] = energyUsed - (heart[2] * 256);      
  Serial.print("BPM = ");
  Serial.println(bpm);
  
  // Send Heart Rate Notify
  heartRateMeasurementCharacteristics.setValue(heart, 8);
  heartRateMeasurementCharacteristics.notify();

  bpm++; // Pretend heartbeat

  // _______________________
  // SPEED + CADENCE SECTION
  // ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
  // Read Cadence RPM Pins 

  // Stand-in Cadence Reed Switch

  // Calculate Speed from cadence 
  //
  
  // Set and Send Speed and Cadence Notify

  crankrev2++;   // Pretend pedaling
  
  // Reflect Wahoo Cadence sensor values - These are a constant revolution time
  lastcrank1 = 227;  // Hex 0xE3 
  lastcrank2 = 232;  // Hex 0xE8
    
  //Populate CSC measurement array and notify
  cscmeasurement[1] = crankrev1;
  cscmeasurement[2] = crankrev2;
  cscmeasurement[3] = lastcrank1;
  cscmeasurement[4] = lastcrank2;
  cscMeasurementCharacteristics.setValue(cscmeasurement, 5);
  cscMeasurementCharacteristics.notify();
  
  Serial.print(cscmeasurement[1]); // crank rev byte 1
  Serial.print(" ");
  Serial.print(cscmeasurement[2]); // crank rev byte 2
  Serial.print(" ");
  Serial.print(cscmeasurement[3]); // lastcrank byte 1
  Serial.print(" ");
  Serial.println(cscmeasurement[4]); // lastcrank byte 2
  Serial.println(" ");
  delay(2048);                    // Wait 2 seconds
}
