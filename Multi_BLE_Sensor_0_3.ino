/*
    Multi BLE Sensor 0.3 - Richard Hedderly 2019

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
byte crankrev;                                           // Cadence RPM
//uint16_t lastcrank;                                    // Last crank time
byte lastcrank1;
byte lastcrank2;
byte lastcrank3;
byte speedkph;                                           // Speed in KPH
byte speedmph;                                           // Speed in MPH 
byte heart[8] = {0b00001110, 60, 0, 0, 0 , 0, 0, 0};
byte cscmeasurement[5] = {0b00000010, 0, 0, 0 ,0};
byte cscfeature[1] = {0b0000000000000010}; 
byte sensorlocation[1] = {6};             

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
BLECharacteristic scControlPointCharacteristics(BLEUUID((uint16_t)0x2A55), BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE); // SC Control Point Characteristic
BLEDescriptor cscMeasurementDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description
BLEDescriptor cscFeatureDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description
BLEDescriptor sensorLocationDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description
BLEDescriptor scControlPointDescriptor(BLEUUID((uint16_t)0x2901));  //0x2901 is a custom user description

// 0x2A5B - Notify -  Client Charactaristic Config is 1 - DONE                  : CSC Measurement
// 0x2A5C - Read - 0x200 - DONE                                                 : CSC Feature 
// 0x2A5D - Read - None or 0x06 when active - Done                              : Sensor Location - 6 is right crank
// 0x2A55 - Write Indicate -  Client Characteristic Config is 2 - NOT REQUIRED  : SC Control Point

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
  pSpeed->addCharacteristic(&cscMeasurementCharacteristics);
  pSpeed->addCharacteristic(&cscFeatureCharacteristics);
  pSpeed->addCharacteristic(&sensorLocationCharacteristics);
  pSpeed->addCharacteristic(&scControlPointCharacteristics);
  cscMeasurementDescriptor.setValue("Exercise Bike CSC Measurement");
  cscMeasurementCharacteristics.addDescriptor(&cscMeasurementDescriptor);
  cscFeatureDescriptor.setValue("Exercise Bike CSC Feature");
  cscFeatureCharacteristics.addDescriptor(&cscFeatureDescriptor);
  sensorLocationDescriptor.setValue("Exercise Bike CSC Sensor Location");
  sensorLocationCharacteristics.addDescriptor(&sensorLocationDescriptor);
  scControlPointDescriptor.setValue("Exercise Bike CSC SC Control Point");
  scControlPointCharacteristics.addDescriptor(&scControlPointDescriptor);
  
  // Add UUIDs for Services to BLE Service Advertising
  pServer->getAdvertising()->addServiceUUID(heartRateService);
  pServer->getAdvertising()->addServiceUUID(speedService);

  // Start p Instances
  pSpeed->start();
  pHeart->start();

  
  // Placeholding compatibility advertising code to merge
  //pAdvertising->setScanResponse(true);
  //pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  //pAdvertising->setMinPreferred(0x12);
  
  // Start Advertising
  pServer->getAdvertising()->start();

}

void setup() {
  Serial.begin(115200);
  Serial.println("Start");
  InitBLE();
  bpm = 1;
  cscFeatureCharacteristics.setValue(cscfeature, 1);
  sensorLocationCharacteristics.setValue(sensorlocation, 1); 
}

void loop() {
 
  // _____________
  // HEART SECTION
  // ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
  // Read Heart Pins
  // *****
   
  // Stand-in Heart Sensor Grips
  heart[1] = (byte)bpm;
  int energyUsed = 3000;                        // 3000
  heart[3] = energyUsed / 256;                  // 3000/ 256 = 11.71875
  heart[2] = energyUsed - (heart[2] * 256);     // 3000 - (0 * 256) = 3000 
  Serial.print("BPM = ");
  Serial.println(bpm);
  
  // Send Heart Rate Notify
  heartRateMeasurementCharacteristics.setValue(heart, 8);
  heartRateMeasurementCharacteristics.notify();

  bpm++;

  // _______________________
  // SPEED + CADENCE SECTION
  // ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅ ̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅̅
  // Read Cadence RPM Pins

  // Stand-in Cadence Reed Switch
  crankrev = crankrev + 2;   // Increase revolution counter
  //lastcrank = 2048;

  // Calculate Speed from cadence 
  //
  
  // Set and Send Speed and Cadence Notify

  lastcrank1 = 0;
  lastcrank2 = 8;
  lastcrank3 = 0;
  
  cscmeasurement[1] = crankrev;
  cscmeasurement[2] = lastcrank1;
  cscmeasurement[3] = lastcrank2;
  cscmeasurement[4] = lastcrank3;
  cscMeasurementCharacteristics.setValue(cscmeasurement, 5);
  cscMeasurementCharacteristics.notify();
  
  Serial.print(cscmeasurement[1]); // Crank rev counter
  Serial.print(" ");
  Serial.print(cscmeasurement[2]); // Byte 1 of lastcrank
  Serial.print(" ");
  Serial.println(cscmeasurement[3]); // Byte 2 of lastcrank
  Serial.println(" ");
  Serial.println(cscmeasurement[4]); // Byte 3 of lastcrank
  Serial.println(" ");
  delay(2048);
}
